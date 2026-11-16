#include "macos/cocoa.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <Carbon/Carbon.h>

#include <algorithm>
#include <cstdlib>


// Repeated downs (held keys auto-repeat) and ups without a down are ignored.
static void
set_button(InputButton *button, bool down)
{
    if (down && !button->held) {
        button->pressed = true;
        button->held = true;
    } else if (!down && button->held) {
        button->released = true;
        button->held = false;
    }
}

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
    // Releases after losing focus go to whichever app has it now.
    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidResignKeyNotification object:this->window
                                                       queue:nil usingBlock:^(NSNotification *) {
        for (u32 i = 0; i < HH_KEY_COUNT; i++) set_button(&this->input.keys[i], false);
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
    for (u32 i = 0; i < HH_KEY_COUNT; i++) {
        this->input.keys[i].pressed = false;
        this->input.keys[i].released = false;
    }

    @autoreleasepool {
        NSView *view = this->window.contentView;
        while (true) {
            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil
                                                   inMode:NSDefaultRunLoopMode dequeue:YES];
            if (event == nil) break;

            NSEventType type = event.type;
            bool mouse_down = type == NSEventTypeLeftMouseDown || type == NSEventTypeRightMouseDown ||
                              type == NSEventTypeOtherMouseDown;
            bool mouse_up = type == NSEventTypeLeftMouseUp || type == NSEventTypeRightMouseUp ||
                            type == NSEventTypeOtherMouseUp;
            if (mouse_down || mouse_up) {
                Key key = HH_KEY_COUNT;
                switch (event.buttonNumber) {
                    case 0: key = HH_KEY_MOUSE_LEFT;   break;
                    case 1: key = HH_KEY_MOUSE_RIGHT;  break;
                    case 2: key = HH_KEY_MOUSE_MIDDLE; break;
                }
                // Presses on the title bar are AppKit's; releases always count so buttons can't stick.
                NSPoint point = [view convertPoint:event.locationInWindow fromView:nil];
                if (key != HH_KEY_COUNT && (mouse_up || NSPointInRect(point, view.bounds))) {
                    set_button(&this->input.keys[key], mouse_down);
                }
            }

            // Key events aren't forwarded: AppKit beeps at key events nothing handles.
            if (type != NSEventTypeKeyDown && type != NSEventTypeKeyUp) {
                [NSApp sendEvent:event];
                continue;
            }

            Key key;
            switch (event.keyCode) {
                case kVK_ANSI_W:        key = HH_KEY_W;        break;
                case kVK_ANSI_S:        key = HH_KEY_S;        break;
                case kVK_ANSI_A:        key = HH_KEY_A;        break;
                case kVK_ANSI_D:        key = HH_KEY_D;        break;
                case kVK_UpArrow:       key = HH_KEY_UP;       break;
                case kVK_DownArrow:     key = HH_KEY_DOWN;     break;
                case kVK_LeftArrow:     key = HH_KEY_LEFT;     break;
                case kVK_RightArrow:    key = HH_KEY_RIGHT;    break;
                case kVK_ANSI_Q:        key = HH_KEY_Q;        break;
                default: continue;
            }

            set_button(&this->input.keys[key], type == NSEventTypeKeyDown);
        }

        // The view's origin is bottom-left and the backbuffer is stretched to fill it.
        NSPoint mouse = [view convertPoint:this->window.mouseLocationOutsideOfEventStream fromView:nil];
        NSSize size = view.bounds.size;
        f64 x = mouse.x / size.width * this->backbuffer.width;
        f64 y = (size.height - mouse.y) / size.height * this->backbuffer.height;
        this->input.mouse_x = (i32)std::clamp(x, 0.0, (f64)(this->backbuffer.width - 1));
        this->input.mouse_y = (i32)std::clamp(y, 0.0, (f64)(this->backbuffer.height - 1));
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
