#include "backbuffer.hpp"
#include "window.hpp"
#include "input.hpp"

#include "types.h"

#include <cmath>
#include <print>
#include <stdint.h>

struct Config {
    char name[32] = "Handmade Hero";
    u32 screen_width  = 640;
    u32 screen_height = 480;
};

struct State {
    u32 frame = 0;
    f32 mouse_x = 0;
    f32 mouse_y = 0;
    bool running = true;
};

void
update_game(Input *input, State *state) {
    if (input->close_request || input->buttons[KEY_Q].pressed) state->running = false;
    state->mouse_x = input->mouse_x;
    state->mouse_y = input->mouse_y;
}

void
render_to_buffer(BackBuffer *buf, State *state)
{
    f32 time = 0.03f * (f32)state->frame;

    for (u32 j = 0; j < buf->height; j++) {
        for (u32 i = 0; i < buf->width; i++) {
            // Scale both axes by height so circles stay circular.
            f32 x = 2.0f * ((f32)i - state->mouse_x) / (f32)buf->height;
            f32 y = 2.0f * ((f32)j - state->mouse_y) / (f32)buf->height;
            f32 radius = std::sqrt(x * x + y * y);
            f32 angle = std::atan2(y, x);

            // Four spiral arms, rippling rings, and a soft fade at the edges.
            f32 phase = 12.0f * radius - 4.0f * angle + time;
            phase += 2.0f * std::sin(5.0f * radius - time);
            f32 glow = std::exp(-1.0f * radius * radius);

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

    Input input;
    State state;

    while (state.running) {
        input.begin_frame();
        window.process_events(&input, window.ready_to_render() ? 0 : 8);
        if (input.has_input) std::println("{}", input);

        update_game(&input, &state);
        if (!state.running) break;

        if (window.ready_to_render()) {
            render_to_buffer(window.back_buffer(), &state);
            window.present();
            window.swap_buffers();
            state.frame++;
        }
    }
    window.free();
    return 0;
}
