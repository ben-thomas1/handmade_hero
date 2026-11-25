#include "handmade.hpp"

#include "macos/cocoa.hpp"
#include "macos/coreaudio.hpp"


// Global so it stays reachable at exit: the OS frees it, and LeakSanitizer doesn't report it.
static Window window;

i32
main()
{
    window.init(1280, 720);
    Audio audio = {};
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
    audio.shutdown();
    return 0;
}
