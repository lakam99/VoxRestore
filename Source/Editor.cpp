#include "Editor.h"

namespace {
juce::Font labelFont(float size=12,bool bold=true){return juce::Font(juce::FontOptions("Arial",size,bold?juce::Font::bold:juce::Font::plain));}
void engraved(juce::Graphics& g,const juce::String& text,juce::Rectangle<float> r,juce::Colour ink,float size=12) {
    g.setFont(labelFont(size));g.setColour(ink);g.drawText(text,r,juce::Justification::centred,false);
}
}
HardwareLook::HardwareLook(){setColourScheme(juce::Desktop::getInstance().isDarkModeActive()?getDarkColourScheme():getLightColourScheme());}
juce::Colour HardwareLook::panel() const {return skin==0?juce::Colour(0xffd4cbb6):skin==1?juce::Colour(0xff282b2c):juce::Colour(0xffb9a17b);}
juce::Colour HardwareLook::ink() const {return skin==1?juce::Colour(0xffd6d2bd):juce::Colour(0xff292a25);}
juce::Colour HardwareLook::accent() const {return skin==1?juce::Colour(0xffc84e3b):juce::Colour(0xff7b382a);}
void HardwareLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider& slider) {
    auto area=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(14);
    float radius=std::min(area.getWidth(),area.getHeight())*.5f,cx=area.getCentreX(),cy=area.getCentreY();
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    if(w>90)for(int i=0;i<=12;++i){float a=start+(end-start)*i/12;g.setColour(ink().withAlpha(.65f));g.drawLine(cx+std::sin(a)*(radius+5),cy-std::cos(a)*(radius+5),cx+std::sin(a)*(radius+(i%3==0?11:8)),cy-std::cos(a)*(radius+(i%3==0?11:8)),i%3==0?1.3f:.7f);}
    g.drawImage(RasterAssets::get().knob,juce::Rectangle<float>{cx-radius,cy-radius,radius*2,radius*2},juce::RectanglePlacement::stretchToFit);
    // Keep the photographed highlights fixed; only the live index moves.
    float angle=start+position*(end-start),inner=radius*.81f;
    g.setColour(juce::Colours::black.withAlpha(.60f));g.drawLine(cx+std::sin(angle)*inner*.42f+1,cy-std::cos(angle)*inner*.42f+1,cx+std::sin(angle)*inner*.88f+1,cy-std::cos(angle)*inner*.88f+1,3.3f);
    g.setColour(juce::Colour(0xffeee7cb));g.drawLine(cx+std::sin(angle)*inner*.42f,cy-std::cos(angle)*inner*.42f,cx+std::sin(angle)*inner*.88f,cy-std::cos(angle)*inner*.88f,2.8f);
    if(w<=90)for(int i=0;i<3;++i){float a=start+(end-start)*i*.5f;g.setColour(ink().withAlpha(i==(int)std::round(slider.getValue())?.9f:.45f));g.drawLine(cx+std::sin(a)*(radius+3),cy-std::cos(a)*(radius+3),cx+std::sin(a)*(radius+7),cy-std::cos(a)*(radius+7),1.4f);}
}
void HardwareLook::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool){
 g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);auto r=b.getLocalBounds().toFloat();auto art=r.removeFromLeft(43).reduced(5,0);g.setOpacity(b.isEnabled()?1.0f:.5f);g.drawImage(RasterAssets::get().switches[b.getToggleState()?0:1],art,juce::RectanglePlacement::stretchToFit);g.setOpacity(1);bool ref=b.getButtonText()=="REFERENCE";g.setFont(labelFont(10));auto text=r.reduced(4,1);auto top=text.removeFromTop(15);g.setColour(ink().withAlpha(b.getToggleState()?1.0f:.62f));g.drawText(ref?"MATCH":"BYPASS",top,juce::Justification::centredLeft);g.setColour(ink().withAlpha(b.getToggleState()?.62f:1.0f));g.drawText(ref?"FREE":"IN",text,juce::Justification::centredLeft);
}
void HardwareLook::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool,bool down) {
    auto r=b.getLocalBounds().toFloat().reduced(1);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.setOpacity(b.isEnabled()?1.0f:.48f);
    if(b.getComponentID().startsWith("transport-")) {
        int index=b.getComponentID()=="transport-play"?0:b.getComponentID()=="transport-pause"?1:b.getComponentID()=="transport-stop"?2:b.getComponentID()=="transport-loop"?3:4;
        g.drawImage(RasterAssets::get().transport[(size_t)index],r.translated(0,down?1.0f:0.0f),juce::RectanglePlacement::centred);
        if(index==3&&b.getToggleState())g.drawImage(RasterAssets::get().lamps[1],juce::Rectangle<float>{r.getRight()-9,r.getBottom()-9,7,7},juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1);return;
    }
    g.drawImage(RasterAssets::get().buttons[down?1:0],r,juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1);
}
void HardwareLook::drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) {
    if(b.getComponentID().startsWith("transport-"))return;
    g.setFont(labelFont(11));g.setColour(juce::Colours::black.withAlpha(.85f));g.drawText(b.getButtonText(),b.getLocalBounds().translated(0,1),juce::Justification::centred);
    g.setColour(juce::Colour(0xffeee8d7).withAlpha(b.isEnabled()?1.0f:.55f));g.drawText(b.getButtonText(),b.getLocalBounds(),juce::Justification::centred);
}

VoxEditor::VoxEditor(VoxProcessor& p):AudioProcessorEditor(p),processor(p){
    setLookAndFeel(&look);setOpaque(true);juce::Desktop::getInstance().addDarkModeSettingListener(this);
    juce::Slider* knobs[]{&restore,&dewater,&rebuild,&mix,&trim,&skin};const char* ids[]{"restore","dewater","rebuild","mix","output","skin"};
    for(int i=0;i<6;++i){auto* knob=knobs[i];knob->setComponentID(ids[i]);knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);knob->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);knob->setRotaryParameters(juce::MathConstants<float>::pi*1.23f,juce::MathConstants<float>::pi*2.77f,true);addAndMakeVisible(knob);sliders.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.params,ids[i],*knob));}
    restore.setTooltip("Reference-guided spectral balance and gentle dynamic contour repair. Uses the input voice's words and pitch.");dewater.setTooltip("Repair short missing harmonics and soften isolated watery spectral spikes. Reduce if consonants soften.");rebuild.setTooltip("Add limited, input-derived upper harmonics only during voiced speech. Reduce if the result sounds buzzy.");mix.setTooltip("Latency-aligned dry / restored blend");trim.setTooltip("Output trim, -12 to +12 dB; bypass ignores trim");skin.setTooltip("Ivory / Graphite / Walnut generated hardware faceplates");
    for(auto* item:{(juce::Component*)&match,(juce::Component*)&bypass,(juce::Component*)&load,(juce::Component*)&learn,(juce::Component*)&clear,(juce::Component*)&guide})addAndMakeVisible(item);
    matchAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.params,"reference",match);bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.params,"bypass",bypass);
    match.setTooltip("MATCH uses the cleaner reference profile. FREE keeps artifact repair and harmonic rebuilding without matching its tone.");bypass.setTooltip("Compare with the latency-aligned original voice");
    load.onClick=[this]{openReference();};learn.onClick=[this]{processor.reanalyseReference();};clear.onClick=[this]{processor.clearReference();};
    load.setTooltip("Load a cleaner sample of the same speaker; the entire recording is analyzed automatically");learn.setTooltip("Manually re-analyze the whole reference file");clear.setTooltip("Remove the saved reference profile");
    guide.setButtonText(processor.standalone.load()?"UPDATES":"GUIDE");guide.onClick=[this] {if(processor.standalone.load()){if(onUpdates)onUpdates();return;}juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"VoxRestore guide","1. Load a cleaner 5-20 second sample of the same speaker. Analysis starts automatically.\n2. Play the damaged voice through the plug-in. MATCH guides tone and dynamics.\n3. Start with moderate DEWATER and low REBUILD; use MIX and BYPASS to compare.\n\nDEWATER targets brief watery gaps and isolated artifacts. REBUILD derives extra harmonics from the input, following its pitch. It cannot recover words or consonants that were completely removed. Heavy settings can soften detail or add buzz. The saved reference contains statistics, not a recorded voice. Audio stays on this computer.");};
    if(processor.standalone.load()){
        for(auto* b:{&voice,&beginning,&play,&stop,&loop,&save,&devices})addAndMakeVisible(b);
        beginning.setComponentID("transport-start");play.setComponentID("transport-play");stop.setComponentID("transport-stop");loop.setComponentID("transport-loop");
        beginning.setTooltip("Return to the beginning, keeping play or pause");play.setTooltip("Play / pause");stop.setTooltip("Stop and rewind");loop.setTooltip("Loop the voice for auditioning");loop.setClickingTogglesState(true);
        voice.onClick=[this]{if(onLoadVoice)onLoadVoice();};beginning.onClick=[this]{if(onStart)onStart();};play.onClick=[this]{if(onPlay)onPlay();};stop.onClick=[this]{if(onStop)onStop();};loop.onClick=[this]{if(onLoop)onLoop();};save.onClick=[this]{if(onExport)onExport();};devices.onClick=[this]{if(onDevices)onDevices();};
    }
    setSize(960,668);timerCallback();startTimerHz(30);
}
VoxEditor::~VoxEditor(){stopTimer();if(auto* desktop=juce::Desktop::getInstanceWithoutCreating())desktop->removeDarkModeSettingListener(this);setLookAndFeel(nullptr);}
void VoxEditor::darkModeSettingChanged(){look.setColourScheme(juce::Desktop::getInstance().isDarkModeActive()?HardwareLook::getDarkColourScheme():HardwareLook::getLightColourScheme());sendLookAndFeelChange();repaint();}
void VoxEditor::meter(juce::Graphics& g,juce::Rectangle<float> r,const juce::String& title,float value,bool repair){
    const auto& art=RasterAssets::get();g.drawImage(art.meter,r,juce::RectanglePlacement::stretchToFit);auto paper=art.meterPaper;juce::Rectangle<float> face{r.getX()+paper.getX()*r.getWidth(),r.getY()+paper.getY()*r.getHeight(),paper.getWidth()*r.getWidth(),paper.getHeight()*r.getHeight()};float cx=face.getCentreX(),cy=face.getBottom()-7,radius=juce::jlimit(42.0f,84.0f,cy-face.getY()-21);
    int ticks=repair?25:24;for(int i=0;i<=ticks;++i){float a=-1.02f+2.04f*i/ticks;bool major=i%(repair?5:4)==0;g.setColour(i>ticks*.84f?juce::Colour(0xff943c26):juce::Colour(0xff39352b));g.drawLine(cx+std::sin(a)*(radius-(major?10:5)),cy-std::cos(a)*(radius-(major?10:5)),cx+std::sin(a)*radius,cy-std::cos(a)*radius,major?1.5f:.7f);if(major){g.setFont(labelFont(10));g.drawText(repair?juce::String(i*4):juce::String(-48+i*2),juce::Rectangle<float>{cx+std::sin(a)*(radius+13)-14,cy-std::cos(a)*(radius+13)-6,28,12},juce::Justification::centred);}}
    float normal=repair?juce::jlimit(0.0f,1.0f,value/100):juce::jlimit(0.0f,1.0f,(juce::Decibels::gainToDecibels(value,-60.0f)+48)/48),a=-1.02f+normal*2.04f;g.setColour(juce::Colour(0xffa63822));g.drawLine(cx,cy,cx+std::sin(a)*(radius-4),cy-std::cos(a)*(radius-4),1.7f);g.drawImage(art.knob,juce::Rectangle<float>{cx-5,cy-5,10,10},juce::RectanglePlacement::stretchToFit);engraved(g,title,{r.getX()+12,r.getBottom()-17,r.getWidth()-24,14},juce::Colour(0xffe9dfc4),11);
}
void VoxEditor::paint(juce::Graphics& g){
    const auto& art=RasterAssets::get();g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImage(art.panels[(size_t)look.skin],getLocalBounds().toFloat(),juce::RectanglePlacement::stretchToFit);
    engraved(g,"V O X R E S T O R E",{270,30,420,38},look.ink(),26);engraved(g,"HUMAN VOICE RESTORATION",{270,70,420,16},look.ink().withAlpha(.75f),10);engraved(g,"FACEPLATE",{65,22,110,16},look.ink(),9);engraved(g,"I  /  G  /  W",{65,94,110,11},look.ink(),10);engraved(g,"OUTPUT TRIM",{778,22,118,16},look.ink(),9);engraved(g,"-12  /  0  /  +12",{778,94,118,11},look.ink(),9);
    meter(g,{64,112,260,150},"INPUT / dBFS",inNeedle,false);meter(g,{350,112,260,150},"REPAIR / %",repairNeedle,true);meter(g,{636,112,260,150},"OUTPUT / dBFS",outNeedle,false);
    engraved(g,"REFERENCE GUIDED RECONSTRUCTION",{325,277,300,20},look.ink().withAlpha(.85f),10);
    juce::StringArray names{"RESTORE","DEWATER","REBUILD","MIX"};for(int j=0;j<4;++j){float x=87.0f+j*222,cx=147+j*222,cy=398;engraved(g,names[j],{x,305,120,20},look.ink(),11);for(int m=0;m<3;++m){float angle=juce::MathConstants<float>::pi*(1.23f+1.54f*m*.5f);g.setFont(labelFont(10));g.setColour(look.ink());g.drawText(juce::String(m*50),juce::Rectangle<float>{cx+std::sin(angle)*66-19,cy-std::cos(angle)*66-6,38,12},juce::Justification::centred);}engraved(g,j==3?"DRY / RESTORED":"%",{x-8,459,136,16},look.ink().withAlpha(.85f),j==3?10:9);}
    drawVfdHousing(g,{64,485,832,92});juce::String top=juce::StringArray{"IVORY","GRAPHITE","WALNUT"}[look.skin]+"  REST "+juce::String(restore.getValue(),1)+"%  DEW "+juce::String(dewater.getValue(),1)+"%  REBUILD "+juce::String(rebuild.getValue(),1)+"%  MIX "+juce::String(mix.getValue(),1)+"%  OUT "+juce::String(trim.getValue(),1)+" DB";lines[0].draw(g,top,{82,495,796,17},1.65f,false);
    bool busy=processor.busy.load();juce::String status=processor.statusText();
    if(busy||updating){auto update=onUpdateProgress?onUpdateProgress():std::pair<double,juce::String>{-1,"CHECKING"};float value=updating?(float)update.first:juce::jlimit(0.0f,1.0f,processor.progress.load());juce::String label=updating?"UPDATE / "+update.second:status.startsWith("SCANNING WHOLE REFERENCE")?"ANALYZING REFERENCE":status.startsWith("SCANNING WHOLE VOICE")?"ANALYZING VOICE TRACK":"PRINTING RESTORED VOICE";lines[1].draw(g,label,{82,519,398,17},1.65f);int cursor=(int)(juce::Time::getMillisecondCounter()/55)%31;for(int i=0;i<31;++i){bool lit=value>=0?i<(int)(value*31):(i-cursor+31)%31<5;g.setColour(lit?juce::Colour(0xffb3ffed):juce::Colour(0xff245849));g.fillRect(491.0f+i*10,523.0f,7.5f,8.0f);}lines[3].draw(g,value>=0?juce::String((int)(value*100))+"%":"...",{817,519,61,17},1.65f,false);}
    else {if(status.startsWith("ANALYSIS:")||status.startsWith("PRINT:" )||status.startsWith("PRINT COMPLETE")){}else if(bypass.getToggleState())status="BYPASS / ORIGINAL VOICE";else if(processor.referenceReady()&&match.getToggleState())status="MATCH ACTIVE / "+juce::String(processor.referenceSnapshot().speechSeconds,1)+"S CLEAN SPEECH / "+(processor.standalone.load()&&voiceName?voiceName():"HOST PLAYBACK");else status="FREE RESTORATION / LOAD A CLEANER SAMPLE FOR MATCH";lines[1].draw(g,status,{82,519,590,17},1.65f);juce::String clock;
        if(processor.standalone.load()&&position&&duration){auto time=[](double value){int seconds=(int)std::max(0.0,value);return juce::String(seconds/60).paddedLeft('0',2)+":"+juce::String(seconds%60).paddedLeft('0',2);};clock=time(position())+" / "+time(duration());}else clock=processor.confidence.load()>.3f?"F0 "+juce::String(processor.pitch.load(),0)+" HZ":"VOICE FOLLOW";lines[3].draw(g,clock,{691,519,187,17},1.65f,false);}
    lines[2].draw(g,"REF / "+processor.referenceName(),{82,544,796,17},1.6f);
    if(!processor.standalone.load())engraved(g,"HOST PLAYBACK / WORDS AND TIMING FOLLOW INPUT",{270,598,475,28},look.ink(),10);
}
void VoxEditor::resized(){skin.setBounds(85,37,74,63);trim.setBounds(800,37,74,63);juce::Slider* knobs[]{&restore,&dewater,&rebuild,&mix};for(int j=0;j<4;++j)knobs[j]->setBounds(78+j*222,337,138,122);match.setBounds(68,270,145,32);learn.setBounds(224,274,82,25);clear.setBounds(636,274,78,25);bypass.setBounds(750,270,140,32);load.setBounds(72,596,118,32);guide.setBounds(808,596,88,32);voice.setBounds(200,596,96,32);beginning.setBounds(306,592,52,40);play.setBounds(366,592,52,40);stop.setBounds(426,592,52,40);loop.setBounds(486,592,52,40);save.setBounds(566,596,118,32);devices.setBounds(692,596,108,32);}
void VoxEditor::timerCallback(){processor.serviceJobs();int next=juce::jlimit(0,2,(int)processor.params.getRawParameterValue("skin")->load());if(next!=look.skin){look.skin=next;sendLookAndFeelChange();}inNeedle=.78f*inNeedle+.22f*processor.inputLevel.load();outNeedle=.78f*outNeedle+.22f*processor.outputLevel.load();repairNeedle=.78f*repairNeedle+.22f*processor.repairLevel.load();bool busy=processor.busy.load();load.setEnabled(!busy);learn.setEnabled(!busy&&processor.canReanalyse());clear.setEnabled(!busy&&processor.referenceReady());match.setEnabled(processor.referenceReady());voice.setEnabled(!busy);save.setEnabled(!busy&&duration&&duration()>0);bool loaded=duration&&duration()>0;for(auto* b:{&beginning,&play,&stop,&loop})b->setEnabled(loaded);play.setComponentID(isPlaying&&isPlaying()?"transport-pause":"transport-play");repaint();}
void VoxEditor::openReference(){chooser=std::make_unique<juce::FileChooser>("Cleaner reference of the same speaker",juce::File{},"*.wav;*.aiff;*.aif;*.flac;*.mp3");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<VoxEditor>(this)](const juce::FileChooser& result){if(safe&&result.getResult().existsAsFile())safe->processor.loadReference(result.getResult());});}
