# macOS 开发构建与验收

目标为自包含的 Universal 2（arm64 + x86_64）`Rich4.app`：Intel 最低部署到 OS X 10.10，Apple Silicon 最低部署到 macOS 11。最低版本尚待真机验收。本实现提供开发产物；人工试听、干净系统和真实设备验收完成前，不向 GitHub Release 发布 macOS 包。

## 构建

在 macOS Apple Silicon 或 Intel、已安装支持 Universal 2 的 Xcode Command Line Tools 和 Python 3 的环境执行。构建主机须满足这些工具的系统要求；10.10 是 Intel 产物的运行部署目标。

```sh
python3 scripts/make_minimal.py setup-macos --progress
python3 scripts/make_minimal.py macos --progress
python3 scripts/make_minimal.py package-macos check-macos-package --progress
python3 scripts/make_minimal.py test-macos-package
```

全部工具、下载与源码构建在 `.local/macos/`，不安装 Homebrew 或修改系统依赖。锁定清单 `scripts/macos-dependencies.lock.json` 包含下载校验值、源码版本、补丁和选项。CMake 为固定版本的构建工具；其他项目均从源码构建。SDL_mixer 的源码补丁补齐 Vorbis API 的 `const char *`，并为 macOS 原生 MIDI 的 `Mix_SetMusicPosition` 接入 `MusicSequenceGetBeatsForSeconds`／`MusicPlayerSetTime`，用于原版 `0x454B1A`／`0x454BCC` 临时 BGM 返回时恢复地图曲进度。SDL_ttf 的旧配置接口由构建脚本生成的 `freetype-config` 提供固定前缀；它只链接锁定的 FreeType。SDL_mixer 启用 WAV、OGG 与 macOS 原生 MIDI 后端；原生 MIDI 使用系统 MusicSequence/MusicPlayer，不需要第三方音色库。Vorbis/OGG 库仍为随包动态库。

程序、原生 helper 和全部随包动态库统一编译为 `arm64` + `x86_64`。部署参数使用 `10.10`，Apple Clang 将 arm64 的实际下限设为 `11.0`；依赖锁逐架构记录这两个目标。CMake 依赖使用双架构配置，Autoconf 依赖和应用使用两组 `-arch` 参数，由 Apple 工具链生成 Universal 2 文件。编译器、SDK、依赖锁、构建配方和参数参与构建目录指纹，Universal 缓存位于 `.local/macos/universal/`。原生 helper 可以先独立编译：`python3 scripts/make_minimal.py macos-native`。下载缓存完整后，`setup-macos MACOS_SETUP_ARGS=--offline` 支持离线构建。

应用源码启用 `-Werror=unguarded-availability`，启动器使用 10.10 可用的 AppKit 控件初始化和 NSTask 接口，诊断计时使用 `mach_absolute_time`。原生 Objective-C 源码以 ARC 编译为对象后单独链接 Foundation/libobjc；所用 NSArray/NSDictionary 下标接口从 10.8 可用，不使用 libarclite 的旧系统补丁。`Info.plist` 同时声明整体和逐架构的最低系统版本。

开发签名和 Developer ID 签名均显式包含 SHA-1 与 SHA-256 摘要，包校验要求每个 Intel slice 保留 10.10 所需的 SHA-1 摘要。依据：[Apple 旧版 macOS 签名说明](https://developer.apple.com/forums/thread/673323)。

输出在 `build/macos/universal/Rich4.app`、`build/release/Rich4-SDL-macOS-build-local.zip`
和对应 `.zip.sha256`。CI 用 `build-<run_number>` 或标签替换 `build-local`。打包从明确列出的程序、依赖、图标、许可证和配置模板生成，要求每个程序和动态库包含两种架构，并逐架构检查动态依赖、rpath、部署版本及本地签名。sdl12-compat 的 macOS `dlopen` 搜索先使用 `@loader_path/libSDL2-2.0.0.dylib`；SDL2 必须与兼容库一起位于 `Contents/Frameworks`，不能只检查 `otool -L` 的直接依赖。

`test-macos-package` 在应用的临时副本上检查校验器会拒绝单架构程序、单架构 SDL2、外部依赖和 rpath、任一架构的错误部署版本、错误的 Info.plist 最低系统声明，以及缺少 10.10 签名摘要的动态库。校验器同时支持 Intel 的 `LC_VERSION_MIN_MACOSX` 和 arm64 的 `LC_BUILD_VERSION`。这些检查不需要原版游戏资源。它还分别启动两种架构的游戏 `--help`，确认使用随包 SDL2；主机无法运行某架构时明确跳过该项，Apple Silicon 上的 x86_64 启动检查需要已安装 Rosetta。当前系统和 Rosetta 启动不替代 10.10／11.0 真机验收。

## 首次启动

从 Finder 打开 `.app`，选择完整、已解压的游戏目录。扫描按 7 个 MKF 和 25 个 MIDI 的必需文件集合识别候选，支持大小写无关文件名、中文和空格路径。Steam 安装布局尚未核实，不加入识别。只复制这些 MKF 和原始 MIDI，不复制 EXE、AVI、IDA 数据库。音乐文件复制为 `music/track10..26.mid` 与 `music/MultiverseJourney/track02..09.mid`，可直接播放。

资源位于 `~/Library/Application Support/Rich4/install-<UUID>/`。导入先在同文件系统的 `.import-<UUID>` 暂存，成功后原子更新 `current.json`。失败、取消或转换错误不替换现有安装。导入锁避免并发提交。重新导入保留用户目录根部的存档、配置、输入映射和字体。成功提交后移除上一个有效资源集。中途进程崩溃留下的暂存目录不会被启动器识别为有效安装。

游戏按 OGG → WAV → MID 查找同名曲目，只有可用 MIDI 后端才尝试 MID。启动器直接导入 MIDI，不执行离线转换；原有 WAV 安装仍可使用。独立的 `rich4-midi INPUT.mid OUTPUT.wav --cache-dir DIR` 工具保留为显式离线转换入口，其缓存只在调用时使用，系统更新不自动重建。Apple 系统音色文件不进入应用或导入目录。

字体优先使用用户选择并保存到 `fonts/` 的字体，否则用 CoreText 定位系统繁体字体。字体集合 face 索引传给新增的 `--font-face-index N`；SDL_ttf 使用 `TTF_OpenFontIndex`。字体文件不随包分发。游戏工作目录固定到用户数据目录，资源、音乐、字体、输入配置使用绝对路径。`RICH4.CFG`、`RICH4-SDL.CFG`、`saves/`、trace/perf 日志位于用户数据目录；启动器和游戏 stdout/stderr 位于 `~/Library/Logs/Rich4/`。更新只替换 `.app`。

开场沿用 `0x415872` 的 AVI 失败分支和 JUMP 资源回退；首版无 macOS AVI 后端，结局 AVI 不播放。音乐库只生成 `MultiverseJourney`，固定曲目在 `music/track10..26.mid`；没有从其他版本拼接 `Original`。

应用的 `NSHighResolutionCapable=true` 让原生 AppKit 启动器使用 Retina。启动游戏时，启动器单独向子进程设置 `SDL12COMPAT_HIGHDPI=0`，使 SDL 游戏窗口不请求高 DPI drawable；游戏继续使用原有逻辑分辨率与坐标。

## 真实资源检查

```sh
python3 scripts/make_minimal.py test-macos-original --progress
```

默认只读取 `assets/original/V3.11`，可用环境变量 `RICH4_ORIGINAL_DIR` 指定真实资源目录。检查只使用原件和真实资源的临时副本，不创建合成 MIDI/MKF，不修改原件。无资源时明确报告 `NOT EXECUTED`。报告在 `build/macos/universal/original-validation.json`；转换结果供本地试听，不能上传公开 CI。运行检查使用构建主机的原生架构，Intel 真机运行验收须单独完成。

当前进程能访问系统 Audio Unit 注册表和 Apple DLSMusicDevice 时，覆盖全部 25 曲的原生 MIDI 播放、OGG/WAV/MID 优先级、原始 MIDI 名称、自然结束/循环/停止、进度恢复、离线 PCM 参数和逐帧时长、缓存及取消。Codex 等受限执行沙箱可能看不到 Audio Unit 注册表；此时真实播放、离线渲染和转换导入明确记录为 `NOT EXECUTED`，不能据此判断 macOS 缺少 DLSMusicDevice。取消时保留已有目标并清理临时文件的检查仍执行。完整音频验收应从非沙箱 Terminal 或 Finder 环境运行。其余检查覆盖中文/空格路径、多候选、缺文件、失败回退、导入中途取消、重新导入和源目录移走后的资源可用性。已有用户配置和存档只复制到测试目录，验证重新导入后字节保留。大小写冲突需要大小写敏感文件系统，默认 APFS 上记录未执行。移动后的应用在禁用 Homebrew 搜索路径和 DYLD 覆盖的环境执行菜单/地图 smoke，各运行两次并检查已有 framebuffer/sequence 基线；字体引起的差异必须审查，不自动更新。

## 发布门槛

- 人工试听 8 首 RICH 与 MultiverseJourney 的对应关系、代表性固定曲目。
- 真实资源临时副本的不可读、磁盘不足、转换中断与失败重试。
- Intel 的 OS X 10.10、Apple Silicon 的 macOS 11 以及当前系统：干净用户、无 Homebrew、Finder 导入、移动应用、更新及二次启动。
- 中文文本、菜单/地图/场景音乐、语音音效、音乐开关和音乐库切换、存读档、Retina/外屏光标。
- Developer ID、Hardened Runtime、公证、stapling；当前构建只执行本地 ad-hoc 签名。

全部人工门槛完成后，将 `scripts/sign_macos.py` 中列出的验收键与实际记录对应，在验收 JSON 中逐项设为 `true`；最低系统记录分别为 `macos10_10_intel_clean_finder` 和 `macos11_arm64_clean_finder`。配置钥匙串中的 Developer ID 身份和 notarytool profile 后执行：

```sh
MACOS_SIGN_IDENTITY='Developer ID Application: …' MACOS_NOTARY_PROFILE='…' \
python3 scripts/make_minimal.py release-macos MACOS_ACCEPTANCE_FILE=/absolute/path/to/acceptance.json
```

发布脚本拒绝缺少验收或凭据的请求，依次签署嵌套 Mach-O 与应用，启用 Hardened Runtime，提交公证、staple、验证 Gatekeeper 后才生成正式命名的 ZIP。这个流程尚未用发布凭据执行，公开 CI 不自动调用它。

公开 CI 在 macOS 26 arm64 构建并检查包结构、逐架构部署版本和签名，记录真实资源验收未执行。10.10 Intel 和 11.0 Apple Silicon 的运行验收需由对应系统的本地或私有设备完成。Universal 2 构建依据：[Apple 构建指南](https://developer.apple.com/documentation/apple-silicon/building-a-universal-macos-binary)；SDL2 的架构与旧系统范围依据：[SDL2 macOS 说明](https://wiki.libsdl.org/SDL2/README-macos)。Apple 合成器和离线渲染接口依据：[DLSMusicDevice](https://developer.apple.com/documentation/audiotoolbox/kaudiounitsubtype_dlssynth)、[OfflineRender](https://developer.apple.com/documentation/audiotoolbox/kaudiounitproperty_offlinerender)。
