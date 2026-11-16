#include "handmade.hpp"

#include "linux/wayland.hpp"
#include "linux/pipewire.hpp"


// Global so the audio thread can still read it while the process exits.
static Audio audio;

i32
main()
{
    Window window = {};
    window.init(1280, 720);
    audio.init(48000);

    i16 samples[2 * 48000];
    State state = {};
    state.tone_hz = 440;
    Input input = {};
    Actions actions = {};

    while (!state.quit) {
        window.process_events(&input);
        parse_actions(&actions, &input);
        update(&state, &actions);
        render(&window.backbuffer, &state);
        window.present();

        SoundBuffer sound = { .samples = samples, .frame_count = audio.frames_to_write(), .sample_rate = 48000 };
        output_sound(&sound, &state);
        audio.write(&sound);
    }
    return 0;
}
