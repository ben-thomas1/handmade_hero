#include "platform/linux/pipewire.hpp"
#include "common/config.hpp"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <dbus/dbus.h>

#include <cstring>
#include <print>


static void
on_process(void *data)
{
    Audio *audio = (Audio *)data;
    pw_buffer *buffer = pw_stream_dequeue_buffer(audio->stream);
    if (buffer == nullptr) return;

    if (buffer->buffer->n_datas == 0) {
        pw_stream_queue_buffer(audio->stream, buffer);
        return;
    }
    spa_data *samples = &buffer->buffer->datas[0];
    if (samples->data == nullptr || samples->chunk == nullptr) {
        pw_stream_queue_buffer(audio->stream, buffer);
        return;
    }

    u32 stride = AUDIO_CHANNELS * sizeof(f32);
    u32 frame_count = samples->maxsize / stride;
    if (buffer->requested > 0 && buffer->requested < frame_count) frame_count = (u32)buffer->requested;
    u32 copied = audio->read(audio->data, samples->data, frame_count);
    if (copied > frame_count) copied = frame_count;
    audio->underrun_frames += frame_count - copied;
    std::memset((u8 *)samples->data + copied * stride, 0, (frame_count - copied) * stride);

    samples->chunk->offset = 0;
    samples->chunk->stride = (i32)stride;
    samples->chunk->size = frame_count * stride;
    samples->chunk->flags = 0;
    buffer->size = frame_count;
    pw_stream_queue_buffer(audio->stream, buffer);
}

static void
on_state_changed(void *data, pw_stream_state old, pw_stream_state state, const char *error)
{
    Audio *audio = (Audio *)data;
    audio->ready = state == PW_STREAM_STATE_PAUSED || state == PW_STREAM_STATE_STREAMING;
    if (!audio->stopping && (state == PW_STREAM_STATE_ERROR ||
                            (state == PW_STREAM_STATE_UNCONNECTED && old != PW_STREAM_STATE_UNCONNECTED))) {
        std::println(stderr, "PipeWire stream failed: {}", error != nullptr ? error : "disconnected");
        audio->failed = true;
    }
    pw_thread_loop_signal(audio->loop, false);
}

static void
on_param_changed(void *data, uint32_t id, const spa_pod *param)
{
    if (id != SPA_PARAM_Format || param == nullptr) return;
    Audio *audio = (Audio *)data;
    spa_audio_info_raw format = {};
    if (spa_format_audio_raw_parse(param, &format) < 0 || format.format != SPA_AUDIO_FORMAT_F32 ||
        format.rate != AUDIO_SAMPLE_RATE || format.channels != AUDIO_CHANNELS) {
        std::println(stderr, "PipeWire negotiated an unsupported audio format");
        audio->failed = true;
        pw_thread_loop_signal(audio->loop, false);
    }
}

static pw_stream_events
make_stream_events()
{
    pw_stream_events events = {};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.state_changed = on_state_changed;
    events.param_changed = on_param_changed;
    events.process = on_process;
    return events;
}

static const pw_stream_events STREAM_EVENTS = make_stream_events();

bool
Audio::init(AudioRead reader, void *userdata)
{
    *this = {};
    if (reader == nullptr) return false;
    this->read = reader;
    this->data = userdata;
    pw_init(nullptr, nullptr);
    this->initialized = true;
    this->loop = pw_thread_loop_new("handmade-hero-audio", nullptr);
    if (this->loop == nullptr) {
        std::println(stderr, "Could not create the PipeWire thread loop");
        this->free();
        return false;
    }
    i32 result = pw_thread_loop_start(this->loop);
    if (result < 0) {
        std::println(stderr, "Could not start PipeWire: {}", std::strerror(-result));
        this->free();
        return false;
    }
    this->started = true;
    this->lock();
    this->stream = pw_stream_new_simple(pw_thread_loop_get_loop(this->loop), "Handmade Hero",
                                        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                                          PW_KEY_MEDIA_CATEGORY, "Playback",
                                                          PW_KEY_MEDIA_ROLE, "Game",
                                                          PW_KEY_NODE_NAME, "handmade_hero.audio",
                                                          PW_KEY_NODE_LATENCY, "256/48000", nullptr),
                                        &STREAM_EVENTS, this);
    if (this->stream == nullptr) {
        std::println(stderr, "Could not create the PipeWire stream");
        this->unlock();
        this->free();
        return false;
    }

    alignas(8) u8 pod_buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(pod_buffer, sizeof(pod_buffer));
    spa_audio_info_raw format = {};
    format.format = SPA_AUDIO_FORMAT_F32;
    format.rate = AUDIO_SAMPLE_RATE;
    format.channels = AUDIO_CHANNELS;
    format.position[0] = SPA_AUDIO_CHANNEL_FL;
    format.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod *params[] = { spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &format) };

    // Without RT_PROCESS, callbacks run with the thread-loop lock held.
    pw_stream_flags flags = (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
    result = pw_stream_connect(this->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, 1);
    if (result < 0) {
        std::println(stderr, "Could not connect PipeWire: {}", std::strerror(-result));
        this->unlock();
        this->free();
        return false;
    }

    timespec deadline;
    pw_thread_loop_get_time(this->loop, &deadline, 5 * SPA_NSEC_PER_SEC);
    while (!this->ready && !this->failed) {
        result = pw_thread_loop_timed_wait_full(this->loop, &deadline);
        if (result < 0) {
            std::println(stderr, "Timed out waiting for the PipeWire stream");
            this->failed = true;
        }
    }
    bool ready = this->ready && !this->failed;
    this->unlock();
    if (!ready) this->free();
    return ready;
}

void
Audio::lock()
{
    pw_thread_loop_lock(this->loop);
}

void
Audio::unlock()
{
    pw_thread_loop_unlock(this->loop);
}

void
Audio::free()
{
    if (this->loop != nullptr) {
        this->lock();
        this->stopping = true;
        this->unlock();
        if (this->started) pw_thread_loop_stop(this->loop);
        this->lock();
        if (this->stream != nullptr) pw_stream_destroy(this->stream);
        this->unlock();
        pw_thread_loop_destroy(this->loop);
    }
    if (this->initialized) {
        pw_deinit();
        // PipeWire's RT module uses libdbus; release its process-global caches last.
        dbus_shutdown();
    }
    #ifndef NDEBUG
    if (this->underrun_frames > 0) {
        std::println(stderr, "Audio ring underrun: {} frames filled with silence", this->underrun_frames);
    }
    #endif
    *this = {};
}
