#include "VfdDisplay.h"
#include <map>

namespace {
using Glyph=std::array<int,7>;
const std::map<juce::juce_wchar,Glyph> letters{
 {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},{'C',{14,17,16,16,16,17,14}},
 {'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},{'I',{14,4,4,4,4,4,14}},
 {'J',{7,2,2,2,18,18,12}},{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},{'O',{14,17,17,17,17,17,14}},
 {'P',{30,17,17,30,16,16,16}},{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}},
 {'V',{17,17,17,17,17,10,4}},{'W',{17,17,17,21,21,27,17}},{'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},
 {'3',{30,1,1,14,1,1,30}},{'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},
 {'9',{14,17,17,15,1,1,14}},{' ',{0,0,0,0,0,0,0}},{'.',{0,0,0,0,0,0,4}},
 {':',{0,4,4,0,4,4,0}},{'-',{0,0,0,31,0,0,0}},{'_', {0,0,0,0,0,0,31}},
 {'/',{1,2,2,4,8,8,16}},{'\\',{16,8,8,4,2,2,1}},{'+',{0,4,4,31,4,4,0}},
 {'%',{25,26,2,4,8,11,19}},{'|',{4,4,4,4,4,4,4}},{'?',{14,17,1,2,4,0,4}},
 {'(',{2,4,8,8,8,4,2}},{')',{8,4,2,2,2,4,8}},{'[',{14,8,8,8,8,8,14}},{']',{14,2,2,2,2,2,14}},
 {'=',{0,0,31,0,31,0,0}},{'!',{4,4,4,4,4,0,4}},{',',{0,0,0,0,4,4,8}},
 {'\'',{4,4,8,0,0,0,0}},{'"',{10,10,0,0,0,0,0}},{'#',{10,31,10,10,31,10,0}},
 {'@',{14,17,23,21,23,16,14}}
};
}
void drawVfdHousing(juce::Graphics& g,juce::Rectangle<float> bounds) {
    const auto& art=RasterAssets::get();
    float edge=std::min(8.0f,bounds.getHeight()*.2f);
    float dx[]{bounds.getX(),bounds.getX()+edge,bounds.getRight()-edge,bounds.getRight()};
    float dy[]{bounds.getY(),bounds.getY()+edge,bounds.getBottom()-edge,bounds.getBottom()};
    for(int y=0;y<3;++y)for(int x=0;x<3;++x)
        g.drawImage(art.vfdPieces[(size_t)(y*3+x)],juce::Rectangle<float>{dx[x],dy[y],dx[x+1]-dx[x],dy[y+1]-dy[y]},juce::RectanglePlacement::stretchToFit);
}
void VfdLine::draw(juce::Graphics& g,const juce::String& source,juce::Rectangle<float> bounds,float pitch,bool scroll) {
    auto text=source.toUpperCase().substring(0,1024);
    if(text!=cachedText||pitch!=cachedPitch||!phosphor.isValid()) {
        cachedText=text;cachedPitch=pitch;changedAt=juce::Time::getMillisecondCounterHiRes();
        phosphor=juce::Image(juce::Image::ARGB,std::max(1,(int)std::ceil((text.length()*6+2)*pitch)),(int)std::ceil(9*pitch),true);
        juce::Graphics light(phosphor);
        bool native=false;for(auto c:text)if(!letters.count(c)){native=true;break;}
        if(native) {
            light.setFont(juce::Font(juce::FontOptions("Consolas",pitch*8.5f,juce::Font::plain)));
            light.setColour(juce::Colour(0xff76f9dc).withAlpha(.22f));light.drawText(text,phosphor.getBounds().translated(1,1),juce::Justification::centredLeft);
            light.setColour(juce::Colour(0xffa1ffe7));light.drawText(text,phosphor.getBounds(),juce::Justification::centredLeft);
        } else {
            int cell=0;
            for(auto c:text) {
                auto& glyph=letters.at(c);
                for(int row=0;row<7;++row)for(int column=0;column<5;++column) {
                    bool lit=(glyph[(size_t)row]&(1<<(4-column)))!=0;
                    float x=(cell*6+column+1)*pitch,y=(row+1)*pitch;
                    if(lit){light.setColour(juce::Colour(0xff54eac8).withAlpha(.12f));light.fillEllipse(x-pitch*.65f,y-pitch*.65f,pitch*1.9f,pitch*1.9f);light.setColour(juce::Colour(0xff75fadd).withAlpha(.26f));light.fillEllipse(x-pitch*.25f,y-pitch*.25f,pitch*1.1f,pitch*1.1f);}
                    light.setColour(lit?juce::Colour(0xffb3ffed):juce::Colour(0x0c68cbb2));light.fillRoundedRectangle(x,y,pitch*.82f,pitch*.82f,pitch*.20f);
                }
                ++cell;
            }
        }
    }
    float excess=phosphor.getWidth()-bounds.getWidth();
    float offset=0;
    if(scroll&&excess>0){double age=(juce::Time::getMillisecondCounterHiRes()-changedAt)*.001;double duration=excess/20+5;double phase=std::fmod(age,duration);offset=(float)juce::jlimit(0.0,(double)excess,(phase-2.5)*20);}
    juce::Graphics::ScopedSaveState saved(g);g.reduceClipRegion(bounds.toNearestInt());
    g.drawImageAt(phosphor,(int)std::round(bounds.getX()-offset),(int)std::round(bounds.getY()+(bounds.getHeight()-phosphor.getHeight())*.5f));
}
