#include "RasterAssets.h"
#include "BinaryData.h"

namespace {
juce::Image png(const char* data,int size) {
    auto image=juce::ImageCache::getFromMemory(data,size);
    if(!image.isValid())throw std::runtime_error("Embedded raster skin could not be decoded.");
    // Static GPU images can recreate Direct3D resources during DLL detach,
    // under the Windows loader lock. Retain only CPU-backed raster assets.
    auto software=juce::SoftwareImageType{}.convert(image);
    image=juce::Image{};
    juce::ImageCache::releaseUnusedImages();
    return software;
}

// Ignore isolated alpha/color specks, while retaining the antialiased edge of
// the main photographed object. This is an in-memory sprite viewport, not a
// modified or procedurally recreated image asset.
juce::Rectangle<int> bodyBounds(const juce::Image& image) {
    int w=image.getWidth(),h=image.getHeight();
    std::vector<uint8_t> mask((size_t)w*h,0);
    juce::Image::BitmapData bitmap(image,juce::Image::BitmapData::readOnly);
    int seed=-1;double best=1e30;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(bitmap.getPixelColour(x,y).getAlpha()>=96){
        int index=y*w+x;mask[(size_t)index]=1;
        double distance=(x-w*.5)*(x-w*.5)+(y-h*.5)*(y-h*.5);
        if(distance<best){best=distance;seed=index;}
    }
    if(seed<0)return image.getBounds();
    std::vector<int> queue;queue.reserve((size_t)w*h/2);queue.push_back(seed);mask[(size_t)seed]=0;
    int minx=w,miny=h,maxx=0,maxy=0;
    for(size_t head=0;head<queue.size();++head){
        int index=queue[head],x=index%w,y=index/w;
        minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);
        auto visit=[&](int next){if(mask[(size_t)next]){mask[(size_t)next]=0;queue.push_back(next);}};
        if(x>0)visit(index-1);if(x+1<w)visit(index+1);if(y>0)visit(index-w);if(y+1<h)visit(index+w);
    }
    return juce::Rectangle<int>::leftTopRightBottom(minx,miny,maxx+1,maxy+1).expanded(2).getIntersection(image.getBounds());
}

std::array<juce::Image,2> horizontalStates(const juce::Image& sheet) {
    int w=sheet.getWidth()/2,h=sheet.getHeight();
    std::array<juce::Image,2> cells{sheet.getClippedImage({0,0,w,h}),sheet.getClippedImage({w,0,w,h})};
    // A shared viewport keeps the photographed recess stationary when the
    // lever or cap moves; separate tight crops would stretch the two states.
    auto viewport=bodyBounds(cells[0]).getUnion(bodyBounds(cells[1])).getIntersection(cells[0].getBounds());
    return {cells[0].getClippedImage(viewport),cells[1].getClippedImage(viewport)};
}

std::pair<int,int> longestPaperRun(const juce::Image& image,bool vertical,int fixed) {
    int count=vertical?image.getHeight():image.getWidth(),first=-1,bestFirst=0,bestLast=count;
    int longest=0;
    for(int i=0;i<=count;++i){
        bool paper=false;
        if(i<count){auto c=image.getPixelAt(vertical?fixed:i,vertical?i:fixed);paper=c.getFloatRed()>.50f&&c.getFloatGreen()>.44f&&c.getFloatRed()>=c.getFloatGreen()*.95f;}
        if(paper && first<0)first=i;
        if(!paper && first>=0){if(i-first>longest){longest=i-first;bestFirst=first;bestLast=i;}first=-1;}
    }
    return {bestFirst,bestLast};
}
}

const RasterAssets& RasterAssets::get(){static const RasterAssets assets;return assets;}
RasterAssets::RasterAssets() {
    icon=png(BinaryData::appicon_png,BinaryData::appicon_pngSize);
    panels={png(BinaryData::ivory_png,BinaryData::ivory_pngSize),png(BinaryData::graphite_png,BinaryData::graphite_pngSize),png(BinaryData::walnut_png,BinaryData::walnut_pngSize)};
    auto rawKnob=png(BinaryData::knob_png,BinaryData::knob_pngSize);knob=rawKnob.getClippedImage(bodyBounds(rawKnob));
    auto rawMeter=png(BinaryData::meter_png,BinaryData::meter_pngSize);meter=rawMeter.getClippedImage(bodyBounds(rawMeter));
    switches=horizontalStates(png(BinaryData::switches_png,BinaryData::switches_pngSize));
    buttons=horizontalStates(png(BinaryData::buttons_png,BinaryData::buttons_pngSize));
    auto glass=png(BinaryData::vfd_png,BinaryData::vfd_pngSize);vfd=glass.getClippedImage(bodyBounds(glass));
    // Materialize source tiles at their own origin. Quartz source-rectangle
    // drawing of a clipped image can otherwise offset the nine-slice borders.
    int border=std::min(vfd.getWidth(),vfd.getHeight())/10;
    int vx[]{0,border,vfd.getWidth()-border,vfd.getWidth()},vy[]{0,border,vfd.getHeight()-border,vfd.getHeight()};
    juce::Image::BitmapData source(vfd,juce::Image::BitmapData::readOnly);
    for(int row=0;row<3;++row)for(int column=0;column<3;++column){
        auto& tile=vfdPieces[(size_t)(row*3+column)];
        tile=juce::Image(juce::Image::ARGB,vx[column+1]-vx[column],vy[row+1]-vy[row],true,juce::SoftwareImageType{});
        juce::Image::BitmapData destination(tile,juce::Image::BitmapData::writeOnly);
        for(int y=0;y<tile.getHeight();++y)for(int x=0;x<tile.getWidth();++x)destination.setPixelColour(x,y,source.getPixelColour(vx[column]+x,vy[row]+y));
    }
    auto keys=png(BinaryData::transport_png,BinaryData::transport_pngSize);
    for(int i=0;i<6;++i){auto cell=keys.getClippedImage({(i%3)*(keys.getWidth()/3),(i/3)*(keys.getHeight()/2),keys.getWidth()/3,keys.getHeight()/2});transport[(size_t)i]=cell.getClippedImage(bodyBounds(cell));}
    auto rawLamps=png(BinaryData::lamps_png,BinaryData::lamps_pngSize);
    int tileW=rawLamps.getWidth()/2,tileH=rawLamps.getHeight()/2;
    for(int i=0;i<4;++i){auto cell=rawLamps.getClippedImage({(i%2)*tileW,(i/2)*tileH,tileW,tileH});lamps[(size_t)i]=cell.getClippedImage(bodyBounds(cell));}
    auto vertical=longestPaperRun(meter,true,meter.getWidth()/2);
    auto horizontal=longestPaperRun(meter,false,(vertical.first+vertical.second)/2);
    meterPaper={(float)horizontal.first/meter.getWidth(),(float)vertical.first/meter.getHeight(),
        (float)(horizontal.second-horizontal.first)/meter.getWidth(),(float)(vertical.second-vertical.first)/meter.getHeight()};
}
