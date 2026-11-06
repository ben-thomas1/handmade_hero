#pragma once

#include "base/types.hpp"
#include "base/block_cbuff.hpp"


struct PCMSample {
    float left;
    float right;
};


bool generate_sound(BlockCBuff<PCMSample> *samples, u32 n, u32 sample_rate);
