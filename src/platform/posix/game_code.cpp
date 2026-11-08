#include "platform/posix/game_code.hpp"

#include <cstdio>
#include <print>

#if HOT_RELOAD

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static void
copy_path(char *out, u32 out_size, u32 index)
{
    std::snprintf(out, out_size, "%s.%u", GAME_LIBRARY_PATH, index);
}

static bool
copy_file(const char *src, const char *dst)
{
    int in = open(src, O_RDONLY | O_CLOEXEC);
    if (in == -1) return false;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0755);
    if (out == -1) {
        close(in);
        return false;
    }
    u8 buffer[64 * 1024];
    bool ok = true;
    while (true) {
        ssize_t n = read(in, buffer, sizeof(buffer));
        if (n == 0) break;
        if (n < 0 || write(out, buffer, (size_t)n) != n) {
            ok = false;
            break;
        }
    }
    close(in);
    close(out);
    return ok;
}

// Each load opens a fresh copy: dlopen would hand back a cached image for a path it
// already has open, and macOS rejects a signed binary that changed under its path.
static bool
open_copy(GameCode *code, u32 index)
{
    char path[1024];
    copy_path(path, sizeof(path), index);
    // A killed run leaves its copies behind; a new file gets a new inode.
    unlink(path);
    if (!copy_file(GAME_LIBRARY_PATH, path)) {
        std::println(stderr, "Could not copy {} to {}", GAME_LIBRARY_PATH, path);
        return false;
    }
    void *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        std::println(stderr, "Could not load the game code: {}", dlerror());
        unlink(path);
        return false;
    }
    GameUpdate *update = (GameUpdate *)dlsym(library, "game_update");
    GameRender *render = (GameRender *)dlsym(library, "game_render");
    GameGenerateSound *generate_sound = (GameGenerateSound *)dlsym(library, "game_generate_sound");
    if (update == nullptr || render == nullptr || generate_sound == nullptr) {
        std::println(stderr, "Game code is missing an entry point: {}", dlerror());
        dlclose(library);
        unlink(path);
        return false;
    }
    code->library = library;
    code->update = update;
    code->render = render;
    code->generate_sound = generate_sound;
    code->load_count = index;
    return true;
}

// Copies stay loaded: AddressSanitizer crashes while reporting any error made after a dlclose.
static void
delete_copy(GameCode *code)
{
    if (code->library == nullptr) return;
    char path[1024];
    copy_path(path, sizeof(path), code->load_count);
    unlink(path);
}

bool
GameCode::load()
{
    *this = {};
    struct stat st;
    if (stat(GAME_LIBRARY_PATH, &st) != 0) {
        std::println(stderr, "Could not find the game code at {}", GAME_LIBRARY_PATH);
        return false;
    }
    if (!open_copy(this, 1)) return false;
    this->inode = (u64)st.st_ino;
    this->mtime = (i64)st.st_mtime;
    return true;
}

void
GameCode::reload_if_changed()
{
    struct stat st;
    // Missing while the linker replaces it; check again next frame.
    if (stat(GAME_LIBRARY_PATH, &st) != 0) return;
    // The linkers write a temporary file and rename it, so a new inode means a finished build.
    if ((u64)st.st_ino == this->inode && (i64)st.st_mtime == this->mtime) return;
    this->inode = (u64)st.st_ino;
    this->mtime = (i64)st.st_mtime;

    GameCode next = *this;
    if (!open_copy(&next, this->load_count + 1)) return;
    delete_copy(this);
    *this = next;
    std::println("Reloaded game code ({})", this->load_count);
}

void
GameCode::free()
{
    delete_copy(this);
    *this = {};
}

#else

bool
GameCode::load()
{
    *this = {};
    this->update = game_update;
    this->render = game_render;
    this->generate_sound = game_generate_sound;
    return true;
}

void
GameCode::reload_if_changed()
{
}

void
GameCode::free()
{
    *this = {};
}

#endif
