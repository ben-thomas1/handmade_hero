CC = clang
CXX = clang++
OBJCXX = clang++
export CC CXX OBJCXX

.PHONY: all debug release raddbg clean nuke install
all: debug

debug: build/debug/build.ninja
	meson compile -C build/debug
	ln -sf ./build/debug/handmade_hero ./hh-debug

release: build/release/build.ninja
	meson compile -C build/release
	ln -sf ./build/release/handmade_hero ./hh-release

build/debug/build.ninja:
	meson setup build/debug --buildtype=debug -Db_sanitize=address,undefined -Db_lundef=false

build/release/build.ninja:
	meson setup build/release --buildtype=release

raddbg: debug
	ASAN_OPTIONS=detect_leaks=0 raddbg ./hh-debug

clean:
	if [ -f build/debug/build.ninja ]; then meson compile -C build/debug --clean; fi
	if [ -f build/release/build.ninja ]; then meson compile -C build/release --clean; fi
	rm -f ./hh-debug ./hh-debug.rdi ./hh-release ./hh-release.rdi

nuke:
	rm -rf build ./hh-{debug,release}{,.rdi}

install: release
	meson install -C build/release
