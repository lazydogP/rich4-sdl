# rich4-sdl

<p align="center">
  <img src="logo.png" alt="Rich4 SDL：白色顶面带红色 4 的蓝绿色骰子" width="240">
</p>

`rich4-sdl` 是一个使用 C99 与 SDL 1.2 重实现《大富翁 4》运行时的项目，面向桌面系统与 Linux 开源掌机。项目读取用户自行提供的原版资源，以可测试、可追溯、平台与领域逻辑分离为主要工程目标；它不运行原版 x86 代码，也不追求二进制级复刻。

当前已有地产、设施、股票、卡片、道具和特殊事件核心链路，以及真实 gmap0 终局回放和 SDL smoke。M3 尚缺出售挂牌交易、部分 AI/资金处理、整局磁盘存读档与最终验收；以实际代码和 IDA 核对后的 [M3 进度](docs/m3-core-gameplay-original-evidence-plan.md) 为准。逆向与测试细节见 [证据目录](docs/evidence/README.md)，重构目标见 [精简重构计划](docs/llm-context-simplification-lean.md)。

## 已有能力

- MKF 读取、解压、缓存和单一所有权管理。
- SPR/SMP 与 Autodesk Animator FLC 解析、连续播放及 RGB565 合成。
- 原版地图数据、路线、对象引用和资源编号目录。
- 640×480 SDL1 软件 framebuffer、虚拟光标和窗口坐标适配。
- Menu、NewGame、Intro、Game、Options 场景机。
- 统一 `GameCommand`、路线事件队列、玩家状态、回合轮转和胜利候选规则。
- ViewModel、地图 visible-list/sort、owner palette 和合成 framebuffer golden。
- Windows 本地 WinLibs/SDL1 工具链；资源工具和 core tests 不依赖 SDL。

## 已知BUG

- 保险公司不正常
- 新闻/命运选中了错误的目标，以及错误的演出
- 企鹅挖宝直接展示了地面目标
- 还有很多……

## 构建与测试

自动化开发和 AI agent 使用 `python scripts/make_minimal.py <target>` 代替直接调用 `make`。长任务会显示低频进度；也可显式加入 `--progress`。普通模式默认采用增量构建，并拒绝意外的 `-B/--always-make`；只有确认需要完整重建时才使用 `--allow-force-rebuild`。需要解析 Make dry-run 的工具应显式传入 `--raw-output`。

例如：

```sh
python scripts/make_minimal.py --progress test-unit
```

`test-original-data` 在 `assets/original/Data.mkf` 不存在时会跳过商业资源扫描，其余快速测试只使用合成数据。

Windows 可自动准备项目本地 WinLibs GCC、GNU Make、SDL 1.2、SDL_mixer 和 SDL_ttf：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup-windows-toolchain.ps1
python scripts/make_minimal.py --progress test
python scripts/make_minimal.py --progress sdl
```

存在 `.local/toolchain/mingw64/` 和 `.local/sdl1/mingw64/` 时 Makefile 会优先使用它们。Unix 环境使用 `sdl-config` 查找 SDL；SDL_mixer 和 SDL_ttf 优先使用 `.local/sdl1/` 中同时具备头文件和库的依赖，其余通过 `pkg-config` 查找。默认 make 目标为 `all`，构建 SDL 程序。

Miyoo Mini / Mini Plus 的 Windows 交叉工具链由项目单独准备，全部保存在
`.local/miyoo/`，无需其他工程或外部 SDK 目录：

```powershell
python scripts/make_minimal.py setup-miyoo --progress
python scripts/make_minimal.py miyoo --progress
python scripts/make_minimal.py package-miyoo --progress
```

构建和打包会自动准备缺少的本地工具；固定下载清单、离线使用和 QEMU 验证说明见
[Miyoo Mini 构建与打包](packaging/miyoo/README.md)。

Unix 只安装 SDL1 而没有 SDL_mixer 开发包时仍可构建，程序会明确使用静音 adapter；安装 mixer 后自动启用完整音效/音乐后端。

## 运行

GitHub Actions 的标签自动发布、手动构建和发布包说明见
[自动发布配置](docs/github-actions-release.md)。

可选的 SDL 配置见 [RICH4-SDL.CFG.example](RICH4-SDL.CFG.example)。在运行目录中复制为
`RICH4-SDL.CFG` 后可修改音乐库和地图边缘滚动范围；没有该文件时使用内置默认值。
实际配置由 Git 忽略，发布包仅附带模板，升级时保留已有配置。

```sh
bin/rich4_sdl --data-dir assets/original --music-dir assets/music
bin/rich4_sdl --data-dir assets/original --input-config config/input.toml
bin/rich4_sdl --data-dir assets/original --mkf Panel.mkf --resource 0 --chunk 0
```

有合法的 V3.11 资源时，可用有限帧的真实 SDL smoke 验证资源装配、菜单或游戏首帧和
RGB565 framebuffer；它会自行退出，不需要手动关闭窗口：

```powershell
$env:SDL_VIDEODRIVER = "dummy"
$env:SDL_AUDIODRIVER = "dummy"
bin\rich4_sdl.exe --data-dir assets/original --music-dir assets/music --smoke-frames 2
```

`--smoke-game-frames 2` 使用与新局过场相同的 session initializer，直接绘制真实
gmap0 游戏场景。对应的双回放确定性检查是 `make test-sdl-smoke`；它现在同时锁定
菜单/游戏最终 framebuffer 摘要和逐帧摘要链，但仍不替代原版逐像素截图比对。

默认优先查找 `assets/original/`，随后回退到兼容目录。音频资源或设备不可用时程序可静音继续。

## 目录边界

- `src/core/`：无 SDL 的领域状态与规则。
- `src/data/`：地图解析、对象引用和资源目录。
- `src/resource/`：资源格式、缓存与动画 player。
- `src/view/`：ViewModel 和平台无关的合成规则。
- `src/app/`：场景协议和应用事件。
- `src/platform/sdl1/`：SDL 输入、窗口、计时、文本和 framebuffer runtime。
- `tests/`：unit、resource、characterization、replay 和 render 测试。
- `tools/`：MKF dump、FLC 扫描、地图验证和 contact sheet 工具。
- `docs/evidence/`：IDA/rich4 行为依据与阶段验证记录。

公共 API 的 owned、borrowed、caller-storage 和 null 约定见 [`docs/api-ownership.md`](docs/api-ownership.md)。

## 许可证

本项目代码采用 GNU General Public License v3.0 授权，完整协议见 [LICENSE](LICENSE)。

《大富翁 4》的原版游戏资源不属于本项目代码，其权利归原权利人所有，不受本项目 GPLv3 授权覆盖。

## 法律边界

本仓库不包含、下载或分发《大富翁 4》的可执行文件、MKF、音乐、视频或其他商业资源。使用者必须自行准备合法来源的数据；`assets/original/`、`assets/music/`、本地工具链和构建产物均不会作为商业内容提交。

推荐布局：

```text
assets/
  original/
    Data.mkf
    Jump.mkf
    Panel.mkf
    Map.mkf
    Effect.mkf
    Speaking.mkf
    Help.mkf
  music/
    Original/
      track02.ogg
      track03.ogg
      track04.ogg
      track05.ogg
      track06.ogg
      track07.ogg
      track08.ogg
      track09.ogg
    MultiverseJourney/
      track02.ogg
      track03.ogg
      track04.ogg
      track05.ogg
      track06.ogg
      track07.ogg
      track08.ogg
      track09.ogg
    track10.ogg
    track11.ogg
    track12.ogg
    track13.ogg
    track14.ogg
    track15.ogg
    track16.ogg
    track17.ogg
    track18.ogg
    track19.ogg
    track20.ogg
    track21.ogg
    track22.ogg
    track23.ogg
    track24.ogg
    track25.ogg
    track26.ogg
config/
  input.toml
```

`Original/` 和 `MultiverseJourney/` 分别存放原版与时空之旅版的八首曲目，文件名均为
`track02`～`track09`。在设置中切换音乐库，或通过 `RICH4-SDL.CFG` 的
`music_bank=original` / `music_bank=multiverse_journey` 选择。

两套音乐库共用的场景音乐直接放在 `music/` 根目录，文件名为 `track10`～`track26`；
其中 `track10` 用于主菜单，`track11` 用于新局设置。上述音乐文件均支持 `.ogg` 或
`.wav`，运行时优先尝试 OGG。MIDI 原文件需自行转换为对应音频，完整映射见
[音乐资源说明](assets/music/README.md)。自定义目录时，`--music-dir` 应指向包含这两个
子目录和共用场景音乐的根目录。

## 参与开发

行为改动必须先有 IDA Pro 地址/调用链或 `rich4` 文件依据，并记录输入、输出、副作用、资源来源和测试。未知行为保持 `unknown`/`unimplemented`，不得用猜测玩法填空。具体准入规则和下一阶段任务见 [`PLAN.md`](PLAN.md) 与健康重构目标。
