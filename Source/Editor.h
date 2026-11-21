#pragma once
#include "Processor.h"
#include "RasterAssets.h"
#include "VfdDisplay.h"
struct HardwareLook final:juce::LookAndFeel_V4 {
    HardwareLook();int skin=2;
    juce::Colour panel() const;juce::Colour ink() const;juce::Colour accent() const;
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
};
class VoxEditor final:public juce::AudioProcessorEditor,public juce::FileDragAndDropTarget,private juce::Timer,private juce::DarkModeSettingListener {
public:
    explicit VoxEditor(VoxProcessor&);~VoxEditor() override;
    void paint(juce::Graphics&) override;void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override{return files.size()==1&&!processor.busy.load();}
    void filesDropped(const juce::StringArray& files,int,int) override{if(isInterestedInFileDrag(files))processor.loadReference(juce::File(files[0]));}
    std::function<void()> onLoadVoice,onPlay,onStop,onStart,onLoop,onExport,onDevices;
    std::function<bool()> isPlaying;
    std::function<double()> position,duration;
    std::function<juce::String()> voiceName;
    std::function<void()> onUpdates;
    std::function<std::pair<double,juce::String>()> onUpdateProgress;
    void setUpdateBusy(bool busy,bool downloading=false){updating=busy;downloadingUpdate=downloading;repaint();}
private:
    void timerCallback() override;void darkModeSettingChanged() override;void openReference();
    void meter(juce::Graphics&,juce::Rectangle<float>,const juce::String&,float,bool repair);
    VoxProcessor& processor;HardwareLook look;
    juce::Slider restore,dewater,rebuild,mix,trim,skin;
    juce::ToggleButton match{"REFERENCE"},bypass{"BYPASS"};
    juce::TextButton load{"LOAD REF"},learn{"LEARN"},clear{"CLEAR"},voice{"LOAD VOICE"},play,stop,beginning,loop,save{"PRINT WAV"},devices{"PATCH BAY"},guide{"GUIDE"};
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliders;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> matchAttachment,bypassAttachment;
    std::unique_ptr<juce::FileChooser> chooser;std::array<VfdLine,4> lines;
    float inNeedle=0,outNeedle=0,repairNeedle=0;
    bool updating=false,downloadingUpdate=false;
};
