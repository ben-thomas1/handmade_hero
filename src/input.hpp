#pragma once

#include "types.h"

#include <format>

// OPTIMISE: Pack to bits
struct InputButton {
    bool is_down = false;
    bool pressed = false;
    bool released = false;
};

enum KeyboardButtons : u8 {
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
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
    KEY_COUNT,
};

// Evdev codes in the same order as Input::buttons. Zero means no second key.
inline constexpr u32 KEY_CODES[KEY_COUNT][2] = {
    {17, 0},   // W
    {30, 0},   // A
    {31, 0},   // S
    {32, 0},   // D
    {16, 0},   // Q
    {18, 0},   // E
    {33, 0},   // F
    {2, 0},    // 1
    {3, 0},    // 2
    {4, 0},    // 3
    {5, 0},    // 4
    {57, 0},   // Space
    {1, 0},    // Escape
    {42, 54},  // Left/right Shift
    {29, 97},  // Left/right Ctrl
    {56, 100}, // Left/right Alt
};

enum MouseButtons : u8 {
    MOUSE_LEFT,
    MOUSE_MIDDLE,
    MOUSE_RIGHT,
    MOUSE_COUNT,
};

inline constexpr u32 MOUSE_CODES[MOUSE_COUNT] = {
    272, // Left
    274, // Middle
    273, // Right
};

enum InputEventType : u8 {
    INPUT_EVENT_KEY,
    INPUT_EVENT_KEY_HELD,
    INPUT_EVENT_FOCUS_ENTER,
    INPUT_EVENT_FOCUS_LEAVE,
    INPUT_EVENT_CLOSE,
    INPUT_EVENT_MOUSE_ENTER,
    INPUT_EVENT_MOUSE_LEAVE,
    INPUT_EVENT_MOUSE_MOTION,
    INPUT_EVENT_MOUSE_BUTTON,
    INPUT_EVENT_MOUSE_SCROLL,
};

struct InputEvent {
    InputEventType type;
    u32 key_code;
    bool down;
    f32 x = 0;
    f32 y = 0;
};

struct Input {
    InputButton buttons[KEY_COUNT];
    InputButton mouse_buttons[MOUSE_COUNT];

    // Positions and deltas use backbuffer coordinates.
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
            "1", "2", "3", "4", "space", "esc", "shift", "ctrl", "alt",
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
