#include "macos/cocoa.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <Carbon/Carbon.h>

#include <cstdlib>


void
Window::init(u32 width, u32 height)
{
    [NSApplication sharedApplication];
    // A bare binary started from a terminal needs this for focus and a Dock icon.
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];

    NSRect frame = NSMakeRect(0, 0, width, height);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                              NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    this->window = [[NSWindow alloc] initWithContentRect:frame styleMask:style
                                                 backing:NSBackingStoreBuffered defer:NO];
    this->window.title = @"Handmade Hero";
    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification object:this->window
                                                       queue:nil usingBlock:^(NSNotification *) {
        this->input.quit = true;
    }];

    // The backbuffer is shown as the contents of the view's layer, stretched to fit.
    this->layer = [[CALayer alloc] init];
    this->layer.contentsGravity = kCAGravityResize;
    NSView *view = [[NSView alloc] initWithFrame:frame];
    view.layer = this->layer;
    view.wantsLayer = YES;
    this->window.contentView = view;

    this->backbuffer.width  = width;
    this->backbuffer.height = height;
    this->backbuffer.pitch  = width * 4;
    this->backbuffer.pixels = (u32 *)std::malloc((size_t)this->backbuffer.pitch * height);
    // 0x00RRGGBB in a little-endian u32.
    this->context = CGBitmapContextCreate(this->backbuffer.pixels, width, height, 8, this->backbuffer.pitch,
                                          CGColorSpaceCreateDeviceRGB(),
                                          (u32)kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);

    [this->window center];
    [this->window makeKeyAndOrderFront:nil];
    [NSApp activate];
}

void
Window::process_events(Input *out)
{
    @autoreleasepool {
        while (true) {
            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil
                                                   inMode:NSDefaultRunLoopMode dequeue:YES];
            if (event == nil) break;

            if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp) {
                bool down = event.type == NSEventTypeKeyDown;
                switch (event.keyCode) {
                    case kVK_ANSI_W: case kVK_UpArrow:    this->input.up    = down; break;
                    case kVK_ANSI_S: case kVK_DownArrow:  this->input.down  = down; break;
                    case kVK_ANSI_A: case kVK_LeftArrow:  this->input.left  = down; break;
                    case kVK_ANSI_D: case kVK_RightArrow: this->input.right = down; break;
                    case kVK_Escape:                      this->input.quit  = true; break;
                }
                // Not forwarded: AppKit beeps at key events nothing handles.
                continue;
            }
            [NSApp sendEvent:event];
        }
    }
    *out = this->input;
}

void
Window::present()
{
    @autoreleasepool {
        CGImageRef image = CGBitmapContextCreateImage(this->context);
        [CATransaction begin];
        // Contents changes otherwise cross-fade over a quarter second.
        [CATransaction setDisableActions:YES];
        this->layer.contents = (id)image;
        [CATransaction commit];
        CGImageRelease(image);
    }
}
