# GitHub Actions 自动发布

工作流位于 `.github/workflows/release.yml`，使用现有 Windows 本地工具链脚本和
`scripts/make_minimal.py` 构建，Windows 与 Miyoo 构建运行在 `windows-2022`；macOS Universal 2 开发包在 `macos-26` 构建，PortMaster aarch64 试验包在 `ubuntu-22.04` 构建。

- 推送 `v` 开头、后接数字的标签（例如 `v0.1.0`）自动构建并发布 GitHub Release。
- Actions → Release → Run workflow 手动运行只构建并上传 artifacts，不创建 Release。
- Windows x64 运行 unit/resource 测试，然后构建程序并收集 SDL 运行库。
- Miyoo Mini / Mini Plus 使用固定 ARM 工具链与固定源码构建支持 Tremor Vorbis/MP3 的完整运行库包。
- macOS 构建自包含的 Universal 2（arm64 + x86_64）应用，检查每个 slice 的依赖和部署版本，并运行包校验回归测试。
- PortMaster 使用独立的固定工具链与 glibc sysroot，先检查包隔离和启动器，再构建 aarch64 试验包。
- 全部构建成功后，发布任务下载四个平台的 ZIP 与 `.sha256`，检查附件完整性和 SHA-256 后发布 Release。
- 含 `-` 的标签（例如 `v0.1.0-rc.1`）创建为 prerelease。
- 同标签任务串行执行；重新运行会更新同名附件。首次发布先创建 draft，附件上传完成后公开。

## 发布前准备

先提交工作流、`scripts/package_release.py`、`tests/unit/test_package_release.py`，
以及当前工作区完整的构建依赖。尤其是 Miyoo 相关的 `scripts/build_miyoo.py`、
`scripts/setup_miyoo_toolchain.py`、`scripts/miyoo_toolchain.py`、
`scripts/miyoo_toolchain.lock.json`、`scripts/build_miyoo_mixer.py`、
`scripts/miyoo_mixer.lock.json`、`scripts/miyoo_runtime.py`、`scripts/miyoo_ttf_runtime.py`
和 `packaging/miyoo/launch.sh` 必须已在标签对应的提交中；
`packaging/miyoo/config.json`、`icon.png` 也须随代码提交。
CI 不会读取本机尚未提交的文件。Makefile 引用的源码和架构检查脚本也须一起提交。

仓库需要启用 GitHub Actions，且组织策略允许官方 `actions/*` action 和任务级
`contents: write` 权限。使用自动提供的 `GITHUB_TOKEN`，无需配置个人访问令牌。
构建任务仅有读取权限，只有发布任务有写入 Release 的权限。

将配置合并到默认分支后，可先手动运行检查构建，再发布标签：

```sh
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0
```

## 发布内容

| 附件 | 内容 |
| --- | --- |
| `Rich4-SDL-Windows-x64-<tag>.zip` | Windows 程序、SDL 及工具链运行库、使用说明 |
| `Rich4-SDL-MiyooMini-<tag>.zip` | 程序、启动器、Apps 菜单和图标、输入配置、Tremor SDL_mixer、MP3/libmad、SDL_ttf、FreeType、PNG、zlib、libgcc_s、许可证与使用说明 |
| `Rich4-SDL-macOS-<tag>.zip` | 自包含 Universal 2 开发版 `Rich4.app`、动态库、配置模板和许可证；使用 ad-hoc 签名，尚未 Developer ID 签名或公证 |
| `Rich4-SDL-PortMaster-<tag>.zip` | aarch64 试验程序、启动器、按键配置、SDL 兼容层与运行库、许可证和对应源码 |

每个 ZIP 都附带同名 `.zip.sha256`。ZIP 与校验文件作为独立的原始 artifact 上传，
不会再由 GitHub Actions 包装为外层 ZIP。

Windows、Miyoo 和 PortMaster 的程序目录均附带 `RICH4-SDL.CFG.example`。需要自定义时，复制为同目录的
`RICH4-SDL.CFG` 后修改；没有实际配置时使用内置默认值，升级时保留已有配置。
macOS 的配置模板位于应用资源目录，用户配置由启动器保存在用户数据目录。

发布包使用明确的文件清单，不遍历个人打包目录；不包含 MKF、音乐、字体及字体图集、
存档或个人设置。Windows 使用者需自行放置七个原版 MKF 和 `assets/fonts/rich4.ttc`。
Miyoo 包可作为运行环境的首次安装：自带完整应用运行库和输入配置，使用者补充
原版 MKF 与 CJK 字体即可使用；可选音乐放入 `App/Rich4/assets/music/`，直接使用 OGG。
设备固件仍须提供原生 SDL 1.2 与 glibc 2.28 或更新版本。
CI 不调用读取本机原版资源的 `packaging/miyoo/package.py`。
SDL_mixer 从 `scripts/miyoo_mixer.lock.json` 固定源码构建，libmad、字体库和其余依赖
来自固定 Mini SDK，构建不需要连接个人设备或读取本地私有运行库副本。
`scripts/miyoo_runtime.py` 为个人完整包、CI 和 QEMU 提供同一份依赖清单。
启动器优先使用应用 `lib/`，随后搜索 `/mnt/SDCARD/miyoo/lib`、`/config/lib`、`/lib`。

本地检查或生成同样的包：

```powershell
python scripts/make_minimal.py test-package-release
python scripts/make_minimal.py sdl stage-sdl-runtime --progress
python scripts/package_release.py --platform windows-x64 --version v0.1.0
python scripts/make_minimal.py miyoo --progress
python scripts/package_release.py --platform miyoo-mini --version v0.1.0
```

Windows、Miyoo 和 macOS 产物在 `build/release/`，PortMaster 产物在 `build/portmaster/`；
每个程序 ZIP 旁生成对应的 `.zip.sha256`。
Actions artifacts 保留 14 天，公开 Release 附件独立保存。
macOS 开发包与 PortMaster 试验包同时保存为 CI artifact 和标签 Release 附件。
macOS 仍待真实设备、试听与干净系统验收，当前仅有 ad-hoc 签名；Developer ID 签名与公证
继续使用独立的验收流程，见 [macOS 构建与验收](macos.md)。PortMaster 的设备要求和
真机验收状态见 [PortMaster 试验包](../packaging/portmaster/README.md)。
CI 不提供原版资源，因此不执行真实资源 SDL smoke、QEMU 游戏回放或真机验证。

参考：[GitHub Actions 权限](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#permissions)、
[GitHub CLI Release](https://cli.github.com/manual/gh_release_create)。
