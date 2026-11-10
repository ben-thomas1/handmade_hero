#pragma once

#include "common/types.h"


struct pw_thread_loop;
struct pw_stream;

// Copy at most frame_count frames into dst and return the number copied.
typedef u32 (*AudioRead)(void *data, void *dst, u32 frame_count);

struct Audio {
    pw_thread_loop *loop;
    pw_stream *stream;
    AudioRead read;
    void *data;
    u32 sample_rate;
    bool initialized;
    bool started;
    bool ready;
    bool failed;
    bool stopping;
    u64 underrun_frames;

    // Keep Audio and the reader's userdata alive until free() returns.
    // Preconditions: sample_rate > 0.
    bool init(AudioRead reader, void *userdata, u32 sample_rate, u32 latency_frames);
    // Hold this lock around main-thread access to the ring shared with the callback.
    void lock();
    void unlock();
    // Final application teardown: no other threads may still be using D-Bus.
    void free();
};
