# 梦游精灵、叠加标记与自动回合

- 原版：`Rich4.exe` V3.11，image base `0x400000`，SHA-256 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`。
- 原版二进制及 `rich4/asm` 复核：2026-11-19。函数簇为 `0x40B93B`、`0x40829D`、`0x40C05C`、`0x40C912`、`0x40DD1F`、`0x40D7C4`；标记的资源来源由 `0x4080B2..0x4080D7` 的加载循环确认。
- 置信度：`confirmed`（下述角色资源、标记帧和回合入口）；整局原版录屏逐帧对照未纳入该项测试。

## 角色与标记

普通玩家 `+55`（`0x496B9F + player*0x68`）非零时，`0x40BA16..0x40BAA4` 从 `0x48A0E4` 的 Data.mkf 句柄加载 `128+21*character+16`、`+17`、`+2`，分别对应站立、行走和投骰动作。特殊人物的相应资源是 Data.mkf 的 `380+slot*4` 与 `380+slot*4+3`。梦游采用这些专门精灵，冬眠的蓝色调色函数不用于梦游。

`0x408870..0x40890D` 在普通玩家主精灵之后，另外把 `0x496978` 指向的共享 SPR 加入绘制队列。特殊人物在 `0x408B1A..0x408B6E` 使用相同 SPR。

资源来自 Data.mkf[414]，共六帧。`0x4080B2..0x4080D7` 以 `ebx=0..19` 遍历，使用 `0x48A0E4` 的 Data.mkf 句柄读取 `396+ebx`，将返回指针写入 `0x496930+4*ebx`。因此 `0x496978` 对应 `ebx=(0x496978-0x496930)/4=18`，即资源 `396+18=414`；SDL 使用现有 `data_map_status[18]` 资源槽。

叠加标记沿用角色的绘制锚点与 SPR hotspot；深度偏移为当前角色 `0x0f`、其他角色 `0x0e`，目标对象 ID 为零。它独立于角色身体的方向和动画帧。

六帧计数是 `0x498EA4 + original_actor_index*0x34`。普通玩家在 `0x40C455..0x40C47E`、特殊人物在 `0x40C71F..0x40C744` 的移动调用中递增，到六归零。只有当前角色实际移动时递增，站立、投骰动作及其他角色的回合保持该计数。`0x407F83` 在地图/存档加载时清空全部渲染记录，因此该计数属于不写入存档的瞬时渲染状态。

SDL 的只读视图分别携带普通玩家的梦游标记与帧、特殊人物的 overlay chunk。app 层用独立数组保留各角色的帧，不借用会在新回合或动作结束时清零的身体动画帧。

## 回合与提示

`0x40CBA5..0x40CBB3` 在没有其他阻塞状态且梦游非零时调用 `0x40DD1F`，返回 `-1`。`0x418C55` 不进入真人操作面板或 AI 预行动分支。`0x40DD1F` 启动投骰动作，`0x40D7C4` case 2 在该动作完成后调用 `0x419572` 显示普通骰子动画，然后自动行走。

梦游回合不调用 `0x440CAC` 的“状态中／还剩几天”提醒。普通骰子窗口仍然出现。`0x44EF93` 对梦游、冬眠、消失状态阻止可见的角色发言。

## 本地证据与验证

- [`rich4.asm`](../../../rich4/asm/rich4.asm)：`loc_0040ba16`、`loc_004088bb`、`loc_0040c44c`、`loc_0040cba5`、`fcn_0040d7c4`。
- [`rich4_card_mengyouka.asm`](../../../rich4/asm/rich4_card_mengyouka.asm)：设置梦游、保存并转换交通方式、角色资源刷新。
- [`rich4.asm`](../../../rich4/asm/rich4.asm)：`loc_004080b2` 的 Data.mkf 加载表；`0x4088C7` 与 `0x408B26` 读取同一标记指针。
- [`loadsave.c`](../../../rich4/csrc/loadsave.c)：`memset(0x498ea0,0,0x34*9)`。
- `python scripts/make_minimal.py test-sleepwalking`：12 个普通角色与 4 个特殊人物，站立/行走、八个方向、六帧标记的完整画面对比。期望标记直接读取原版 Data.mkf[414]，同时载入 Map.mkf[25] 作为不同资源的对照，确保误选另一归档的图像不能通过；真人/AI 自动投骰且没有状态提示框；普通/特殊角色独立计数、移动与停留边界、视图转换、读档清空瞬时渲染计数。
- 测试：[`test_sleepwalking_sdl.c`](../../tests/sdl/test_sleepwalking_sdl.c)、[`test_turn_start_flow.c`](../../tests/unit/test_turn_start_flow.c)、[`test_turn_movement_flow.c`](../../tests/unit/test_turn_movement_flow.c)、[`test_special_actor_flow.c`](../../tests/unit/test_special_actor_flow.c)、[`test_game_view_model.c`](../../tests/unit/test_game_view_model.c)、[`test_save_load_replay.c`](../../tests/replay/test_save_load_replay.c)。
