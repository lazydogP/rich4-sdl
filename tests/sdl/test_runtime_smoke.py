"""Run the real SDL runtime for a bounded deterministic framebuffer smoke."""

from __future__ import annotations

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path


SMOKE_LINE = re.compile(
    r"SDL smoke: frames=(?P<frames>\d+) scene=(?P<scene>\d+) "
    r"framebuffer=(?P<digest>[0-9a-fA-F]+) "
    r"sequence=(?P<sequence>[0-9a-fA-F]+)"
)


# V3.11, default configuration, bundled font and RGB565 resource conversion
# from original 0x451801 (green bit 5 stays zero). See m3-12-replay.md.
EXPECTED_SEQUENCE = {
    (False, 2): "0caab095",
    (True, 120): "71e2ffe7",
}

EXPECTED_FRAMEBUFFER = {
    (False, 2): "d2e1daa3",
    (True, 120): "5dd46dfc",
}


def run_smoke(
    root: Path, executable: Path, game_scene: bool, frame_count: int
) -> tuple[str, str, str]:
    smoke_env = os.environ.copy()
    smoke_env["SDL_VIDEODRIVER"] = "dummy"
    smoke_env["SDL_AUDIODRIVER"] = "dummy"
    for name in ("R4DIFF_LINK", "RICH4_TEXT_BACKEND", "RICH4_PROFILE",
                 "RICH4_PROFILE_LOG", "RICH4_UI_QA_BMP"):
        smoke_env.pop(name, None)
    # Personal RICH4*.CFG files and saves must not define the fixture or be
    # changed by it. Keep resources, font and input bindings explicit.
    with tempfile.TemporaryDirectory(prefix="rich4-smoke-") as work_dir:
        result = subprocess.run(
            [
                str(executable.resolve()),
                "--data-dir", str(root / "assets/original"),
                "--music-dir", str(root / "assets/music"),
                "--font", str(root / "assets/fonts/rich4.ttc"),
                "--input-config", str(root / "config/input.toml"),
                "--smoke-game-frames" if game_scene else "--smoke-frames",
                str(frame_count),
            ],
            cwd=work_dir,
            env=smoke_env,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(
            f"SDL runtime smoke exited with status {result.returncode}"
        )
    matches = SMOKE_LINE.findall(result.stdout)
    if not matches:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit("SDL runtime smoke did not emit a framebuffer digest")
    frames, scene, digest, sequence = matches[-1]
    expected_scene = "4" if game_scene else "0"
    if frames != str(frame_count) or scene != expected_scene:
        raise SystemExit(
            f"unexpected SDL smoke endpoint: frames={frames} scene={scene} "
            f"expected_scene={expected_scene}"
        )
    return scene, digest.lower(), sequence.lower()


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    bin_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("bin")
    executable = root / bin_dir / (
        "rich4_sdl.exe" if os.name == "nt" else "rich4_sdl"
    )
    if not executable.exists():
        print("SDL runtime smoke: SKIP (bin/rich4_sdl is not built)")
        return 0
    if not (root / "assets" / "original").is_dir():
        print("SDL runtime smoke: SKIP (assets/original is not available)")
        return 0

    summaries = []
    for game_scene, frame_count in ((False, 2), (True, 120)):
        first = run_smoke(root, executable, game_scene, frame_count)
        second = run_smoke(root, executable, game_scene, frame_count)
        if first != second:
            raise SystemExit(
                "SDL runtime framebuffer is not deterministic: "
                f"{first} != {second}"
            )
        expected_sequence = EXPECTED_SEQUENCE[(game_scene, frame_count)]
        if first[2] != expected_sequence:
            raise SystemExit(
                "SDL runtime frame sequence baseline changed: "
                f"{first[2]} != {expected_sequence} for "
                f"scene={first[0]} frames={frame_count}"
            )
        expected_framebuffer = EXPECTED_FRAMEBUFFER[(game_scene, frame_count)]
        if first[1] != expected_framebuffer:
            raise SystemExit(
                "SDL runtime framebuffer baseline changed: "
                f"{first[1]} != {expected_framebuffer} for "
                f"scene={first[0]} frames={frame_count}"
            )
        summaries.append(first)
    print(
        "SDL runtime smoke: "
        + ", ".join(
            f"frames={frame_count} scene={scene} framebuffer={digest} "
            f"sequence={sequence} deterministic=2x"
            for (scene, digest, sequence), frame_count in zip(
                summaries, (2, 120)
            )
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
