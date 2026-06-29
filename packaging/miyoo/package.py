#!/usr/bin/env python3
"""Stage a personal Mini package using the workspace's original assets."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import zipfile
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from stage_miyoo_original_assets import DEFAULT_DATA_DIR, stage_original_assets
from miyoo_ttf_runtime import ttf_runtime_libraries
from miyoo_toolchain import SYSROOT, STRIP, FFMPEG, require_build_tools, require_files

ROOT = Path(__file__).resolve().parents[2]
TEMPLATES = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, default=DEFAULT_DATA_DIR)
    parser.add_argument('--update-only', action='store_true', help='package only the executable, launcher, app metadata/artwork, font backends and their libraries')
    parser.add_argument('--output', type=Path, default=ROOT / 'bin/miyoo/Rich4-MiyooMini.zip')
    args = parser.parse_args()
    try:
        require_build_tools()
    except FileNotFoundError as error:
        parser.error(str(error))
    binary = ROOT / 'bin/miyoo/rich4_sdl'
    if not binary.is_file():
        parser.error('build the miyoo target first')
    stage = ROOT / 'build/miyoo/package'
    app = stage / 'App/Rich4'
    for rel in ('lib', 'assets/original', 'assets/fonts', 'assets/music', 'config'):
        (app / rel).mkdir(parents=True, exist_ok=True)
    for name in ('Data.mkf', 'Map.mkf', 'Panel.mkf', 'Effect.mkf', 'Speaking.mkf', 'Jump.mkf', 'Help.mkf'):
        source = args.data_dir / name
        if not source.is_file():
            parser.error(f'missing original asset: {source}')
    mkf_manifest = stage_original_assets(args.data_dir, app / 'assets/original')
    shutil.copyfile(binary, app / 'rich4_sdl')
    sdk = SYSROOT
    shutil.copyfile(sdk / 'usr/lib/libSDL_mixer-1.2.so.0', app / 'lib/libSDL_mixer-1.2.so.0')
    for name, source in ttf_runtime_libraries(sdk).items():
        shutil.copyfile(source, app / 'lib' / name)
    shutil.copyfile(ROOT / 'assets/fonts/rich4.ttc', app / 'assets/fonts/rich4.ttc')
    # glibc loads this at pthread_cancel, even without an ELF NEEDED entry.
    shutil.copyfile(sdk / 'lib/libgcc_s.so.1', app / 'lib/libgcc_s.so.1')
    strip = STRIP
    subprocess.run([str(strip), '--strip-unneeded', str(app / 'rich4_sdl'),
                    str(app / 'lib/libSDL_mixer-1.2.so.0'), str(app / 'lib/libgcc_s.so.1')], check=True)
    shutil.copyfile(ROOT / 'assets/fonts/rich4_font.r4atlas', app / 'assets/fonts/rich4_font.r4atlas')
    shutil.copyfile(TEMPLATES / 'input.toml', app / 'config/input.toml')
    shutil.copyfile(ROOT / 'RICH4-SDL.CFG.example', app / 'RICH4-SDL.CFG.example')
    (app / 'launch.sh').write_text((TEMPLATES / 'launch.sh').read_text(encoding='utf-8'),
                                 encoding='utf-8', newline='\n')
    (app / 'debug.env.example').write_text(
        'RICH4_PROFILE=2\nRICH4_LOG_MEMORY=1\nRICH4_TEXT_BACKEND=ttf\n', encoding='ascii', newline='\n')
    for name in ('config.json', 'icon.png'):
        shutil.copyfile(TEMPLATES / name, app / name)
    # The full-size logo belongs to the project README, not the device package.
    # Remove artwork left by an earlier staging run before archiving this tree.
    (app / 'logo.png').unlink(missing_ok=True)
    # This Mini SDL_mixer explicitly lacks Ogg Vorbis. Use the existing
    # WAV fallback paths in the game's music loader, without changing them.
    ffmpeg = str(FFMPEG)
    tracks = sorted((ROOT / 'assets/music').rglob('*.ogg'))
    if tracks:
        try:
            require_files((FFMPEG,), ' --package')
        except FileNotFoundError as error:
            parser.error(str(error))
    for source in tracks:
        dest = app / 'assets/music' / source.relative_to(ROOT / 'assets/music').with_suffix('.wav')
        dest.parent.mkdir(parents=True, exist_ok=True)
        if not dest.exists() or dest.stat().st_mtime < source.stat().st_mtime:
            subprocess.run([ffmpeg, '-nostdin', '-v', 'error', '-y', '-i', str(source),
                '-ar', '22050', '-ac', '2', '-c:a', 'pcm_s16le', str(dest)], check=True)
    for source in (ROOT / 'assets/music').rglob('*.wav'):
        dest = app / 'assets/music' / source.relative_to(ROOT / 'assets/music')
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
    readme = '''大富翁4 · Miyoo Mini / Mini Plus 测试包
解压，将 App 文件夹复制到 SD 卡根目录，在 Apps 中启动“大富翁4”。
方向键：移动光标。A：鼠标左键（按下/松开），选择光标所在位置。
B：鼠标右键，返回/取消。X：D 键，掷骰子。Y / START：Enter，确认。
L1 / R1：Page Up / Page Down，帮助及信息页翻页。
L2：C 键（默认卡片快捷键）。R2：W 键（默认道具快捷键）。
SELECT：Tab，切换视图。MENU：Esc，返回/取消、解除电脑托管。
原版快捷键可在游戏设置中改动；A/B 可操作仅接受鼠标的面板。
存档及设置在 App/Rich4 中，启动日志为 logs/console.log。
可将 App/Rich4/RICH4-SDL.CFG.example 复制为同目录的 RICH4-SDL.CFG 后修改 SDL 设置。
没有实际配置文件时使用内置默认值；更新时保留已有 RICH4-SDL.CFG。
画面倒置时，将 launch.sh 中 RICH4_MIYOO_ROTATE 的默认值从 1 改成 0。
使用预烘焙中文字库；音乐已转为 22050Hz 立体声 WAV。
使用本工作区 V3.11 资源，作为本机个人测试包。尚未进行真机验证。
资源按场景、当前地图和实际角色加载；事件、面板、角色动作及开局 FLC 按需取得，近期事件原始数据最多保留 4 MiB。
128 MB、无交换空间的 QEMU 已通过启动、中文显示、按键及资源释放验证。
128 MB 无交换 QEMU 验证使用原始 MKF，四人开局的八段角色动画自然播完。
地面复用相同镜头的绘制结果，精灵预映射调色板，FLC 保留差分解码状态逐帧推进。
指定帧的地面、FLC 与面板像素回归通过；播放计时、末帧停留和声音顺序按原版修正。
全部 105 个 FLC、3099 帧顺序解码通过；真实时钟注入停顿后不跳帧、不追赶。
事件只加载目标资源，当前动画的计时查询和绘制共享资源，开局保留所选角色两组 FLC。
普通 FLC 首帧提交后开始音效；新闻和命运先准备图片，再启动文字对应的语音。
地产闪烁先生成遮挡掩码并保存源图，完整显示 16 档亮暗变化；每帧显示后等待 30ms，结束保持 400ms。
包内全部七个 MKF 与原始文件逐字节一致，保留原有压缩、索引和头部；打包不重写 MKF。
设备按需读取和解压；常用 HUD、首次落地和日志中最慢的五项事件资源在 loading 界面准备。
内存按实际峰值、可用量和换页停顿判断，不设固定 60/16 MiB 门槛。
交互帧的绘图和提交计入等待期限，超时后不追加固定 16 ms；动画继续使用原版各自周期。
默认使用 TTF 字体；debug.env 中设置 RICH4_TEXT_BACKEND=atlas 并重启可切回现有字库。
存档批量编码和查表 CRC 不改变格式；保留临时文件、同步落盘及原子替换流程。
RICH4_PROFILE=2 增加 save 的 encode/crc/write/flush/replace 耗时；DETAIL text 统计字体绘制。
未完成全流程或真机验证；QEMU 的 dummy 驱动不验证真机画面方向、声音和性能。
启动环境及退出码：logs/launch.log；程序输出及错误：logs/console.log。
上一次启动日志保留为 *.previous.log。RICH4_LOG_MEMORY=1 开启每秒内存、VmSwap、缺页及换页采样。
选人退场结束后先显示 loading，再加载地图和角色资源，没有额外的固定等待。
排查卡顿：把 App/Rich4/debug.env.example 改名为 debug.env，再启动游戏。
耗时在 logs/perf.log，内存在 logs/memory.log；排查后删除 debug.env。
RICH4_PROFILE=2 可列出每个资源的读取/解压耗时；默认 1 统计慢资源和各阶段。
SPAN 分段计时覆盖更新分派、移动、骰子、落地、自动存档、音乐和音频事件。
语音分别记录 speech.stop/free/read/convert/play/total，包含资源号；嵌套分段耗时不能相加。
'''
    (stage / 'README.txt').write_text(readme, encoding='utf-8-sig')
    if args.update_only:
        readme = '增量更新：将 App 合并覆盖到已有 Rich4 安装。包含 Apps 菜单配置及图标，不包含存档、游戏配置或 MKF。\n' + readme
        (stage / 'README.txt').write_text(readme, encoding='utf-8-sig')
    update_files = {'README.txt', 'App/Rich4/rich4_sdl', 'App/Rich4/launch.sh',
                    'App/Rich4/RICH4-SDL.CFG.example',
                    'App/Rich4/config.json', 'App/Rich4/icon.png',
                    'App/Rich4/debug.env.example', 'App/Rich4/assets/fonts/rich4.ttc',
                    'App/Rich4/assets/fonts/rich4_font.r4atlas'}
    update_files.update('App/Rich4/lib/' + name for name in
                        ('libSDL_ttf-2.0.so.0', 'libfreetype.so.6', 'libpng16.so.16', 'libz.so.1'))
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for source in sorted(stage.rglob('*')):
            if not source.is_file():
                continue
            relative = source.relative_to(stage).as_posix()
            if relative == 'App/Rich4/RICH4-SDL.CFG':
                continue
            if args.update_only and relative not in update_files:
                continue
            entry = zipfile.ZipInfo(relative)
            entry.create_system = 3
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = (0o100755 if source.name in ('launch.sh', 'rich4_sdl') else 0o100644) << 16
            archive.writestr(entry, source.read_bytes())
        assert archive.testzip() is None
    with zipfile.ZipFile(output) as archive:
        for name, expected in ({} if args.update_only else mkf_manifest).items():
            actual = archive.read('App/Rich4/assets/original/' + name)
            assert hashlib.sha256(actual).hexdigest() == expected['source_sha256'], name
    checksum = output.parent / ('SHA256.txt' if output.name == 'Rich4-MiyooMini.zip' else output.name + '.sha256')
    checksum.write_text(
        hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n', encoding='ascii')
    print(f'Miyoo package: {output} ({output.stat().st_size} bytes; {len(tracks)} converted tracks)')


if __name__ == '__main__':
    main()
