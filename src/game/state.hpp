#pragma once

#include "base/types.hpp"
#include "game/sound.hpp"


struct State {
    u32 frame = 0;
    f32 mouse_x = 0;
    f32 mouse_y = 0;
    bool running = true;
    SoundState sound;
};
