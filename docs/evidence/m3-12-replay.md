# M3.12 核心 replay、存读档与 SDL smoke 证据

置信度：`partial`，限于以下原版控制流、测试驱动和 Windows 本地回归；原版整局逐帧等价仍未验收。本次使用现有本地原版资料，未重新通过 IDA MCP 核验 IDB。

## 原版边界

- `0x40C05C` 完成医院/监狱返回路线格的移动，`0x40D7C4` 设置到格标志，下一逻辑脉冲在 `0x40D932` 消费标志并调用 `0x41B42D`。然后五次停步递减进入 `0x418E7F`，返回位对应三次 EA5 倒计时，再由 `0x418EBD` 轮转；详见 [收容退出](confinement-special-actor-behavior.md)。本地汇编为 `rich4/asm/rich4.asm` 的 `loc_0040d8d3..loc_0040d966`。
- `0x418F59/0x418F78` 的同角色返回继续处理神明与工程车效果。到格时触发的地雷/炸弹先经 app 演出队列完成效果，不能直接清等待或把角色强制改为 IDLE。
- `0x41CF67` 日界先检查胜负，随后 `0x41D066/0x41D06E` 按 tick 重新播种；因此回放驱动的等待时长会影响后续随机流。[随机流依据](ai-rng-pixel-compatibility.md)
- `0x41D89E` 评估日限/资产目标；破产的最后玩家路径由账本提交终局，不能用目标资产胜利覆盖它。[胜利依据](41d89e-victory.md)
- `0x402F99/0x402FA1` 的 LOAD 返回按当前 tick 播种，区别于存档解码本身。[存档/RNG 边界](ai-rng-pixel-compatibility.md)

## 真实地图回放

`tests/replay/test_real_map_bootstrap.c` 读取 V3.11 `Map.mkf` 的 gmap0，并通过正式 new-game initializer、app movement、路线执行器、演出续接和回合轮转运行。范围包括：

1. 地图/玩家初始化、八个动态对象及逐步路线事实。
2. 可达的路线 action 1–16、新闻/命运、奖励格与魔法屋。
3. 真实地图的地产/设施自动购买、升级/收费、设施轮盘与连续资产巡游。
4. 保留动态池、新局现金/银行分配、关闭日限的 X3 资产目标长走。`initial_fund=20000`、`win_target=60000`，默认目标布局 seed 为 `REAL_MAP_REPLAY_SEED=29`；严格要求 `TARGET_ASSETS`，胜者资产达到目标且实际发生地产收费。
5. 使用真实 AI 前置动作、骰子、移动和路线事件的自然 AI 日限终局。

BFS 资产长走与无目标引导的自然 AI 回放分别验收，不能合称自然 AI 资产长局。相同配置运行两个独立 session，比较节点/路线、动态对象、收费、命令/事件、完整状态摘要和终局字段。种子、X3 门槛、胜利类型及双运行断言均由测试固定。

`complete_real_map_route_turn` 用固定 `frame_ms=20` 的测试时钟推进收容退出、到格效果、五次停步及三次回合倒计时，随后处理神明/工程车返回。EA5 倒计时只由对应回合更新消费，不交给提示窗口的 2000 ms 快进。该 20 ms 是 headless fixture 参数，不宣称原版所有移动速度均为 20 ms；原版 timer 分频见移动专项证据。

回放把资源演出建模为确定的一帧，仍通过 app 的动画、文本/语音、理赔和后续效果接口。它验证规则与续接，不证明 SDL 画面或音频等价。原始资源缺失时明确 SKIP，不能记为真实地图通过。

## SDL smoke 的固定输入与像素基线

`tests/sdl/test_runtime_smoke.py` 对菜单 2 帧和 gmap0 游戏 120 帧各运行两次，要求最终帧与逐帧摘要链均一致，再与固定基线比较：

| 场景 | 帧数 | framebuffer | sequence |
| --- | ---: | --- | --- |
| 菜单 | 2 | `d2e1daa3` | `0caab095` |
| 游戏 | 120 | `5dd46dfc` | `71e2ffe7` |

每次使用独立临时工作目录和默认配置，显式传入 V3.11 资源目录、项目字体及输入配置；不读取用户工作目录的 `RICH4*.CFG`、存档或诊断输出设置，也不连接外部锁步控制器。音视频使用 dummy driver，运行时使用固定 RNG seed 和 smoke 逻辑时钟。每帧按 RGB565 实际像素行做 FNV-1a，排除 pitch padding；sequence 对每帧摘要的四个小端字节继续做 FNV-1a。仍检查退出码、场景及帧数，未放宽像素容差。

取色依据为原版 `0x451801`、`n2_6==1`：`((pixel << 1) & 0xffc0) | (pixel & 0x001f)`。RGB555 绿色低位搬到 RGB565 bit 6，bit 5 保持零。原始反编译位于 `rich4/docs/evidence/monthly_finance_ui_ida.json`，还原代码见 `rich4/csrc/mkf/mkf.c:update_pixels`。当前 `map_resource_pixel` 与 `rich4_rgb555_row` 遵守同一规则；`test_sdl1_blit` 穷举 65536 个输入，`test-render-equivalence` 验证标量/优化绘制等价。[取色证据](render-cache-and-flc-state.md)

这些摘要只定义当前 SDL 的固定资源回归。原版逐像素/时序对照由 [双端锁步方案](../original-sdl-differential-testing.md) 单独验收，不能用 smoke 通过替代。

## 存读档与市场回放

`tests/replay/test_save_load_replay.c` 使用自有 R4S 格式 v9，对交互暂停、半步移动、日界待保存、淘汰后、异地图 stage/map 身份做磁盘往返。比较 `GameState`、flow 与时光机快照，再次编码的规范字节相同；语音指针重绑到 session 自有文本，瞬态音频队列清空。

AI 市场分叉从同一存档继续执行真实 `rich4_ai_run_preactions` 与命令派发，比较挂牌、调价、购买的命令/事件及终态；测试自行找到能覆盖这些动作的固定种子。坏版本、坏 CRC、截断及非法槽号均须拒绝且不改写目标 session。

`runtime_load_reseeds_special_targets` 另测 LOAD：解码保留随机字，resume 根据当前 tick 与当前进程 offset 播种；覆盖不同 tick 和 32 位回绕，并检查新闻/命运排列及游标保留、目标随新随机流选择。因此数据往返等价和运行时加载语义是两项独立保证。

异地图实际资源切换、完整槽位 UI、长期同虚拟时钟下的 LOAD 后轨迹仍属于 M3 验收范围。

## 验证入口与边界

Windows 隔离构建统一使用 `python scripts/make_minimal.py`。

`test-replay test-sdl-smoke test-render-equivalence test-frame-pacing` 已通过，其中真实地图读取了本地 V3.11 数据，未 SKIP。最终串行 `test test-render tools sdl test-original-data test-sdl-smoke test-frame-diff test-ai-pixel-candidates test-options-sale test-engineering-regression test-hotel-regression` 全部通过。剩余平台/交互工作统一见 [M3 进度](../m3-core-gameplay-original-evidence-plan.md)。
