# macOS 开发构建与验收

目标为 Apple Silicon、macOS 14 起的自包含 `Rich4.app`。本实现提供开发产物；人工试听、干净系统和真实设备验收完成前，不向 GitHub Release 发布 macOS 包。

## 构建

在 macOS Apple Silicon、已安装 Xcode Command Line Tools 的环境执行：

```sh
python3 scripts/make_minimal.py setup-macos --progress
python3 scripts/make_minimal.py macos --progress
python3 scripts/make_minimal.py package-macos check-macos-package --progress
```

全部工具、下载与源码构建在 `.local/macos/`，不安装 Homebrew 或修改系统依赖。锁定清单 `scripts/macos-dependencies.lock.json` 包含下载校验值、源码版本、补丁和选项。CMake 为固定版本的构建工具；其他项目均从源码构建。SDL_mixer 的唯一源码补丁补齐 Vorbis API 的 `const char *`，不改变音频行为。SDL_ttf 的旧配置接口由构建脚本生成的 `freetype-config` 提供固定前缀；它只链接锁定的 FreeType。SDL_mixer 只启用 WAV 与静态链接接口的 OGG 解码（Vorbis/OGG 库仍为随包动态库）。

架构与部署版本统一为 `arm64` / `14.0`。编译器、SDK、依赖锁、构建配方和参数参与构建目录指纹。原生 helper 可以先独立编译：`python3 scripts/make_minimal.py macos-native`。下载缓存完整后，`setup-macos MACOS_SETUP_ARGS=--offline` 支持离线构建。

输出在 `build/macos/arm64/Rich4.app` 和 `build/release/Rich4-macOS-arm64-development.zip`。打包从明确列出的程序、依赖、图标、许可证和配置模板生成，检查所有动态依赖、rpath、架构及本地签名。sdl12-compat 的 macOS `dlopen` 搜索先使用 `@loader_path/libSDL2-2.0.0.dylib`；SDL2 必须与兼容库一起位于 `Contents/Frameworks`，不能只检查 `otool -L` 的直接依赖。

## 首次启动

从 Finder 打开 `.app`，选择完整、已解压的游戏目录。扫描按 7 个 MKF 和 25 个 MIDI 的必需文件集合识别候选，支持大小写无关文件名、中文和空格路径。Steam 安装布局尚未核实，不加入识别。只复制这些 MKF 和原始 MIDI，不复制 EXE、AVI、IDA 数据库。

资源位于 `~/Library/Application Support/Rich4/install-<UUID>/`。导入先在同文件系统的 `.import-<UUID>` 暂存，成功后原子更新 `current.json`。失败、取消或转换错误不替换现有安装。导入锁避免并发提交。重新导入保留用户目录根部的存档、配置、输入映射和字体。成功提交后移除上一个有效资源集。中途进程崩溃留下的暂存目录不会被启动器识别为有效安装。

WAV 缓存位于 `midi-cache/`，键包含源 MIDI、转换器源码版本与输出参数；复用前检查缓存文件完整性。系统更新不自动重建。每曲新建 DLS 合成器；MusicSequence 负责 MIDI 解析和速度图，按事件的采样帧离线渲染，严格在序列长度结束，不追加尾音或淡出。Apple 音色文件不进入应用或缓存。

字体优先使用用户选择并保存到 `fonts/` 的字体，否则用 CoreText 定位系统繁体字体。字体集合 face 索引传给新增的 `--font-face-index N`；SDL_ttf 使用 `TTF_OpenFontIndex`。字体文件不随包分发。游戏工作目录固定到用户数据目录，资源、音乐、字体、输入配置使用绝对路径。`RICH4.CFG`、`RICH4-SDL.CFG`、`saves/`、trace/perf 日志位于用户数据目录；启动器和游戏 stdout/stderr 位于 `~/Library/Logs/Rich4/`。更新只替换 `.app`。

开场沿用 `0x415872` 的 AVI 失败分支和 JUMP 资源回退；首版无 macOS AVI 后端，结局 AVI 不播放。音乐库只生成 `MultiverseJourney`，固定曲目在 `music/track10..26.wav`；没有从其他版本拼接 `Original`。

应用的 `NSHighResolutionCapable=true` 让原生 AppKit 启动器使用 Retina。启动游戏时，启动器单独向子进程设置 `SDL12COMPAT_HIGHDPI=0`，使 SDL 游戏窗口不请求高 DPI drawable；游戏继续使用原有逻辑分辨率与坐标。

## 真实资源检查

```sh
python3 scripts/make_minimal.py test-macos-original --progress
```

默认只读取 `assets/original/V3.11`，可用环境变量 `RICH4_ORIGINAL_DIR` 指定真实资源目录。检查只使用原件和真实资源的临时副本，不创建合成 MIDI/MKF，不修改原件。无资源时明确报告 `NOT EXECUTED`。报告在 `build/macos/arm64/original-validation.json`；转换结果供本地试听，不能上传公开 CI。

覆盖全部 25 曲的实际音频、PCM 参数和逐帧时长、缓存、取消、中文/空格路径、多候选、缺文件、失败回退、导入中途取消、重新导入和源目录移走后的资源可用性。已有用户配置和存档只复制到测试目录，验证重新导入后字节保留。大小写冲突需要大小写敏感文件系统，默认 APFS 上记录未执行。移动后的应用在禁用 Homebrew 搜索路径和 DYLD 覆盖的环境执行菜单/地图 smoke，各运行两次并检查已有 framebuffer/sequence 基线；字体引起的差异必须审查，不自动更新。

## 发布门槛

- 人工试听 8 首 RICH 与 MultiverseJourney 的对应关系、代表性固定曲目。
- 真实资源临时副本的不可读、磁盘不足、转换中断与失败重试。
- macOS 14 和当前系统：干净用户、无 Homebrew、Finder 导入、移动应用、更新及二次启动。
- 中文文本、菜单/地图/场景音乐、语音音效、音乐开关和音乐库切换、存读档、Retina/外屏光标。
- Developer ID、Hardened Runtime、公证、stapling；当前构建只执行本地 ad-hoc 签名。

全部人工门槛完成后，将 `scripts/sign_macos.py` 中列出的六个验收键与实际记录对应，在验收 JSON 中逐项设为 `true`。配置钥匙串中的 Developer ID 身份和 notarytool profile 后执行：

```sh
MACOS_SIGN_IDENTITY='Developer ID Application: …' MACOS_NOTARY_PROFILE='…' \
python3 scripts/make_minimal.py release-macos MACOS_ACCEPTANCE_FILE=/absolute/path/to/acceptance.json
```

发布脚本拒绝缺少验收或凭据的请求，依次签署嵌套 Mach-O 与应用，启用 Hardened Runtime，提交公证、staple、验证 Gatekeeper 后才生成正式命名的 ZIP。这个流程尚未用发布凭据执行，公开 CI 不自动调用它。

公开 CI 只构建并检查包结构和签名，记录真实资源验收未执行。macOS 14 GitHub 托管镜像已于 2026-11-02 退役，公开矩阵使用 macOS 26 arm64，最低系统运行验收由本地或私有 macOS 14 runner 完成。依据：[GitHub runner images](https://github.com/actions/runner-images)。Apple 合成器和离线渲染接口依据：[DLSMusicDevice](https://developer.apple.com/documentation/audiotoolbox/kaudiounitsubtype_dlssynth)、[OfflineRender](https://developer.apple.com/documentation/audiotoolbox/kaudiounitproperty_offlinerender)。
