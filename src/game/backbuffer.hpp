#pragma once

#include "base/types.hpp"

struct BackBuffer {
    u32 *buffer;
    u32 width;
    u32 height;
    u8 bytes_per_pixel = 4;
};
