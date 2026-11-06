#pragma once

#include "game/backbuffer.hpp"
#include "game/input.hpp"
#include "base/queue.hpp"

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shm;
struct wl_buffer;
struct wl_callback;
struct wl_seat;
struct wl_keyboard;
struct wl_pointer;
struct xdg_wm_base;
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;
struct wp_viewporter;
struct wp_viewport;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;

struct Window {
    wl_display *display;
    wl_registry *registry;
    wl_compositor *compositor;
    wl_shm *shm;
    wl_seat *seat;
    wl_keyboard *keyboard;
    wl_pointer *pointer;
    bool keyboard_available;
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
    bool close_requested;
    bool frame_ready;
    bool keys_down[128];
    f32 mouse_window_x;
    f32 mouse_window_y;
    InputEvent event_buffer[256];
    Queue<InputEvent> events;

    void init(u32 buffer_width, u32 buffer_height);
    void process_events(Input *input, i32 timeout_ms = 0);
    bool ready_to_render() const;
    BackBuffer *back_buffer();
    void present();
    void swap_buffers();
    void free();
};
