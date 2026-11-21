#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>

constexpr int voiceBands=48;
using VoiceEnvelope=std::array<float,voiceBands>;
struct VoiceProfile {
    VoiceEnvelope power{}; // normalized long-term speech spectrum
    double speechSeconds=0;
    float dynamicRangeDb=0,crestDb=0,medianPitch=0;
    bool valid() const;
};
juce::ValueTree profileState(const VoiceProfile&);
bool readProfileState(const juce::ValueTree&,VoiceProfile&);
int voiceBand(double hz);
VoiceEnvelope spectralEnvelope(const std::vector<float>& power,double rate,int size);
VoiceProfile analyseVoice(const juce::File&,std::atomic<float>* progress=nullptr,std::atomic<bool>* cancel=nullptr,float start=0,float span=1);

struct RestoreControls {float restore=.65f,dewater=.45f,rebuild=.25f,mix=1,outputDb=0;bool match=true,bypass=false;};

class VoxEngine {
public:
    void prepare(double sampleRate,int channels);
    void setReference(const VoiceProfile& p){reference=p;}
    void setSource(const VoiceProfile& p){sourcePreset=p;sourceInitialised=false;}
    void process(juce::AudioBuffer<float>&,const RestoreControls&);
    int latency() const{return size;}
    float inputMeter() const{return meterIn;}
    float outputMeter() const{return meterOut;}
    float repairMeter() const{return meterRepair;}
    float pitchHz() const{return detectedPitch;}
    float voicedConfidence() const{return voicing;}
private:
    void frame(const RestoreControls&);
    void detectPitch(int channel,float rms);
    double rate=48000;
    int size=2048,hop=512,capacity=8192,channels=2,inputPos=0,outputPos=0,countdown=512;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window,power,bandPositions,pitchInput,pitchDifference;
    std::vector<int> envelopeBand;
    std::array<int,voiceBands> envelopeCount{};
    std::array<std::vector<float>,2> input,output,data,excitation,previousMagnitude,previousPhase,phaseAdvance;
    VoiceProfile reference,sourcePreset;
    VoiceEnvelope sourceEnvelope{},recentEnvelope{},bandGain{};
    bool sourceInitialised=false;
    float detectedPitch=0,voicing=0,previousRms=0,slowRms=.03f,activity=0,sceneEvidence=0;
    float meterIn=0,meterOut=0,meterRepair=0;
    juce::SmoothedValue<float> mixRamp,trimRamp,bypassRamp;
};

void renderVoice(const juce::File& input,const juce::File& output,const VoiceProfile&,const RestoreControls&,
                 std::atomic<float>* progress=nullptr,std::atomic<bool>* cancel=nullptr);
juce::var runVoxTests(const juce::File& folder);
