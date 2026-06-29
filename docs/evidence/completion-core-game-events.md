# Completion: core rule side effects use GameEvent

日期：2026-07-13。

## 统一事件边界

`core/game_event.[ch]` 的有界 FIFO 现在覆盖 command 接受/拒绝、回合流、掷骰、骰子数量、actor/scene refresh、文本/语音请求、状态 tick/到期/handler 结果、dirty、整轮/日期、victory candidate、route movement 和 route-event handler 结果。

通用 payload 为 `player/subject/value/value2/flags`；command 事件继续携带原 `GameCommand` 和拒绝原因。仍未迁移的状态 handler 保留 `UNIMPLEMENTED_CONFIRMED`，已确认的 `+50/+51/+54/+55` 结果返回 `APPLIED`；路线 handler 的 `WAITING/UNIMPLEMENTED/INVALID` 均原样进入事件 payload，没有被伪装成成功。

## 核心入口和 runtime 消费

player status、status countdown/consume、turn rotation、victory、route movement 和 route event queue 均提供 `*_events` 正式入口：规则状态变更仍同步完成，所有需要外层处理的副作用同时写入调用方提供的 `GameEventQueue`。旧结果结构保留为同步控制流和兼容 characterization，不再是 SDL runtime 的副作用通道。

`platform/sdl1/runtime_main.c` 已无上述模块旧入口调用。status tick 事件触发 god relation 更新，status effect 建立诊断记录；turn 事件触发 dirty、普通玩家状态 tick 和整轮 victory 检查；victory candidate 更新候选快照；player-status refresh、route movement 和 route handler completion 均从统一队列消费。

音频仍通过 R8.2 的场景/领域 `Rich4AudioEvent` 队列交给 SDL_mixer adapter；core 不调用音频后端。

## 测试与门槛

模块单测验证 player-status refresh、9 个 status expiry + god tick、12 个 status handler result、turn status/dirty/full-round/day 顺序、victory candidate/零事件、route begin/commit 和 route handler completion 的事件类型与 payload。command dispatcher 测试继续覆盖接受、七类拒绝和 16 项队列容量/dropped 边界。

- Windows：`mingw32-make.exe -B test test-render tools sdl test-original-data` 通过，无 `-Wall -Wextra` 警告。
- 固定 seed replay 保持相同 9 条 trace。
- WSL：`make test-sanitize` 全部 unit/resource/render 测试通过，无 ASan/UBSan 诊断。
