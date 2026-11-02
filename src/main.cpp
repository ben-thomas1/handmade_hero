#include "types.h"
#include "backbuffer.hpp"
#include "window.hpp"

#include <cmath>
#include <print>
#include <stdint.h>

struct Config
{
    char name[32] = "Handmade Hero";
    u32 screen_width = 1280;
    u32 screen_height = 720;
};

struct State
{
    u32 frame = 0;
    bool running = true;
};


void
render_to_buffer(BackBuffer *buf, State *state)
{
    f32 time = 0.03f * (f32)state->frame;

    for (u32 j = 0; j < buf->height; j++) {
        for (u32 i = 0; i < buf->width; i++) {
            // Scale both axes by height so circles stay circular.
            f32 x = (2.0f * (f32)i - (f32)buf->width)  / (f32)buf->height;
            f32 y = (2.0f * (f32)j - (f32)buf->height) / (f32)buf->height;
            f32 radius = std::sqrt(x * x + y * y);
            f32 angle = std::atan2(y, x);

            // Four spiral arms, rippling rings, and a soft fade at the edges.
            f32 phase = 12.0f * radius - 4.0f * angle + time;
            phase += 2.0f * std::sin(5.0f * radius - time);
            f32 glow = std::exp(-0.45f * radius * radius);

            u32 r = (u32)(255.0f * glow * (0.5f + 0.5f * std::sin(phase)));
            u32 g = (u32)(255.0f * glow * (0.5f + 0.5f * std::sin(phase + 2.0943951f)));
            u32 b = (u32)(255.0f * glow * (0.5f + 0.5f * std::sin(phase + 4.1887902f)));
            buf->buffer[j * buf->width + i] = (r << 16) | (g << 8) | b;
        }
    }
}

i32
main(i32 argc, char **argv)
{
    (void)argc; (void)argv;

    Config conf;

    std::println("Running {}", conf.name);
    std::println("Window dims: {} x {}", conf.screen_width, conf.screen_height);

    Window window;
    window.init(conf.screen_width, conf.screen_height);

    State state;

    while (window.running) {
        window.wait_until_ready();
        if (!window.running) {
            break;
        }
        render_to_buffer(window.back_buffer(), &state);
        window.present();
        window.swap_buffers();
        state.frame++;
    }
    window.free();
    return 0;
}
