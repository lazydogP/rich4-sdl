"""One runtime library inventory shared by Mini packaging and ARM validation."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ttf_runtime_libraries(sdk):
    # Local packages, CI and QEMU use the same pinned SDK, including FreeType.
    libraries = {name: Path(sdk) / 'usr/lib' / name for name in
                 ('libSDL_ttf-2.0.so.0', 'libpng16.so.16', 'libz.so.1', 'libfreetype.so.6')}
    for path in libraries.values():
        if not path.is_file():
            raise FileNotFoundError(path)
    return libraries
