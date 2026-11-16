#include "linux/wayland.hpp"

#include "xdg-shell-client-protocol.h"

#include <wayland-client.h>

#include <algorithm>
#include <cstring>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>


// Ups without a down are ignored.
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

static void
on_ping(void *, xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static const xdg_wm_base_listener SHELL_LISTENER = { .ping = on_ping };

static void
on_surface_configure(void *data, xdg_surface *surface, uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
    ((Window *)data)->configured = true;
}

static const xdg_surface_listener SURFACE_LISTENER = { .configure = on_surface_configure };

static void
on_toplevel_configure(void *, xdg_toplevel *, int32_t, int32_t, wl_array *)
{
}

static void
on_close(void *data, xdg_toplevel *)
{
    ((Window *)data)->input.quit = true;
}

static const xdg_toplevel_listener TOPLEVEL_LISTENER = {
    .configure = on_toplevel_configure,
    .close = on_close,
    .configure_bounds = nullptr,
    .wm_capabilities = nullptr,
};

static void
on_frame_done(void *data, wl_callback *callback, uint32_t)
{
    wl_callback_destroy(callback);
    ((Window *)data)->frame_pending = false;
}

static const wl_callback_listener FRAME_LISTENER = { .done = on_frame_done };

static void
on_keymap(void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t)
{
    // Evdev key codes don't need the layout's keymap.
    close(fd);
}

static void
on_enter(void *, wl_keyboard *, uint32_t, wl_surface *, wl_array *)
{
}

static void
on_leave(void *data, wl_keyboard *, uint32_t, wl_surface *)
{
    // Releases after losing focus never arrive.
    Input *input = &((Window *)data)->input;
    for (u32 i = 0; i < HH_KEY_COUNT; i++) set_button(&input->keys[i], false);
}

static void
on_key(void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t code, uint32_t state)
{
    Key key;
    switch (code) {
        case KEY_W:        key = HH_KEY_W;        break;
        case KEY_S:        key = HH_KEY_S;        break;
        case KEY_A:        key = HH_KEY_A;        break;
        case KEY_D:        key = HH_KEY_D;        break;
        case KEY_UP:       key = HH_KEY_UP;       break;
        case KEY_DOWN:     key = HH_KEY_DOWN;     break;
        case KEY_LEFT:     key = HH_KEY_LEFT;     break;
        case KEY_RIGHT:    key = HH_KEY_RIGHT;    break;
        case KEY_Q:        key = HH_KEY_Q;        break;
        default: return;
    }
    set_button(&((Window *)data)->input.keys[key], state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

static void
on_modifiers(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
{
}

static const wl_keyboard_listener KEYBOARD_LISTENER = {
    .keymap = on_keymap,
    .enter = on_enter,
    .leave = on_leave,
    .key = on_key,
    .modifiers = on_modifiers,
    .repeat_info = nullptr,
};

static void
on_pointer_enter(void *data, wl_pointer *, uint32_t, wl_surface *, wl_fixed_t x, wl_fixed_t y)
{
    Window *window = (Window *)data;
    window->pointer_x = wl_fixed_to_double(x);
    window->pointer_y = wl_fixed_to_double(y);
}

static void
on_pointer_leave(void *, wl_pointer *, uint32_t, wl_surface *)
{
}

static void
on_pointer_motion(void *data, wl_pointer *, uint32_t, wl_fixed_t x, wl_fixed_t y)
{
    Window *window = (Window *)data;
    window->pointer_x = wl_fixed_to_double(x);
    window->pointer_y = wl_fixed_to_double(y);
}

static void
on_pointer_button(void *data, wl_pointer *, uint32_t, uint32_t, uint32_t button, uint32_t state)
{
    // Presses only arrive over the surface; the compositor sends the release even if the pointer has left.
    Key key;
    switch (button) {
        case BTN_LEFT:   key = HH_KEY_MOUSE_LEFT;   break;
        case BTN_RIGHT:  key = HH_KEY_MOUSE_RIGHT;  break;
        case BTN_MIDDLE: key = HH_KEY_MOUSE_MIDDLE; break;
        default: return;
    }
    set_button(&((Window *)data)->input.keys[key], state == WL_POINTER_BUTTON_STATE_PRESSED);
}

static void
on_pointer_axis(void *, wl_pointer *, uint32_t, uint32_t, wl_fixed_t)
{
}

static const wl_pointer_listener POINTER_LISTENER = {
    .enter = on_pointer_enter,
    .leave = on_pointer_leave,
    .motion = on_pointer_motion,
    .button = on_pointer_button,
    .axis = on_pointer_axis,
    .frame = nullptr,
    .axis_source = nullptr,
    .axis_stop = nullptr,
    .axis_discrete = nullptr,
    .axis_value120 = nullptr,
    .axis_relative_direction = nullptr,
    .warp = nullptr,
};

static void
on_seat_capabilities(void *data, wl_seat *seat, uint32_t capabilities)
{
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        wl_keyboard_add_listener(wl_seat_get_keyboard(seat), &KEYBOARD_LISTENER, data);
    }
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        wl_pointer_add_listener(wl_seat_get_pointer(seat), &POINTER_LISTENER, data);
    }
}

static const wl_seat_listener SEAT_LISTENER = { .capabilities = on_seat_capabilities, .name = nullptr };

static void
on_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t)
{
    Window *window = (Window *)data;
    if (std::strcmp(interface, "wl_compositor") == 0) {
        window->compositor = (wl_compositor *)wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (std::strcmp(interface, "wl_shm") == 0) {
        window->shm = (wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
        window->shell = (xdg_wm_base *)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(window->shell, &SHELL_LISTENER, window);
    } else if (std::strcmp(interface, "wl_seat") == 0) {
        wl_seat *seat = (wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &SEAT_LISTENER, window);
    }
}

static void
on_global_remove(void *, wl_registry *, uint32_t)
{
}

static const wl_registry_listener REGISTRY_LISTENER = { .global = on_global, .global_remove = on_global_remove };

void
Window::init(u32 width, u32 height)
{
    this->display = wl_display_connect(nullptr);
    wl_registry_add_listener(wl_display_get_registry(this->display), &REGISTRY_LISTENER, this);
    wl_display_roundtrip(this->display);

    this->surface = wl_compositor_create_surface(this->compositor);
    xdg_surface *shell_surface = xdg_wm_base_get_xdg_surface(this->shell, this->surface);
    xdg_surface_add_listener(shell_surface, &SURFACE_LISTENER, this);
    xdg_toplevel *toplevel = xdg_surface_get_toplevel(shell_surface);
    xdg_toplevel_add_listener(toplevel, &TOPLEVEL_LISTENER, this);
    xdg_toplevel_set_title(toplevel, "Handmade Hero");
    // The compositor configures the window in reply to the first commit; nothing can be drawn before that.
    wl_surface_commit(this->surface);
    while (!this->configured) {
        wl_display_dispatch(this->display);
    }

    this->backbuffer.width  = width;
    this->backbuffer.height = height;
    this->backbuffer.pitch  = width * 4;
    u32 size = this->backbuffer.pitch * height;
    // The compositor reads the pixels straight from this shared memory.
    int fd = memfd_create("handmade-hero-backbuffer", MFD_CLOEXEC);
    ftruncate(fd, size);
    this->backbuffer.pixels = (u32 *)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    wl_shm_pool *pool = wl_shm_create_pool(this->shm, fd, (i32)size);
    this->buffer = wl_shm_pool_create_buffer(pool, 0, (i32)width, (i32)height, (i32)this->backbuffer.pitch,
                                             WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
}

void
Window::process_events(Input *out)
{
    for (u32 i = 0; i < HH_KEY_COUNT; i++) {
        this->input.keys[i].pressed = false;
        this->input.keys[i].released = false;
    }

    wl_display_roundtrip(this->display);

    // The surface is the backbuffer's size at scale 1, so surface coordinates are backbuffer pixels.
    this->input.mouse_x = (i32)std::clamp(this->pointer_x, 0.0, (f64)(this->backbuffer.width - 1));
    this->input.mouse_y = (i32)std::clamp(this->pointer_y, 0.0, (f64)(this->backbuffer.height - 1));
    *out = this->input;
}

void
Window::present()
{
    // Sending frames faster than the compositor shows them only queues them up.
    if (this->frame_pending) return;
    this->frame_pending = true;
    wl_callback_add_listener(wl_surface_frame(this->surface), &FRAME_LISTENER, this);
    wl_surface_attach(this->surface, this->buffer, 0, 0);
    wl_surface_damage(this->surface, 0, 0, (i32)this->backbuffer.width, (i32)this->backbuffer.height);
    wl_surface_commit(this->surface);
    wl_display_flush(this->display);
}
