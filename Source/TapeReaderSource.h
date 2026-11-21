#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

// Loop mode is atomic because the read-ahead worker and the transport both
// inspect it. The reader itself is accessed only by the read-ahead source.
class TapeReaderSource final : public juce::PositionableAudioSource {
public:
    explicit TapeReaderSource(std::unique_ptr<juce::AudioFormatReader> source):reader(std::move(source)){}
    void prepareToPlay(int,double) override {}
    void releaseResources() override {}
    void setNextReadPosition(juce::int64 position) override {next.store(std::max<juce::int64>(0,position));}
    juce::int64 getNextReadPosition() const override {auto position=next.load();return isLooping()&&getTotalLength()>0?position%getTotalLength():position;}
    juce::int64 getTotalLength() const override {return reader->lengthInSamples;}
    bool isLooping() const override {return looping.load();}
    void setLooping(bool enabled) override {looping.store(enabled);}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        auto position=next.load(),length=getTotalLength();
        int offset=0;
        while(offset<info.numSamples) {
            bool repeat=looping.load();
            auto sourcePosition=repeat&&length>0?position%length:position;
            if(length<=0||sourcePosition>=length){info.buffer->clear(info.startSample+offset,info.numSamples-offset);position+=info.numSamples-offset;break;}
            int count=(int)std::min<juce::int64>(info.numSamples-offset,length-sourcePosition);
            reader->read(info.buffer,info.startSample+offset,count,sourcePosition,true,true);
            offset+=count;position+=count;
        }
        next.store(position);
    }
private:
    std::unique_ptr<juce::AudioFormatReader> reader;
    std::atomic<juce::int64> next{0};
    std::atomic<bool> looping{false};
};
