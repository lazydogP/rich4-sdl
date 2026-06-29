# M3.1 路线事件执行器与交互暂停/恢复

## 地址与目标模块

- 固定模块：`Rich4.exe` V3.11，SHA-256 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`，32-bit x86，image base `0x400000`。
- 复核日期：2026-07-16。原版落点事件总调度为 `0x41982D`，唯一上游来自移动/回合链 `0x418E7F`；本步骤对应 `src/core/route_event_queue.[ch]`、`game_dispatch.c` 与 SDL runtime 装配边界。
- 普通地产主体约为 `0x4198D9..0x41A154`，设施平行分支约为 `0x41A17F..0x41A9AF`。特殊调用位于 `0x41B11E`、`0x41B128`、`0x41B132`、`0x41B13C`。

## IDA 与本地证据

- 当前计划记录的 2026-07-16 IDA 复核确认：`0x41982D` 按原始对象 ID 进入 land、facility 和两个 special table 家族，而不是由 UI 分散修改玩法状态。
- `../rich4/asm/rich4.asm:26459` 是 `fcn_0041982d` 入口。
- 同文件 28488–28500 行可直接看到 `0x44B6DF`、`0x44DB81`、`0x43D304`、`0x43E9A4` 四个特殊事件调用。
- 本步骤只实现总调度、暂停和恢复协议；上述特殊 handler 及地产经济效果仍保持 `UNIMPLEMENTED_CONFIRMED`，没有从地址范围推导占位玩法。

## 输入、输出和副作用

- 输入：actor、PASS/STOP、原始对象 ID、节点、owner/value/branch 快照和非零 turn token。
- `rich4_route_event_family_from_raw_id` 先按原始 ID 分为 road、land、facility、special table 1、special table 2 或 unknown，再由执行器决定 no-op、deferred 或交互请求。
- 同次移动严格保留 `PASS* -> STOP? -> DONE`。执行器每次只 inspect/apply 一个事件；真人停在无主地产时发出 `GAME_EVENT_INTERACTION_REQUESTED`，携带 typed subject、接受/拒绝命令类型和 turn token，游标保持不变。
- 回复仍经统一 `rich4_game_dispatch_command`。错误 token 和错误目标被拒绝且不推进；购买因 M3.3 尚未实现返回 `GAME_COMMAND_REJECT_UNSUPPORTED`；明确拒绝购买产生 `ROUTE_EVENT_DECISION_DECLINED` 并恢复队列。
- SDL runtime 只把交互请求事件转换为 flow 中的 accept/decline 命令；确认键提交购买命令，取消键提交拒绝命令。规则状态与队列游标只由 core dispatcher/执行器修改。
- actor 在事件中淘汰时产生 `ROUTE_EVENT_ACTOR_ELIMINATED` 并丢弃其剩余事件；unknown 产生 `ROUTE_EVENT_UNKNOWN_SKIPPED` 后允许正常回合轮转。

## 边界修正与 deferred

- M3.0 已冻结零基领域 ID。本步骤把 runtime 的地产/设施状态复制、估值和胜利资产扫描同步改为零基，消除此前 `MapData` one-based 原始表与 `GameState` 数组之间残留的重复偏移。
- 地图导入仍保留原始 `raw_object_id`；facility `0xFA0`、special table 1 `0x1770`、special table 2 `0x1F40` 在适配层转换为零基索引。
- AI 的无主地产决策属于 M3.10；当前明确 deferred 并推进，不伪造“自动购买/拒绝”策略。
- 已确认家族中尚无 handler 的分支继续产生 `ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED`；unknown 不伪装为成功效果。

## 测试与 replay

- `tests/unit/test_route_event_queue.c`：PASS/STOP 顺序、等待不推进、unknown 结果和事件队列满时不半提交。
- `tests/unit/test_route_event_executor.c`：多个 PASS 后 STOP、无主地产暂停、重复 step 冻结、过期 token、购买 deferred、拒绝恢复、事件中淘汰、unknown 后下一玩家正常获得回合，以及五个原始对象家族边界。
- `tests/replay/test_route_event_executor_replay.c`：无 UI 地预置拒绝命令，两次生成完全相同的 resolved/deferred/request/declined 事件序列。
- `tests/unit/test_map_object_ref.c`、`test_victory.c` 和 `tests/resource/test_map_data.c` 固定零基导入与资产扫描边界。

## 验证与置信度

- Windows：`make -B test sdl test-original-data` 全部通过，包含 unit/resource/fault-injection/characterization/replay、SDL 链接和 V3.11 Data/Jump/Panel/Map 扫描；`-Wall -Wextra` 无警告。
- Linux/WSL：`make test-sanitize` 通过全部 unit/resource/render/replay，无 ASan/UBSan 诊断。
- 置信度：对象家族分派、队列顺序和暂停协议为 `confirmed`；具体地产经济、设施、特殊事件和 AI 决策仍为 `partial/deferred`。
