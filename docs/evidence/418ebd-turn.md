# `0x418EBD` 回合轮转

- 地址与目标模块：`0x418EBD`；`core/turn.c`。
- IDA 结论：2026-07-12 完整复核；函数大小 `0x1AD`。`0x418F9B` 起逐次递增索引，普通玩家末尾转 4，特殊槽 4..7 按 `0x498DF2` 选择，索引 8 回到 0；普通玩家仅在 play-state 为 0 且 `+8` 节点非零时跳过。`0x41902E` 在回卷时触发整轮回调，`0x419039` 调用状态 tick，`0x41905F` 标记所选记录 dirty。
- 本地证据：[`../../rich4/asm/rich4.asm`](../../../rich4/asm/rich4.asm) 第 25608 行开始处理当前索引 8 的回卷，普通玩家状态 `0x30` 的清理；从第 25670 行附近递增玩家，普通玩家结束后转到 4，特殊槽 4..7 依据 `0x498DF2` 跳过，8 再回到 0；普通玩家还依据 `+21` 与 `+8` 过滤。
- 置信度：`confirmed`。
- 测试链接：[`tests/unit/test_turn_rotation.c`](../../tests/unit/test_turn_rotation.c)；纯实现为 [`src/core/turn_rotation.c`](../../src/core/turn_rotation.c)。
- 领域事件：状态 tick、dirty、整轮、日期推进和普通玩家 `0x30` 状态清理均由 `TurnAdvanceResult.events` 输出；SDL wrapper 消费特殊槽 tick 后继续到可玩的普通玩家。
