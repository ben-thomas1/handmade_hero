#pragma once

#include "types.h"
#include "block_cbuff.hpp"


struct PCMSample {
    float left;
    float right;
};


bool generate_sound(BlockCBuff<PCMSample> *samples, u32 n, u32 sample_rate);
u32 read_sound(void *data, void *dst, u32 frame_count);
