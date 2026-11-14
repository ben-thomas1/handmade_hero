#include "linux/pipewire.hpp"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <cstdlib>
#include <cstring>


static void
on_process(void *data)
{
    Audio *audio = (Audio *)data;
    pw_buffer *buffer = pw_stream_dequeue_buffer(audio->stream);
    spa_data *out_data = &buffer->buffer->datas[0];
    i16 *out = (i16 *)out_data->data;

    u32 stride = 2 * sizeof(i16);
    u32 frame_count = out_data->maxsize / stride;
    if (buffer->requested > 0 && buffer->requested < frame_count) frame_count = (u32)buffer->requested;

    u32 available = (u32)(audio->frames_written - audio->frames_played);
    u32 n = frame_count < available ? frame_count : available;
    u32 start = (u32)(audio->frames_played % audio->frame_count);
    u32 first = audio->frame_count - start < n ? audio->frame_count - start : n;
    std::memcpy(out, audio->samples + 2 * start, first * stride);
    std::memcpy(out + 2 * first, audio->samples, (n - first) * stride);
    audio->frames_played += n;
    // Silence if the game fell behind.
    std::memset(out + 2 * n, 0, (frame_count - n) * stride);

    out_data->chunk->offset = 0;
    out_data->chunk->stride = (i32)stride;
    out_data->chunk->size = frame_count * stride;
    pw_stream_queue_buffer(audio->stream, buffer);
}

void
Audio::init(u32 sample_rate)
{
    this->sample_rate = sample_rate;
    this->frame_count = sample_rate;
    this->samples = (i16 *)std::malloc((size_t)this->frame_count * 2 * sizeof(i16));

    static pw_stream_events events = {};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.process = on_process;

    pw_init(nullptr, nullptr);
    this->loop = pw_thread_loop_new("handmade-hero-audio", nullptr);
    pw_thread_loop_start(this->loop);
    pw_thread_loop_lock(this->loop);
    this->stream = pw_stream_new_simple(pw_thread_loop_get_loop(this->loop), "Handmade Hero",
                                        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                                          PW_KEY_MEDIA_CATEGORY, "Playback",
                                                          PW_KEY_MEDIA_ROLE, "Game", nullptr),
                                        &events, this);

    alignas(8) u8 pod_buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));
    spa_audio_info_raw format = {};
    format.format = SPA_AUDIO_FORMAT_S16;
    format.rate = sample_rate;
    format.channels = 2;
    format.position[0] = SPA_AUDIO_CHANNEL_FL;
    format.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod *params[] = { spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &format) };
    pw_stream_connect(this->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                      (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params, 1);
    pw_thread_loop_unlock(this->loop);
}

u32
Audio::frames_to_write()
{
    u32 latency = this->sample_rate / 15;
    pw_thread_loop_lock(this->loop);
    u32 queued = (u32)(this->frames_written - this->frames_played);
    pw_thread_loop_unlock(this->loop);
    return queued < latency ? latency - queued : 0;
}

void
Audio::write(SoundBuffer *sound)
{
    // The audio thread only reads frames before frames_written, so this region is ours.
    u32 n = sound->frame_count;
    u32 start = (u32)(this->frames_written % this->frame_count);
    u32 first = this->frame_count - start < n ? this->frame_count - start : n;
    std::memcpy(this->samples + 2 * start, sound->samples, first * 2 * sizeof(i16));
    std::memcpy(this->samples, sound->samples + 2 * first, (n - first) * 2 * sizeof(i16));

    pw_thread_loop_lock(this->loop);
    this->frames_written += n;
    pw_thread_loop_unlock(this->loop);
}
