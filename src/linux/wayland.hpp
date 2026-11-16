#pragma once

#include "handmade.hpp"

struct wl_display;
struct wl_compositor;
struct wl_shm;
struct wl_surface;
struct wl_buffer;
struct xdg_wm_base;


struct Window {
    wl_display *display;
    wl_compositor *compositor;
    wl_shm *shm;
    xdg_wm_base *shell;
    wl_surface *surface;
    wl_buffer *buffer;
    bool configured;
    // Set while the compositor hasn't yet shown the last frame we sent.
    bool frame_pending;
    Backbuffer backbuffer;
    Input input;
    // Surface coordinates of the pointer, as of its last motion over the window.
    f64 pointer_x;
    f64 pointer_y;

    void init(u32 width, u32 height);
    void process_events(Input *out);
    void present();
};
