#include "platform/linux/wayland.hpp"

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-client-protocol.h"
#include "viewporter-client-protocol.h"

#include <wayland-client.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <print>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

// Evdev codes in the same order as Input::buttons.
inline constexpr u32 KEY_CODES[KEY_COUNT] = {
    17,  // W
    30,  // A
    31,  // S
    32,  // D
    16,  // Q
    18,  // E
    33,  // F
    2,   // 1
    3,   // 2
    4,   // 3
    5,   // 4
    57,  // Space
    1,   // Escape
    42,  // Left Shift
    54,  // Right Shift
    29,  // Left Ctrl
    97,  // Right Ctrl
    56,  // Left Alt
    100, // Right Alt
};

inline constexpr u32 MOUSE_CODES[MOUSE_COUNT] = {
    272, // Left
    274, // Middle
    273, // Right
};

static void
enqueue_input_event(Window *window, InputEvent event)
{
    // Stop rather than continue with a potentially missing key release.
    if (!window->events.push(event))
    {
        std::println(stderr, "Wayland input event queue full (capacity: {})", window->events.cap);
        window->close_requested = true;
    }
}

static void
on_ping(void *, xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static const xdg_wm_base_listener SHELL_LISTENER = { .ping = on_ping };

static void
on_frame_done(void *data, wl_callback *callback, uint32_t)
{
    Window *window = (Window *)data;
    wl_callback_destroy(callback);
    window->frame_callback = nullptr;
    window->frame_ready = true;
}

static const wl_callback_listener FRAME_LISTENER = { .done = on_frame_done };

static void
on_buffer_release(void *data, wl_buffer *)
{
    bool *busy = (bool *)data;
    *busy = false;
}

static const wl_buffer_listener BUFFER_LISTENER = { .release = on_buffer_release };

static void
on_surface_configure(void *data, xdg_surface *surface, uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
    Window *window = (Window *)data;
    window->configured = true;
}

static const xdg_surface_listener SURFACE_LISTENER = { .configure = on_surface_configure };

static void
on_toplevel_configure(void *data, xdg_toplevel *, int32_t width, int32_t height,
                      wl_array *)
{
    Window *window = (Window *)data;
    // Zero means the compositor leaves this dimension up to us.
    if (width > 0) {
        window->width = (u32)width;
    }
    if (height > 0) {
        window->height = (u32)height;
    }
}

static void
on_close(void *data, xdg_toplevel *)
{
    Window *window = (Window *)data;
    window->close_requested = true;
    enqueue_input_event(window, { .type = INPUT_EVENT_CLOSE, .key_code = 0, .down = false });
}

static const xdg_toplevel_listener TOPLEVEL_LISTENER = {
    .configure = on_toplevel_configure,
    .close = on_close,
    .configure_bounds = nullptr,
    .wm_capabilities = nullptr,
};

static void
on_decoration_configure(void *, zxdg_toplevel_decoration_v1 *, uint32_t mode)
{
    std::println("Wayland decoration mode: {} (2 = server-side)", mode);
}

static const zxdg_toplevel_decoration_v1_listener DECORATION_LISTENER = {
    .configure = on_decoration_configure,
};

static void
update_keyboard_buttons(Window *window, Input *input, bool press_edges = true)
{
    for (u32 i = 0; i < KEY_COUNT; i++) {
        InputButton *button = &input->buttons[i];
        bool down = window->keys_down[KEY_CODES[i]];
        if (button->is_down != down)
        {
            button->is_down = down;
            if (down && press_edges) button->pressed = true;
            if (!down) button->released = true;
        }
    }
}

static void
on_keyboard_keymap(void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t)
{
    // Physical key codes do not need the layout keymap.
    close(fd);
}

static void
on_keyboard_enter(void *data, wl_keyboard *, uint32_t, wl_surface *, wl_array *keys)
{
    Window *window = (Window *)data;
    enqueue_input_event(window, { .type = INPUT_EVENT_FOCUS_ENTER, .key_code = 0, .down = false });

    u32 *key_codes = (u32 *)keys->data;
    for (u32 i = 0; i < keys->size / sizeof(u32); i++)
    {
        u32 key_code = key_codes[i];
        if (key_code > 0 && key_code < 128)
        {
            enqueue_input_event(window, { .type = INPUT_EVENT_KEY_HELD, .key_code = key_code, .down = true });
        }
    }
}

static void
on_keyboard_leave(void *data, wl_keyboard *, uint32_t, wl_surface *)
{
    Window *window = (Window *)data;
    enqueue_input_event(window, { .type = INPUT_EVENT_FOCUS_LEAVE, .key_code = 0, .down = false });
}

static void
on_keyboard_key(void *data, wl_keyboard *, uint32_t, uint32_t,
                uint32_t key_code, uint32_t state)
{
    Window *window = (Window *)data;
    if (key_code == 0 || key_code >= 128) return;

    enqueue_input_event(window, { .type = INPUT_EVENT_KEY, .key_code = key_code,
                                 .down = state == WL_KEYBOARD_KEY_STATE_PRESSED });
}

static void
on_keyboard_modifiers(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
{
}

static const wl_keyboard_listener KEYBOARD_LISTENER = {
    .keymap = on_keyboard_keymap,
    .enter = on_keyboard_enter,
    .leave = on_keyboard_leave,
    .key = on_keyboard_key,
    .modifiers = on_keyboard_modifiers,
    .repeat_info = nullptr,
};

static void
on_pointer_enter(void *data, wl_pointer *, uint32_t, wl_surface *, wl_fixed_t x, wl_fixed_t y)
{
    Window *window = (Window *)data;
    enqueue_input_event(window, { .type = INPUT_EVENT_MOUSE_ENTER, .key_code = 0, .down = false,
                                 .x = (f32)wl_fixed_to_double(x), .y = (f32)wl_fixed_to_double(y) });
}

static void
on_pointer_leave(void *data, wl_pointer *, uint32_t, wl_surface *)
{
    Window *window = (Window *)data;
    enqueue_input_event(window, { .type = INPUT_EVENT_MOUSE_LEAVE, .key_code = 0, .down = false });
}

static void
on_pointer_motion(void *data, wl_pointer *, uint32_t, wl_fixed_t x, wl_fixed_t y)
{
    Window *window = (Window *)data;
    enqueue_input_event(window, { .type = INPUT_EVENT_MOUSE_MOTION, .key_code = 0, .down = false,
                                 .x = (f32)wl_fixed_to_double(x), .y = (f32)wl_fixed_to_double(y) });
}

static void
on_pointer_button(void *data, wl_pointer *, uint32_t, uint32_t, uint32_t button_code, uint32_t state)
{
    Window *window = (Window *)data;
    for (u32 i = 0; i < MOUSE_COUNT; i++)
    {
        if (button_code == MOUSE_CODES[i])
        {
            enqueue_input_event(window, { .type = INPUT_EVENT_MOUSE_BUTTON, .key_code = i,
                                         .down = state == WL_POINTER_BUTTON_STATE_PRESSED });
            break;
        }
    }
}

static void
on_pointer_axis(void *data, wl_pointer *, uint32_t, uint32_t axis, wl_fixed_t value)
{
    Window *window = (Window *)data;
    InputEvent event = { .type = INPUT_EVENT_MOUSE_SCROLL, .key_code = 0, .down = false };
    f32 amount = (f32)wl_fixed_to_double(value);

    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) event.y = amount;
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) event.x = amount;
    else return;

    enqueue_input_event(window, event);
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
    Window *window = (Window *)data;
    window->keyboard_available = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;

    if (window->keyboard_available && window->keyboard == nullptr)
    {
        window->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(window->keyboard, &KEYBOARD_LISTENER, window);
    }
    else if (!window->keyboard_available && window->keyboard != nullptr)
    {
        wl_keyboard_destroy(window->keyboard);
        window->keyboard = nullptr;
        enqueue_input_event(window, { .type = INPUT_EVENT_FOCUS_LEAVE, .key_code = 0, .down = false });
    }

    bool pointer_available = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;
    if (pointer_available && window->pointer == nullptr)
    {
        window->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(window->pointer, &POINTER_LISTENER, window);
    }
    else if (!pointer_available && window->pointer != nullptr)
    {
        wl_pointer_destroy(window->pointer);
        window->pointer = nullptr;
        enqueue_input_event(window, { .type = INPUT_EVENT_MOUSE_LEAVE, .key_code = 0, .down = false });
    }
}

static const wl_seat_listener SEAT_LISTENER = {
    .capabilities = on_seat_capabilities,
    .name = nullptr,
};

static void
on_global(void *data, wl_registry *registry, uint32_t name,
          const char *interface, uint32_t)
{
    Window *window = (Window *)data;

    if (std::strcmp(interface, "wl_compositor") == 0)
    {
        window->compositor = (wl_compositor *)wl_registry_bind(
            registry, name,
            &wl_compositor_interface, 1);
    }
    else if (std::strcmp(interface, "wl_shm") == 0)
    {
        window->shm = (wl_shm *)wl_registry_bind(
            registry, name,
            &wl_shm_interface, 1);
    }
    else if (std::strcmp(interface, "wl_seat") == 0)
    {
        window->seat = (wl_seat *)wl_registry_bind(
            registry, name,
            &wl_seat_interface, 1);
        wl_seat_add_listener(window->seat, &SEAT_LISTENER, window);
    }
    else if (std::strcmp(interface, "xdg_wm_base") == 0)
    {
        window->shell = (xdg_wm_base *)wl_registry_bind(
            registry, name,
            &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(window->shell, &SHELL_LISTENER, window);
    }
    else if (std::strcmp(interface, "wp_viewporter") == 0)
    {
        window->viewporter = (wp_viewporter *)wl_registry_bind(
            registry, name,
            &wp_viewporter_interface, 1);
    }
    else if (std::strcmp(interface, "zxdg_decoration_manager_v1") == 0)
    {
        window->decoration_manager = (zxdg_decoration_manager_v1 *)wl_registry_bind(
            registry, name,
            &zxdg_decoration_manager_v1_interface, 1);
    }
}

static void
on_global_remove(void *, wl_registry *, uint32_t)
{
}

static const wl_registry_listener REGISTRY_LISTENER = {
    .global = on_global,
    .global_remove = on_global_remove,
};

void
Window::init(u32 buffer_width, u32 buffer_height)
{
    this->width = buffer_width;
    this->height = buffer_height;
    this->viewporter = nullptr;
    this->decoration_manager = nullptr;
    this->seat = nullptr;
    this->keyboard = nullptr;
    this->pointer = nullptr;
    this->close_requested = false;
    this->events.init(this->event_buffer, 256);
    std::memset(this->keys_down, 0, sizeof(this->keys_down));
    this->mouse_window_x = 0;
    this->mouse_window_y = 0;
    this->keyboard_available = false;
    this->display = wl_display_connect(nullptr);
    this->registry = wl_display_get_registry(this->display);
    wl_registry_add_listener(this->registry, &REGISTRY_LISTENER, this);
    wl_display_roundtrip(this->display);
    assert(this->viewporter != nullptr && "Compositor must support wp_viewporter");
    assert(this->decoration_manager != nullptr && "Compositor must support xdg-decoration");

    this->surface = wl_compositor_create_surface(this->compositor);
    this->viewport = wp_viewporter_get_viewport(this->viewporter, this->surface);
    this->shell_surface = xdg_wm_base_get_xdg_surface(this->shell, this->surface);
    this->toplevel = xdg_surface_get_toplevel(this->shell_surface);
    this->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
        this->decoration_manager, this->toplevel);
    zxdg_toplevel_decoration_v1_add_listener(this->decoration, &DECORATION_LISTENER, this);
    zxdg_toplevel_decoration_v1_set_mode(
        this->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    xdg_toplevel_add_listener(this->toplevel, &TOPLEVEL_LISTENER, this);
    xdg_toplevel_set_title(this->toplevel, "Handmade Hero");
    xdg_surface_add_listener(this->shell_surface, &SURFACE_LISTENER, this);

    this->configured = false;
    this->frame_ready = true;
    wl_surface_commit(this->surface);
    while (!this->configured)
    {
        wl_display_dispatch(this->display);
    }

    for (u32 i = 0; i < 2; i++)
    {
        PixelBuffer *buffer = &this->back_buffers[i];
        buffer->width = buffer_width;
        buffer->height = buffer_height;
        buffer->pitch = buffer_width * buffer->bytes_per_pixel;
        u32 size = buffer->pitch * buffer_height;
        int fd = memfd_create("handmade-hero-backbuffer", MFD_CLOEXEC);
        ftruncate(fd, size);
        buffer->buffer = (u32 *)mmap(nullptr, size,
                                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        wl_shm_pool *pool = wl_shm_create_pool(this->shm, fd, (i32)size);
        this->wayland_buffers[i] = wl_shm_pool_create_buffer(
            pool, 0,
            (i32)buffer_width, (i32)buffer_height, (i32)buffer->pitch,
            WL_SHM_FORMAT_XRGB8888);
        wl_buffer_add_listener(this->wayland_buffers[i], &BUFFER_LISTENER,
                               &this->buffer_busy[i]);
        wl_shm_pool_destroy(pool);
        close(fd);
        this->buffer_busy[i] = false;
    }
    this->render_index = 0;
    this->frame_callback = nullptr;
}

void
Window::process_events(Input *input, i32 timeout_ms) {
    update_keyboard_buttons(this, input, false);

    while (true)
    {
        while (!this->events.empty())
        {
            InputEvent event = this->events.pop();
            input->has_input = true;
            timeout_ms = 0;
            switch (event.type)
            {
                case INPUT_EVENT_KEY:
                case INPUT_EVENT_KEY_HELD:
                    this->keys_down[event.key_code] = event.down;
                    update_keyboard_buttons(this, input, event.type == INPUT_EVENT_KEY);
                    break;
                case INPUT_EVENT_FOCUS_ENTER:
                case INPUT_EVENT_FOCUS_LEAVE:
                    std::memset(this->keys_down, 0, sizeof(this->keys_down));
                    update_keyboard_buttons(this, input, false);
                    break;
                case INPUT_EVENT_CLOSE:
                    input->close_request = true;
                    break;
                case INPUT_EVENT_MOUSE_ENTER:
                case INPUT_EVENT_MOUSE_MOTION:
                {
                    f32 scale_x = (f32)this->back_buffers[0].width / (f32)this->width;
                    f32 scale_y = (f32)this->back_buffers[0].height / (f32)this->height;
                    if (event.type == INPUT_EVENT_MOUSE_MOTION && input->mouse_inside)
                    {
                        input->mouse_dx += (event.x - this->mouse_window_x) * scale_x;
                        input->mouse_dy += (event.y - this->mouse_window_y) * scale_y;
                    }
                    this->mouse_window_x = event.x;
                    this->mouse_window_y = event.y;
                    input->mouse_x = event.x * scale_x;
                    input->mouse_y = event.y * scale_y;
                    if (event.type == INPUT_EVENT_MOUSE_ENTER) input->mouse_inside = true;
                    break;
                }
                case INPUT_EVENT_MOUSE_LEAVE:
                    input->mouse_inside = false;
                    for (u32 i = 0; i < MOUSE_COUNT; i++)
                    {
                        InputButton *button = &input->mouse_buttons[i];
                        if (button->is_down)
                        {
                            button->is_down = false;
                            button->released = true;
                        }
                    }
                    break;
                case INPUT_EVENT_MOUSE_SCROLL:
                    input->scroll_x += event.x;
                    input->scroll_y += event.y;
                    break;
                case INPUT_EVENT_MOUSE_BUTTON:
                {
                    InputButton *button = &input->mouse_buttons[event.key_code];
                    if (button->is_down != event.down)
                    {
                        button->is_down = event.down;
                        if (event.down) button->pressed = true;
                        else button->released = true;
                    }
                    break;
                }
            }
        }
        if (this->close_requested) break;

        if (wl_display_dispatch_pending(this->display) == -1)
        {
            this->close_requested = true;
            break;
        }
        if (!this->events.empty()) continue;
        if (this->close_requested) break;
        if (wl_display_prepare_read(this->display) != 0) continue;

        pollfd connection = { .fd = wl_display_get_fd(this->display), .events = POLLIN, .revents = 0 };
        if (wl_display_flush(this->display) == -1)
        {
            if (errno == EAGAIN) connection.events |= POLLOUT;
            else
            {
                wl_display_cancel_read(this->display);
                this->close_requested = true;
                break;
            }
        }

        // Wait at most once, then drain the events available on the connection.
        i32 count = poll(&connection, 1, timeout_ms);
        timeout_ms = 0;
        if (count == -1)
        {
            i32 poll_error = errno;
            wl_display_cancel_read(this->display);
            if (poll_error == EINTR) continue;
            this->close_requested = true;
            break;
        }
        if (count == 0)
        {
            wl_display_cancel_read(this->display);
            break;
        }
        if (connection.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            wl_display_cancel_read(this->display);
            this->close_requested = true;
            break;
        }

        if (connection.revents & POLLIN)
        {
            if (wl_display_read_events(this->display) == -1) this->close_requested = true;
        }
        else
        {
            wl_display_cancel_read(this->display);
        }
    }

    if (this->close_requested) input->close_request = true;
    input->mouse_x = this->mouse_window_x * (f32)this->back_buffers[0].width / (f32)this->width;
    input->mouse_y = this->mouse_window_y * (f32)this->back_buffers[0].height / (f32)this->height;
}

bool
Window::ready_to_render() const
{
    return !this->close_requested && this->frame_ready && !this->buffer_busy[this->render_index];
}

PixelBuffer *
Window::back_buffer()
{
    return &this->back_buffers[this->render_index];
}

void
Window::present()
{
    this->frame_ready = false;
    this->frame_callback = wl_surface_frame(this->surface);
    wl_callback_add_listener(this->frame_callback, &FRAME_LISTENER, this);
    // The entire fixed-size buffer is scaled to the window's logical size.
    wp_viewport_set_destination(this->viewport, (i32)this->width, (i32)this->height);
    wl_surface_attach(this->surface, this->wayland_buffers[this->render_index], 0, 0);
    wl_surface_damage(this->surface, 0, 0,
                      (i32)this->width, (i32)this->height);
    this->buffer_busy[this->render_index] = true;
    wl_surface_commit(this->surface);
    wl_display_flush(this->display);
}

void
Window::swap_buffers()
{
    this->render_index = (this->render_index + 1) % 2;
}

void
Window::free()
{
    if (this->frame_callback != nullptr)
    {
        wl_callback_destroy(this->frame_callback);
    }
    for (u32 i = 0; i < 2; i++)
    {
        PixelBuffer *buffer = &this->back_buffers[i];
        wl_buffer_destroy(this->wayland_buffers[i]);
        munmap(buffer->buffer, (size_t)buffer->pitch * buffer->height);
    }
    zxdg_toplevel_decoration_v1_destroy(this->decoration);
    xdg_toplevel_destroy(this->toplevel);
    xdg_surface_destroy(this->shell_surface);
    wp_viewport_destroy(this->viewport);
    wl_surface_destroy(this->surface);
    wp_viewporter_destroy(this->viewporter);
    zxdg_decoration_manager_v1_destroy(this->decoration_manager);
    xdg_wm_base_destroy(this->shell);
    wl_shm_destroy(this->shm);
    if (this->keyboard != nullptr)
    {
        wl_keyboard_destroy(this->keyboard);
    }
    if (this->pointer != nullptr)
    {
        wl_pointer_destroy(this->pointer);
    }
    if (this->seat != nullptr)
    {
        wl_seat_destroy(this->seat);
    }
    wl_compositor_destroy(this->compositor);
    wl_registry_destroy(this->registry);
    wl_display_disconnect(this->display);
    *this = {};
}
