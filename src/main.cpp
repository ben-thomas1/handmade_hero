#include "window.hpp"
#include "input.hpp"
#include "sound.hpp"
#include "p_audio.hpp"
#include "block_cbuff.hpp"
#include "state.hpp"
#include "types.h"

#include "render.hpp"
#include <print>
#include <stdint.h>

struct Config {
    char name[32] = "Handmade Hero";
    u32 screen_width  = 640;
    u32 screen_height = 480;
};

void
update_game(Input *input, State *state)
{
    if (input->close_request || input->buttons[KEY_Q].pressed) state->running = false;
    state->mouse_x = input->mouse_x;
    state->mouse_y = input->mouse_y;
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

    Window window;
    window.init(conf.screen_width, conf.screen_height);
    
    PCMSample samples[BUFFER_SIZE];
    BlockCBuff<PCMSample> sound_buf;
    sound_buf.init(samples, BUFFER_SIZE);
    // Extra startup audio covers cold debug code and first-frame initialization.
    if (!generate_sound(&sound_buf, AUDIO_STARTUP_FRAMES, AUDIO_SAMPLE_RATE)) {
        window.free();
        return 1;
    }
    Audio audio;
    if (!audio_init(&audio, read_sound, &sound_buf)) {
        window.free();
        return 1;
    }

    Input input;
    State state;

    while (state.running) {
        input.begin_frame();
        window.process_events(&input, window.ready_to_render() ? 0 : 8);
        if (input.has_input) std::println("{}", input);

        update_game(&input, &state);

        audio_lock(&audio);
        if (sound_buf.len < AUDIO_LEADING_FRAMES) {
            u32 frames = AUDIO_LEADING_FRAMES - sound_buf.len;
            if (!generate_sound(&sound_buf, frames, AUDIO_SAMPLE_RATE)) state.running = false;
        }
        audio_unlock(&audio);

        if (window.ready_to_render()) {
            render_to_buffer(window.back_buffer(), &state);
            window.present();
            window.swap_buffers();
            state.frame++;
        }
    }
    #ifndef NDEBUG
    audio_shutdown(&audio);
    window.free();
    #endif
    return 0;
}
