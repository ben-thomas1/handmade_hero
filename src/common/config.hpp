#pragma once

#include "common/types.h"


struct Config {
    char name[32] = "Handmade Hero";
    u32 screen_width  = 640;
    u32 screen_height = 480;

    i32 event_timeout_ms = 8;

    u32 audio_sample_rate    = 48'000;
    static constexpr u32 AUDIO_CHANNELS = 2; // Must be known at compile time
    // Capacity of the ring the game fills and the audio callback drains.
    u32 audio_buffer_frames  = 2 * 48'000;
    // Frames kept queued ahead of the audio callback.
    u32 audio_leading_frames = 2048;
    u32 audio_startup_frames = 8192;
    // Buffer size requested from PipeWire; CoreAudio keeps the device default.
    u32 audio_latency_frames = 256;
};
