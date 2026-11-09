#pragma once

#include "common/types.hpp"


struct Config {
    char name[32] = "Handmade Hero";
    u32 screen_width  = 640;
    u32 screen_height = 480;
};

inline constexpr u32 AUDIO_SAMPLE_RATE = 48'000;
inline constexpr u32 AUDIO_CHANNELS    = 2;

// Capacity of the ring the game fills and the audio callback drains.
inline constexpr u32 AUDIO_BUFFER_FRAMES  = 2 * AUDIO_SAMPLE_RATE;
// Frames kept queued ahead of the audio callback.
inline constexpr u32 AUDIO_LEADING_FRAMES = 2048;
// Extra startup audio covers cold debug code and first-frame initialization.
inline constexpr u32 AUDIO_STARTUP_FRAMES = 8192;
