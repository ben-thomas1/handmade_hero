#pragma once

#include "handmade.hpp"

struct pw_thread_loop;
struct pw_stream;


struct Audio {
    // The audio thread runs with the loop's lock held, which guards frames_played and frames_written.
    pw_thread_loop *loop;
    pw_stream *stream;
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
