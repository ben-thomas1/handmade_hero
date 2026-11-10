#pragma once

#include "common/types.h"

#include <os/lock.h>


struct ComponentInstanceRecord;

// Copy at most frame_count frames into dst and return the number copied.
typedef u32 (*AudioRead)(void *data, void *dst, u32 frame_count);

struct Audio {
    ComponentInstanceRecord *unit;
    os_unfair_lock ring_lock;
    AudioRead read;
    void *data;
    bool initialized;
    bool started;
    u64 underrun_frames;

    // Keep Audio and the reader's userdata alive until free() returns.
    // Preconditions: sample_rate > 0.
    bool init(AudioRead reader, void *userdata, u32 sample_rate);
    // Hold this lock around main-thread access to the ring shared with the callback.
    void lock();
    void unlock();
    void free();
};
