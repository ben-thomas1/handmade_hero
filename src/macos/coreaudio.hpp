#pragma once

#include "handmade.hpp"

#include <os/lock.h>

struct ComponentInstanceRecord;


struct Audio {
    ComponentInstanceRecord *unit;
    // Guards frames_played and frames_written, shared with the audio thread.
    os_unfair_lock lock;
    // Ring of interleaved stereo frames that the audio thread plays from.
    i16 *samples;
    u32 frame_count;
    u32 sample_rate;
    u64 frames_played;
    u64 frames_written;

    void init(u32 sample_rate);
    // Frames to write to keep the ring a fixed latency ahead of playback.
    u32 frames_to_write();
    void write(SoundBuffer *sound);
};
