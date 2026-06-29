"""Copy original MKFs byte for byte and verify the package staging files."""
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATA_DIR = ROOT / 'assets/original'
MKF_NAMES = ('Data.mkf', 'Map.mkf', 'Panel.mkf', 'Effect.mkf',
             'Speaking.mkf', 'Jump.mkf', 'Help.mkf')


def stage_original_assets(data_dir, destination):
    data_dir, destination = Path(data_dir).resolve(), Path(destination).resolve()
    if data_dir == destination:
        raise ValueError('Use a separate package directory; source MKFs are read-only.')
    for name in MKF_NAMES:
        if not (data_dir / name).is_file():
            raise FileNotFoundError(data_dir / name)
    destination.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name in MKF_NAMES:
        source, target = data_dir / name, destination / name
        # Keep both the original compression and every header/index byte.
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        shutil.copyfile(source, target)
        package_hash = hashlib.sha256(target.read_bytes()).hexdigest()
        if source_hash != package_hash:
            raise ValueError(f'Original MKF copy differs: {name}')
        manifest[name] = {'source_sha256': source_hash, 'package_sha256': package_hash,
                          'bytes': source.stat().st_size, 'byte_identical': True}
    # Remove the obsolete generated manifest from this package stage only.
    (destination / 'raw-mkf-manifest.json').unlink(missing_ok=True)
    (destination / 'original-mkf-manifest.json').write_text(
        json.dumps(manifest, indent=2), encoding='utf-8')
    return manifest


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, default=DEFAULT_DATA_DIR)
    parser.add_argument('--destination', type=Path,
                        default=ROOT / 'build/miyoo/package/App/Rich4/assets/original')
    args = parser.parse_args()
    print(json.dumps(stage_original_assets(args.data_dir, args.destination), indent=2))
