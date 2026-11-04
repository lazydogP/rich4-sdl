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

核心经济/路线事件、市场挂牌、AI 策略及自有格式整局存读档均已接入；真实 gmap0 终局双回放、存档回放和 SDL smoke 通过。M3 处于原版一致性与完整交互验收阶段，不能据本地回归通过标为完成。统一状态、当前 Windows 验证范围及原版依据见 [M3 进度](docs/m3-core-gameplay-original-evidence-plan.md)。

## 近期路线

1. 以真实地图 replay、存档 replay、SDL smoke 和架构检查为持续回归门槛。
2. 用双端锁步复验选人后的计时器相位及菜单像素，随后覆盖新局到首回合；保持相同虚拟时间、输入与原版随机调用顺序。
3. 扩展到长期 AI、动态对象、模态交互和终局生命周期；对没有安全 raw 映射的分支先补原版证据。
4. 分开验收存档数据往返与运行时 LOAD 的重新播种，补齐同虚拟时钟下的后续轨迹、跨地图资源切换和完整槽位交互。
5. 对最终版本重验 Windows、桌面 Linux SDL1、ASan/UBSan 和 Miyoo 真机完整流程，并完成状态、资源所有权及 unknown 行为审计。

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
python scripts/make_minimal.py test test-render tools sdl test-original-data test-sdl-smoke --progress
```

平台矩阵最终要求 Windows 本地 WinLibs + SDL1、桌面 Linux SDL1、Linux ASan/UBSan，以及无 SDL 的 tools/core tests 全部通过。公共 API 约定见 [`docs/api-ownership.md`](docs/api-ownership.md)。

## 发布方向

Miyoo/Onion 已有交叉构建、打包、128 MB 无交换 QEMU 和部分真机验证，范围见 [Miyoo 文档](packaging/miyoo/README.md)；QEMU 耗时不替代真机显示、音频、SD 卡及整局验收。后续发布沿用 M3 门槛，更广泛的 mod 覆盖另行推进。原版 DAT 存档导入、实时 MIDI、脚本语言和未经证据确认的玩法不在当前承诺内。
