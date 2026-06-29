#!/usr/bin/env python3
"""Emit the single source/object inventory used by the SDL1 build audit."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLATFORM_ROOT = ROOT / "src" / "platform" / "sdl1"


def sources(profile: str | None = None) -> list[Path]:
    excluded: set[str] = set()
    if profile == "desktop":
        excluded = {"font_atlas.c", "text_backend_atlas.c"}
    elif profile == "handheld":
        excluded = {"text_backend_freetype.c"}
    return sorted(
        PLATFORM_ROOT.rglob("*.c"),
        # The profile filter only changes the production link set.  The
        # unfiltered inventory remains the source of generated test rules.
        key=lambda path: path.relative_to(ROOT).as_posix().lower(),
    ) if profile is None else sorted(
        (path for path in PLATFORM_ROOT.rglob("*.c")
         if path.name not in excluded),
        key=lambda path: path.relative_to(ROOT).as_posix().lower(),
    )


def object_name(source: Path) -> str:
    name = source.stem
    if source.name == "input_config_sdl.c":
        return "input.o"
    if source.name == "audio_sdl_mixer.c":
        return "audio.o"
    if source.name == "audio_stub.c":
        return "audio_stub.o"
    if source.name == "game_event_presentation.c":
        return "sdl1_game_event_render.o"
    if source.name == "game_preview_frame.c":
        return "sdl1_game_preview_render.o"
    if source.name == "runtime_text_backend.c":
        return "sdl1_runtime_text.o"
    return f"sdl1_{name}.o"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--objects", action="store_true")
    parser.add_argument("--pairs", action="store_true")
    parser.add_argument("--profile", choices=("desktop", "handheld", "miyoo"))
    args = parser.parse_args()
    for source in sources(args.profile):
        if args.pairs:
            print(
                f"{source.relative_to(ROOT).as_posix()}|"
                f"build/{object_name(source)}"
            )
        elif args.objects:
            print(f"build/{object_name(source)}")
        else:
            print(source.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
