CXX = clang++
export CXX

.PHONY: all debug release clean nuke install
all: debug

debug: build/debug/build.ninja
	meson compile -C build/debug
	cp build/debug/handmade_hero ./hh-debug

release: build/release/build.ninja
	meson compile -C build/release
	cp build/release/handmade_hero ./hh-release

build/debug/build.ninja:
	meson setup build/debug --buildtype=debug -Dwerror=true -Db_sanitize=address,undefined -Db_lundef=false

build/release/build.ninja:
	meson setup build/release --buildtype=release

clean:
	if [ -f build/debug/build.ninja ]; then meson compile -C build/debug --clean; fi
	if [ -f build/release/build.ninja ]; then meson compile -C build/release --clean; fi
	rm -f ./hh-debug ./hh-release

nuke:
	rm -rf build ./hh-debug ./hh-release

install: release
	meson install -C build/release
