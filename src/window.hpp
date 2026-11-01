#pragma once

#include "backbuffer.hpp"

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shm;
struct wl_buffer;
struct wl_callback;
struct xdg_wm_base;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;

struct Window
{
    wl_display *display;
    wl_registry *registry;
    wl_compositor *compositor;
    wl_shm *shm;
    xdg_wm_base *shell;
    wl_surface *surface;
    xdg_surface *shell_surface;
    xdg_toplevel *toplevel;
    BackBuffer back_buffers[2];
    u32 render_index;
    wl_buffer *wayland_buffers[2];
    bool buffer_busy[2];
    wl_callback *frame_callback;
    bool configured;
    bool running;
    bool frame_ready;

    void init(u32 width, u32 height);
    BackBuffer *back_buffer();
    void wait_until_ready();
    void present();
    void swap_buffers();
    void free();
};
