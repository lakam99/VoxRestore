#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class OsTheme final : private juce::DarkModeSettingListener,private juce::Timer {
public:
    OsTheme();
    ~OsTheme() override;
    static juce::Colour background(bool dark);
    static bool setNativeFrame(juce::Component&,bool dark);
    static int nativeFrameState(juce::Component&);
private:
    void darkModeSettingChanged() override;
    void timerCallback() override;
    void updateWindows(bool notify=false);
    juce::LookAndFeel_V4 look;
};
juce::var runWindowThemeTests();
