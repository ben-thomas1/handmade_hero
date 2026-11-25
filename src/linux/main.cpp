#include "handmade.hpp"

#include "linux/wayland.hpp"
#include "linux/pipewire.hpp"


constexpr u32 WINDOW_WIDTH  = 1280;
constexpr u32 WINDOW_HEIGHT =  720;
constexpr u32 AUDIO_RATE = 48'000;
constexpr u32 AUDIO_TONE_HZ = 440;


// Global so mem leak detector doesn't trigger on exit
static Audio audio;
static Window window;

i32
main()
{
    window.init(WINDOW_WIDTH, WINDOW_HEIGHT);
    audio.init(AUDIO_RATE);
    
    SoundBuffer sound = {};
    i16 samples[2 * AUDIO_RATE];
    sound.samples = samples;
    sound.sample_rate = AUDIO_RATE;
    
    State state = {};
    state.tone_hz = AUDIO_TONE_HZ;
    
    Input input = {};
    Actions actions = {};

    while (!state.quit) {
        window.process_events(&input);
        parse_actions(&actions, &input);
        update(&state, &actions);
        render(&window.backbuffer, &state);
        window.present();

        sound.frame_count = audio.frames_to_write();
        output_sound(&sound, &state);
        audio.write(&sound);
    }
    audio.shutdown();
    return 0;
}
