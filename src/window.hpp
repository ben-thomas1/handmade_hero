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
struct wp_viewporter;
struct wp_viewport;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;

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
    wp_viewporter *viewporter;
    wp_viewport *viewport;
    zxdg_decoration_manager_v1 *decoration_manager;
    zxdg_toplevel_decoration_v1 *decoration;
    u32 width;
    u32 height;
    BackBuffer back_buffers[2];
    u32 render_index;
    wl_buffer *wayland_buffers[2];
    bool buffer_busy[2];
    wl_callback *frame_callback;
    bool configured;
    bool running;
    bool frame_ready;

    void init(u32 buffer_width, u32 buffer_height);
    BackBuffer *back_buffer();
    void wait_until_ready();
    void present();
    void swap_buffers();
    void free();
};
