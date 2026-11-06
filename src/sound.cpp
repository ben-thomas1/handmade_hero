#include "sound.hpp"

#include <cmath>
#include <numbers>


static float phase = 0;

bool generate_sound(BlockCBuff<PCMSample> *dst, u32 n, u32 sample_rate) {
    if (sample_rate == 0 || n > dst->remaining()) return false;
    PCMSample samples[256];
    f32 tau = 2.0f * std::numbers::pi_v<f32>;
    f32 step = tau * 440.0f / (f32)sample_rate;
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
    return true;
}

u32
read_sound(void *data, void *dst, u32 frame_count)
{
    BlockCBuff<PCMSample> *samples = (BlockCBuff<PCMSample> *)data;
    u32 count = frame_count < samples->len ? frame_count : samples->len;
    samples->pop_block((PCMSample *)dst, count);
    return count;
}
