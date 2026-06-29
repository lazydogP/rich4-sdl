# M3.0 玩法协议与稳定 ID

## 地址与目标模块

- 固定模块：`Rich4.exe` V3.11，SHA-256 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`，32-bit x86，image base `0x400000`。
- 复核日期：2026-07-16。总调度依据为回合/自动动作 `0x418C55` 和落点事件 `0x41982D`；本步骤只冻结 `src/core/game_domain.[ch]`、`game_command.[ch]`、`game_event.[ch]` 的协议，不实现这些入口中的具体玩法分支。
- `0x41982D` 的对象范围经地图导入层 `src/data/map_object_ref.c` 转换为零基 ID；原始 ID 仍保存在 `raw_object_id`，领域层不再重复做 one-based 修正。

## IDA 与本地证据

- 当前计划所记录的 2026-07-16 IDA 复核确认：`0x41982D` 是落点事件总调度，`0x418C55` 是自动回合动作总调度。协议枚举只表达这些总入口需要的命令/事件边界，不声称未知 handler 已实现。
- `../rich4/asm/rich4.asm:26459` 是 `0x41982D` 入口；同文件 28488–28500 行是四个特殊事件调用。
- `../rich4/asm/rich4_card_table.c:3` 给出 30 张卡，`../rich4/asm/rich4_tool_table.c:3` 给出 13 种道具，`../rich4/asm/rich4_stocks.h` 与 `../rich4/asm/rich4_all_stocks.c:102` 给出 12 支地图股票。
- 新闻 36 项、命运 37 个逻辑槽和魔法屋 12 个动作分别来自 `0x44B6DF`、`0x44DB81`、`0x43380A` 的既有专项复核；本步骤只把数量作为 ID 上界。

## 输入、输出与副作用

- `GameDomainRef` 使用 `kind + zero-based id`，覆盖玩家、土地、设施、股票、卡片、道具、状态、新闻、命运、魔法屋动作和五个存档槽。`rich4_game_domain_ref_from_original` 是 one-based 导入的唯一公共转换入口。
- `GameCommand` 新增购买/拒绝、升级/拒绝、选择目标、卡片、道具、股票买卖、出售资产、保存和读取命令，并携带 typed `subject`/`target`。命令验证拒绝非法 kind、负数、上界、数量和保留字段。
- `GameEvent` 新增资金、所有权、等级、库存、状态、市场、交互、淘汰、结束和存读档结果类型，并携带 typed `subject_ref`/`target_ref`。
- 本步骤没有新增现金、所有权、库存或市场副作用。新增命令进入现有 dispatcher 时明确返回 `GAME_COMMAND_REJECT_UNSUPPORTED`，状态逐字节不变；队列满时现有骰子数量变更也不会半提交。

## 正常、拒绝、边界与 unknown

- `tests/unit/test_game_domain.c` 覆盖每类 ID 的首项、末项、负数、越上界和 one-based 转换。
- `tests/unit/test_game_command.c` 覆盖每种新增命令的合法形状以及非法 subject/target/数量/flags。
- `tests/unit/test_game_dispatch.c` 覆盖新增命令的显式 unsupported 拒绝、拒绝不修改 `GameState`/`GameFlowState`、全部新增事件的 FIFO 顺序和满队列不半提交。
- `tests/unit/test_map_object_ref.c` 与 `tests/resource/test_map_data.c` 固定原始地图对象到零基领域索引的适配边界。
- 未实现玩法 handler 继续保持 `unsupported` 或 `ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED`；本步骤没有给 unknown 分支添加默认玩法。

## 验证与置信度

- `make -B test sdl`：通过，包含 core header 无 SDL、全部 unit/resource/characterization、固定 replay 和 Windows SDL 构建；编译使用 `-std=c99 -Wall -Wextra`，无警告。
- 置信度：`confirmed`（限于协议形状、ID 上界/转换和拒绝事务语义）；具体玩法 handler 仍按各后续 M3 步骤保持 `unknown`/`deferred`。
