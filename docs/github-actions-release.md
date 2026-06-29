# GitHub Actions 自动发布

工作流位于 `.github/workflows/release.yml`，使用现有 Windows 本地工具链脚本和
`scripts/make_minimal.py` 构建，两个平台均运行在 `windows-2022`。

- 推送 `v` 开头、后接数字的标签（例如 `v0.1.0`）自动构建并发布 GitHub Release。
- Actions → Release → Run workflow 手动运行只构建并上传 artifacts，不创建 Release。
- Windows x64 运行 unit/resource 测试，然后构建程序并收集 SDL 运行库。
- Miyoo Mini / Mini Plus 使用项目固定版本的 ARM 工具链构建程序更新包。
- 两个平台均成功后，发布任务下载 ZIP 并发布 Release。
- 含 `-` 的标签（例如 `v0.1.0-rc.1`）创建为 prerelease。
- 同标签任务串行执行；重新运行会更新同名附件。首次发布先创建 draft，附件上传完成后公开。

## 发布前准备

先提交工作流、`scripts/package_release.py`、`tests/unit/test_package_release.py`，
以及当前工作区完整的构建依赖。尤其是 Miyoo 相关的 `scripts/build_miyoo.py`、
`scripts/setup_miyoo_toolchain.py`、`scripts/miyoo_toolchain.py`、
`scripts/miyoo_toolchain.lock.json` 和 `packaging/miyoo/launch.sh` 必须已在标签对应的提交中；
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
| `Rich4-Windows-x64-<tag>.zip` | Windows 程序、SDL 及工具链运行库、使用说明 |
| `Rich4-MiyooMini-update-<tag>.zip` | `App/Rich4/rich4_sdl`、`launch.sh`、Apps 菜单 `config.json`、64×64 透明 `icon.png`、更新说明 |

两个平台的程序目录均附带 `RICH4-SDL.CFG.example`。需要自定义时，复制为同目录的
`RICH4-SDL.CFG` 后修改；没有实际配置时使用内置默认值，升级时保留已有配置。

发布包使用明确的文件清单，不遍历个人打包目录；不包含 MKF、音乐、字体及字体图集、
存档或个人设置。Windows 使用者需自行放置七个原版 MKF 和 `assets/fonts/rich4.ttc`。
Miyoo 包用于覆盖已有安装，保留原版数据、字体、输入配置和存档；
它不作为首次安装包，也不调用会读取本机原版资源的 `packaging/miyoo/package.py`。
在已测试的 Onion 设备上，SDL、SDL_mixer、SDL_ttf、FreeType 和 libgcc_s 均由设备提供，自动发布包无需额外携带运行库，也不要求已有 `App/Rich4/lib/`。
启动器明确加入 `/mnt/SDCARD/miyoo/lib`、`/config/lib`、`/lib`，不依赖启动菜单继承的环境变量。
已有的应用 `lib/` 仍优先使用；其他固件的库版本兼容性需单独确认。

本地检查或生成同样的包：

```powershell
python scripts/make_minimal.py test-package-release
python scripts/make_minimal.py sdl stage-sdl-runtime --progress
python scripts/package_release.py --platform windows-x64 --version v0.1.0
python scripts/make_minimal.py miyoo --progress
python scripts/package_release.py --platform miyoo-mini --version v0.1.0
```

产物在 `build/release/`，只生成程序 ZIP，不生成 SHA256 文件。
Actions artifacts 保留 14 天，公开 Release 附件独立保存。
CI 不提供原版资源，因此不执行真实资源 SDL smoke、QEMU 游戏回放或真机验证。

参考：[GitHub Actions 权限](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#permissions)、
[GitHub CLI Release](https://cli.github.com/manual/gh_release_create)。
