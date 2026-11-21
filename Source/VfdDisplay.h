#pragma once
#include "RasterAssets.h"

void drawVfdHousing(juce::Graphics&,juce::Rectangle<float>);
class VfdLine {
public:
    void draw(juce::Graphics&,const juce::String&,juce::Rectangle<float>,float pitch=1.65f,bool scroll=true);
private:
    juce::String cachedText;
    float cachedPitch=0;
    double changedAt=0;
    juce::Image phosphor;
};
