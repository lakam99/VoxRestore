#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct RasterAssets {
    static const RasterAssets& get();
    std::array<juce::Image,3> panels;
    juce::Image knob,meter,vfd,icon;
    std::array<juce::Image,9> vfdPieces;
    std::array<juce::Image,6> transport;
    std::array<juce::Image,2> switches,buttons;
    std::array<juce::Image,4> lamps;
    juce::Rectangle<float> meterPaper;
private:
    RasterAssets();
};
