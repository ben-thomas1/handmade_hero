#include "common/config.hpp"
#include "common/ring.hpp"
#include "common/types.h"
#include "game/game.hpp"
#include "platform/linux/wayland.hpp"
#include "platform/linux/pipewire.hpp"
#include "platform/posix/game_code.hpp"

#include <cassert>
#include <cstdlib>
#include <print>

static u32
read_sound(void *data, void *dst, u32 frame_count)
{
    Ring<AudioFrame> *samples = (Ring<AudioFrame> *)data;
    u32 count = frame_count < samples->len ? frame_count : samples->len;
    samples->pop_block((AudioFrame *)dst, count);
    return count;
}


i32
main(i32 argc, char **argv)
{
    (void)argc; (void)argv;

    Config conf;
    assert(conf.audio_sample_rate > 0);
    assert(conf.audio_startup_frames <= conf.audio_buffer_frames);
    assert(conf.audio_leading_frames <= conf.audio_buffer_frames);

    std::println("Running {}", conf.name);
    std::println("Render dims: {} x {}", conf.screen_width, conf.screen_height);

    GameCode game;
    if (!game.load()) return 1;

    Window window;
    window.init(conf.screen_width, conf.screen_height);
    
    AudioFrame *samples = (AudioFrame *)std::malloc((size_t)conf.audio_buffer_frames * sizeof(AudioFrame));
    if (samples == nullptr) {
        window.free();
        game.free();
        return 1;
    }
    Ring<AudioFrame> sound_buf;
    sound_buf.init(samples, conf.audio_buffer_frames);
    State state;
    if (!game.generate_sound(&state, &sound_buf, conf.audio_startup_frames, conf.audio_sample_rate)) {
        std::free(samples);
        window.free();
        game.free();
        return 1;
    }
    Audio audio;
    if (!audio.init(read_sound, &sound_buf, conf.audio_sample_rate, conf.audio_latency_frames)) {
        std::free(samples);
        window.free();
        game.free();
        return 1;
    }

    Input input;

    while (state.running) {
        game.reload_if_changed();
        input.begin_frame();
        window.process_events(&input, window.ready_to_render() ? 0 : conf.event_timeout_ms);
        if (input.has_input) std::println("{}", input);

        game.update(&input, &state);

        audio.lock();
        if (sound_buf.len < conf.audio_leading_frames) {
            u32 frames = conf.audio_leading_frames - sound_buf.len;
            if (!game.generate_sound(&state, &sound_buf, frames, conf.audio_sample_rate)) state.running = false;
        }
        audio.unlock();

        if (window.ready_to_render()) {
            game.render(window.back_buffer(), &state);
            window.present();
            window.swap_buffers();
            state.frame++;
        }
    }
    // The audio thread reads sound_buf on this stack, so stop it in every build.
    audio.free();
    #ifndef NDEBUG
    std::free(samples);
    window.free();
    game.free();
    #endif
    return 0;
}
