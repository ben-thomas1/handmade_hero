#include "platform/macos/cocoa.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/IOLLEvent.h>

#include <cassert>

// Virtual key codes in the same order as Input::buttons.
static constexpr u16 KEY_CODES[KEY_COUNT] = {
    kVK_ANSI_W,
    kVK_ANSI_A,
    kVK_ANSI_S,
    kVK_ANSI_D,
    kVK_ANSI_Q,
    kVK_ANSI_E,
    kVK_ANSI_F,
    kVK_ANSI_1,
    kVK_ANSI_2,
    kVK_ANSI_3,
    kVK_ANSI_4,
    kVK_Space,
    kVK_Escape,
    kVK_Shift,
    kVK_RightShift,
    kVK_Control,
    kVK_RightControl,
    kVK_Option,
    kVK_RightOption,
};

// Modifier keys arrive as flagsChanged events; this flag says whether each one is down.
static constexpr u32 KEY_MODIFIER_MASKS[KEY_COUNT] = {
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    NX_DEVICELSHIFTKEYMASK,
    NX_DEVICERSHIFTKEYMASK,
    NX_DEVICELCTLKEYMASK,
    NX_DEVICERCTLKEYMASK,
    NX_DEVICELALTKEYMASK,
    NX_DEVICERALTKEYMASK,
};

// NSEvent button numbers in the same order as Input::mouse_buttons.
static constexpr NSInteger MOUSE_CODES[MOUSE_COUNT] = {
    0, // Left
    2, // Middle
    1, // Right
};

@interface WindowDelegate : NSObject <NSWindowDelegate> {
@public
    Window *window;
}
- (void)onFrame:(CADisplayLink *)link;
@end

@implementation WindowDelegate

- (BOOL)windowShouldClose:(NSWindow *)sender
{
    (void)sender;
    window->close_requested = true;
    return NO;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    (void)notification;
    window->focus_gained = true;
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    (void)notification;
    window->focus_lost = true;
}

- (void)onFrame:(CADisplayLink *)link
{
    (void)link;
    window->frame_ready = true;
    // Wake process_events if it is waiting for the next event.
    NSEvent *wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
                                  modifierFlags:0 timestamp:0 windowNumber:0 context:nil
                                        subtype:0 data1:0 data2:0];
    [NSApp postEvent:wake atStart:NO];
}

@end

static u32
key_index(u16 key_code)
{
    for (u32 i = 0; i < KEY_COUNT; i++) {
        if (KEY_CODES[i] == key_code) return i;
    }
    return KEY_COUNT;
}

static void
set_button(InputButton *button, bool down, bool press_edges)
{
    if (button->is_down == down) return;
    button->is_down = down;
    if (down && press_edges) button->pressed = true;
    if (!down) button->released = true;
}

static bool
any_mouse_button_down(const Input *input)
{
    for (u32 i = 0; i < MOUSE_COUNT; i++) {
        if (input->mouse_buttons[i].is_down) return true;
    }
    return false;
}

static void
mouse_leave(Input *input)
{
    input->mouse_inside = false;
    for (u32 i = 0; i < MOUSE_COUNT; i++) {
        set_button(&input->mouse_buttons[i], false, false);
    }
}

void
Window::init(u32 buffer_width, u32 buffer_height)
{
    *this = {};
    @autoreleasepool {
        [NSApplication sharedApplication];
        // A bare binary started from a terminal needs this for focus and a Dock icon.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];

        NSRect frame = NSMakeRect(0, 0, buffer_width, buffer_height);
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        this->window = [[NSWindow alloc] initWithContentRect:frame styleMask:style
                                                     backing:NSBackingStoreBuffered defer:NO];
        this->window.releasedWhenClosed = NO;
        this->window.title = @"Handmade Hero";
        this->delegate = [[WindowDelegate alloc] init];
        this->delegate->window = this;
        this->window.delegate = this->delegate;

        // Layer-hosting view: AppKit never draws into the layer we present to.
        // The entire fixed-size buffer is scaled to the view's size.
        this->layer = [[CALayer alloc] init];
        this->layer.contentsGravity = kCAGravityResize;
        this->layer.opaque = YES;
        // The X byte is zero, so contents composite as premultiplied colour over this background.
        this->layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
        this->view = [[NSView alloc] initWithFrame:frame];
        this->view.layer = this->layer;
        this->view.wantsLayer = YES;
        this->window.contentView = this->view;

        NSTrackingAreaOptions tracking = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                                         NSTrackingActiveAlways | NSTrackingInVisibleRect;
        this->tracking_area = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:tracking
                                                             owner:this->view userInfo:nil];
        [this->view addTrackingArea:this->tracking_area];

        this->display_link = [[this->view displayLinkWithTarget:this->delegate selector:@selector(onFrame:)] retain];
        [this->display_link addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];

        for (u32 i = 0; i < 2; i++) {
            PixelBuffer *buffer = &this->back_buffers[i];
            buffer->width = buffer_width;
            buffer->height = buffer_height;
            size_t pitch = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow,
                                                  (size_t)buffer_width * buffer->bytes_per_pixel);
            NSDictionary *properties = @{
                (id)kIOSurfaceWidth : @(buffer_width),
                (id)kIOSurfaceHeight : @(buffer_height),
                (id)kIOSurfaceBytesPerElement : @(buffer->bytes_per_pixel),
                (id)kIOSurfaceBytesPerRow : @(pitch),
                // XRGB8888 in a little-endian u32 is B, G, R, X in memory.
                (id)kIOSurfacePixelFormat : @((u32)'BGRA'),
            };
            this->surfaces[i] = IOSurfaceCreate((CFDictionaryRef)properties);
            assert(this->surfaces[i] != nullptr && "Could not create the back buffer surface");
            buffer->pitch = (u32)IOSurfaceGetBytesPerRow(this->surfaces[i]);
        }

        [this->window center];
        [this->window makeKeyAndOrderFront:nil];
        [NSApp activate];
    }
    this->render_index = 0;
    this->frame_ready = true;
}

void
Window::process_events(Input *input, i32 timeout_ms)
{
    @autoreleasepool {
        NSDate *until = timeout_ms > 0 ? [NSDate dateWithTimeIntervalSinceNow:timeout_ms / 1000.0]
                                       : [NSDate distantPast];
        while (true) {
            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until
                                                   inMode:NSDefaultRunLoopMode dequeue:YES];
            // Wait at most once, then drain the events already queued.
            until = [NSDate distantPast];
            if (event == nil) break;

            NSRect bounds = this->view.bounds;
            f32 scale_x = (f32)this->back_buffers[0].width / (f32)bounds.size.width;
            f32 scale_y = (f32)this->back_buffers[0].height / (f32)bounds.size.height;
            bool forward = true;

            switch (event.type) {
                case NSEventTypeApplicationDefined:
                    forward = false;
                    break;
                case NSEventTypeKeyDown:
                case NSEventTypeKeyUp:
                {
                    // AppKit beeps at key events nothing handles.
                    forward = false;
                    if (event.isARepeat) break;
                    u32 key = key_index(event.keyCode);
                    if (key == KEY_COUNT) break;
                    set_button(&input->buttons[key], event.type == NSEventTypeKeyDown, true);
                    input->has_input = true;
                    break;
                }
                case NSEventTypeFlagsChanged:
                {
                    u32 key = key_index(event.keyCode);
                    if (key == KEY_COUNT || KEY_MODIFIER_MASKS[key] == 0) break;
                    set_button(&input->buttons[key], (event.modifierFlags & KEY_MODIFIER_MASKS[key]) != 0, true);
                    input->has_input = true;
                    break;
                }
                case NSEventTypeMouseEntered:
                case NSEventTypeMouseMoved:
                case NSEventTypeLeftMouseDragged:
                case NSEventTypeRightMouseDragged:
                case NSEventTypeOtherMouseDragged:
                {
                    if (event.window != this->window) break;
                    if (event.type == NSEventTypeMouseEntered && event.trackingArea != this->tracking_area) break;
                    bool dragged = event.type != NSEventTypeMouseEntered && event.type != NSEventTypeMouseMoved;
                    // Like Wayland, drags only report motion once the pointer has entered.
                    if (dragged && !input->mouse_inside) break;

                    NSPoint point = [this->view convertPoint:event.locationInWindow fromView:nil];
                    f32 x = (f32)point.x;
                    f32 y = (f32)(bounds.size.height - point.y);
                    if (event.type != NSEventTypeMouseEntered && input->mouse_inside) {
                        input->mouse_dx += (x - this->mouse_window_x) * scale_x;
                        input->mouse_dy += (y - this->mouse_window_y) * scale_y;
                    }
                    this->mouse_window_x = x;
                    this->mouse_window_y = y;
                    input->mouse_x = x * scale_x;
                    input->mouse_y = y * scale_y;
                    // The pointer may already be inside when the window opens, with no entered event.
                    input->mouse_inside = true;
                    input->has_input = true;
                    break;
                }
                case NSEventTypeMouseExited:
                    if (event.trackingArea != this->tracking_area) break;
                    // Wayland keeps the pointer on the surface until the buttons are released.
                    if (any_mouse_button_down(input)) break;
                    mouse_leave(input);
                    input->has_input = true;
                    break;
                case NSEventTypeLeftMouseDown:
                case NSEventTypeRightMouseDown:
                case NSEventTypeOtherMouseDown:
                case NSEventTypeLeftMouseUp:
                case NSEventTypeRightMouseUp:
                case NSEventTypeOtherMouseUp:
                {
                    if (event.window != this->window) break;
                    bool down = event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown ||
                                event.type == NSEventTypeOtherMouseDown;
                    NSPoint point = [this->view convertPoint:event.locationInWindow fromView:nil];
                    bool inside = NSPointInRect(point, bounds);
                    // Presses outside the view land on the title bar or resize edges.
                    if (down && (!input->mouse_inside || !inside)) break;
                    for (u32 i = 0; i < MOUSE_COUNT; i++) {
                        if (event.buttonNumber == MOUSE_CODES[i]) {
                            set_button(&input->mouse_buttons[i], down, true);
                            input->has_input = true;
                            break;
                        }
                    }
                    if (!down && !inside && input->mouse_inside && !any_mouse_button_down(input)) {
                        mouse_leave(input);
                    }
                    break;
                }
                case NSEventTypeScrollWheel:
                {
                    if (event.window != this->window || !input->mouse_inside) break;
                    f32 dx = (f32)event.scrollingDeltaX;
                    f32 dy = (f32)event.scrollingDeltaY;
                    // Match Wayland: positive is right and down, about 10 units per wheel notch.
                    if (!event.hasPreciseScrollingDeltas) {
                        dx *= 10.0f;
                        dy *= 10.0f;
                    }
                    input->scroll_x -= dx;
                    input->scroll_y -= dy;
                    input->has_input = true;
                    break;
                }
                default:
                    break;
            }
            if (forward) [NSApp sendEvent:event];

            // Set by the delegate while AppKit handles an event.
            if (this->focus_lost) {
                this->focus_lost = false;
                for (u32 i = 0; i < KEY_COUNT; i++) {
                    set_button(&input->buttons[i], false, false);
                }
                input->has_input = true;
            }
            if (this->focus_gained) {
                this->focus_gained = false;
                // Keys already held on focus are down, but were not pressed this frame.
                for (u32 i = 0; i < KEY_COUNT; i++) {
                    bool down = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, KEY_CODES[i]);
                    set_button(&input->buttons[i], down, false);
                }
                input->has_input = true;
            }
        }

        if (this->close_requested) input->close_request = true;
        NSRect bounds = this->view.bounds;
        input->mouse_x = this->mouse_window_x * (f32)this->back_buffers[0].width / (f32)bounds.size.width;
        input->mouse_y = this->mouse_window_y * (f32)this->back_buffers[0].height / (f32)bounds.size.height;
    }
}

bool
Window::ready_to_render() const
{
    return !this->close_requested && this->frame_ready && !IOSurfaceIsInUse(this->surfaces[this->render_index]);
}

PixelBuffer *
Window::back_buffer()
{
    PixelBuffer *buffer = &this->back_buffers[this->render_index];
    IOSurfaceRef surface = this->surfaces[this->render_index];
    IOSurfaceLock(surface, 0, nullptr);
    buffer->buffer = (u32 *)IOSurfaceGetBaseAddress(surface);
    return buffer;
}

void
Window::present()
{
    IOSurfaceRef surface = this->surfaces[this->render_index];
    IOSurfaceUnlock(surface, 0, nullptr);
    this->frame_ready = false;
    @autoreleasepool {
        [CATransaction begin];
        // Contents changes otherwise cross-fade over a quarter second.
        [CATransaction setDisableActions:YES];
        this->layer.contents = (id)surface;
        [CATransaction commit];
    }
}

void
Window::swap_buffers()
{
    this->render_index = (this->render_index + 1) % 2;
}

void
Window::free()
{
    @autoreleasepool {
        [this->display_link invalidate];
        [this->display_link release];
        [this->view removeTrackingArea:this->tracking_area];
        [this->tracking_area release];
        this->window.delegate = nil;
        [this->window close];
        [this->window release];
        [this->view release];
        [this->layer release];
        [this->delegate release];
        for (u32 i = 0; i < 2; i++) {
            if (this->surfaces[i] != nullptr) CFRelease(this->surfaces[i]);
        }
    }
    *this = {};
}
