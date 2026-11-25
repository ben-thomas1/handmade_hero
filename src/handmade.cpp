#include "handmade.hpp"

#include <cmath>
#include <numbers>
#include <cstring>


void
parse_actions(Actions *actions, Input *input)
{
    std::memset(actions, 0, sizeof(Actions));
    if (input->keys[HH_KEY_W].held || input->keys[HH_KEY_UP].held)    actions->move_up = true;
    if (input->keys[HH_KEY_S].held || input->keys[HH_KEY_DOWN].held)  actions->move_down = true;
    if (input->keys[HH_KEY_A].held || input->keys[HH_KEY_LEFT].held)  actions->move_left = true;
    if (input->keys[HH_KEY_D].held || input->keys[HH_KEY_RIGHT].held) actions->move_right = true;
    if (input->keys[HH_KEY_Q].held || input->quit)                    actions->quit = true;
    if (input->keys[HH_KEY_MOUSE_LEFT].pressed)                       actions->toggle_red_render = true;
    if (input->keys[HH_KEY_MOUSE_MIDDLE].pressed)                     actions->toggle_green_render = true;
    if (input->keys[HH_KEY_MOUSE_RIGHT].pressed)                      actions->toggle_blue_render = true;
}


void
update(State *state, Actions *actions)
{
    if (actions->quit) state->quit = true;

    i32 dx = (i32)actions->move_left - (i32)actions->move_right;
    i32 dy = (i32)actions->move_up - (i32)actions->move_down;

    if (actions->toggle_red_render)   state->hide_red   = !state->hide_red;
    if (actions->toggle_green_render) state->hide_green = !state->hide_green;
    if (actions->toggle_blue_render)  state->hide_blue  = !state->hide_blue;

    state->x_offset += 4 * dx;
    state->y_offset += 4 * dy;
    state->tone_hz = 256.0f + 128.0f * (f32)dy;
}

void
render(Backbuffer *buf, State *state)
{
    u8 *row = (u8 *)buf->pixels;
    for (i32 y = 0; y < (i32)buf->height; y++) {
        u32 *pixel = (u32 *)(void *)row;
        for (i32 x = 0; x < (i32)buf->width; x++) {
            u8 red   = !state->hide_red   ? 127 : 0;
            u8 green = !state->hide_green ? (u8)(y + state->y_offset) : 0;
            u8 blue  = !state->hide_blue  ? (u8)(x + state->x_offset) : 0;
            *pixel++ = ((u32)red << 16) | ((u32)green << 8) | blue;
        }
        row += buf->pitch;
    }
}

void
output_sound(SoundBuffer *sound, State *state)
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
