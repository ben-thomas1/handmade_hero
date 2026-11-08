#include "platform/linux/wayland.hpp"
#include "game/input.hpp"
#include "game/sound.hpp"
#include "platform/linux/pipewire.hpp"
#include "base/block_cbuff.hpp"
#include "game/state.hpp"
#include "game/game.hpp"
#include "platform/posix/game_code.hpp"
#include "base/types.hpp"

#include <print>
#include <stdint.h>

struct Config {
    char name[32] = "Handmade Hero";
    u32 screen_width  = 640;
    u32 screen_height = 480;
};

static u32
read_sound(void *data, void *dst, u32 frame_count)
{
    BlockCBuff<PCMSample> *samples = (BlockCBuff<PCMSample> *)data;
    u32 count = frame_count < samples->len ? frame_count : samples->len;
    samples->pop_block((PCMSample *)dst, count);
    return count;
}


#define BUFFER_SIZE (2 * AUDIO_SAMPLE_RATE)
#define AUDIO_LEADING_FRAMES 2048
#define AUDIO_STARTUP_FRAMES 8192


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
    
    PCMSample samples[BUFFER_SIZE];
    BlockCBuff<PCMSample> sound_buf;
    sound_buf.init(samples, BUFFER_SIZE);
    State state;
    // Extra startup audio covers cold debug code and first-frame initialization.
    if (!game.generate_sound(&state, &sound_buf, AUDIO_STARTUP_FRAMES, AUDIO_SAMPLE_RATE)) {
        window.free();
        game.free();
        return 1;
    }
    Audio audio;
    if (!audio_init(&audio, read_sound, &sound_buf)) {
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

        audio_lock(&audio);
        if (sound_buf.len < AUDIO_LEADING_FRAMES) {
            u32 frames = AUDIO_LEADING_FRAMES - sound_buf.len;
            if (!game.generate_sound(&state, &sound_buf, frames, AUDIO_SAMPLE_RATE)) state.running = false;
        }
        audio_unlock(&audio);

        if (window.ready_to_render()) {
            game.render(window.back_buffer(), &state);
            window.present();
            window.swap_buffers();
            state.frame++;
        }
    }
    // The audio thread reads sound_buf on this stack, so stop it in every build.
    audio_shutdown(&audio);
    #ifndef NDEBUG
    window.free();
    game.free();
    #endif
    return 0;
}
