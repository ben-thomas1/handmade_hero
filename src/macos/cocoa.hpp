#pragma once

#include "handmade.hpp"

#include <CoreGraphics/CoreGraphics.h>

@class NSWindow;
@class CALayer;


struct Window {
    NSWindow *window;
    CALayer *layer;
    CGContextRef context;
    Backbuffer backbuffer;
    Input input;

    void init(u32 width, u32 height);
    void process_events(Input *out);
    void present();
};
