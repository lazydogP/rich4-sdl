# Rich4 SDL — experimental PortMaster package

Thanks to Softstar for the original Richman 4, the rich4-sdl contributors for
the reimplementation, and the SDL and PortMaster teams.

This package targets **aarch64, 640×480, glibc 2.31 or newer**. It has not yet
been accepted into PortMaster or verified on handheld firmware. It uses the
firmware's SDL2 through sdl12-compat; no SDL2, libc or graphics drivers are bundled.

## Install

The CI download is named `Rich4-SDL-PortMaster-<version>.zip`. Rename it to the
canonical `rich4.zip` from `port.json` before placing it in your firmware's
PortMaster autoinstall queue, or merge its top-level launcher and `rich4/` into your ports folder.
Keep existing game data, configuration and saves when updating.

Supply legally obtained **V3.11** files in `ports/rich4/assets/original/`:

`Data.mkf`, `Map.mkf`, `Panel.mkf`, `Effect.mkf`, `Speaking.mkf`, `Jump.mkf`, `Help.mkf`.

Supply a CJK TrueType/OpenType font as `ports/rich4/assets/fonts/rich4.ttc`.
A `.ttf` can be copied under this filename; it is detected by file contents.
No original MKFs, music, fonts, executable or personal saves are included.

Optional music lives in `ports/rich4/assets/music/`: `Original/track02.ogg`
through `Original/track09.ogg`, optionally `MultiverseJourney/track02.ogg`
through `MultiverseJourney/track09.ogg`, and shared `track10.ogg` through
`track26.ogg` at the music root. WAV is also supported. This build supports
OGG/WAV music and WAV effects; MIDI and original AVI playback are unavailable.

Run **Rich4 SDL** from the firmware's ports menu. The internal framebuffer is
640×480; scaling and orientation depend on the firmware's SDL2 display driver.

## Controls

Names below use PortMaster's effective SDL mapping; printed button labels vary.

| Control | Action |
| --- | --- |
| D-pad / left stick | Move pointer |
| A / B | Left / right mouse button, including holds and releases |
| X | Roll dice (D shortcut) |
| Y / Start | Enter |
| Select / Back | Tab |
| L1 / R1 | Page Up / Page Down |
| L2 / R2 | Cards / tools (C / W shortcuts) |
| L3 | Slow pointer while held |
| R3 | Escape |
| PortMaster exit chord | Exit; save through the game before using it |

Without stick clicks, use mouse right-click for the game's cancel/back actions.
Card/tool button meanings follow the game's configured keyboard shortcuts.

## User files and diagnostics

Saves: `ports/rich4/saves/`. Settings: `RICH4.CFG` and `RICH4-SDL.CFG` in
`ports/rich4/`. Copy `RICH4-SDL.CFG.example` to `RICH4-SDL.CFG` only when needed.
The launch log is `ports/rich4/log.txt`. Preserve these files on update.
It records PortMaster discovery, detected firmware/architecture, libc version,
loader/SDL errors and the game's exit status. If the payload directory itself
is missing, the launcher instead writes `rich4-launch.log` beside the launch script.

Current gameplay limitations are described in the included engine source README.
Successful host/QEMU checks do not establish handheld display, sound, input,
suspend/resume, full-game stability or cross-firmware compatibility.

## Build and source

From the rich4-sdl source tree, on Linux x86_64 with Python 3.12+, GNU Make,
CMake, pkg-config, dpkg-deb and a C build environment, or Windows with those tools in WSL:

```sh
python scripts/make_minimal.py package-portmaster --progress
python scripts/make_minimal.py test-portmaster --progress
```

Windows uses the default WSL distribution. Select another with
`PORTMASTER_ARGS="--wsl-distro Legacy"` or `RICH4_PORTMASTER_WSL`.
Downloads, the compiler and dependency builds stay in `.local/portmaster/`;
`scripts/portmaster.lock.json` records URLs and SHA-256 checksums. Reuse cached
downloads offline with `PORTMASTER_ARGS=--offline`.

The package ABI/structure report is `build/portmaster/package-validation.json`.
The user-facing archive is `build/portmaster/Rich4-SDL-PortMaster-build-local.zip`
with its checksum in the adjacent `.zip.sha256` file.
The optional ARM/QEMU check extracts this archive, runs its binary and bundled
libraries with dummy display/audio, and records results in
`build/portmaster-aarch64/validation.json`. It uses separate test save/config
directories; the user's original MKFs, music and font are read in place.

`sources/rich4-sdl-source.tar.gz` contains the exact engine source used for
this build, including build scripts. `sources/SDL_mixer-1.2.12.tar.gz` contains
the unmodified LGPL SDL_mixer source. Other dependency sources are available
at the checksum-pinned URLs in the included lock file. FreeType, Ogg and Vorbis
are statically included in the shared TTF/mixer libraries; their notices are
in the flat `licenses/` directory. Portions of this software are copyright
The FreeType Project (https://www.freetype.org/).

PortMaster upstream references: [packaging](https://portmaster.games/contribute/porting/packaging/)
and [submission instructions](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md).
