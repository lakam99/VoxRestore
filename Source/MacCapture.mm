#include "MacCapture.h"
#import <Cocoa/Cocoa.h>

juce::Image captureMacNativeView(juce::Component& component){
    @autoreleasepool {
        auto* peer=component.getPeer();if(!peer)throw std::runtime_error("The Mac editor has no native peer.");
        NSView* view=(__bridge NSView*)peer->getNativeHandle();
        [view displayIfNeeded];
        NSBitmapImageRep* bitmap=[view bitmapImageRepForCachingDisplayInRect:[view bounds]];
        [view cacheDisplayInRect:[view bounds] toBitmapImageRep:bitmap];
        NSData* png=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        auto image=juce::ImageFileFormat::loadFrom([png bytes],(size_t)[png length]);
        if(!image.isValid())throw std::runtime_error("Native Mac editor capture failed.");
        bool visible=false;for(int y=0;y<image.getHeight();y+=8)for(int x=0;x<image.getWidth();x+=8){auto pixel=image.getPixelAt(x,y);visible|=pixel.getFloatRed()+pixel.getFloatGreen()+pixel.getFloatBlue()>.2f;}
        if(!visible)throw std::runtime_error("Native Mac editor capture is blank.");
        return image;
    }
}
