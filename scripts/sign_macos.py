#!/usr/bin/env python3
"""Sign and notarize only an explicitly accepted macOS bundle."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
from macos import ROOT, BUILD, run, verify

GATES = ('midi_listening', 'import_failure_cases', 'macos14_clean_finder',
         'current_macos_clean_finder', 'text_audio_gameplay_saves', 'retina_external_pointer')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--acceptance-file', type=Path, required=True)
    args = parser.parse_args()
    try:
        acceptance = json.loads(args.acceptance_file.read_text())
        if any(acceptance.get(gate) is not True for gate in GATES):
            raise ValueError('Every human acceptance gate must be explicitly recorded as true')
        identity = os.environ.get('MACOS_SIGN_IDENTITY', '')
        profile = os.environ.get('MACOS_NOTARY_PROFILE', '')
        if not identity.startswith('Developer ID Application:') or not profile:
            raise ValueError('MACOS_SIGN_IDENTITY and MACOS_NOTARY_PROFILE are required; development bundle remains available')
        app = BUILD / 'Rich4.app'; verify(app)
        contents = app / 'Contents'
        for binary in list((contents / 'Frameworks').iterdir()) + list((contents / 'MacOS').iterdir()):
            run(['codesign', '--force', '--timestamp', '--options', 'runtime', '--sign', identity, binary])
        run(['codesign', '--force', '--timestamp', '--options', 'runtime', '--sign', identity, app])
        archive = ROOT / 'build/release/Rich4-macOS-arm64-notarization.zip'
        run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', app, archive])
        run(['xcrun', 'notarytool', 'submit', archive, '--keychain-profile', profile, '--wait'])
        run(['xcrun', 'stapler', 'staple', app])
        run(['xcrun', 'stapler', 'validate', app]); verify(app)
        run(['spctl', '--assess', '--type', 'execute', '--verbose', app])
        release = archive.with_name('Rich4-macOS-arm64.zip')
        run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', app, release])
        print(f'Notarized release: {release}')
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'macOS public signing failed: {error}\n')


if __name__ == '__main__':
    main()
