#include "game/game.hpp"

#include <cmath>
#include <numbers>


bool game_generate_sound(State *state, Ring<AudioFrame> *dst, u32 n, u32 sample_rate) {
    if (sample_rate == 0 || n > dst->remaining()) return false;
    AudioFrame samples[256];
    f32 tau = 2.0f * std::numbers::pi_v<f32>;
    f32 step = tau * 440.0f / (f32)sample_rate;
    f32 phase = state->sound.phase;
    while (n > 0) {
        u32 count = n < 256 ? n : 256;
        for (u32 i = 0; i < count; i++) {
            f32 value = 0.05f * std::sin(phase);
            samples[i] = { value, value };
            phase += step;
            if (phase >= tau) phase -= tau;
        }
        dst->push_block(samples, count);
        n -= count;
    }
    state->sound.phase = phase;
    return true;
}
