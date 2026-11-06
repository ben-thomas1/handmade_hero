#pragma once

#include "game/backbuffer.hpp"
#include "game/input.hpp"

#include <IOSurface/IOSurfaceRef.h>

@class NSWindow;
@class NSView;
@class NSTrackingArea;
@class CALayer;
@class CADisplayLink;
@class WindowDelegate;

struct Window {
    NSWindow *window;
    NSView *view;
    CALayer *layer;
    NSTrackingArea *tracking_area;
    CADisplayLink *display_link;
    WindowDelegate *delegate;
    BackBuffer back_buffers[2];
    IOSurfaceRef surfaces[2];
    u32 render_index;
    bool close_requested;
    bool frame_ready;
    bool focus_gained;
    bool focus_lost;
    // Pointer position in view points, origin at the top left like Wayland.
    f32 mouse_window_x;
    f32 mouse_window_y;

    void init(u32 buffer_width, u32 buffer_height);
    void process_events(Input *input, i32 timeout_ms = 0);
    bool ready_to_render() const;
    // Locks the buffer's surface for CPU writes until present().
    BackBuffer *back_buffer();
    void present();
    void swap_buffers();
    void free();
};
