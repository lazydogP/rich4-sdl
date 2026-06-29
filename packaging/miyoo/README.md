# Miyoo Mini / Mini Plus

在项目根目录运行：

```powershell
python scripts/make_minimal.py setup-miyoo --progress
python scripts/make_minimal.py miyoo --progress
python scripts/make_minimal.py package-miyoo --progress
python scripts/make_minimal.py test-miyoo-input
python scripts/make_minimal.py font-bake-miyoo
python scripts/make_minimal.py test-miyoo-qemu MIYOO_QEMU_MEMORY_MB=256 --progress
```

Windows 下使用 Python 3.11 或更新版本。先按项目根 README 准备本地
GNU Make；Mini 工具链由项目自己的 `scripts/setup_miyoo_toolchain.py`
下载、校验并解包，所有下载缓存、编译器、sysroot 和验证工具均在
`.local/miyoo/` 内，不读取其他工程目录，也不从 PATH 查找 ARM 工具。
`miyoo`、`package-miyoo`、`test-miyoo-qemu` 会自动准备各自需要的工具；
重复运行复用已完成的安装。打包和 QEMU 共用项目内的 GCC strip。
ARMv7 Cortex-A7、NEON、hard-float，使用 Mini 的 glibc 头文件和 SDL 1.2。
编译对象位于 `build/miyoo-ttf`，带调试信息的程序位于 `bin/miyoo/rich4_sdl`，
打包与验证产物位于 `build/miyoo`。

可提前准备完整环境或离线复用下载缓存：

```powershell
python scripts/setup_miyoo_toolchain.py --package --qemu
python scripts/make_minimal.py miyoo MIYOO_SETUP_ARGS=--offline --progress
python scripts/make_minimal.py test-miyoo-toolchain
```

固定版本、下载地址、SHA-256 和安装探针位于
`scripts/miyoo_toolchain.lock.json`。离线机器需保留 `.local/miyoo`，或将清单中
原始下载文件按 `filename` 放入 `.local/miyoo/cache/` 后运行 `--offline`。
缓存校验失败会明确报错；删除该缓存文件后可重新下载。安装完成标记仅在解包和
文件检查成功后写入 `.local/miyoo/.installed/`。这些本地文件由现有 `.gitignore`
排除，不提交大体积二进制工具链。

| 本地目录 | 固定来源与用途 |
| --- | --- |
| `gcc` | [Arm GNU-A 10.3-2021.07](https://developer.arm.com/downloads/-/gnu-a) 的 Windows ARM hard-float 编译器 |
| `sysroot` | [Mini Buildroot v0.0.3](https://github.com/shauninman/miyoomini-toolchain-buildroot/releases/tag/v0.0.3) 的 glibc、SDL 1.2、mixer 和 TTF 头文件/库 |
| `ffmpeg` | [Gyan FFmpeg 8.1.2 essentials](https://www.gyan.dev/ffmpeg/builds/)；完整打包时转换音乐 |
| `qemu`、`7zip` | [QEMU Windows 20260729](https://qemu.weilnetz.de/w64/)、[7-Zip 25.01](https://github.com/ip7z/7zip/releases/tag/25.01)；ARM 验证与证据提取 |
| `platform-tools` | Google Platform-Tools 37.0.1 的 mke2fs；创建验证磁盘 |
| `vm` | Debian ARM 内核 6.12.111 和静态 BusyBox 1.38.0；内核包同时提供匹配模块 |

sysroot 中的库链接在 Windows 上展开为项目内普通文件，不要求管理员权限。
可选工具从下载包解出到本地目录，不运行系统安装器，不修改系统 PATH。

打包读取本工作区 `assets/original` 的七个 MKF 和预烘焙中文字库，
输出 `bin/miyoo/Rich4-MiyooMini.zip`。解压，将 `App` 合并到 SD 卡根目录，
在 Apps 启动“大富翁4”。包内程序及 SDL_mixer 已剥离调试符号；系统提供
SDL 和 glibc。Mini SDL_mixer 不支持 OGG，打包时通过项目内 ffmpeg 转成
22050Hz 立体声 WAV，使用现有音乐加载器的 WAV 路径。

CI 程序更新包使用设备自带运行库，不要求先安装完整个人包的 `lib/`。
启动器将应用 `lib/`、`/mnt/SDCARD/miyoo/lib`、`/config/lib`、`/lib` 加入搜索路径。
2026-10-27 在 Onion 设备、glibc 2.28 上，仅用设备自带的 SDL、SDL_mixer、SDL_ttf、
FreeType 和 libgcc_s 完成菜单、2000 帧游戏场景、TTF 中文绘制和音频退出测试；
无需额外补库。120 帧游戏对照与个人包运行库的 framebuffer 和序列摘要相同。
该结论对应这台设备的库版本，其他固件需另行确认。

完整包、`--update-only` 增量包和 CI 发布更新包均包含本目录的
`config.json` 与 `icon.png`，安装到 `App/Rich4/`。
`config.json` 来自提供的 `G:/App/Rich4/config.json`，按原文件复制，
Apps 菜单通过其中的 `icon` 字段读取 64×64 RGBA 透明图标。
项目根目录的 [`logo.png`](../../logo.png) 是参考原版 `assets/original/Rich4.exe` 的 32×32 图标，
使用内置 imagegen 重绘的向量风格透明 PNG；`icon.png` 由它以 Lanczos 缩小得到。
大图用于项目 README 展示，不放入设备安装包。
更新 Apps 菜单配置不涉及游戏内配置或存档。

| 按键 | 操作 |
| --- | --- |
| 方向键 | 移动虚拟光标 |
| A | 鼠标左键按下/松开，选择光标所在位置 |
| B | 鼠标右键按下/松开，返回/取消 |
| X | D：掷骰子 |
| Y / Start | Enter：确认 |
| L1 / R1 | Page Up / Page Down：帮助和信息页翻页 |
| L2 | C：默认卡片快捷键 |
| R2 | W：默认道具快捷键 |
| Select | Tab：切换视图 |
| Menu | Esc：返回/取消、解除电脑托管 |

物理键码见本项目 `packaging/miyoo/input.toml` 与
`src/platform/sdl1/miyoo_input.c`。A/B 保留原版鼠标
回调的按下、松开时机（例如只接受鼠标的彩票/小地图面板）。映射位于
`src/platform/sdl1/miyoo_input.c`，仅 Mini 构建在事件入口应用。
快捷键依据原版表 `0x47EDC2` 和现有 SDL 输入实现；修改游戏快捷键后，
相应按钮的含义随之改变。

Mini 默认将显示画面旋转 180 度，逻辑坐标仍为 640×480。若 OS 的 SDL
已经处理旋转，设置 `RICH4_MIYOO_ROTATE=0`。存档及配置保存在应用目录，
启动环境、参数、PID、退出码保存在 `logs/launch.log`，程序标准输出和错误
保存在 `logs/console.log`。上次启动记录保留为 `*.previous.log`。设置
`RICH4_LOG_MEMORY=1` 可每秒记录 `/proc` 的 RSS、HWM、VmSwap、缺页计数、
系统可用内存和换入换出计数到 `logs/memory.log`。启动器会将终止信号转发给游戏进程。

排查卡顿时，在 SD 卡 `App/Rich4/debug.env` 中写入以下两行，再正常启动：

```sh
RICH4_PROFILE=1
RICH4_LOG_MEMORY=1
```

耗时写入 `logs/perf.log`：`STAGE` 是 loading、各档案/地图/角色加载及新局初始化；
`FRAMES` 每秒汇总输入、更新、绘图、提交屏幕和等待；`SLOW_FRAME` 默认记录
超过 50 ms 的帧，每个汇总周期最多八条。`RICH4_PROFILE_SLOW_MS=100` 可调阈值。
`RICH4_PROFILE=2` 额外列出每个资源的档案、索引、字节数和读取/解压总耗时，
可定位重复加载或特别慢的资源。`RENDER_PART` 分别统计视图准备、开局演出、
地面、角色/物件和界面叠加。使用系统单调墙钟，独立于 SDL 初始化和测试模拟时钟。
日志缓冲写入，阶段结束/每秒刷盘，最多 20000 行；诊断默认关闭。
排查后删除 `debug.env`，即可关闭诊断。带符号的 ARM 程序保留在
`bin/miyoo/rich4_sdl`，包中为剥离符号后的同一构建。
将日志复制回电脑，可用 `python scripts/summarize_miyoo_profile.py perf.log`
汇总档案耗时、最慢资源、重复读取和各场景绘图开销；`--json report.json`
保存结构化结果。

选人 OK 后的退场继续持有选人资源；退场结束后先提交 loading，再读取游戏
资源。loading 覆盖实际资源加载和初始化时间。

QEMU 使用 `.local/miyoo` 中的 Cortex-A7 双核模拟器、Linux 内核和 Mini sysroot，
关闭交换空间，资源放在虚拟 ext4 磁盘，使用 SDL dummy 视频/音频驱动。
按键注入库和字库测试程序仅位于测试环境，正式安装包不包含它们。
结果保存在 `build/miyoo/qemu<N>/validation.json`，截图及日志在其
`extracted/results` 子目录。

运行时按标题、设置、选人、游戏场景持有资源。标题只保留约 1.2 MB
解码缓存；进入游戏后加载当前地图及实际角色，退出或更换场景时释放旧包。
资源通过显式持有/释放计数保护正在使用的视图；完整库存加载仅用于资源审计。
地图的小型路线表保留用于跨地图存档校验。

256 MB、无交换空间的 QEMU 验证通过：标题/设置 80 帧，地图场景 600 帧，
15 个物理按键的 30 组按下/松开映射，中文图集渲染，三个循环的场景/地图/
角色切换。资源测试还按档案大小选取占用最大的四个角色，验证四角色资源包。
进程峰值：标题/设置约 31 MiB，双人地图约 143 MiB，资源切换测试约
121 MiB。地图测试中系统可用内存最低约 68 MiB，无 OOM。
这是本次场景范围的验证；QEMU 不验证真机显示驱动、OSS 音频或运行性能。

性能与内存验收以实机为准：2026-10-15 日志中 Linux MemTotal 为
100.94 MiB，启动可用 77.64 MiB，系统有约 128 MiB swap；开局资源缓存
123.58 MiB，加载 21.35 秒，最大系统 swap 使用量 83.95 MiB。
内存按完整流程的 VmHWM、MemAvailable、VmSwap 与换页停顿验收，不再固定
要求 60/16 MiB；当前演出需要的资源可驻留，避免为了压低 RSS 反复解压。
256 MB QEMU 保留为行为与像素回归；内存压力辅测使用 128 MB、关闭 swap。
这些是待达成的目标，完整优先级和原版计时依据见
[实机内存与性能目标](../../docs/miyoo-memory-budget.md)。

绘图优化后的相同 256 MB QEMU 回放：600 帧地图的地面平均
46.97 → 2.25 ms，角色/物件 25.96 → 5.64 ms，完整绘图
81.93 → 14.53 ms；134 帧开局 FLC 为 87.43 → 20.29 ms。
双人地图 RSS 峰值 143.4 → 144.4 MiB，四人新局 148.2 → 147.6 MiB，
没有 OOM。1040 帧回放摘要和 66 张截图保持一致。
FLC 仅缓存当前差分解码状态，最多八个流及 8 MiB 像素缓冲；地面缓存
约 568 KiB，在资源释放前清理。原有投影、取色和透明效果不变。

事件/面板按需加载版本：113 项面板、Data 事件、角色动作和所选玩家的开局
FLC 在使用时取得资源，连续一帧未再使用后归还；活动计时查询与绘制共用资源。
113 项面板均验证了释放后重载，并完成 324 次全屏像素比较。
交互帧只等待输入或动画期限的剩余时间，绘图超时不追加固定 16 ms；
地图动作和各类 FLC 继续遵守原版各自周期。确定性 smoke 时钟保持原样，
另用无 smoke 参数的真实时钟场景验证等待。

128 MB、无交换 QEMU 已覆盖标题、四人新局、600 帧地图、面板逐项重载和
场景往返；旧版的新局和地图在相同内存下 OOM。完整对照保存到
`build/miyoo/demand-comparison.json`。旧版无法完成的耗时与 RSS 不作估算，
其完成的 256 MB 结果仅作单独标注的画面/耗时参考。

```powershell
python scripts/make_minimal.py test-render-equivalence
python scripts/compare_miyoo_render.py build/miyoo/render-before build/miyoo/qemu256 --output build/miyoo/render-comparison.json
```


2026-10-16 的 FLC 播放修复包为 `bin/miyoo/Rich4-MiyooMini-flc.zip`。
逐帧计时、末帧停留、普通 FLC 音效和新闻／命运语音的调用顺序见
[原版 FLC 证据](../../docs/evidence/450ced-45144f-flc.md)。
`test-flc-regression` 的 18 项检查、全量 unit、资源和架构检查通过；
105 个 FLC、3099 帧在 ARM QEMU 中顺序解码通过。四人开局不跳过，
八段角色动画自然结束，帧号连续且每次只解码一帧。128 MB、无交换的
已测游戏场景峰值 RSS 为 55.93 MiB，最低采样可用内存为 29.42 MiB。
事件 Data[508] 的同机查找参考从 68 次读取／1144 ms 改为 1 次／16 ms；
这是单项查找对照，测试先运行旧扫描，不能据此推断整机或实机提速倍数。
最终报告为 `build/miyoo/flc-playback-validation.json`，包内 ELF 的加载段
与 QEMU 受测 ELF 一致；安装包未包含 QA 程序、活动 debug.env 或运行日志。
`RICH4_PROFILE=2` 的 perf.log 增加 FLC 资源索引、帧号与实际解码帧数。

当前地产闪烁与首次加载优化包为 `bin/miyoo/Rich4-MiyooMini-stalls.zip`。
地产闪烁复用一次生成的遮挡掩码和源图，16 帧分别显示后等待 30 ms，
结束保持 400 ms；原版依据是 `0x409B18(1)`、`0x451985` 和 `0x4554FC`。
本机用 32 块地产、两个遮挡角色、8 个方向进行 128 次全屏像素对照；
ARM QEMU 用 8 块地产对照每个方向的正负亮度掩码，再校验全部 128 帧缓存输出。
当前安装包保留全部七个原始 MKF，压缩方式、头部、索引和文件内容均不改变。
打包和 QEMU 均使用逐字节复制，并校验每个文件的 SHA-256。
常用 HUD 和所选角色首次落地资源仍在 loading 界面准备；其他资源按需读取、
解压并在使用结束后释放。MKF 转换步骤及工具已移除。
当前原始 MKF 版本的验证证据位于 `build/miyoo/qemu-stalls128-original`，
对应发布报告为 `build/miyoo/stalls-original-release-validation.json`。

`RICH4_PROFILE=2` 增加 `SPAN` 分段：更新分派、回合开始、移动、骰子、
落地、自动存档、音乐和音频事件；语音分为 `speech.stop/free/read/convert/play/total`。
语音记录每次调用，其他分段记录至少 2 ms 的调用；级别 1 只记录至少 50 ms。
分段可能嵌套，不能相加估计整帧；汇总工具列出最慢分段和语音资源号。

`RICH4_PROFILE=1` 或 `2` 同时提供 `DETAIL` 记录，每五秒、场景切换和退出时
汇总调用次数、总耗时、平均值及最大值，避免逐帧输出增加 SD 卡写入：

- `group=present`：`lock`、`convert_rotate`、`unlock`、`debug_capture`、
  `flip`、`cursor`、`audio_after_flip`。`flip` 包含 SDL 驱动内部等待；
  不要将其解释为纯 CPU 转色耗时。
- `group=ui`：`toolbar`、`hud`、`calendar`、`status`、`events`、`save_panel`。
  这些记录属于原有 `render_ms`，其中主要地图 UI 属于 `ui_overlays`。
- `python scripts/summarize_miyoo_profile.py <perf.log>` 汇总为
  `frame_details`，保留原有 `FRAMES`、`RENDER_PART` 和 `SPAN` 口径。
  子项不能再与父项相加，计时分辨率为 1 ms。

Mini 的标准 RGB565 图块和 RGB888 提交表面使用 NEON 每批八像素转换，
非标准格式保留原转换路径。RGB555 的绿色取整、透明零像素、显示低位补零、
180 度旋转、裁剪和绘制次序保持一致；不改变动画时钟和 SDL_Flip 调用频率。
布局与取色依据为 `0x415D31`、`0x415F69`、`0x4169BC`、`0x455B3A`，见
[主界面布局](../../docs/evidence/417e26-window-layouts.md)与
[取色和播放计时](../../docs/evidence/render-cache-and-flc-state.md)。

验证入口：`python scripts/make_minimal.py test-present-pixels test-render-equivalence
test-hud-layout test-frame-pacing`；ARM QEMU 也运行全部 65536 色值、透明零、
向量尾部、裁剪、正向与 180 度提交测试。QEMU dummy 驱动的耗时不代表真机帧率。


Miyoo 的 TTF 版本默认选择 SDL_ttf/FreeType，并一起打包原有 CJK 字体
`assets/fonts/rich4.ttc`。在 `debug.env` 写入 `RICH4_TEXT_BACKEND=atlas`，
重启即可切回原有预烘焙字库；`ttf` 为默认值。`launch.log` 记录选择，
`console.log` 的 `TEXT_BACKEND` 记录初始化结果。TTF 会产生与 atlas 不同的
文字像素和度量；非文字画面、绘制次序和游戏规则不变。
`DETAIL group=text name=render` 包含首次字体初始化与文字绘制，嵌套在 UI
或其他绘制分段中，不能与它们相加。构建使用独立的 `build/miyoo-ttf` 目录，
避免复用未启用 TTF 时的目标文件；普通 handheld 仍使用 atlas。

自动存档的固定宽度小端编码对连续基本类型数组批量复制，CRC32 使用同多项式
的不可变查找表。存档版本和字段顺序不变，仍然先写临时文件、`fflush`/`fsync`
（Windows 为 `_commit`）、关闭文件，成功替换后才返回成功。未改为异步确认，
未减少存档次数；SD 卡同步写入本身仍可能产生停顿。`RICH4_PROFILE=2` 记录
`SPAN group=save` 的 `encode`、`crc`、`write`、`flush`、`replace`，包括零毫秒
分段，便于定位真实设备的剩余写入停顿。

事件缓存仅保留不可变资源数据，最多 4 MiB、64 项，使用最近释放顺序淘汰。
事件结束仍清空公开资源槽和 FLC 播放状态，场景销毁时释放全部缓存；开局 Jump
和角色动作资源不进入此缓存。Mini 在已有 loading 界面预热日志中耗时最高的
Panel[63/78/79/80] 与 Data[553]，占用同一个预算，不固定驻留。后续资源需求
可能使它们被淘汰。原始 MKF 和解码算法保持不变。

增量打包命令：

```powershell
python scripts/make_minimal.py package-miyoo "MIYOO_PACKAGE_ARGS=--update-only --output bin/miyoo/Rich4-MiyooMini-save-event-ttf-update.zip"
```

增量包包含程序、启动脚本、两种字体资源及 TTF 的四项库依赖，不包含存档、
游戏配置、音乐或 MKF。量化结果与验证状态见
`build/miyoo/save-event-ttf-validation.json`；ARM/QEMU 的 CPU、dummy 显示与
内存结果不代表 Miyoo 真机 SD 卡耗时或帧率。

FreeType 随包使用设备 SD 卡原有库的副本，仅要求 GLIBC_2.4；SDL_ttf 所需
全部 FT_* 符号已核对。`scripts/miyoo_ttf_runtime.py` 为打包和 QEMU 提供同一
依赖清单，避免验证使用一种库、安装包使用另一种库。

本次发布测量（ARM/QEMU，128 MB，无交换，atlas 与 TTF 分别独立运行）：
600 帧地图、7152 次文字绘制，atlas 累计 256 ms，TTF 累计 465 ms；包含
首次字体加载。整帧绘图平均 7.22 → 7.31 ms，地图峰值 RSS 53.37 → 46.93 MiB。
TTF 四人新局峰值 56.95 MiB，最低采样可用内存 24.96 MiB。TTF 字体与四项
依赖新增磁盘占用 27.0 MiB；增量包 14.78 MiB。字体会改变文字像素和度量，
这些 QEMU 数据不能替代真机显示驱动、SD 卡及音频测试。

同一份 459340 字节真机存档在 ARM 中各写十次，新旧编码输出逐字节一致；
平均进程 CPU 耗时 59.429 → 19.697 ms。打开临时文件失败和非法槽位测试均
保持旧存档不变；损坏 CRC、截断文件及旧版本回放检查通过。磁盘同步仍保留，
本次 CPU 数字不代表真机端到端写入延迟。

最终 atlas 回放的 1040 帧摘要、66 张 BMP 与优化前一致；105 个 FLC、3099 帧
及四人开局的完整时序验证通过。打包 ELF 的加载代码/数据（忽略 strip 修改的
节表头字段）和全部 TTF 库字节，与两个最终 QEMU 受测负载相符。

```powershell
python scripts/compare_miyoo_text.py build/miyoo/stalls-atlas-release128 build/miyoo/stalls-ttf-release128 --baseline build/miyoo/ui-present-after --save-benchmark build/miyoo/stalls-atlas-final/extracted/results/save_benchmark.log --package bin/miyoo/Rich4-MiyooMini-save-event-ttf-update.zip --output build/miyoo/save-event-ttf-validation.json
```
