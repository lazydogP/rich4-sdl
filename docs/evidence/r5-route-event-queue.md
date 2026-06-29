# R5.3 路线事件队列（历史记录）

> 本文件记录的是 R5 旧实现的行为合同，不能作为当前 queue 结构的状态依据。
> 当前结构和验证以 `src/core/route_event_queue.*` 与
> `src/core/route_event_executor.*` 为准。

## 领域模型

`src/core/route_event_queue.[ch]` 定义：

- `RouteEvent { phase, node, object_ref, snapshot }`。
- `RouteEventSnapshot` 固化事件发生时的玩家、事件种类、地产分支、owner 和 value，避免消费阶段重新读取已变化状态。
- 旧版 `RouteEventQueue` 曾保存有界事件数组、消费游标、finalized 状态和最后一次 handler 结果。
- `RouteEventHandlerResult` 明确区分 `APPLIED`、`WAITING_FOR_COMMAND`、`UNIMPLEMENTED_CONFIRMED` 和 `INVALID_STATE`。

旧版队列只允许零个或多个 `PASS`，随后最多一个 `STOP`；finalize 总是追加 `DONE`。这些约束已由当前的纯 FIFO queue 与独立 executor 承担，不应据此推断当前 queue API。

## 会话接入

移动到达节点时将 typed `MapObjectRef` 和属性快照压入队列，停止节点随后压入 STOP；路线事件阶段直接按队列游标消费：

- 普通 road node 返回 `APPLIED`。
- 尚未实现的地产、设施与特殊格返回 `UNIMPLEMENTED_CONFIRMED`，并写入既有 deferred 诊断后才推进。
- 未知或无效事件返回 `INVALID_STATE` 并保持当前游标，不再静默轮转。
- `WAITING_FOR_COMMAND` 同样保持游标，供后续交互 handler 使用。

旧的 debug 数组当前仅作为诊断镜像，由队列当前事件填充，不再决定消费顺序。

## 验证

`tests/unit/test_route_event_queue.c` 覆盖严格的 `pass, pass, stop, done`、STOP 后拒绝追加、等待时不推进、显式未实现推进和非法事件分类。`make -B test tools sdl test-original-data` 全部通过，固定 replay 未变化，`-Wall -Wextra` 无警告。
