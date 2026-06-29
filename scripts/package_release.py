#!/usr/bin/env python3
"""Package CI runtimes without reading personal game assets or settings."""
import argparse
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def package(root, output_dir, platform, version):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._+-]*', version):
        raise ValueError('version must contain only letters, digits, dots, +, _ or -')
    files = {}
    if platform == 'windows-x64':
        prefix = 'Rich4-Windows-x64'
        files['rich4_sdl.exe'] = root / 'bin/rich4_sdl.exe'
        files['RICH4-SDL.CFG.example'] = root / 'RICH4-SDL.CFG.example'
        for name in ('SDL.dll', 'SDL_ttf.dll', 'libSDL_mixer-1-2-0.dll',
                     'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
            files[name] = root / 'bin' / name
        # stage-sdl-runtime copies the SDL toolchain's transitive DLLs here.
        for source in sorted((root / '.local/sdl1/mingw64/bin').glob('*.dll')):
            files[source.name] = root / 'bin' / source.name
        readme = '''Rich4 SDL - Windows x64
Extract this archive to a writable directory.
Copy your legally obtained original files to assets/original/:
Data.mkf, Map.mkf, Panel.mkf, Effect.mkf, Speaking.mkf, Jump.mkf, Help.mkf.
Place your own CJK font at assets/fonts/rich4.ttc.
Optional music goes in assets/music/. Then run rich4_sdl.exe.
Saves and configuration are created at runtime.
This package contains no original game data, music, fonts or personal settings.
'''
    elif platform == 'miyoo-mini':
        prefix = 'Rich4-MiyooMini-update'
        files['App/Rich4/rich4_sdl'] = root / 'bin/miyoo/rich4_sdl'
        files['App/Rich4/launch.sh'] = root / 'packaging/miyoo/launch.sh'
        files['App/Rich4/RICH4-SDL.CFG.example'] = root / 'RICH4-SDL.CFG.example'
        for name in ('config.json', 'icon.png'):
            files['App/Rich4/' + name] = root / 'packaging/miyoo' / name
        readme = '''Rich4 SDL - Miyoo Mini / Mini Plus program update
Merge App/ into your SD card root, replacing the executable, launcher,
Apps menu config.json and transparent 64x64 icon.png.
Requires original MKF data, a CJK font and input configuration in App/Rich4.
Keep assets/fonts/rich4.ttc and your existing data when updating.
On the tested Onion device, SDL, SDL_mixer, SDL_ttf, FreeType and libgcc_s
are supplied by the system; no additional runtime library files are needed.
The launcher includes /mnt/SDCARD/miyoo/lib, /config/lib and /lib in its
library search path. An existing App/Rich4/lib directory is optional.
Game data, fonts, music, saves and in-game configuration are not included or replaced.
'''
    else:
        raise ValueError(f'unsupported platform: {platform}')

    readme += '''Optional SDL settings are documented in RICH4-SDL.CFG.example beside the executable.
To customize, copy it to RICH4-SDL.CFG in that directory (App/Rich4 on Miyoo).
Keep your existing RICH4-SDL.CFG when updating. Without it, built-in defaults apply.
'''

    # Check everything before creating the output; never zip a staging tree
    # which might still contain local MKFs, fonts, logs or saves.
    for source in files.values():
        if not source.is_file():
            raise FileNotFoundError(source)
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f'{prefix}-{version}.zip'
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, source in sorted(files.items()):
            entry = zipfile.ZipInfo(name)
            entry.create_system = 3
            executable = name.endswith(('/rich4_sdl', '/launch.sh', '.exe'))
            entry.external_attr = (0o100755 if executable else 0o100644) << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            data = source.read_bytes()
            if name.endswith('.sh'):
                data = data.replace(b'\r\n', b'\n')
            archive.writestr(entry, data)
        archive.writestr('README.txt', readme.encode('utf-8'))
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError(f'corrupt release archive: {output}')
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=('windows-x64', 'miyoo-mini'), required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/release')
    args = parser.parse_args()
    try:
        output = package(ROOT, args.output_dir, args.platform, args.version)
    except (OSError, ValueError) as error:
        parser.exit(1, f'Release packaging failed: {error}\n')
    print(f'Release package: {output}')


if __name__ == '__main__':
    main()
