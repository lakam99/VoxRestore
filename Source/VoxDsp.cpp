#include "VoxDsp.h"
#include <numeric>

namespace {
float wrap(float v){return std::remainder(v,juce::MathConstants<float>::twoPi);}
float position(double hz){return (float)juce::jlimit(0.0,47.0,std::log1p(std::max(0.0,hz)/100)/std::log(201.0)*47);}
}
int voiceBand(double hz){return (int)position(hz);}
VoiceEnvelope spectralEnvelope(const std::vector<float>& p,double rate,int size){
    VoiceEnvelope result{};std::array<int,voiceBands> count{};
    for(size_t k=1;k<p.size();++k){double hz=k*rate/size;if(hz<65||hz>18000)continue;int b=voiceBand(hz);result[(size_t)b]+=p[k];++count[(size_t)b];}
    float total=0;for(size_t b=0;b<result.size();++b){result[b]/=std::max(1,count[b]);total+=result[b];}
    if(total>1e-20f)for(float& v:result)v=std::max(1e-9f,v/total);
    return result;
}
bool VoiceProfile::valid() const{
    if(!std::isfinite(speechSeconds)||speechSeconds<.5||speechSeconds>86400||!std::isfinite(dynamicRangeDb)||dynamicRangeDb<0||dynamicRangeDb>80||!std::isfinite(crestDb)||crestDb<0||crestDb>80||!std::isfinite(medianPitch)||medianPitch<0||medianPitch>1000)return false;
    double total=0;for(float v:power){if(!std::isfinite(v)||v<0||v>1)return false;total+=v;}return total>.8&&total<1.2;
}
juce::ValueTree profileState(const VoiceProfile& p){
    if(!p.valid())return {};juce::ValueTree tree("VoiceReference");tree.setProperty("schema",1,nullptr);tree.setProperty("speechSeconds",p.speechSeconds,nullptr);tree.setProperty("dynamicRangeDb",p.dynamicRangeDb,nullptr);tree.setProperty("crestDb",p.crestDb,nullptr);tree.setProperty("medianPitch",p.medianPitch,nullptr);
    juce::MemoryOutputStream bytes;for(float value:p.power)bytes.writeFloat(value);tree.setProperty("spectrum",bytes.getMemoryBlock().toBase64Encoding(),nullptr);return tree;
}
bool readProfileState(const juce::ValueTree& tree,VoiceProfile& result){
    if(!tree.hasType("VoiceReference")||(int)tree["schema"]!=1)return false;auto encoded=tree["spectrum"].toString();if(encoded.length()>512)return false;juce::MemoryBlock bytes;if(!bytes.fromBase64Encoding(encoded)||bytes.getSize()!=voiceBands*4)return false;
    VoiceProfile p;p.speechSeconds=(double)tree["speechSeconds"];p.dynamicRangeDb=(float)tree["dynamicRangeDb"];p.crestDb=(float)tree["crestDb"];p.medianPitch=(float)tree["medianPitch"];
    juce::MemoryInputStream input(bytes,false);for(float& value:p.power)value=input.readFloat();if(!p.valid())return false;result=p;return true;
}
void VoxEngine::prepare(double sr,int count){
    rate=sr;channels=juce::jlimit(1,2,count);int order=juce::jlimit(8,13,(int)std::round(std::log2(rate*.046)));size=1<<order;hop=size/4;capacity=size*4;inputPos=outputPos=0;countdown=hop;
    fft=std::make_unique<juce::dsp::FFT>(order);window.resize((size_t)size);power.assign((size_t)size/2+1,0);bandPositions.resize(power.size());envelopeBand.assign(power.size(),-1);envelopeCount.fill(0);
    for(size_t k=0;k<power.size();++k){double hz=k*rate/size;bandPositions[k]=position(hz);if(hz>=65&&hz<=18000){envelopeBand[k]=(int)bandPositions[k];++envelopeCount[(size_t)envelopeBand[k]];}}
    for(int i=0;i<size;++i)window[(size_t)i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/size);
    for(int c=0;c<2;++c){input[(size_t)c].assign((size_t)size,0);output[(size_t)c].assign((size_t)capacity,0);data[(size_t)c].resize((size_t)size*2);excitation[(size_t)c].resize((size_t)size*2);previousMagnitude[(size_t)c].assign(power.size(),0);previousPhase[(size_t)c].assign(power.size(),0);phaseAdvance[(size_t)c].assign(power.size(),0);}
    pitchInput.resize((size_t)size);pitchDifference.resize((size_t)size);bandGain.fill(1);sourceEnvelope.fill(0);recentEnvelope.fill(0);sourceInitialised=false;previousRms=0;slowRms=.03f;activity=sceneEvidence=0;detectedPitch=voicing=0;meterIn=meterOut=meterRepair=0;
    mixRamp.reset(rate,.035);mixRamp.setCurrentAndTargetValue(1);trimRamp.reset(rate,.035);trimRamp.setCurrentAndTargetValue(1);bypassRamp.reset(rate,.025);bypassRamp.setCurrentAndTargetValue(1);
}
void VoxEngine::detectPitch(int channel,float rms){
    detectedPitch=0;voicing=0;if(rms<.00015f)return;
    int step=std::max(1,(int)std::round(rate/8000)),count=size/step;double pitchRate=rate/step;
    float mean=0;
    for(int i=0;i<count;++i){float value=0;for(int j=0;j<step;++j)value+=input[(size_t)channel][(size_t)((inputPos+i*step+j)&(size-1))];pitchInput[(size_t)i]=value/step;mean+=value/step;}
    mean/=count;for(int i=0;i<count;++i)pitchInput[(size_t)i]-=mean;
    int maxLag=std::min(count/2-1,(int)(pitchRate/55)),minLag=std::max(2,(int)(pitchRate/600));if(maxLag<=minLag)return;
    double running=0;pitchDifference[0]=1;
    for(int lag=1;lag<=maxLag;++lag){double sum=0;for(int i=0;i<count-maxLag;++i){float d=pitchInput[(size_t)i]-pitchInput[(size_t)(i+lag)];sum+=d*d;}running+=sum;pitchDifference[(size_t)lag]=running>1e-20?(float)(sum*lag/running):1;}
    int best=minLag;for(int lag=minLag;lag<maxLag;++lag){if(pitchDifference[(size_t)lag]<.22f){while(lag+1<maxLag&&pitchDifference[(size_t)(lag+1)]<pitchDifference[(size_t)lag])++lag;best=lag;break;}if(pitchDifference[(size_t)lag]<pitchDifference[(size_t)best])best=lag;}
    float confidence=juce::jlimit(0.0f,1.0f,1-pitchDifference[(size_t)best]);if(confidence<.62f)return;
    float refined=(float)best;if(best>1&&best<maxLag){float a=pitchDifference[(size_t)best-1],b=pitchDifference[(size_t)best],c=pitchDifference[(size_t)best+1],den=a-2*b+c;if(std::abs(den)>1e-9f)refined+=juce::jlimit(-.5f,.5f,.5f*(a-c)/den);}
    detectedPitch=(float)(pitchRate/refined);voicing=juce::jlimit(0.0f,1.0f,(confidence-.6f)/.35f);
}
void VoxEngine::frame(const RestoreControls& requested){
    auto controls=requested;if(controls.bypass){controls.restore=controls.dewater=controls.rebuild=0;controls.match=false;}
    bool matching=controls.match&&reference.valid();
    std::fill(power.begin(),power.end(),0);double energy[2]{};
    for(int c=0;c<channels;++c){auto& values=data[(size_t)c];std::fill(values.begin(),values.end(),0);
        for(int i=0;i<size;++i){float sample=input[(size_t)c][(size_t)((inputPos+i)&(size-1))];values[(size_t)i]=sample*window[(size_t)i];energy[c]+=sample*sample;}if(energy[c]>1e-20)fft->performRealOnlyForwardTransform(values.data(),true);
        for(size_t k=0;k<power.size();++k)power[k]=std::max(power[k],values[k*2]*values[k*2]+values[k*2+1]*values[k*2+1]);}
    int channel=channels==2&&energy[1]>energy[0]?1:0;float rms=(float)std::sqrt(energy[channel]/size);
    if(rms<1e-10f&&previousRms<1e-10f){detectedPitch=voicing=0;meterRepair*=.8f;return;}
    if(controls.dewater>0||controls.rebuild>0)detectPitch(channel,rms);else detectedPitch=voicing=0;
    float targetActivity=juce::jlimit(0.0f,1.0f,(juce::Decibels::gainToDecibels(rms,-120.0f)+65)/20);
    activity=targetActivity>activity?targetActivity:.65f*activity+.35f*targetActivity;
    VoiceEnvelope envelope{};for(size_t k=1;k<power.size();++k)if(envelopeBand[k]>=0)envelope[(size_t)envelopeBand[k]]+=power[k];float envelopeTotal=0;for(size_t b=0;b<envelope.size();++b){envelope[b]/=std::max(1,envelopeCount[b]);envelopeTotal+=envelope[b];}if(envelopeTotal>1e-20f)for(auto& value:envelope)value=std::max(1e-9f,value/envelopeTotal);
    if(rms>.0003f){
        if(!sourceInitialised){sourceEnvelope=sourcePreset.valid()?sourcePreset.power:envelope;recentEnvelope=envelope;sourceInitialised=true;}
        // Stable large changes in a stitched track must not be rejected as an
        // outlier against the first clip. Faster adaptation follows local tone.
        double distance=0;int measured=0;float recentAlpha=(float)(1-std::exp(-hop/(rate*.25)));
        for(size_t b=0;b<envelope.size();++b)recentEnvelope[b]+=recentAlpha*(envelope[b]-recentEnvelope[b]);
        for(size_t b=4;b<44;++b)if(recentEnvelope[b]>1e-6f&&sourceEnvelope[b]>1e-6f){distance+=std::abs(10*std::log10(recentEnvelope[b]/sourceEnvelope[b]));++measured;}
        sceneEvidence=measured&&distance/measured>7?std::min(1.0f,sceneEvidence+(float)(hop/rate)):std::max(0.0f,sceneEvidence-(float)(2*hop/rate));
        float seconds=sceneEvidence>.6f?.5f:1.8f;float alpha=(float)(1-std::exp(-hop/(rate*seconds)));
        for(size_t b=0;b<sourceEnvelope.size();++b)sourceEnvelope[b]+=alpha*(envelope[b]-sourceEnvelope[b]);
    }
    VoiceEnvelope desired{};float weight=0,weighted=0;
    for(size_t b=0;b<desired.size();++b){float db=0;if(matching&&sourceInitialised&&b>2&&b<46){db=10*std::log10(std::max(1e-7f,reference.power[b])/std::max(1e-7f,sourceEnvelope[b]));db=juce::jlimit(-9.0f,9.0f,db)*controls.restore;}
        desired[b]=juce::Decibels::decibelsToGain(db);weight+=envelope[b];weighted+=envelope[b]*desired[b]*desired[b];}
    float normalizer=weighted>1e-12f?std::sqrt(weight/weighted):1;
    float contour=1;
    if(rms>.0003f){slowRms+=(float)(1-std::exp(-hop/(rate*1.5)))*(rms-slowRms);
        float sourceRange=sourcePreset.valid()?sourcePreset.dynamicRangeDb:12;
        float expansion=matching?juce::jlimit(0.0f,.25f,(reference.dynamicRangeDb-sourceRange)*.012f):.04f;
        float delta=juce::jlimit(-2.5f,2.0f,20*std::log10(std::max(.1f,rms/std::max(.0003f,slowRms)))*expansion*controls.restore);contour=juce::Decibels::decibelsToGain(delta);}
    for(size_t b=0;b<desired.size();++b){float smoothed=desired[b]*.5f+desired[b?b-1:b]*.25f+desired[std::min(b+1,desired.size()-1)]*.25f;bandGain[b]+=.18f*(smoothed*normalizer*contour-bandGain[b]);}
    bool active=rms>.00006f;bool sustaining=active&&rms>previousRms*.4f;double repair=0;
    for(int c=0;c<channels;++c){auto& values=data[(size_t)c];auto& exciter=excitation[(size_t)c];std::fill(exciter.begin(),exciter.end(),0);
        if(controls.rebuild>0&&voicing>.2f&&active){float scale=std::max(.0001f,rms*2.5f);
            for(int i=0;i<size;++i){float raw=input[(size_t)c][(size_t)((inputPos+i)&(size-1))],u=juce::jlimit(-4.0f,4.0f,raw/scale);exciter[(size_t)i]=(std::tanh(u*2.4f)/2.4f-u)*scale*window[(size_t)i];}
            fft->performRealOnlyForwardTransform(exciter.data(),true);}
        for(size_t k=0;k<power.size();++k){float real=values[k*2],imag=values[k*2+1],magnitude=std::sqrt(real*real+imag*imag),hz=(float)(k*rate/size);bool repairRange=active&&hz>120&&hz<10000;
            float phase=repairRange&&controls.dewater>0?std::atan2(imag,real):0;
            float binAdvance=juce::MathConstants<float>::twoPi*(float)k*hop/size;
            float advance=binAdvance+wrap(phase-previousPhase[(size_t)c][k]-binAdvance);
            float tonal=0;if(repairRange&&detectedPitch>0){float harmonic=hz/detectedPitch;float distance=std::abs(harmonic-std::round(harmonic));tonal=std::exp(-distance*distance/(2*.14f*.14f))*voicing;}
            float prior=previousMagnitude[(size_t)c][k];float coherent=magnitude<prior*.1f?.65f:1-std::min(1.0f,std::abs(wrap(advance-phaseAdvance[(size_t)c][k]))/1.5f);
            // Brief missing harmonics are continued from observed phase, never
            // recursively from generated material, and only while input speaks.
            if(sustaining&&tonal>.15f&&prior>magnitude*2&&hz>120&&hz<10000){float amount=controls.dewater*tonal*coherent*.65f;float predicted=phaseAdvance[(size_t)c][k]+previousPhase[(size_t)c][k];float fill=std::min(prior*.72f,std::sqrt(std::max(1e-20f,power[k]))*4+.01f*rms*size)*amount;
                real=real*(1-amount)+fill*std::cos(predicted);imag=imag*(1-amount)+fill*std::sin(predicted);repair+=amount;}
            if(active&&controls.dewater>0&&k>2&&k+2<power.size()&&tonal<.15f){float neighbors=.25f*(power[k-2]+power[k-1]+power[k+1]+power[k+2]);if(power[k]>neighbors*12&&magnitude>prior*2){float smooth=1-controls.dewater*.45f;real*=smooth;imag*=smooth;repair+=1-smooth;}}
            float b=bandPositions[k];int a=(int)b,next=std::min(a+1,voiceBands-1);float eq=bandGain[(size_t)a]+(b-a)*(bandGain[(size_t)next]-bandGain[(size_t)a]);
            if(!active)eq=1;
            real*=eq;imag*=eq;
            if(active&&voicing>.2f&&controls.rebuild>0&&hz>std::max(950.0f,detectedPitch*3)&&hz<std::min(10000.0,rate*.45)){
                float deficit=.25f;if(matching&&sourceInitialised)deficit=juce::jlimit(.1f,1.0f,1-sourceEnvelope[(size_t)a]/std::max(1e-6f,reference.power[(size_t)a]));
                float amount=controls.rebuild*tonal*deficit*1.7f;float extraReal=exciter[k*2]*amount,extraImag=exciter[k*2+1]*amount;
                float limit=rms*size*.035f*controls.rebuild;float extra=std::hypot(extraReal,extraImag);if(extra>limit&&extra>0){extraReal*=limit/extra;extraImag*=limit/extra;}
                real+=extraReal;imag+=extraImag;repair+=amount;
            }
            values[k*2]=real;values[k*2+1]=k==0||k==power.size()-1?0:imag;
            previousMagnitude[(size_t)c][k]=magnitude;previousPhase[(size_t)c][k]=phase;phaseAdvance[(size_t)c][k]=advance;
        }
        fft->performRealOnlyInverseTransform(values.data());for(int i=0;i<size;++i)output[(size_t)c][(size_t)((outputPos+1+i)&(capacity-1))]+=values[(size_t)i]*window[(size_t)i]*(2.0f/3.0f);
    }
    previousRms=rms;meterRepair=.8f*meterRepair+.2f*(float)(repair/(power.size()*channels))*100;
}
void VoxEngine::process(juce::AudioBuffer<float>& audio,const RestoreControls& controls){
    juce::ScopedNoDenormals noDenormals;mixRamp.setTargetValue(juce::jlimit(0.0f,1.0f,controls.mix));trimRamp.setTargetValue(controls.bypass?1.0f:juce::Decibels::decibelsToGain(controls.outputDb));bypassRamp.setTargetValue(controls.bypass?0.0f:1.0f);float peakIn=0,peakOut=0;
    for(int i=0;i<audio.getNumSamples();++i){float blend=mixRamp.getNextValue()*bypassRamp.getNextValue(),trim=trimRamp.getNextValue();
        for(int c=0;c<std::min(channels,audio.getNumChannels());++c){float sample=audio.getSample(c,i);if(!std::isfinite(sample))sample=0;peakIn=std::max(peakIn,std::abs(sample));float dry=input[(size_t)c][(size_t)inputPos];input[(size_t)c][(size_t)inputPos]=sample;float wet=output[(size_t)c][(size_t)outputPos];output[(size_t)c][(size_t)outputPos]=0;float result=(dry+(wet-dry)*blend)*trim;audio.setSample(c,i,std::isfinite(result)?result:0);peakOut=std::max(peakOut,std::abs(result));}
        inputPos=(inputPos+1)&(size-1);if(--countdown==0){frame(controls);countdown=hop;}outputPos=(outputPos+1)&(capacity-1);
    }
    float decay=(float)std::exp(-audio.getNumSamples()/(rate*.25));meterIn=std::max(peakIn,meterIn*decay);meterOut=std::max(peakOut,meterOut*decay);
}
