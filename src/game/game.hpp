#pragma once

#include "game/backbuffer.hpp"
#include "game/input.hpp"
#include "game/sound.hpp"
#include "game/state.hpp"
#include "base/block_cbuff.hpp"

// The game library hides every other symbol; these are the only ones the platform can load.
#define GAME_EXPORT __attribute__((visibility("default")))

extern "C" {
GAME_EXPORT void game_update(Input *input, State *state);
GAME_EXPORT void game_render(BackBuffer *buf, State *state);
GAME_EXPORT bool game_generate_sound(State *state, BlockCBuff<PCMSample> *dst, u32 n, u32 sample_rate);
}

typedef void GameUpdate(Input *input, State *state);
typedef void GameRender(BackBuffer *buf, State *state);
typedef bool GameGenerateSound(State *state, BlockCBuff<PCMSample> *dst, u32 n, u32 sample_rate);
