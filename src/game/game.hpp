#pragma once

// The game library's public interface: everything the platform layer may use.

#include "common/config.hpp"
#include "common/ring.hpp"
#include "common/types.hpp"

#include <format>

struct PixelBuffer {
    u32 *buffer;
    u32 width;
    u32 height;
    // Bytes from the start of one row to the next; may exceed width * bytes_per_pixel.
    u32 pitch;
    u8 bytes_per_pixel = 4;
};

// OPTIMISE: Pack to bits
struct InputButton {
    bool is_down = false;
    bool pressed = false;
    bool released = false;
};

enum Key : u8 {
    KEY_W,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_Q,
    KEY_E,
    KEY_F,
    KEY_NUM1,
    KEY_NUM2,
    KEY_NUM3,
    KEY_NUM4,
    KEY_SPACE,
    KEY_ESC,
    KEY_LSHIFT,
    KEY_RSHIFT,
    KEY_LCTRL,
    KEY_RCTRL,
    KEY_LALT,
    KEY_RALT,
    KEY_COUNT,
};

enum MouseButton : u8 {
    MOUSE_LEFT,
    MOUSE_MIDDLE,
    MOUSE_RIGHT,
    MOUSE_COUNT,
};

struct Input {
    InputButton buttons[KEY_COUNT];
    InputButton mouse_buttons[MOUSE_COUNT];

    // Positions and deltas use pixel buffer coordinates.
    f32 mouse_x = 0;
    f32 mouse_y = 0;
    f32 mouse_dx = 0;
    f32 mouse_dy = 0;
    bool mouse_inside = false;
    // Wayland scroll units; positive values mean right and down.
    f32 scroll_x = 0;
    f32 scroll_y = 0;

    bool close_request = false;
    bool has_input = false;

    void
    begin_frame()
    {
        this->has_input = false;
        for (i32 i = 0; i < KEY_COUNT; i++) {
            this->buttons[i].pressed  = false;
            this->buttons[i].released = false;
        }
        for (i32 i = 0; i < MOUSE_COUNT; i++) {
            this->mouse_buttons[i].pressed  = false;
            this->mouse_buttons[i].released = false;
        }
        this->mouse_dx = 0;
        this->mouse_dy = 0;
        this->scroll_x = 0;
        this->scroll_y = 0;
    }
};

// One sample per channel.
struct AudioFrame {
    f32 left;
    f32 right;
};

struct SoundState {
    f32 phase = 0;
};

struct State {
    u32 frame = 0;
    f32 mouse_x = 0;
    f32 mouse_y = 0;
    bool running = true;
    SoundState sound;
};

// The game library hides every other symbol; these are the only ones the platform can load.
#define GAME_EXPORT __attribute__((visibility("default")))

extern "C" {
GAME_EXPORT void game_update(Input *input, State *state);
GAME_EXPORT void game_render(PixelBuffer *buf, State *state);
GAME_EXPORT bool game_generate_sound(State *state, Ring<AudioFrame> *dst, u32 n, u32 sample_rate);
}

typedef void GameUpdate(Input *input, State *state);
typedef void GameRender(PixelBuffer *buf, State *state);
typedef bool GameGenerateSound(State *state, Ring<AudioFrame> *dst, u32 n, u32 sample_rate);

template <>
struct std::formatter<Input> {
    constexpr std::format_parse_context::iterator
    parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    std::format_context::iterator
    format(const Input &input, std::format_context &ctx) const
    {
        const char *key_names[KEY_COUNT] = {
            "W", "A", "S", "D", "Q", "E", "F",
            "1", "2", "3", "4", "space", "esc", "lshift", "rshift", "lctrl", "rctrl", "lalt", "ralt",
        };
        const char *mouse_names[MOUSE_COUNT] = { "left", "middle", "right" };

        std::format_context::iterator out = std::format_to(ctx.out(), "Input{{ keys=[");
        const char *separator = "";
        for (u32 i = 0; i < KEY_COUNT; i++) {
            const InputButton *button = &input.buttons[i];
            if (!button->is_down && !button->pressed && !button->released) continue;
            out = std::format_to(out, "{}{}(down={}, pressed={}, released={})",
                                 separator, key_names[i], button->is_down, button->pressed, button->released);
            separator = ", ";
        }

        out = std::format_to(out, "], mouse_buttons=[");
        separator = "";
        for (u32 i = 0; i < MOUSE_COUNT; i++) {
            const InputButton *button = &input.mouse_buttons[i];
            if (!button->is_down && !button->pressed && !button->released) continue;
            out = std::format_to(out, "{}{}(down={}, pressed={}, released={})",
                                 separator, mouse_names[i], button->is_down, button->pressed, button->released);
            separator = ", ";
        }

        return std::format_to(out, "], mouse=({}, {}), delta=({}, {}), inside={}, scroll=({}, {}), close={}, has_input={} }}",
                              input.mouse_x, input.mouse_y, input.mouse_dx, input.mouse_dy,
                              input.mouse_inside, input.scroll_x, input.scroll_y, input.close_request, input.has_input);
    }
};
