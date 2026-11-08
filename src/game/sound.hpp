#pragma once

#include "base/types.hpp"
#include "base/block_cbuff.hpp"

#define AUDIO_SAMPLE_RATE 48'000
#define AUDIO_CHANNELS 2

struct PCMSample {
    float left;
    float right;
};

struct SoundState {
    f32 phase = 0;
};
