#!/usr/bin/env python3
"""Run the production Mini binary in project-local QEMU, with disk-backed assets."""
import argparse
import gzip
import io
import json
import lzma
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tarfile
import time
from summarize_miyoo_profile import summarize
from miyoo_ttf_runtime import ttf_runtime_libraries
from miyoo_toolchain import (SYSROOT, STRIP, QEMU, SEVEN_ZIP, MKE2FS,
                             KERNEL, KERNEL_DEB, BUSYBOX, require_qemu_tools)

ROOT = Path(__file__).resolve().parents[1]


def kernel_modules(deb):
    data = deb.read_bytes()
    offset = 8
    while offset < len(data):
        name = data[offset:offset + 16].decode().strip().rstrip('/')
        size = int(data[offset + 48:offset + 58])
        body = data[offset + 60:offset + 60 + size]
        offset += 60 + size + size % 2
        if name.startswith('data.tar'):
            break
    archive = tarfile.open(fileobj=io.BytesIO(body))
    inventory = {Path(m.name).name.removesuffix('.xz').removesuffix('.ko'): m
                 for m in archive if '.ko' in m.name and m.isfile()}
    selected = {}

    def include(name):
        if name in selected or name not in inventory:
            return
        entry = inventory[name]
        content = archive.extractfile(entry).read()
        if entry.name.endswith('.xz'):
            content = lzma.decompress(content)
        depends = re.search(rb'\x00depends=([^\x00]*)', content)
        for dependency in depends.group(1).decode().split(',') if depends else []:
            include(dependency)
        selected[name] = content

    for name in ('virtio_mmio', 'virtio_blk', 'crc32c_generic', 'ext4'):
        include(name)
    return selected


def initramfs(files, modules, script):
    output = bytearray()
    inode = 1

    def entry(name, mode, content=b'', major=0, minor=0):
        nonlocal inode
        label = name.encode() + b'\0'
        fields = [inode, mode, 0, 0, 1, 0, len(content), 0, 0, major, minor, len(label), 0]
        output.extend(b'070701' + b''.join(f'{value:08x}'.encode() for value in fields))
        output.extend(label)
        output.extend(b'\0' * (-len(output) % 4))
        output.extend(content)
        output.extend(b'\0' * (-len(output) % 4))
        inode += 1

    for name in ('bin', 'lib', 'dev', 'proc', 'sys', 'tmp', 'mnt', 'qa', 'modules'):
        entry(name, stat.S_IFDIR | 0o755)
    for name, path in files.items():
        entry(name, stat.S_IFREG | 0o755, path.read_bytes())
    for name, data in modules.items():
        entry('modules/' + name + '.ko', stat.S_IFREG | 0o644, data)
    entry('init', stat.S_IFREG | 0o755, script.encode())
    entry('dev/console', stat.S_IFCHR | 0o600, major=5, minor=1)
    entry('TRAILER!!!', 0)
    return gzip.compress(output, compresslevel=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--memory-mb', type=int, default=256)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--stalls-benchmark', action='store_true', help='run locally built before/after save and decode CPU probes')
    parser.add_argument('--text-backend', choices=('atlas', 'ttf'), default='ttf')
    parser.add_argument('--reuse-disk', action='store_true')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--baseline-binary', type=Path,
                        help='Optional old binary, used only for real-clock menu pacing comparison')
    args = parser.parse_args()
    if args.reuse_disk:
        parser.error('Original-MKF verification requires a fresh disk; omit --reuse-disk to avoid stale assets.')
    try:
        require_qemu_tools()
    except FileNotFoundError as error:
        parser.error(str(error))
    out = args.output_dir.resolve() if args.output_dir else ROOT / f'build/miyoo/qemu{args.memory_mb}'
    out.mkdir(parents=True, exist_ok=True)
    app = ROOT / 'build/miyoo/package/App/Rich4'
    # Update only executable/launcher in our existing personal package stage.
    shutil.copyfile(ROOT / 'bin/miyoo/rich4_sdl', app / 'rich4_sdl')
    (app / 'launch.sh').write_text((ROOT / 'packaging/miyoo/launch.sh').read_text(encoding='utf-8'),
                                 encoding='utf-8', newline='\n')
    shutil.copyfile(ROOT / 'assets/fonts/rich4_font.r4atlas', app / 'assets/fonts/rich4_font.r4atlas')
    shutil.copyfile(ROOT / 'assets/fonts/rich4.ttc', app / 'assets/fonts/rich4.ttc')
    from stage_miyoo_original_assets import DEFAULT_DATA_DIR, stage_original_assets
    stage_original_assets(DEFAULT_DATA_DIR, app / 'assets/original')
    ttf_libraries = ttf_runtime_libraries(SYSROOT)
    for name, source in ttf_libraries.items():
        (app / 'lib').mkdir(exist_ok=True)
        shutil.copyfile(source, app / 'lib' / name)
    disk = out / 'assets.ext4'
    if not args.reuse_disk or not disk.exists():
        mke2fs = str(MKE2FS)
        payload = out / 'assets.tar'
        with tarfile.open(payload, 'w') as archive:
            archive.add(app.parents[1], arcname='.', recursive=True)
            if args.baseline_binary:
                archive.add(args.baseline_binary, arcname='./App/Rich4/qa-baseline')
        # Android's Windows mke2fs cannot populate from a tarball. Attach the
        # tar as a second read-only block device and extract it inside Linux.
        result = subprocess.run([mke2fs, '-q', '-t', 'ext4', '-O', '^metadata_csum,^64bit',
            '-F', '-m', '0', '-b', '4096', str(disk), '262144'],
            env={**os.environ, 'MKE2FS_CONFIG': str(MKE2FS.with_name('mke2fs.conf'))},
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / 'mke2fs.log').write_bytes(result.stdout)
        if result.returncode:
            raise RuntimeError(result.stdout.decode(errors='replace'))
    sdk = SYSROOT
    files = {'bin/busybox': BUSYBOX,
             'qa/probe': ROOT / 'bin/miyoo/test_miyoo_qemu_probe',
             'qa/scope_probe': ROOT / 'bin/miyoo/test_miyoo_asset_scope',
             'qa/flc_playback': ROOT / 'bin/miyoo/test_flc_playback',
             'qa/property_flash': ROOT / 'bin/miyoo/test_property_map_presentation',
             'qa/present_pixels': ROOT / 'bin/miyoo/test_present_pixels',
             'qa/blit_pixels': ROOT / 'bin/miyoo/test_sdl1_blit',
             'qa/libmiyoo_qa.so': ROOT / 'build/miyoo-ttf/libmiyoo_qa.so'}
    for name in ('ld-linux-armhf.so.3', 'libc.so.6', 'libm.so.6', 'libpthread.so.0', 'libdl.so.2', 'libgcc_s.so.1'):
        # Strip local copies so debug info does not inflate the RAM rootfs.
        dest = out / name
        shutil.copyfile(sdk / 'lib' / name, dest)
        strip = STRIP
        subprocess.run([str(strip), '--strip-unneeded', str(dest)], check=True)
        files['lib/' + name] = dest
    files['lib/libSDL-1.2.so.0'] = sdk / 'usr/lib/libSDL-1.2.so.0'
    files['lib/libSDL_mixer-1.2.so.0'] = sdk / 'usr/lib/libSDL_mixer-1.2.so.0'
    for name, source in ttf_libraries.items():
        files['lib/' + name] = source
    if args.stalls_benchmark:
        files['qa/save_benchmark'] = ROOT / 'bin/miyoo/test_save_benchmark'
        files['qa/save_fixture'] = ROOT / 'build/miyoo/stalls-ttf-before/device-save.R4S'
        files['qa/decode_benchmark'] = ROOT / 'bin/miyoo/test_mkf_decode_equivalence'
    modules = kernel_modules(KERNEL_DEB)
    module_commands = '\n'.join(f'insmod /modules/{name}.ko || exit 2' for name in modules)
    script = '''#!/bin/busybox sh
/bin/busybox --install -s /bin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
echo VM_TEST_BEGIN
MODULE_COMMANDS
if mount -t ext4 /dev/vda /mnt; then
    PAYLOAD=/dev/vdb
elif mount -t ext4 /dev/vdb /mnt; then
    PAYLOAD=/dev/vda
else
    echo DISK_MOUNT_FAILED
    dmesg
    poweroff -f
fi
if [ ! -f /mnt/App/Rich4/rich4_sdl ]; then
    tar -xf "$PAYLOAD" -C /mnt || { echo ASSET_EXTRACT_FAILED; poweroff -f; }
    sync
    echo 3 > /proc/sys/vm/drop_caches
fi
mkdir -p /mnt/results/menu /mnt/results/game /mnt/results/new_game
cd /mnt/App/Rich4 || exit 2
export LD_LIBRARY_PATH=/lib SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
export RICH4_MIYOO_ROTATE=0 RICH4_LOG_MEMORY=1
export RICH4_PROFILE=2 RICH4_TEXT_BACKEND=TEXT_BACKEND_CHOICE
/qa/present_pixels > /mnt/results/present_pixels.log 2>&1
echo PRESENT_PIXELS_EXIT=$?
cat /mnt/results/present_pixels.log
/qa/blit_pixels > /mnt/results/blit_pixels.log 2>&1
echo BLIT_PIXELS_EXIT=$?
cat /mnt/results/blit_pixels.log
grep -E 'MemTotal:|MemAvailable:|SwapTotal:' /proc/meminfo
/qa/probe /mnt/results/chinese.bmp > /mnt/results/probe.log 2>&1
echo PROBE_EXIT=$?
cat /mnt/results/probe.log
/qa/scope_probe > /mnt/results/scope.log 2>&1
echo SCOPE_EXIT=$?
tail -n 5 /mnt/results/scope.log
RICH4_PROFILE_LOG=/mnt/results/property-perf.log /qa/property_flash > /mnt/results/property_flash.log 2>&1
echo PROPERTY_FLASH_EXIT=$?
cat /mnt/results/property_flash.log
RICH4_PROFILE_LOG=/mnt/results/flc-audio-perf.log /qa/flc_playback > /mnt/results/flc_playback.log 2>&1
echo FLC_PLAYBACK_EXIT=$?
cat /mnt/results/flc_playback.log
if [ -f /qa/save_benchmark ]; then
    /qa/save_benchmark /qa/save_fixture > /mnt/results/save_benchmark.log 2>&1
    echo SAVE_BENCHMARK_EXIT=$?
    cat /mnt/results/save_benchmark.log
    /qa/decode_benchmark assets/original > /mnt/results/decode_benchmark.log 2>&1
    echo DECODE_BENCHMARK_EXIT=$?
    cat /mnt/results/decode_benchmark.log
fi
export LD_PRELOAD=/qa/libmiyoo_qa.so
export RICH4_QA_OUTPUT=/mnt/results/menu
export RICH4_QA_CAPTURE=1,33,40,54,68,74
export RICH4_QA_KEYS=2:275:1,2:274:1,30:274:0,32:275:0,34:32:1,35:32:0,50:306:1,51:306:0,65:308:1,66:308:0,70:27:1,71:27:0
sh ./launch.sh --smoke-frames 80
echo MENU_EXIT=$?
cp logs/*.log /mnt/results/menu/
unset RICH4_QA_KEYS
export RICH4_QA_OUTPUT=/mnt/results/new_game
export RICH4_QA_POINTER=2:190:380,10:44:50,15:116:50,20:188:50,25:260:50,35:490:195
export RICH4_QA_KEYS=3:32:1,4:32:0,11:32:1,12:32:0,16:32:1,17:32:0,21:32:1,22:32:0,26:32:1,27:32:0,36:32:1,37:32:0,280:13:1,281:13:0
export RICH4_QA_CAPTURE=1,8,34,40,105,130,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,170,190,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,360
sh ./launch.sh --smoke-frames 360
echo NEW_GAME_EXIT=$?
cp logs/*.log /mnt/results/new_game/
unset RICH4_QA_POINTER
export RICH4_QA_OUTPUT=/mnt/results/game
export RICH4_QA_KEYS=4:13:1,5:13:0,80:305:1,81:305:0
export RICH4_QA_CAPTURE=2,30,120,600
sh ./launch.sh --smoke-game-frames 600
echo GAME_EXIT=$?
cp logs/*.log /mnt/results/game/
cat /mnt/results/menu/console.log
cat /mnt/results/new_game/console.log
cat /mnt/results/game/console.log
unset RICH4_QA_KEYS RICH4_QA_CAPTURE RICH4_QA_POINTER
# No smoke option here: both old and new binaries use the real SDL clock
# and actual waits. The injection library exits after 120 presentations.
export RICH4_QA_REAL_FRAMES=120
if [ -f qa-baseline ]; then
    mv rich4_sdl rich4_sdl.optimized
    cp qa-baseline rich4_sdl
    chmod +x rich4_sdl
    mkdir -p /mnt/results/realtime_before
    export RICH4_QA_OUTPUT=/mnt/results/realtime_before
    sh ./launch.sh
    echo REALTIME_BEFORE_EXIT=$?
    cp logs/*.log /mnt/results/realtime_before/
    mv rich4_sdl.optimized rich4_sdl
fi
mkdir -p /mnt/results/realtime_after
export RICH4_QA_OUTPUT=/mnt/results/realtime_after
sh ./launch.sh
echo REALTIME_AFTER_EXIT=$?
cp logs/*.log /mnt/results/realtime_after/
mkdir -p /mnt/results/realtime_game_after
export RICH4_QA_OUTPUT=/mnt/results/realtime_game_after
export RICH4_QA_REAL_FRAMES=360
export RICH4_QA_POINTER=2:190:380,10:44:50,15:116:50,20:188:50,25:260:50,35:490:195
export RICH4_QA_KEYS=3:32:1,4:32:0,11:32:1,12:32:0,16:32:1,17:32:0,21:32:1,22:32:0,26:32:1,27:32:0,36:32:1,37:32:0,280:13:1,281:13:0
sh ./launch.sh
echo REALTIME_GAME_AFTER_EXIT=$?
cp logs/*.log /mnt/results/realtime_game_after/
# Let every selected character finish both intro FLCs without a skip key.
mkdir -p /mnt/results/realtime_intro_after
export RICH4_QA_OUTPUT=/mnt/results/realtime_intro_after
export RICH4_QA_REAL_FRAMES=1800
export RICH4_QA_KEYS=3:32:1,4:32:0,11:32:1,12:32:0,16:32:1,17:32:0,21:32:1,22:32:0,26:32:1,27:32:0,36:32:1,37:32:0
sh ./launch.sh
echo REALTIME_INTRO_AFTER_EXIT=$?
cp logs/*.log /mnt/results/realtime_intro_after/
unset RICH4_QA_REAL_FRAMES
unset LD_PRELOAD
dmesg | grep -i -E 'out of memory|killed process|oom' > /mnt/results/oom.log
grep -E 'MemTotal:|MemAvailable:|SwapTotal:' /proc/meminfo
sync
cd /
sleep 2
umount /mnt
echo VM_TEST_END
poweroff -f
'''.replace('MODULE_COMMANDS', module_commands).replace('TEXT_BACKEND_CHOICE', args.text_backend)
    initrd = out / 'initramfs.gz'
    initrd.write_bytes(initramfs(files, modules, script))
    (out / 'init.sh').write_text(script, encoding='utf-8', newline='\n')
    qemu = QEMU
    command = [str(qemu), '-M', 'virt', '-cpu', 'cortex-a7', '-m', str(args.memory_mb),
               '-smp', '2', '-kernel', str(KERNEL), '-initrd', str(initrd),
               '-append', 'console=ttyAMA0 loglevel=3', '-display', 'none', '-monitor', 'none',
               '-serial', 'file:' + str(out / 'serial.log'), '-no-reboot',
               '-drive', f'file={disk.as_posix()},if=none,id=assets,format=raw',
               '-device', 'virtio-blk-device,drive=assets',
               '-drive', f'file={(out / "assets.tar").as_posix()},if=none,id=payload,format=raw,readonly=on',
               '-device', 'virtio-blk-device,drive=payload']
    (out / 'command.json').write_text(json.dumps(command, indent=2), encoding='utf-8')
    print(f'QEMU started: memory={args.memory_mb} MB; assets on 1 GiB ext4 disk', flush=True)
    with (out / 'qemu.stderr.log').open('wb') as stderr:
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=stderr,
                                   creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        started = time.monotonic()
        while process.poll() is None:
            if time.monotonic() - started > args.timeout:
                process.terminate()
                process.wait()
                raise TimeoutError(f'QEMU timed out; inspect {out / "serial.log"}')
            time.sleep(1)
    print(f'QEMU exited: {process.returncode}; log: {out / "serial.log"}', flush=True)
    extractor = SEVEN_ZIP
    extracted = out / 'extracted'
    extraction = subprocess.run([str(extractor), 'x', str(disk), 'results/*',
                                 '-o' + str(extracted), '-y'], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
    (out / 'extract.log').write_bytes(extraction.stdout)
    if extraction.returncode:
        raise RuntimeError('failed to extract QEMU evidence: ' + extraction.stdout.decode(errors='replace'))
    serial = (out / 'serial.log').read_text(errors='replace')
    if args.stalls_benchmark:
        assert 'SAVE_BENCHMARK_EXIT=0' in serial and 'DECODE_BENCHMARK_EXIT=0' in serial
    results = extracted / 'results'
    summary = {'configured_memory_mb': args.memory_mb,
               'swap_kb': 0, 'cases': {},
               'probe_passed': 'PROBE_EXIT=0' in serial,
               'scope_passed': 'SCOPE_EXIT=0' in serial,
               'flc_playback_passed': 'FLC_PLAYBACK_EXIT=0' in serial and 'QA_FLC_PLAYBACK_PASS' in serial,
               'oom_killed': 'Out of memory:' in serial}
    scope_log = (results / 'scope.log').read_text(errors='replace')
    scope_peak = re.search(r'QA_SCOPE_MEMORY peak_rss_kb=(\d+)', scope_log)
    summary['scope_peak_rss_kb'] = int(scope_peak.group(1)) if scope_peak else None
    summary['four_character_model_bytes'] = [
        {'character': int(character), 'model_bytes': int(size)}
        for character, size in re.findall(r'QA_SCOPE_FOUR character=(\d+) model_bytes=(\d+)', scope_log)]
    for case in ('menu', 'new_game', 'game'):
        console = (results / case / 'console.log').read_text(errors='replace')
        memory = (results / case / 'memory.log').read_text(errors='replace')
        launcher = (results / case / 'launch.log').read_text(errors='replace')
        peak = re.search(r'QA_END .*peak_rss_kb=(\d+)', console)
        sampled = [int(value) for value in re.findall(r'VmHWM:\s*(\d+)', memory)]
        available = [int(value) for value in re.findall(r'MemAvailable:\s*(\d+)', memory)]
        swapped = [int(value) for value in re.findall(r'VmSwap:\s*(\d+)', memory)]
        exit_status = re.search(r'exit_status=(\d+)', launcher)
        summary['cases'][case] = {
            'exit_status': int(exit_status.group(1)) if exit_status else None,
            'peak_rss_kb': int(peak.group(1)) if peak else None,
            'sampled_peak_rss_kb': max(sampled) if sampled else None,
            'minimum_sampled_available_kb': min(available) if available else None,
            'maximum_sampled_process_swap_kb': max(swapped) if swapped else None,
            'captures': sorted(path.name for path in (results / case).glob('*.bmp'))}
        perf = (results / case / 'perf.log').read_text(errors='replace')
        (out / f'performance-{case}.json').write_text(
            json.dumps(summarize(results / case / 'perf.log'), indent=2), encoding='utf-8')
        summary['cases'][case]['stages'] = [
            {'name': name, 'elapsed_ms': int(elapsed)}
            for name, elapsed in re.findall(r'STAGE .*name=(\S+) elapsed_ms=(\d+)', perf)]
    for case in ('realtime_before', 'realtime_after', 'realtime_game_after', 'realtime_intro_after'):
        path = results / case
        if (path / 'perf.log').exists():
            report = summarize(path / 'perf.log')
            (out / f'performance-{case}.json').write_text(
                json.dumps(report, indent=2), encoding='utf-8')
            summary[case] = report['frames']
    summary['real_clock_game_reached_map'] = any(
        part.endswith(':ground') and stats['count'] > 0
        for part, stats in report['render_parts'].items())
    intro_log = (results / 'realtime_intro_after/perf.log').read_text(errors='replace')
    intro_streams = {}
    for archive, index, frame, decoded in re.findall(
            r'FLC .*archive_id=(\d+) index=(\d+) frame=(\d+) decoded=(\d+)', intro_log):
        if 47 <= int(index) <= 70:
            intro_streams.setdefault((int(archive),int(index)), []).append((int(frame),int(decoded)))
    summary['intro_actor_streams'] = [
        {'archive_id': key[0], 'index': key[1], 'frames': len(values),
         'sequential': all(frame == i and decoded == 1 for i,(frame,decoded) in enumerate(values))}
        for key, values in intro_streams.items()]
    summary['real_clock_intro_completed'] = any(
        part.endswith(':ground') and stats['count'] > 0
        for part, stats in report['render_parts'].items())
    summary['intro_no_skips_or_redecode'] = (len(intro_streams) == 8 and
        all(item['sequential'] for item in summary['intro_actor_streams']))
    perf = (results / 'new_game/perf.log').read_text(errors='replace')
    # Verify the real menu/setup path, not the smoke session shortcut.
    exit_frame = perf.find('name=setup.exit.frame')
    loading = perf.find('name=loading.presented')
    session_load = perf.find('name=assets.begin', loading)
    ready = perf.find('name=assets.ready', session_load)
    summary['new_game_loading_order_passed'] = 0 <= exit_frame < loading < session_load < ready
    summary['new_game_exit_has_no_resource_reads'] = (
        exit_frame >= 0 and loading > exit_frame and
        'RESOURCE ' not in perf[exit_frame:loading])
    times = [int(value) for value in re.findall(r't_ms=(\d+)', perf)]
    summary['profile_clock_passed'] = bool(times) and max(times) < args.timeout * 1000
    console = (results / 'new_game/console.log').read_text(errors='replace')
    summary['new_game_reached_game'] = bool(re.search(r'SDL smoke: frames=360 scene=4 ', console))
    summary['oom_killed'] = bool((results / 'oom.log').read_text(errors='replace').strip())
    (out / 'validation.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    required = ['PRESENT_PIXELS_EXIT=0', 'BLIT_PIXELS_EXIT=0', 'PROBE_EXIT=0', 'SCOPE_EXIT=0', 'FLC_PLAYBACK_EXIT=0', 'PROPERTY_FLASH_EXIT=0', 'MENU_EXIT=0',
                'NEW_GAME_EXIT=0', 'GAME_EXIT=0', 'REALTIME_AFTER_EXIT=0',
                'REALTIME_GAME_AFTER_EXIT=0', 'REALTIME_INTRO_AFTER_EXIT=0', 'VM_TEST_END']
    if args.baseline_binary:
        required.append('REALTIME_BEFORE_EXIT=0')
    if summary['oom_killed'] or not summary['real_clock_game_reached_map'] or not all(marker in serial for marker in required):
        raise RuntimeError('QEMU validation did not pass; inspect serial.log')
    if not all(summary[key] for key in ('new_game_loading_order_passed',
            'new_game_reached_game', 'new_game_exit_has_no_resource_reads', 'profile_clock_passed',
            'real_clock_intro_completed', 'intro_no_skips_or_redecode')):
        raise RuntimeError('new-game transition/loading regression failed; inspect new_game/perf.log')
    print('QEMU validation completed; inspect extracted/results screenshots and memory logs')


if __name__ == '__main__':
    main()
