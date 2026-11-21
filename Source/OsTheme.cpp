#include "OsTheme.h"
#include "RasterAssets.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#endif

juce::Colour OsTheme::background(bool dark){return dark?juce::Colour(0xff25282b):juce::Colour(0xfff0f0f0);}
bool OsTheme::setNativeFrame(juce::Component& component,bool dark) {
#if JUCE_WINDOWS
    if(auto* peer=component.getPeer()) {
        BOOL enabled=dark?TRUE:FALSE;auto handle=(HWND)peer->getNativeHandle();
        if(SUCCEEDED(DwmSetWindowAttribute(handle,20,&enabled,sizeof(enabled))))return true;
        return SUCCEEDED(DwmSetWindowAttribute(handle,19,&enabled,sizeof(enabled)));
    }
#endif
    return false;
}
int OsTheme::nativeFrameState(juce::Component& component) {
#if JUCE_WINDOWS
    if(auto* peer=component.getPeer()) {
        BOOL dark=FALSE;auto handle=(HWND)peer->getNativeHandle();
        if(SUCCEEDED(DwmGetWindowAttribute(handle,20,&dark,sizeof(dark)))||SUCCEEDED(DwmGetWindowAttribute(handle,19,&dark,sizeof(dark))))return dark?1:0;
    }
#endif
    return -1;
}
OsTheme::OsTheme(){juce::Desktop::getInstance().addDarkModeSettingListener(this);juce::LookAndFeel::setDefaultLookAndFeel(&look);darkModeSettingChanged();startTimer(1000);}
OsTheme::~OsTheme(){stopTimer();if(auto* desktop=juce::Desktop::getInstanceWithoutCreating())desktop->removeDarkModeSettingListener(this);juce::LookAndFeel::setDefaultLookAndFeel(nullptr);}
void OsTheme::darkModeSettingChanged(){look.setColourScheme(juce::Desktop::getInstance().isDarkModeActive()?juce::LookAndFeel_V4::getDarkColourScheme():juce::LookAndFeel_V4::getLightColourScheme());updateWindows(true);}
void OsTheme::timerCallback(){updateWindows();}
void OsTheme::updateWindows(bool notify){
    auto& desktop=juce::Desktop::getInstance();bool dark=desktop.isDarkModeActive();
    for(int i=0;i<desktop.getNumComponents();++i)if(auto* component=desktop.getComponent(i)){
        if(nativeFrameState(*component)!=(dark?1:0))setNativeFrame(*component,dark);
        if(auto* window=dynamic_cast<juce::ResizableWindow*>(component))if(window->getBackgroundColour()!=background(dark))window->setBackgroundColour(background(dark));
        if(notify)component->sendLookAndFeelChange();
    }
}
juce::var runWindowThemeTests(){
    juce::DocumentWindow probe("NoiseBase theme probe",OsTheme::background(false),0);
    probe.setUsingNativeTitleBar(true);probe.setSize(180,80);probe.setVisible(false);probe.setIcon(RasterAssets::get().icon);
    bool lightApplied=OsTheme::setNativeFrame(probe,false);int light=OsTheme::nativeFrameState(probe);
    bool darkApplied=OsTheme::setNativeFrame(probe,true);int dark=OsTheme::nativeFrameState(probe);
    bool current=juce::Desktop::getInstance().isDarkModeActive();OsTheme::setNativeFrame(probe,current);
    bool iconResource=false;
#if JUCE_WINDOWS
    auto exe=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName();
    iconResource=ExtractIconExW(exe.toWideCharPointer(),-1,nullptr,nullptr,0)>0;
#elif JUCE_MAC
    auto resources=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getSiblingFile("Resources");
    iconResource=!resources.findChildFiles(juce::File::findFiles,false,"*.icns").isEmpty();
#endif
    auto* report=new juce::DynamicObject;
    bool supported=lightApplied&&darkApplied;
    report->setProperty("native_theme_supported",supported);report->setProperty("light_frame_state",light);report->setProperty("dark_frame_state",dark);
    report->setProperty("os_dark_mode",current);report->setProperty("embedded_icon_resource",iconResource);
    report->setProperty("passed",(!supported||(light==0&&dark==1))&&iconResource&&RasterAssets::get().icon.isValid());return juce::var(report);
}
