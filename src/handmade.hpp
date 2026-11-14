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

// Whether each key is held this frame.
struct Input {
    bool up;
    bool down;
    bool left;
    bool right;
    bool quit;
};

struct State {
    bool running;
    i32 x_offset;
    i32 y_offset;
    f32 tone_hz;
    f32 sine_phase;
};

void update(State *state, Input *input);
void render(State *state, Backbuffer *buf);
void output_sound(State *state, SoundBuffer *sound);
