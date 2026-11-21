#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "VoxDsp.h"

class VoxProcessor final:public juce::AudioProcessor {
public:
    VoxProcessor();
    ~VoxProcessor() override;
    juce::AudioProcessorValueTreeState params;
    void prepareToPlay(double,int) override;
    void releaseResources() override{}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorParameter* getBypassParameter() const override{return cachedBypass;}
    const juce::String getName() const override{return "VoxRestore";}
    bool acceptsMidi() const override{return false;}
    bool producesMidi() const override{return false;}
    double getTailLengthSeconds() const override{return .15;}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {};}
    void changeProgramName(int,const juce::String&) override{}
    bool hasEditor() const override{return true;}
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void loadReference(const juce::File&);
    void reanalyseReference();
    void clearReference();
    void analyseSource(const juce::File&);
    void exportVoice(const juce::File&,const juce::File&);
    void serviceJobs();
    bool canReanalyse() const{return referenceFile.existsAsFile();}
    bool referenceReady() const;
    VoiceProfile referenceSnapshot() const;
    RestoreControls controls() const;
    juce::String referenceName() const;
    juce::String statusText() const;
    void delayNextAnalysisForTest(int milliseconds){testDelay.store(milliseconds);}
    bool analysisStartedForTest() const;
    std::atomic<bool> busy{false},standalone{false};
    std::atomic<float> progress{0},inputLevel{0},outputLevel{0},repairLevel{0},pitch{0},confidence{0};
private:
    struct Mailbox;
    struct JobState;
    void beginAnalysis(const juce::File&,bool reference);
    void runAudio(juce::AudioBuffer<float>&,bool bypass);
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    std::shared_ptr<Mailbox> mailbox;
    std::shared_ptr<JobState> activeJob;
    std::unique_ptr<juce::ThreadPool> jobs;
    VoxEngine engine;
    std::array<std::atomic<float>*,7> parameterValues{};
    juce::AudioProcessorParameter* cachedBypass=nullptr;
    uint64_t appliedRevision=0;
    mutable juce::CriticalSection statusLock;
    juce::String status="LOAD A CLEAN REFERENCE / OR USE FREE RESTORATION";
    juce::File referenceFile;
    std::atomic<int> testDelay{0};
};
