"""One runtime library inventory shared by Mini packaging and ARM validation."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ttf_runtime_libraries(sdk):
    libraries = {name: Path(sdk) / 'usr/lib' / name for name in
                 ('libSDL_ttf-2.0.so.0', 'libpng16.so.16', 'libz.so.1')}
    # The device's FreeType has the same required FT_* ABI and needs only
    # GLIBC_2.4. The upstream SDK copy adds a GLIBC_2.28 requirement.
    libraries['libfreetype.so.6'] = ROOT / 'packaging/miyoo/lib/libfreetype.so.6'
    for path in libraries.values():
        if not path.is_file():
            raise FileNotFoundError(path)
    return libraries
