#include "Processor.h"
#include "Editor.h"
#include "OsTheme.h"
#include "TapeReaderSource.h"
#include "AsyncCleanup.h"
#include "Updater.h"
#if JUCE_MAC
#include "MacCapture.h"
#endif
#include <cstdio>

namespace {
void snapshot(const juce::File& file,juce::Component& component){
#if JUCE_MAC
    auto image=captureMacNativeView(component);
#else
    auto image=component.createComponentSnapshot(component.getLocalBounds(),true,1);
#endif
    juce::MemoryOutputStream bytes;juce::PNGImageFormat png;if(!png.writeImageToStream(image,bytes)||!file.replaceWithData(bytes.getData(),bytes.getDataSize()))throw std::runtime_error("Could not save interface preview.");}
bool waitJob(VoxProcessor& p,int seconds=90){auto end=juce::Time::getMillisecondCounterHiRes()+seconds*1000;while(p.busy.load()&&juce::Time::getMillisecondCounterHiRes()<end){p.serviceJobs();juce::MessageManager::getInstance()->runDispatchLoopUntil(10);}return !p.busy.load();}
juce::var workflow(const juce::File& input,const juce::File& ref,const juce::File& output){
    VoxProcessor processor;processor.prepareToPlay(48000,64);processor.loadReference(ref);auto end=juce::Time::getMillisecondCounterHiRes()+90000;bool monotonic=true;float last=0;double peak=0;
    // Intentionally do not create an editor or service its timer while loading.
    while(!processor.referenceReady()&&juce::Time::getMillisecondCounterHiRes()<end){juce::AudioBuffer<float> block(2,64);block.clear();juce::MidiBuffer midi;auto begin=juce::Time::getMillisecondCounterHiRes();processor.processBlock(block,midi);peak=std::max(peak,juce::Time::getMillisecondCounterHiRes()-begin);juce::Thread::sleep(2);}
    bool withoutEditor=processor.referenceReady();if(!waitJob(processor))throw std::runtime_error("Reference analysis did not finish.");auto profile=processor.referenceSnapshot();juce::MemoryBlock state;processor.getStateInformation(state);VoxProcessor restored;restored.setStateInformation(state.getData(),(int)state.getSize());bool saved=restored.referenceReady()&&restored.referenceSnapshot().power==profile.power&&!restored.canReanalyse();
    processor.reanalyseReference();bool manual=processor.busy.load()&&waitJob(processor)&&processor.referenceReady();
    processor.exportVoice(input,output);end=juce::Time::getMillisecondCounterHiRes()+90000;
    while(processor.busy.load()&&juce::Time::getMillisecondCounterHiRes()<end){processor.serviceJobs();float next=processor.progress.load();monotonic&=next+.00001f>=last;last=next;juce::Thread::sleep(2);}
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(input)),b(formats.createReaderFor(output));bool aligned=a&&b&&a->lengthInSamples==b->lengthInSamples&&a->sampleRate==b->sampleRate&&a->numChannels==b->numChannels&&a->bitsPerSample==b->bitsPerSample;
    processor.clearReference();processor.delayNextAnalysisForTest(150);processor.loadReference(ref);auto started=juce::Time::getMillisecondCounterHiRes()+1000;while(!processor.analysisStartedForTest()&&juce::Time::getMillisecondCounterHiRes()<started)juce::Thread::sleep(1);processor.clearReference();bool cancelled=waitJob(processor)&&!processor.referenceReady();
    auto* result=new juce::DynamicObject;result->setProperty("passed",withoutEditor&&saved&&manual&&aligned&&monotonic&&cancelled&&peak<100);result->setProperty("whole_reference_publishes_with_editor_closed",withoutEditor);result->setProperty("saved_profile_without_original_file",saved);result->setProperty("learn_reanalyses_whole_reference",manual);result->setProperty("export_preserves_length_rate_channels_depth",aligned);result->setProperty("progress_monotonic",monotonic);result->setProperty("cancelled_scan_cannot_restore_cleared_model",cancelled);result->setProperty("peak_callback_during_scan_ms",peak);return juce::var(result);
}
juce::var validate(const juce::File& file,const juce::File& ref){
    juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> found;format.findAllTypesForFile(found,file.getFullPathName());if(found.isEmpty())throw std::runtime_error("VoxRestore VST3 was not discovered.");juce::String error;auto plugin=format.createInstanceFromDescription(*found[0],48000,137,error);if(!plugin)throw std::runtime_error(error.toStdString());
    auto* result=new juce::DynamicObject;bool finite=true;double peakCallback=0,peakRemoval=0;juce::MemoryBlock modelState;
    VoxProcessor builder;if(ref.existsAsFile()){builder.loadReference(ref);if(!waitJob(builder)||!builder.referenceReady())throw std::runtime_error("Reference did not load in state builder.");}builder.getStateInformation(modelState);
    juce::XmlElement wrapper("VST3PluginState");wrapper.createNewChildElement("IComponent")->addTextElement(modelState.toBase64Encoding());juce::MemoryBlock hosted;juce::AudioProcessor::copyXmlToBinary(wrapper,hosted);
    plugin->setStateInformation(hosted.getData(),(int)hosted.getSize());juce::MemoryBlock saved;plugin->getStateInformation(saved);plugin->setStateInformation(saved.getData(),(int)saved.getSize());
    for(double rate:{44100.0,48000.0,96000.0}){plugin->setRateAndBufferSizeDetails(rate,137);plugin->prepareToPlay(rate,137);for(int block=0;block<120;++block){juce::AudioBuffer<float> audio(2,137);for(int i=0;i<137;++i){float sample=.04f*std::sin((float)(block*137+i)*juce::MathConstants<float>::twoPi*150/(float)rate);audio.setSample(0,i,sample);audio.setSample(1,i,-sample);}juce::MidiBuffer midi;auto begin=juce::Time::getMillisecondCounterHiRes();plugin->processBlock(audio,midi);peakCallback=std::max(peakCallback,juce::Time::getMillisecondCounterHiRes()-begin);for(int c=0;c<2;++c)for(int i=0;i<137;++i)finite&=std::isfinite(audio.getSample(c,i));}plugin->releaseResources();}
    {std::unique_ptr<juce::AudioProcessorEditor> view(plugin->createEditorIfNeeded());if(!view)throw std::runtime_error("VST3 editor did not open.");}
    for(int cycle=0;cycle<16;++cycle){auto trial=format.createInstanceFromDescription(*found[0],48000,64,error);if(!trial)throw std::runtime_error("VST3 lifecycle instance failed.");trial->setStateInformation(saved.getData(),(int)saved.getSize());trial->setNonRealtime(cycle%2!=0);trial->prepareToPlay(48000,64);juce::AudioBuffer<float> audio(2,64);audio.clear();juce::MidiBuffer midi;trial->processBlock(audio,midi);{std::unique_ptr<juce::AudioProcessorEditor> view(trial->createEditorIfNeeded());}trial->releaseResources();auto begin=juce::Time::getMillisecondCounterHiRes();trial.reset();peakRemoval=std::max(peakRemoval,juce::Time::getMillisecondCounterHiRes()-begin);}
    result->setProperty("passed",finite&&peakCallback<100&&peakRemoval<1000);result->setProperty("name",plugin->getName());result->setProperty("finite_audio",finite);result->setProperty("state_bytes",(int)saved.getSize());result->setProperty("add_restore_remove_cycles",16);result->setProperty("peak_callback_ms",peakCallback);result->setProperty("peak_removal_ms",peakRemoval);return juce::var(result);
}
}

class Deck final:public juce::AudioAppComponent,private juce::Timer {
public:
    Deck(){processor.standalone.store(true);juce::PropertiesFile::Options options;options.applicationName="VoxRestore";options.filenameSuffix="settings";options.folderName="VoxRestore";options.osxLibrarySubFolder="Application Support";preferences=std::make_unique<juce::PropertiesFile>(options);juce::MemoryBlock state;if(state.fromBase64Encoding(preferences->getValue("controls")))processor.setStateInformation(state.getData(),(int)state.getSize());editor=std::make_unique<VoxEditor>(processor);addAndMakeVisible(*editor);setSize(960,668);
        editor->onLoadVoice=[this]{chooser=std::make_unique<juce::FileChooser>("Load the damaged voice",juce::File{},"*.wav;*.aiff;*.aif;*.flac;*.mp3");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<Deck>(this)](const juce::FileChooser& result){if(safe&&result.getResult().existsAsFile())safe->load(result.getResult());});};
        editor->isPlaying=[this]{return transport.isPlaying();};editor->position=[this]{return transport.getCurrentPosition();};editor->duration=[this]{return transport.getLengthInSeconds();};editor->voiceName=[this]{return voiceFile.getFileName();};
        editor->onPlay=[this]{if(transport.isPlaying())transport.stop();else{if(transport.getCurrentPosition()>=transport.getLengthInSeconds()-.01)transport.setPosition(0);transport.start();}};editor->onStop=[this]{transport.stop();transport.setPosition(0);};editor->onStart=[this]{transport.setPosition(0);};editor->onLoop=[this]{loop=!loop;if(reader)reader->setLooping(loop);};
        editor->onExport=[this]{if(!voiceFile.existsAsFile())return;chooser=std::make_unique<juce::FileChooser>("Print restored voice to a new WAV",voiceFile.getSiblingFile(voiceFile.getFileNameWithoutExtension()+"-restored.wav"),"*.wav");chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<Deck>(this)](const juce::FileChooser& result){if(safe&&result.getResult()!=juce::File{})safe->processor.exportVoice(safe->voiceFile,result.getResult().withFileExtension("wav"));});};
        editor->onDevices=[this]{auto selector=std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,0,0,0,2,false,false,true,false);selector->setSize(500,420);juce::DialogWindow::LaunchOptions options;options.content.setOwned(selector.release());options.dialogTitle="VoxRestore patch bay";options.dialogBackgroundColour=OsTheme::background(juce::Desktop::getInstance().isDarkModeActive());options.useNativeTitleBar=true;options.escapeKeyTriggersCloseButton=true;options.launchAsync();};setAudioChannels(0,2);
        editor->onUpdates=[this]{showUpdatesMenu();};editor->onUpdateProgress=[this]{return std::pair<double,juce::String>{updater.progress.load(),updater.progressStage()};};startTimer(5000);
    }
    ~Deck() override{stopTimer();shutdownAudio();juce::MemoryBlock state;processor.getStateInformation(state);preferences->setValue("controls",state.toBase64Encoding());preferences->saveIfNeeded();editor.reset();transport.setSource(nullptr);if(readAhead.isThreadRunning())readAhead.stopThread(3000);}
    void load(const juce::File& file){juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> source(formats.createReaderFor(file));if(!source||source->numChannels>2)return;transport.stop();transport.setSource(nullptr);double rate=source->sampleRate;reader=std::make_unique<TapeReaderSource>(std::move(source));reader->setLooping(loop);if(!readAhead.isThreadRunning())readAhead.startThread();transport.setSource(reader.get(),32768,&readAhead,rate);voiceFile=file;processor.analyseSource(file);}
    void prepareToPlay(int block,double rate) override{processor.prepareToPlay(rate,block);transport.prepareToPlay(block,rate);}
    void releaseResources() override{transport.releaseResources();}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override{transport.getNextAudioBlock(info);juce::AudioBuffer<float> view(info.buffer->getArrayOfWritePointers(),std::min(2,info.buffer->getNumChannels()),info.startSample,info.numSamples);juce::MidiBuffer midi;processor.processBlock(view,midi);}
    void resized() override{if(editor)editor->setBounds(getLocalBounds());}
private:
    void timerCallback() override{startTimer(3600000);auto now=juce::Time::getCurrentTime().toMilliseconds(),last=preferences->getValue("lastUpdateCheck","0").getLargeIntValue();if(preferences->getBoolValue("automaticUpdates",true)&&now-last>=86400000)checkUpdates(false,false);}
    void showUpdatesMenu(){juce::PopupMenu menu;menu.addItem(1,"Check for updates...");menu.addItem(2,"Automatically check for updates",true,preferences->getBoolValue("automaticUpdates",true));menu.addItem(3,"Open release page");menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(editor.get()),[safe=juce::Component::SafePointer<Deck>(this)](int item){if(!safe)return;if(item==1)safe->checkUpdates(true,false);if(item==2){safe->preferences->setValue("automaticUpdates",!safe->preferences->getBoolValue("automaticUpdates",true));safe->preferences->saveIfNeeded();}if(item==3)juce::URL("https://github.com/lakam99/VoxRestore/releases").launchInDefaultBrowser();});}
    void checkUpdates(bool manual,bool prepare){
        if(updateBusy)return;updateBusy=true;editor->setUpdateBusy(true,prepare);
        updater.request(prepare,[safe=juce::Component::SafePointer<Deck>(this),manual,prepare](juce::var result){if(!safe)return;safe->updateBusy=false;safe->editor->setUpdateBusy(false);
            if(!(bool)result["passed"]){if(manual)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"VoxRestore updates",result["error"].toString());return;}
            safe->preferences->setValue("lastUpdateCheck",juce::String(juce::Time::getCurrentTime().toMilliseconds()));safe->preferences->saveIfNeeded();
            if(!(bool)result["available"]){if(manual)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"VoxRestore updates","You are running VoxRestore " VOX_VERSION ". No newer stable release is available.");return;}
#if JUCE_MAC
            auto options=juce::MessageBoxOptions{}.withTitle("VoxRestore update").withMessage("A newer Apple Silicon package is available. Open its release page?").withButton("Open release",1).withButton("Later",0);juce::AlertWindow::showAsync(options,[url=result["releaseUrl"].toString()](int choice){if(choice==1)juce::URL(url).launchInDefaultBrowser();});
#else
            auto options=juce::MessageBoxOptions{}.withTitle("VoxRestore "+result["version"].toString()).withMessage(prepare?"The package is downloaded and verified. Close DAWs using VoxRestore before installing.":"Download and verify this update?").withButton(prepare?"Install and restart":"Download update",1).withButton("Later",0);
            juce::AlertWindow::showAsync(options,[safe,prepare,result](int choice){if(!safe||choice!=1)return;if(!prepare){safe->checkUpdates(true,true);return;}if(safe->processor.busy.load()||safe->transport.isPlaying()){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"VoxRestore updates","Stop playback and wait for analysis or export before installing.");return;}juce::String error;if(VoxRestoreUpdater::installAndRestart(result,error))juce::JUCEApplication::getInstance()->systemRequestedQuit();else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"VoxRestore updates",error);});
#endif
        });
    }
    VoxRestoreUpdater updater;bool updateBusy=false;
    VoxProcessor processor;juce::AudioTransportSource transport;juce::TimeSliceThread readAhead{"VoxRestore playback"};std::unique_ptr<TapeReaderSource> reader;std::unique_ptr<VoxEditor> editor;std::unique_ptr<juce::FileChooser> chooser;std::unique_ptr<juce::PropertiesFile> preferences;juce::File voiceFile;bool loop=false;
};
class Window final:public juce::DocumentWindow {
public:Window():DocumentWindow("VoxRestore",OsTheme::background(juce::Desktop::getInstance().isDarkModeActive()),allButtons){setUsingNativeTitleBar(true);setContentOwned(new Deck,true);setResizable(false,false);setIcon(RasterAssets::get().icon);centreWithSize(getWidth(),getHeight());OsTheme::setNativeFrame(*this,juce::Desktop::getInstance().isDarkModeActive());setVisible(true);}void closeButtonPressed() override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
};
class VoxApplication final:public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override{return "VoxRestore";}const juce::String getApplicationVersion() override{return VOX_VERSION;}
    void initialise(const juce::String& command) override{
        auto args=juce::StringArray::fromTokens(command,true);args.trim();args.removeEmptyStrings();for(auto& arg:args)arg=arg.unquoted();auto option=[&](const char* key){int i=args.indexOf(key);return i>=0&&i+1<args.size()?args[i+1]:juce::String{};};
        if(!args.isEmpty()){
            juce::var report;try{
                if(args.contains("--self-test"))report=runVoxTests(juce::File(option("--folder")));
                else if(args.contains("--validate-plugin"))report=validate(juce::File(option("--validate-plugin")),juce::File(option("--reference")));
                else if(args.contains("--check-updates")){VoxRestoreUpdater updater;bool done=false;updater.request(false,[&](juce::var result){report=result;done=true;});auto end=juce::Time::getMillisecondCounterHiRes()+40000;while(!done&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(25);if(!done)throw std::runtime_error("Update check timed out.");}
                else if(args.contains("--updater-test")){auto updater=std::make_unique<VoxRestoreUpdater>();updater->delayNextRequestForTest(600);updater->request(false,{});auto end=juce::Time::getMillisecondCounterHiRes()+1000;while(!updater->startedForTest()&&juce::Time::getMillisecondCounterHiRes()<end)juce::Thread::sleep(1);bool started=updater->startedForTest();auto begin=juce::Time::getMillisecondCounterHiRes();updater.reset();double elapsed=juce::Time::getMillisecondCounterHiRes()-begin;bool deferred=pendingVoxRestoreCleanups()>0;end=juce::Time::getMillisecondCounterHiRes()+3000;while(pendingVoxRestoreCleanups()>0&&juce::Time::getMillisecondCounterHiRes()<end)juce::Thread::sleep(2);auto* result=new juce::DynamicObject;result->setProperty("passed",started&&deferred&&elapsed<350&&pendingVoxRestoreCleanups()==0);result->setProperty("slow_updater_retirement_ms",elapsed);result->setProperty("deferred_cleanup",deferred);report=juce::var(result);}
                else if(args.contains("--workflow")){int index=args.indexOf("--workflow");if(index+2>=args.size())throw std::runtime_error("Use --workflow voice.wav output.wav --reference clean.wav");report=workflow(juce::File(args[index+1].unquoted()),juce::File(option("--reference")),juce::File(args[index+2].unquoted()));}
                else if(args.contains("--process")){int index=args.indexOf("--process");if(index+2>=args.size())throw std::runtime_error("Use --process input.wav output.wav --reference clean.wav");VoiceProfile profile;if(!option("--reference").isEmpty())profile=analyseVoice(juce::File(option("--reference")));RestoreControls controls;auto value=[&](const char* id,float fallback){return option(id).isEmpty()?fallback:juce::jlimit(0.0f,1.0f,option(id).getFloatValue()*.01f);};controls.restore=value("--restore",controls.restore);controls.dewater=value("--dewater",controls.dewater);controls.rebuild=value("--rebuild",controls.rebuild);controls.mix=value("--mix",controls.mix);controls.match=!args.contains("--free");controls.bypass=args.contains("--bypass");renderVoice(juce::File(args[index+1].unquoted()),juce::File(args[index+2].unquoted()),profile,controls);auto* result=new juce::DynamicObject;result->setProperty("passed",true);result->setProperty("reference_speech_seconds",profile.speechSeconds);report=juce::var(result);}
                else if(args.contains("--render-ui")){VoxProcessor processor;processor.standalone.store(args.contains("--standalone"));processor.params.getParameter("skin")->setValueNotifyingHost(juce::jlimit(0,2,option("--skin").getIntValue())*.5f);if(!option("--reference").isEmpty()){processor.loadReference(juce::File(option("--reference")));if(!waitJob(processor))throw std::runtime_error("Reference scan timed out.");}VoxEditor editor(processor);juce::MessageManager::getInstance()->runDispatchLoopUntil(60);snapshot(juce::File(option("--render-ui")),editor);auto* result=new juce::DynamicObject;result->setProperty("passed",true);result->setProperty("reference_loaded",processor.referenceReady());report=juce::var(result);}
                else throw std::runtime_error("Unknown verification command.");
                if(report.hasProperty("passed")&&!(bool)report["passed"])setApplicationReturnValue(2);
            }catch(const std::exception& error){auto* result=new juce::DynamicObject;result->setProperty("passed",false);result->setProperty("error",juce::String(error.what()));report=juce::var(result);setApplicationReturnValue(1);}
            if(!option("--report").isEmpty())juce::File(option("--report")).replaceWithText(juce::JSON::toString(report,true));quit();return;
        }
        theme=std::make_unique<OsTheme>();window=std::make_unique<Window>();
    }
    void shutdown() override{window.reset();theme.reset();auto end=juce::Time::getMillisecondCounterHiRes()+2000;while(pendingVoxRestoreCleanups()>0&&juce::Time::getMillisecondCounterHiRes()<end)juce::Thread::sleep(5);}
private:std::unique_ptr<OsTheme> theme;std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(VoxApplication)
