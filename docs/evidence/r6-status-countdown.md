# R6.2 状态倒计时与 pending 消费

## 原版复核

IDA `sub_41C84F`（`0x41C84F`，由 `0x418EBD` 调用）确认：

- 普通玩家按原记录偏移顺序处理状态，并调用 `0x40D6BE`、`0x40D4E5`、`0x43D7BF`、`0x43EE6E`、`0x40B8D8`/`0x40B93B`、`0x40CC1A` 等 handler family。
- 计数到零时写入高位 pending；`+51` 使用低 6 位归零规则，因此 `0x41 -> 0xC0`，而非普通的 `0x80`。
- `+50..+53` dword block 非零时守卫 `+54/+55/+57`。
- 特殊玩家分支 `0x41CE51..0x41CF57` 先消费高位 pending，再依次递减四个状态字节；第一个状态结束会清 actor `0x40` 并调用 `0x40B8D8`、`0x40B93B`。

## 核心实现

`src/core/status_countdown.[ch]` 提供：

- `rich4_status_tick_player`：严格按 `50,51,52,53,54,55,57,56,59,60,61,62` 的领域顺序递减，并输出实际变化偏移 trace 与 `+61` relation tick 请求。
- `rich4_status_player_has_pending`：集中检查高位 pending。
- `rich4_status_consume_pending`：按相同原版顺序消费事件，维持 block guard，处理已确认的 `+54` frozen 位清除、`+55` 交通/骰子恢复和 `+61` 双方关联清除。
- `StatusHandlerFamily`：以地址命名 handler family。
- `StatusEffectResult`：clear-only、监狱/医院释放以及 `+50/+51/+54/+55` 已确认的状态变更返回 `APPLIED`；仍未移植的原版 handler 内部副作用返回 `UNIMPLEMENTED_CONFIRMED`，无效调用返回失败，不伪装为已完成。

SDL 会话只保留 `+61` 已确认 relation-balance 数值更新和 debug trace 适配，字段递减与 pending 状态变更均调用核心 API。

## 测试

`tests/unit/test_status_countdown.c` 覆盖普通/消失规则、高位 pending、block guard、完整十二字段消费顺序、handler family、未实现状态、frozen 清除、sleepwalk 恢复及双方 god 状态清除。`make -B test tools sdl test-original-data` 全部通过，固定 replay 未变化且无编译警告。
