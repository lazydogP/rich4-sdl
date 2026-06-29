# R9 build matrix evidence

## Windows local WinLibs + SDL1

The configured compiler reports `MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r3`, GCC 16.1.0. The project-local `.local/sdl1/mingw64` SDL 1.2, mixer, and TTF import libraries are selected by Makefile.

` .local/toolchain/mingw64/bin/mingw32-make.exe -B sdl test-render` builds and links `bin/rich4_sdl` and executes both render tests successfully.

## SDL-free matrix

`make -B core tools test-unit test-resource test-render` succeeds without compiling or linking the SDL runtime executable. Core headers pass their dedicated no-SDL compile check; resource tools, parsers, scene/app reducers, RGB565 map golden, and FLC compositor tests build independently.

## Warnings and render gate

Every project compile rule uses C99 `-Wall -Wextra`. A forced Windows SDL/render build was captured and checked for GCC `warning:` diagnostics; none were emitted.

Makefile now provides the previously specified `test-render` target. It executes the synthetic RGB565 map framebuffer golden and the FLC player/compositor test.

## Linux sanitizer run

An installed WSL Ubuntu 18.04 environment reports Linux 4.4 (WSL1), x86_64, GCC 7.5.0. Makefile now provides `test-sanitize`, which uses isolated `build/sanitize` and `bin/sanitize` directories with `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`.

Running `make test-sanitize` inside WSL compiled and executed the complete unit, resource/fault-injection, and render matrices. All tests passed with no AddressSanitizer or UndefinedBehaviorSanitizer diagnostic.

## Desktop Linux SDL1

The legacy WSL network path could not run apt directly, so the official Ubuntu Bionic `libsdl1.2-dev` package was downloaded through the Windows host and installed into WSL. `sdl-config --version` reports 1.2.15 and the runtime links the system `/usr/lib/x86_64-linux-gnu/libSDL`.

Makefile platform discovery was corrected so Unix cannot select mounted Windows `.local` executables/import libraries. Two FLC/map tool rules accidentally nested under a Windows-only block were made portable. When SDL_mixer development files are absent, Linux selects `platform/sdl1/audio_stub.c` and runs explicitly without audio; Windows with mixer continues to build the real backend. SDL_ttf was already optional.

The first Linux replay exposed libc-dependent `rand()` output. `core/rng.c` now implements the fixed-width 15-bit MSVCRT sequence used by the original Windows runtime; an eight-value golden and replay dice golden prove identical Windows/Linux output.

`make -B test test-render tools sdl test-original-data` then passed under Linux GCC 7.5.0, linked real SDL 1.2.15, emitted no warnings, and scanned the mounted V3.11 resources successfully.
