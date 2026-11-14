#include "handmade.hpp"

#include <cmath>
#include <numbers>


void
update(State *state, Input *input)
{
    if (input->quit) state->running = false;

    i32 dx = (i32)input->right - (i32)input->left;
    i32 dy = (i32)input->down - (i32)input->up;
    state->x_offset += 4 * dx;
    state->y_offset += 4 * dy;
    state->tone_hz = 256.0f - 128.0f * (f32)dy;
}

void
render(State *state, Backbuffer *buf)
{
    u8 *row = (u8 *)buf->pixels;
    for (i32 y = 0; y < (i32)buf->height; y++) {
        u32 *pixel = (u32 *)(void *)row;
        for (i32 x = 0; x < (i32)buf->width; x++) {
            u8 blue  = (u8)(x + state->x_offset);
            u8 green = (u8)(y + state->y_offset);
            *pixel++ = ((u32)green << 8) | blue;
        }
        row += buf->pitch;
    }
}

void
output_sound(State *state, SoundBuffer *sound)
{
    f32 tau  = 2.0f * std::numbers::pi_v<f32>;
    f32 step = tau * state->tone_hz / (f32)sound->sample_rate;
    i16 *out = sound->samples;
    for (u32 i = 0; i < sound->frame_count; i++) {
        i16 value = (i16)(3000.0f * std::sin(state->sine_phase));
        *out++ = value;
        *out++ = value;
        state->sine_phase += step;
        if (state->sine_phase >= tau) state->sine_phase -= tau;
    }
}
