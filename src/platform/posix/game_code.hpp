#pragma once

#include "game/game.hpp"


struct GameCode {
    void *library;
    GameUpdate *update;
    GameRender *render;
    GameGenerateSound *generate_sound;
    u64 inode;
    i64 mtime;
    u32 load_count;

    // HOT_RELOAD builds load a copy of GAME_LIBRARY_PATH; other builds use the linked-in functions.
    bool load();
    // Keeps the current code if the rebuilt library fails to load.
    void reload_if_changed();
    void free();
};
