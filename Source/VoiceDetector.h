#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
class VoiceDetector {
public:
    VoiceDetector();
    ~VoiceDetector();
    void reset();
    float analyse(const float* frame512);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
