#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>
#include <atomic>
#include <fstream>
#define NOMINMAX
#include <windows.h>

struct Clock:juce::AudioPlayHead {int64_t samples=0;bool playing=true;juce::Optional<PositionInfo> getPosition()const override{PositionInfo p;p.setIsPlaying(playing);p.setTimeInSamples(samples);p.setTimeInSeconds(samples/48000.);return p;}};
class Lab:public juce::JUCEApplication {
    juce::StringArray args;juce::File logFile,reportFile;double origin=0;
    void log(const juce::String& text){logFile.appendText(juce::String(juce::Time::getMillisecondCounterHiRes()-origin,2)+" "+text+"\n");}
    juce::String option(const juce::String& id,const juce::String& fallback={}){int i=args.indexOf(id);return i>=0&&i+1<args.size()?args[i+1]:fallback;}
    void run(){
        try {
            juce::VST3PluginFormat format;std::vector<juce::File> files;
            for(const char* id:{"--a","--b","--c"})if(option(id).isNotEmpty())files.emplace_back(option(id));
            if(files.empty())throw std::runtime_error("Supply --a plugin.vst3");
            int count=option("--instances","8").getIntValue(),cycles=option("--cycles","2").getIntValue();
            double maxRemove=0,maxClose=0,maxGui=0;juce::Array<juce::var> details;Clock clock;
            for(int cycle=0;cycle<cycles;++cycle){
                log("cycle "+juce::String(cycle)+" create");
                std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins;
                std::vector<std::unique_ptr<juce::AudioProcessorEditor>> editors;
                std::vector<std::unique_ptr<juce::DocumentWindow>> windows;
                for(const auto& file:files){
                    juce::OwnedArray<juce::PluginDescription> desc;format.findAllTypesForFile(desc,file.getFullPathName());if(desc.isEmpty())throw std::runtime_error("Discovery failed");
                    for(int j=0;j<count;++j){juce::String error;auto p=format.createInstanceFromDescription(*desc[0],48000,128,error);if(!p)throw std::runtime_error(error.toStdString());p->setPlayHead(&clock);p->setRateAndBufferSizeDetails(48000,128);p->prepareToPlay(48000,128);plugins.push_back(std::move(p));}
                }
                log("created "+juce::String((int)plugins.size()));
                if(args.contains("--gui"))for(auto& p:plugins){auto e=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditorIfNeeded());if(!e)throw std::runtime_error("Editor failed");auto w=std::make_unique<juce::DocumentWindow>(p->getName(),juce::Colours::black,0);w->setUsingNativeTitleBar(true);w->setContentNonOwned(e.get(),true);w->setTopLeftPosition(-4000,-4000);w->setVisible(true);editors.push_back(std::move(e));windows.push_back(std::move(w));}
                juce::MessageManager::getInstance()->runDispatchLoopUntil(250);log("editors ready");
                juce::MidiBuffer midi;juce::AudioBuffer<float> audio(4,128);juce::Random random(271);double phase=0;
                for(int block=0;block<300;++block){clock.samples=(int64_t)block*128;for(auto& p:plugins){audio.setSize(std::max(2,std::max(p->getTotalNumInputChannels(),p->getTotalNumOutputChannels())),128,false,false,true);for(int i=0;i<128;++i){float sample=.025f*std::sin((float)phase)+.006f*(random.nextFloat()-.5f);phase+=2*juce::MathConstants<double>::pi*185/48000;for(int c=0;c<audio.getNumChannels();++c)audio.setSample(c,i,sample);}p->processBlock(audio,midi);}if(block%30==0)juce::MessageManager::getInstance()->runDispatchLoopUntil(1);}
                juce::MessageManager::getInstance()->runDispatchLoopUntil(option("--settle-ms","300").getIntValue());log("close editors");
                double started=juce::Time::getMillisecondCounterHiRes();
                for(auto& w:windows){w->setVisible(false);w->clearContentComponent();}editors.clear();windows.clear();
                maxGui=std::max(maxGui,juce::Time::getMillisecondCounterHiRes()-started);log("editors closed");
                for(auto& p:plugins){p->releaseResources();p->setPlayHead(nullptr);}
                started=juce::Time::getMillisecondCounterHiRes();
                auto remove=[&]{for(size_t n=0;n<plugins.size();++n){auto start=juce::Time::getMillisecondCounterHiRes();plugins[n].reset();double elapsed=juce::Time::getMillisecondCounterHiRes()-start;maxRemove=std::max(maxRemove,elapsed);log("removed "+juce::String((int)n)+" / "+juce::String(elapsed,2)+" ms");}plugins.clear();};
                // A background project-close request must marshal ALL VST3
                // interface release and module destruction onto the UI thread.
                // Only process/setProcessing are exempt from that SDK rule.
                if(args.contains("--worker-close")){std::atomic<bool> done{false};std::thread t([&]{juce::MessageManager::callSync(remove);done.store(true);});while(!done.load())juce::MessageManager::getInstance()->runDispatchLoopUntil(10);t.join();}else remove();
                double close=juce::Time::getMillisecondCounterHiRes()-started;maxClose=std::max(maxClose,close);log("project closed");
                juce::MessageManager::getInstance()->runDispatchLoopUntil(500);
                for(const auto& file:files){auto binary=file.isDirectory()?file.getChildFile("Contents/x86_64-win/"+file.getFileName()):file;auto* row=new juce::DynamicObject;row->setProperty("cycle",cycle);row->setProperty("plugin",file.getFullPathName());row->setProperty("module_still_mapped",GetModuleHandleW(binary.getFullPathName().toWideCharPointer())!=nullptr);details.add(juce::var(row));}
            }
            auto* report=new juce::DynamicObject;report->setProperty("passed",true);report->setProperty("instances_per_plugin",count);report->setProperty("cycles",cycles);report->setProperty("gui",args.contains("--gui"));report->setProperty("worker_close",args.contains("--worker-close"));report->setProperty("max_single_removal_ms",maxRemove);report->setProperty("max_project_removal_ms",maxClose);report->setProperty("max_editor_close_ms",maxGui);report->setProperty("module_observations",details);reportFile.replaceWithText(juce::JSON::toString(juce::var(report)));log("all tests returned; app quit");
        }catch(const std::exception& e){log("ERROR "+juce::String(e.what()));reportFile.replaceWithText("{\"passed\":false}");setApplicationReturnValue(1);}
        quit();
    }
public:
    const juce::String getApplicationName()override{return "VST Teardown Lab";}
    const juce::String getApplicationVersion()override{return "1";}
    bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String& command)override{args=juce::StringArray::fromTokens(command,true);for(auto& a:args)a=a.unquoted();origin=juce::Time::getMillisecondCounterHiRes();logFile=juce::File(option("--log"));reportFile=juce::File(option("--report"));logFile.replaceWithText("");log("start");juce::MessageManager::callAsync([this]{run();});}
    void shutdown()override{log("JUCE shutdown returned");}
};
START_JUCE_APPLICATION(Lab)
