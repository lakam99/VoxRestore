#include "VoxDsp.h"
#include "VoiceDetector.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <numeric>

namespace {
void cancelled(std::atomic<bool>* cancel){if(cancel&&cancel->load())throw std::runtime_error("Cancelled.");}
std::unique_ptr<juce::AudioFormatReader> open(const juce::File& file){juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));if(!reader||reader->numChannels<1||reader->numChannels>2||reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<1)throw std::runtime_error("Choose a nonempty mono/stereo voice recording, 8 to 192 kHz.");return reader;}
}
VoiceProfile analyseVoice(const juce::File& file,std::atomic<float>* progress,std::atomic<bool>* cancel,float start,float span){
    cancelled(cancel);auto reader=open(file);VoiceDetector detector;
    auto update=[&](float value){if(progress)progress->store(start+span*juce::jlimit(0.0f,1.0f,value));};
    juce::AudioBuffer<float> block((int)reader->numChannels,8192);juce::dsp::IIR::Filter<float> lowpass;lowpass.coefficients=juce::dsp::IIR::Coefficients<float>::makeLowPass(reader->sampleRate,std::min(6500.0,reader->sampleRate*.4));
    std::vector<float> probabilities;probabilities.reserve((size_t)(reader->lengthInSamples/reader->sampleRate/.032)+1);
    std::array<float,512> frame{};int fill=0;double phase=0;
    for(int64_t position=0;position<reader->lengthInSamples;position+=8192){cancelled(cancel);int count=(int)std::min<int64_t>(8192,reader->lengthInSamples-position);block.clear();if(!reader->read(&block,0,count,position,true,true))throw std::runtime_error("Could not read the whole reference.");int channel=reader->numChannels==2&&block.getRMSLevel(1,0,count)>block.getRMSLevel(0,0,count)?1:0;
        for(int i=0;i<count;++i){float sample=block.getSample(channel,i);if(!std::isfinite(sample))throw std::runtime_error("The recording contains non-finite samples.");float filtered=lowpass.processSample(sample);phase+=16000/reader->sampleRate;while(phase>=1){phase-=1;frame[(size_t)fill++]=filtered;if(fill==512){probabilities.push_back(detector.analyse(frame.data()));fill=0;}}}
        update(.55f*(float)((double)(position+count)/reader->lengthInSamples));}
    if(fill){std::fill(frame.begin()+fill,frame.end(),0);probabilities.push_back(detector.analyse(frame.data()));}if(probabilities.empty())throw std::runtime_error("Reference is too short.");
    int order=juce::jlimit(8,13,(int)std::round(std::log2(reader->sampleRate*.046))),size=1<<order,hop=size/4;juce::dsp::FFT fft(order);
    std::vector<float> window((size_t)size),data((size_t)size*2),power((size_t)size/2+1),levels,crests;VoiceEnvelope sum{};
    for(int i=0;i<size;++i)window[(size_t)i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/size);
    block.setSize((int)reader->numChannels,size);int accepted=0;
    for(int64_t position=0;position+size<=reader->lengthInSamples;position+=hop){cancelled(cancel);int a=std::max(0,(int)(position/reader->sampleRate/.032)-2),b=std::min((int)probabilities.size()-1,(int)((position+size)/reader->sampleRate/.032)+2);bool speech=false;for(int i=a;i<=b;++i)speech|=probabilities[(size_t)i]>.45f;
        if(speech){block.clear();reader->read(&block,0,size,position,true,true);int channel=reader->numChannels==2&&block.getRMSLevel(1,0,size)>block.getRMSLevel(0,0,size)?1:0;float rms=block.getRMSLevel(channel,0,size),peak=block.getMagnitude(channel,0,size);
            if(rms>.0003f&&peak<.999f){std::fill(data.begin(),data.end(),0);for(int i=0;i<size;++i)data[(size_t)i]=block.getSample(channel,i)*window[(size_t)i];fft.performRealOnlyForwardTransform(data.data(),true);for(size_t k=0;k<power.size();++k)power[k]=data[k*2]*data[k*2]+data[k*2+1]*data[k*2+1];auto envelope=spectralEnvelope(power,reader->sampleRate,size);for(size_t k=0;k<sum.size();++k)sum[k]+=envelope[k];++accepted;levels.push_back(juce::Decibels::gainToDecibels(rms));crests.push_back(juce::Decibels::gainToDecibels(peak/std::max(rms,1e-9f)));}}
        update(.55f+.45f*(float)((double)(position+size)/reader->lengthInSamples));}
    VoiceProfile profile;profile.speechSeconds=accepted*hop/reader->sampleRate;if(profile.speechSeconds<.5)throw std::runtime_error("Need at least half a second of detected, unclipped human speech; use a cleaner 5-20 second sample.");
    float total=0;for(size_t k=0;k<sum.size();++k){profile.power[k]=sum[k]/accepted;total+=profile.power[k];}for(float& value:profile.power)value/=total;
    std::sort(levels.begin(),levels.end());std::sort(crests.begin(),crests.end());profile.dynamicRangeDb=levels[levels.size()*9/10]-levels[levels.size()/5];profile.crestDb=crests[crests.size()/2];if(!profile.valid())throw std::runtime_error("The speech reference did not produce a usable profile.");update(1);return profile;
}
void renderVoice(const juce::File& source,const juce::File& destination,const VoiceProfile& reference,const RestoreControls& controls,std::atomic<float>* progress,std::atomic<bool>* cancel){
    if(destination==source||destination.existsAsFile())throw std::runtime_error("Choose a new output file; the original cannot be overwritten.");cancelled(cancel);auto reader=open(source);
    VoiceProfile sourceProfile;try{sourceProfile=analyseVoice(source,progress,cancel,0,.25f);}catch(const std::exception&){cancelled(cancel);}
    VoxEngine engine;engine.prepare(reader->sampleRate,(int)reader->numChannels);engine.setReference(reference);engine.setSource(sourceProfile);
    juce::TemporaryFile temporary(destination);std::unique_ptr<juce::OutputStream> stream(temporary.getFile().createOutputStream());if(!stream)throw std::runtime_error("Cannot create output.");juce::WavAudioFormat wav;
    auto options=juce::AudioFormatWriterOptions{}.withSampleRate(reader->sampleRate).withNumChannels((int)reader->numChannels).withBitsPerSample(reader->usesFloatingPointData?32:juce::jlimit(16,32,(int)reader->bitsPerSample)).withSampleFormat(reader->usesFloatingPointData?juce::AudioFormatWriterOptions::SampleFormat::floatingPoint:juce::AudioFormatWriterOptions::SampleFormat::integral);
    for(int i=0;i<reader->metadataValues.size();++i)options=options.withMetadata(reader->metadataValues.getAllKeys()[i],reader->metadataValues.getAllValues()[i]);
    auto writer=wav.createWriterFor(stream,options);if(!writer)throw std::runtime_error("Cannot create WAV writer.");juce::AudioBuffer<float> block((int)reader->numChannels,512);
    for(int i=0;i<engine.latency();i+=512){block.clear();engine.process(block,controls);}int64_t rendered=0,written=0;
    while(rendered<reader->lengthInSamples+engine.latency()){cancelled(cancel);int count=(int)std::min<int64_t>(512,reader->lengthInSamples+engine.latency()-rendered);block.setSize((int)reader->numChannels,count,false,false,true);block.clear();if(rendered<reader->lengthInSamples)reader->read(&block,0,(int)std::min<int64_t>(count,reader->lengthInSamples-rendered),rendered,true,true);engine.process(block,controls);
        int skip=(int)std::max<int64_t>(0,engine.latency()-rendered);int keep=(int)std::min<int64_t>(count-skip,reader->lengthInSamples-written);if(keep>0){if(!writer->writeFromAudioSampleBuffer(block,skip,keep))throw std::runtime_error("Could not write output audio.");written+=keep;}rendered+=count;if(progress)progress->store(.25f+.75f*(float)((double)written/reader->lengthInSamples));}
    writer.reset();if(written!=reader->lengthInSamples||!temporary.overwriteTargetFileWithTemporary())throw std::runtime_error("Could not finish the sample-aligned WAV.");
}
