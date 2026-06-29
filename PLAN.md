# rich4-sdl 路线图

## 定位与原则

项目以 C99/SDL 1.2 重实现《大富翁 4》，优先支持 640×480 RGB565 软件渲染和 Linux 掌机。原版行为以当前加载的 V3.11 `Rich4.exe`、IDA 证据和 `rich4/` 参考工程为准。

长期原则：

- core/data/resource/view 在无 SDL 环境可构建和测试。
- SDL1 平台层只负责输入、计时、音视频、framebuffer 和装配。
- 输入、AI 和 replay 统一产生 `GameCommand`；规则副作用统一输出 `GameEvent`。
- 资源由 ResourceStore 或明确 caller 单一拥有。
- 未确认行为保持显式 unknown/unimplemented。
- 不分发商业资源，不运行原版 x86/Win32 代码。

IDA 锚点和原版行为依据只在 [`docs/evidence/`](docs/evidence/README.md) 维护，当前重构目标见 [`docs/llm-context-simplification-lean.md`](docs/llm-context-simplification-lean.md)；本文件不记录字段级实现台账。

## 当前基线

已建立的模块边界包括：

- MKF、SPR/SMP、FLC、ResourceStore 和真实 V3.11 archive 扫描。
- GameState/GameFlowState、GameCommand、路线移动/事件队列、状态倒计时、回合与胜利候选。
- GameViewModel、地图 renderer、FLC player/compositor。
- 五场景协议、内部 640×480 输入命令、音频事件和 SDL1 平台 runtime。
- Windows WinLibs SDL1、无 SDL tests/tools、RGB565 render gate 和资源 fault injection。

当前可验证链路已覆盖核心经济/路线事件及真实 gmap0 的终局回放。M3 仍缺出售挂牌交易、部分 AI/资金处理、整局磁盘存读档与最终验收；统一进度和代码/IDA 依据见 [M3 进度](docs/m3-core-gameplay-original-evidence-plan.md)。

## 近期路线

1. 完成 Linux SDL1 构建与 ASan/UBSan 调试矩阵。
2. 将 SDL runtime 中剩余的应用编排继续迁入 Menu/NewGame/Intro/Game/Options 场景，使平台层只保留 I/O 与装配。
3. 完成 `GameCommand -> GameEvent` 流水线，让输入、AI、replay 和音频/界面副作用使用同一事件边界。
4. 按 M3 缺口清单补齐交易、AI 与存读档，并扩展现有 replay 到存读档等价验收。
5. 完成最终状态/所有权/renderer/unknown 行为审计。

## 新玩法准入

地产、设施、股票、卡片、道具、新闻和特殊人物 handler 在进入实现前必须同时具备：

- IDA 地址或 `rich4` 调用链证据；
- 输入状态、输出状态和副作用列表；
- 资源编号或文本来源；
- 正常、拒绝和边界测试；
- 统一 `GameCommand -> GameEvent` 路径。

任何一项缺失时只记录候选或 deferred event，不实现推测效果。

## 构建门槛

提交前至少运行：

```sh
make test
make test-render
make tools
make sdl
make test-original-data
```

平台矩阵最终要求 Windows 本地 WinLibs + SDL1、桌面 Linux SDL1、Linux ASan/UBSan，以及无 SDL 的 tools/core tests 全部通过。公共 API 约定见 [`docs/api-ownership.md`](docs/api-ownership.md)。

## 发布方向

完成健康重构与核心 replay 门槛后，再推进 Miyoo/Onion OS/OpenDingux 打包、配置覆盖、音乐替换和 mod 资源覆盖。存档兼容、实时 MIDI、脚本语言和未经证据确认的玩法不在当前承诺内。
