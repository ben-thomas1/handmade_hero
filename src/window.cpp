#include "window.hpp"
#include "xdg-shell-client-protocol.h"

#include <wayland-client.h>

#include <cstring>
#include <print>
#include <sys/mman.h>
#include <unistd.h>

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
on_toplevel_configure(void *, xdg_toplevel *, int32_t, int32_t, wl_array *)
{
}

static void
on_close(void *data, xdg_toplevel *)
{
    Window *window = (Window *)data;
    window->running = false;
}

static const xdg_toplevel_listener TOPLEVEL_LISTENER = {
    .configure = on_toplevel_configure,
    .close = on_close,
    .configure_bounds = nullptr,
    .wm_capabilities = nullptr,
};

static void
on_global(void *data, wl_registry *registry, uint32_t name,
          const char *interface, uint32_t version)
{
    std::println("Wayland: {} (version {}, id {})", interface, version, name);
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
    else if (std::strcmp(interface, "xdg_wm_base") == 0)
    {
        window->shell = (xdg_wm_base *)wl_registry_bind(
            registry, name,
            &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(window->shell, &SHELL_LISTENER, window);
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
Window::init(u32 width, u32 height)
{
    this->display = wl_display_connect(nullptr);
    this->registry = wl_display_get_registry(this->display);
    wl_registry_add_listener(this->registry, &REGISTRY_LISTENER, this);
    wl_display_roundtrip(this->display);

    this->surface = wl_compositor_create_surface(this->compositor);
    this->shell_surface = xdg_wm_base_get_xdg_surface(this->shell, this->surface);
    this->toplevel = xdg_surface_get_toplevel(this->shell_surface);
    xdg_toplevel_add_listener(this->toplevel, &TOPLEVEL_LISTENER, this);
    xdg_toplevel_set_title(this->toplevel, "Handmade Hero");
    xdg_surface_add_listener(this->shell_surface, &SURFACE_LISTENER, this);

    this->configured = false;
    this->running = true;
    this->frame_ready = true;
    wl_surface_commit(this->surface);
    while (!this->configured)
    {
        wl_display_dispatch(this->display);
    }

    for (u32 i = 0; i < 2; i++)
    {
        BackBuffer *buffer = &this->back_buffers[i];
        buffer->width = width;
        buffer->height = height;
        u32 stride = width * buffer->bytes_per_pixel;
        u32 size = stride * height;
        int fd = memfd_create("handmade-hero-backbuffer", MFD_CLOEXEC);
        ftruncate(fd, size);
        buffer->buffer = (u32 *)mmap(nullptr, size,
                                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        wl_shm_pool *pool = wl_shm_create_pool(this->shm, fd, (i32)size);
        this->wayland_buffers[i] = wl_shm_pool_create_buffer(
            pool, 0,
            (i32)width, (i32)height, (i32)stride,
            WL_SHM_FORMAT_XRGB8888);
        wl_buffer_add_listener(this->wayland_buffers[i], &BUFFER_LISTENER,
                               &this->buffer_busy[i]);
        wl_shm_pool_destroy(pool);
        close(fd);
    }
}

BackBuffer *
Window::back_buffer()
{
    return &this->back_buffers[this->render_index];
}

void
Window::wait_until_ready()
{
    while (this->running &&
           (!this->frame_ready || this->buffer_busy[this->render_index]))
    {
        wl_display_dispatch(this->display);
    }
}

void
Window::present()
{
    BackBuffer *buffer = &this->back_buffers[this->render_index];
    this->frame_ready = false;
    this->frame_callback = wl_surface_frame(this->surface);
    wl_callback_add_listener(this->frame_callback, &FRAME_LISTENER, this);
    wl_surface_attach(this->surface, this->wayland_buffers[this->render_index], 0, 0);
    wl_surface_damage(this->surface, 0, 0,
                      (i32)buffer->width, (i32)buffer->height);
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
        BackBuffer *buffer = &this->back_buffers[i];
        wl_buffer_destroy(this->wayland_buffers[i]);
        munmap(buffer->buffer, (size_t)buffer->width * buffer->height * buffer->bytes_per_pixel);
    }
    xdg_toplevel_destroy(this->toplevel);
    xdg_surface_destroy(this->shell_surface);
    wl_surface_destroy(this->surface);
    xdg_wm_base_destroy(this->shell);
    wl_shm_destroy(this->shm);
    wl_compositor_destroy(this->compositor);
    wl_registry_destroy(this->registry);
    wl_display_disconnect(this->display);
    *this = {};
}
