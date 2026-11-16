#pragma once

#include "types.h"


// 0x00RRGGBB pixels; pitch is the number of bytes from one row to the next.
struct Backbuffer {
    u32 *pixels;
    u32 width;
    u32 height;
    u32 pitch;
};

// Interleaved stereo: left, right, left, right, ...
struct SoundBuffer {
    i16 *samples;
    u32 frame_count;
    u32 sample_rate;
};


enum Key {
    HH_KEY_W,
    HH_KEY_A,
    HH_KEY_S,
    HH_KEY_D,
    HH_KEY_UP,
    HH_KEY_DOWN,
    HH_KEY_LEFT,
    HH_KEY_RIGHT,
    HH_KEY_Q,
    HH_KEY_MOUSE_LEFT,
    HH_KEY_MOUSE_RIGHT,
    HH_KEY_MOUSE_MIDDLE,
    HH_KEY_COUNT,
};

struct InputButton {
    bool pressed;
    bool held;
    bool released;
};

struct Input {
    InputButton keys[HH_KEY_COUNT];
    // Backbuffer pixels from the top-left, clamped to the backbuffer.
    i32 mouse_x;
    i32 mouse_y;

    bool quit;
};

struct Actions {
    bool move_up;
    bool move_down;
    bool move_left;
    bool move_right;
    bool toggle_red_render;
    bool toggle_green_render;
    bool toggle_blue_render;
    bool quit;
};

struct State {
    bool quit;
    i32 x_offset;
    i32 y_offset;
    bool hide_red;
    bool hide_green;
    bool hide_blue;
    f32 tone_hz;
    f32 sine_phase;
};

void parse_actions(Actions *actions, Input *input);
void update(State *state, Actions *actions);
void render(Backbuffer *buf, State *state);
void output_sound(SoundBuffer *sound, State *state);
