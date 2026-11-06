#pragma once

#include "base/types.hpp"

#include <os/lock.h>


struct ComponentInstanceRecord;

// Copy at most frame_count frames into dst and return the number copied.
typedef u32 (*AudioRead)(void *data, void *dst, u32 frame_count);

struct Audio {
    ComponentInstanceRecord *unit;
    os_unfair_lock lock;
    AudioRead read;
    void *data;
    bool initialized;
    bool started;
    u64 underrun_frames;
};

// Keep Audio and the reader's userdata alive until audio_shutdown() returns.
bool audio_init(Audio *audio, AudioRead read, void *data);
// Hold this lock around main-thread access to the ring shared with the callback.
void audio_lock(Audio *audio);
void audio_unlock(Audio *audio);
void audio_shutdown(Audio *audio);
