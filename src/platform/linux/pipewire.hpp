#pragma once

#include "base/types.hpp"

#define AUDIO_SAMPLE_RATE 48'000
#define AUDIO_CHANNELS 2


struct pw_thread_loop;
struct pw_stream;

// Copy at most frame_count frames into dst and return the number copied.
typedef u32 (*AudioRead)(void *data, void *dst, u32 frame_count);

struct Audio {
    pw_thread_loop *loop;
    pw_stream *stream;
    AudioRead read;
    void *data;
    bool initialized;
    bool started;
    bool ready;
    bool failed;
    bool stopping;
    u64 underrun_frames;
};

// Keep Audio and the reader's userdata alive until audio_shutdown() returns.
bool audio_init(Audio *audio, AudioRead read, void *data);
// Hold this lock around main-thread access to the ring shared with the callback.
void audio_lock(Audio *audio);
void audio_unlock(Audio *audio);
// Final application teardown: no other threads may still be using D-Bus.
void audio_shutdown(Audio *audio);
