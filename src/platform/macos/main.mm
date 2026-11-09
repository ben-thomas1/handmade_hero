#include "platform/macos/cocoa.hpp"
#include "platform/macos/coreaudio.hpp"
#include "platform/posix/game_code.hpp"
#include "game/game.hpp"
#include "common/config.hpp"
#include "common/ring.hpp"
#include "common/types.hpp"

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

    std::println("Running {}", conf.name);
    std::println("Render dims: {} x {}", conf.screen_width, conf.screen_height);

    GameCode game;
    if (!game.load()) return 1;

    Window window;
    window.init(conf.screen_width, conf.screen_height);
    
    AudioFrame samples[AUDIO_BUFFER_FRAMES];
    Ring<AudioFrame> sound_buf;
    sound_buf.init(samples, AUDIO_BUFFER_FRAMES);
    State state;
    if (!game.generate_sound(&state, &sound_buf, AUDIO_STARTUP_FRAMES, AUDIO_SAMPLE_RATE)) {
        window.free();
        game.free();
        return 1;
    }
    Audio audio;
    if (!audio.init(read_sound, &sound_buf)) {
        window.free();
        game.free();
        return 1;
    }

    Input input;

    while (state.running) {
        game.reload_if_changed();
        input.begin_frame();
        window.process_events(&input, window.ready_to_render() ? 0 : 8);
        if (input.has_input) std::println("{}", input);

        game.update(&input, &state);

        audio.lock();
        if (sound_buf.len < AUDIO_LEADING_FRAMES) {
            u32 frames = AUDIO_LEADING_FRAMES - sound_buf.len;
            if (!game.generate_sound(&state, &sound_buf, frames, AUDIO_SAMPLE_RATE)) state.running = false;
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
    window.free();
    game.free();
    #endif
    return 0;
}
