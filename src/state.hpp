#pragma once

#include "types.h"


struct State {
    u32 frame = 0;
    f32 mouse_x = 0;
    f32 mouse_y = 0;
    bool running = true;
};
