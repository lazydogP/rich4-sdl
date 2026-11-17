"""The native Mini runtime payload shared by personal packages, CI and QEMU."""
from pathlib import Path
from miyoo_toolchain import ROOT
from miyoo_ttf_runtime import ttf_runtime_libraries


def runtime_libraries(root=ROOT):
    root = Path(root)
    sdk = root / '.local/miyoo/sysroot'
    built = root / 'build/miyoo/mixer/tremor'
    files = ttf_runtime_libraries(sdk)
    files.update({
        'libSDL_mixer-1.2.so.0': built / 'libSDL_mixer-1.2.so.0',
        'libmad.so.0': built / 'libmad.so.0',
        # glibc loads this at pthread_cancel without an ELF NEEDED entry.
        'libgcc_s.so.1': built / 'libgcc_s.so.1',
    })
    for source in files.values():
        if not source.is_file():
            raise FileNotFoundError(source)
    return files


def runtime_licenses(root=ROOT):
    source = Path(root) / '.local/miyoo/mixer/src'
    files = {name + '.txt': source / name / folder / 'COPYING' for name, folder in (
        ('SDL_mixer', 'SDL_mixer-1.2.12'), ('libogg', 'libogg-1.3.5'),
        ('tremor', 'libvorbisidec-1.2.1+git20180316'), ('libmad', 'libmad-0.15.1b'))}
    for path in files.values():
        if not path.is_file():
            raise FileNotFoundError(path)
    return files
