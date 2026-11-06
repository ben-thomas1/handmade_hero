#include "game/backbuffer.hpp"
#include "game/state.hpp"

#include <cmath>


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
