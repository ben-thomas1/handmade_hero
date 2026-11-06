#pragma once

#include "base/types.hpp"

struct BackBuffer {
    u32 *buffer;
    u32 width;
    u32 height;
    // Bytes from the start of one row to the next; may exceed width * bytes_per_pixel.
    u32 pitch;
    u8 bytes_per_pixel = 4;
};
