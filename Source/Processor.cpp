#include "Processor.h"
#include "Editor.h"
#include "AsyncCleanup.h"

namespace {
class FunctionJob:public juce::ThreadPoolJob {
public:explicit FunctionJob(std::function<void()> fn):ThreadPoolJob("VoxRestore file analysis"),function(std::move(fn)){}
    JobStatus runJob() override{function();return jobHasFinished;}
private:std::function<void()> function;
};
}
struct VoxProcessor::Mailbox {
    mutable juce::SpinLock lock;
    VoiceProfile reference,source;
    juce::String label="NO REFERENCE LOADED";
    std::atomic<uint64_t> revision{1},ticket{0};
    std::atomic<bool> ready{false};
};
struct VoxProcessor::JobState {
    std::atomic<bool> cancel{false},finished{false},started{false};std::atomic<float> progress{0};
    uint64_t ticket=0;juce::String message;
};
juce::AudioProcessorValueTreeState::ParameterLayout VoxProcessor::layout(){
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"restore",1},"Restore",juce::NormalisableRange<float>(0,100,.1f),65));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"dewater",1},"Dewater",juce::NormalisableRange<float>(0,100,.1f),45));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"rebuild",1},"Rebuild",juce::NormalisableRange<float>(0,100,.1f),25));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"mix",1},"Mix",juce::NormalisableRange<float>(0,100,.1f),100));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1},"Output",juce::NormalisableRange<float>(-12,12,.1f),0));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"reference",1},"Reference match",false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"skin",1},"Faceplate",juce::StringArray{"Ivory","Graphite","Walnut"},2));
    return result;
}
VoxProcessor::VoxProcessor():AudioProcessor(BusesProperties().withInput("Voice",juce::AudioChannelSet::stereo(),true).withOutput("Restored voice",juce::AudioChannelSet::stereo(),true)),params(*this,nullptr,"VoxRestoreState",layout()),mailbox(std::make_shared<Mailbox>()){
    const char* ids[]{"restore","dewater","rebuild","mix","output","reference","bypass"};for(size_t i=0;i<parameterValues.size();++i)parameterValues[i]=params.getRawParameterValue(ids[i]);cachedBypass=params.getParameter("bypass");
}
VoxProcessor::~VoxProcessor(){if(activeJob)activeJob->cancel.store(true);if(jobs){auto* pool=jobs.release();deferVoxRestoreCleanup([pool]{pool->removeAllJobs(true,-1);delete pool;});}}
bool VoxProcessor::isBusesLayoutSupported(const BusesLayout& buses) const {auto out=buses.getMainOutputChannelSet();return buses.getMainInputChannelSet()==out&&(out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo());}
void VoxProcessor::prepareToPlay(double rate,int){engine.prepare(rate,getTotalNumOutputChannels());setLatencySamples(engine.latency());appliedRevision=0;}
RestoreControls VoxProcessor::controls() const{auto value=[this](size_t index){return parameterValues[index]->load(std::memory_order_relaxed);};RestoreControls result;result.restore=value(0)*.01f;result.dewater=value(1)*.01f;result.rebuild=value(2)*.01f;result.mix=value(3)*.01f;result.outputDb=value(4);result.match=value(5)>.5f;result.bypass=value(6)>.5f;return result;}
void VoxProcessor::runAudio(juce::AudioBuffer<float>& audio,bool bypass){
    if(appliedRevision!=mailbox->revision.load(std::memory_order_acquire)){const juce::SpinLock::ScopedTryLockType lock(mailbox->lock);if(lock.isLocked()){engine.setReference(mailbox->reference);engine.setSource(mailbox->source);appliedRevision=mailbox->revision.load();}}
    auto settings=controls();settings.bypass|=bypass;engine.process(audio,settings);inputLevel.store(engine.inputMeter());outputLevel.store(engine.outputMeter());repairLevel.store(engine.repairMeter());pitch.store(engine.pitchHz());confidence.store(engine.voicedConfidence());
}
void VoxProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&){runAudio(audio,false);}
void VoxProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer&){runAudio(audio,true);}
juce::AudioProcessorEditor* VoxProcessor::createEditor(){return new VoxEditor(*this);}
VoiceProfile VoxProcessor::referenceSnapshot() const{const juce::SpinLock::ScopedLockType lock(mailbox->lock);return mailbox->reference;}
bool VoxProcessor::referenceReady() const{return mailbox->ready.load();}
bool VoxProcessor::analysisStartedForTest() const{return activeJob&&activeJob->started.load();}
juce::String VoxProcessor::referenceName() const{const juce::SpinLock::ScopedLockType lock(mailbox->lock);return mailbox->label;}
juce::String VoxProcessor::statusText() const{const juce::ScopedLock lock(statusLock);return status;}
void VoxProcessor::getStateInformation(juce::MemoryBlock& dest){auto state=params.copyState();state.removeChild(state.getChildWithName("VoiceReference"),nullptr);auto model=profileState(referenceSnapshot());if(model.isValid()){model.setProperty("label",referenceName(),nullptr);state.appendChild(model,nullptr);}copyXmlToBinary(*state.createXml(),dest);}
void VoxProcessor::setStateInformation(const void* data,int bytes){if(bytes<0||bytes>1024*1024)return;if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName(params.state.getType())){
    auto state=juce::ValueTree::fromXml(*xml);VoiceProfile p;auto model=state.getChildWithName("VoiceReference");readProfileState(model,p);++mailbox->ticket;
    {const juce::SpinLock::ScopedLockType lock(mailbox->lock);mailbox->reference=p;mailbox->source={};mailbox->label=p.valid()?model["label"].toString().substring(0,256):"NO REFERENCE LOADED";mailbox->ready.store(p.valid());++mailbox->revision;}
    params.replaceState(state);referenceFile=juce::File{};const juce::ScopedLock lock(statusLock);status=p.valid()?"REFERENCE RESTORED / ORIGINAL FILE NOT REQUIRED":"FREE RESTORATION / LOAD A CLEAN REFERENCE";
}}
void VoxProcessor::beginAnalysis(const juce::File& file,bool reference){
    if(busy.exchange(true))return;auto state=std::make_shared<JobState>();state->ticket=++mailbox->ticket;activeJob=state;progress.store(0);auto shared=mailbox;int delay=testDelay.exchange(0);
    if(reference){referenceFile=file;params.getParameter("reference")->setValueNotifyingHost(1);}
    {const juce::ScopedLock lock(statusLock);status=reference?"SCANNING WHOLE REFERENCE / FINDING SPEECH":"SCANNING WHOLE VOICE TRACK / MAPPING TONE";}
    if(!jobs)jobs=std::make_unique<juce::ThreadPool>(1);
    jobs->addJob(new FunctionJob([state,shared,file,reference,delay]{try{
        state->started.store(true);if(delay>0)juce::Thread::sleep(delay);if(state->cancel.load())throw std::runtime_error("Cancelled.");auto profile=analyseVoice(file,&state->progress,&state->cancel);
        {const juce::SpinLock::ScopedLockType lock(shared->lock);if(state->ticket==shared->ticket.load()&&!state->cancel.load()){if(reference){shared->reference=profile;shared->label=file.getFileName();shared->ready.store(true);}else shared->source=profile;++shared->revision;}}
        state->message=(reference?"REFERENCE READY / ":"TRACK READY / ")+juce::String(profile.speechSeconds,1)+"S OF SPEECH ANALYZED";
    }catch(const std::exception& error){state->message=juce::String("ANALYSIS: ")+error.what();}state->finished.store(true,std::memory_order_release);}),true);
}
void VoxProcessor::loadReference(const juce::File& file){beginAnalysis(file,true);}
void VoxProcessor::analyseSource(const juce::File& file){beginAnalysis(file,false);}
void VoxProcessor::reanalyseReference(){if(referenceFile.existsAsFile())loadReference(referenceFile);}
void VoxProcessor::clearReference(){if(activeJob)activeJob->cancel.store(true);++mailbox->ticket;params.getParameter("reference")->setValueNotifyingHost(0);{const juce::SpinLock::ScopedLockType lock(mailbox->lock);mailbox->reference={};mailbox->label="NO REFERENCE LOADED";mailbox->ready.store(false);++mailbox->revision;}referenceFile=juce::File{};const juce::ScopedLock lock(statusLock);status="REFERENCE CLEARED / FREE RESTORATION";}
void VoxProcessor::serviceJobs(){auto state=activeJob;if(!state)return;progress.store(state->progress.load());if(!state->finished.load(std::memory_order_acquire))return;if(state->ticket==mailbox->ticket.load()){const juce::ScopedLock lock(statusLock);status=state->message;}activeJob.reset();busy.store(false);}
void VoxProcessor::exportVoice(const juce::File& input,const juce::File& output){
    if(busy.exchange(true))return;auto state=std::make_shared<JobState>();state->ticket=++mailbox->ticket;activeJob=state;progress.store(0);auto profile=referenceSnapshot();auto settings=controls();
    {const juce::ScopedLock lock(statusLock);status="PRINTING RESTORED VOICE / FULL TRACK ANALYSIS";}
    if(!jobs)jobs=std::make_unique<juce::ThreadPool>(1);jobs->addJob(new FunctionJob([state,input,output,profile,settings]{try{renderVoice(input,output,profile,settings,&state->progress,&state->cancel);state->message="PRINT COMPLETE / "+output.getFileName();}catch(const std::exception& error){state->message=juce::String("PRINT: ")+error.what();}state->finished.store(true,std::memory_order_release);}),true);
}
