# `0x41D89E` 胜利条件

- 地址与目标模块：`0x41D89E`；`core/victory.c`。
- IDA 结论：2026-07-12 完整复核；`0x41D89E` 函数大小 `0x1C3`，只遍历 play-state 非零玩家并以严格大于选择第一个最高资产者。命中条件为资产非零且（日数限制已到或资产目标已到）。被调用的 `0x4239B9` 在 `0x4239DB` 计算现金+银行−贷款，在 `0x4239DE` 累加 12 项持股×`stock.f20`，在 `0x423A25`/`0x423A88` 累加土地和设施价值。
- 本地证据：[`../../rich4/asm/rich4.asm`](../../../rich4/asm/rich4.asm) 第 31752 行开始先检查 `0x49911C` 与 `0x499108` 两个条件全为 0 时直接返回；遍历 `_num_players`，只对 `+21` 非零玩家调用 `0x4239B9` 取值并选择最大者；随后比较 `0x49911C/0x4990E4` 与 `0x499108`，命中后设置当前玩家并进入结算副作用。
- 置信度：`confirmed`。
- 测试链接：[`tests/unit/test_victory.c`](../../tests/unit/test_victory.c)；纯实现为 [`src/core/victory.c`](../../src/core/victory.c)。
- 融资边界：贷款已由 `0x4239DB` 确认并扣除；`special_finance` 未被 `0x4239B9` 单独读取。原结构说明 bank 已包含特殊融资，因此不得再次叠加该字段。
- 结算边界：候选计算本身仍由 `rich4_victory_evaluate_events` 保持兼容。新增的
  `rich4_victory_finalize_events` 用于整回合日结边界：原子输出
  `VICTORY_CANDIDATE -> GAME_ENDED`，再写入 `game_over/winner_*`；账本发现的
  破产/最后玩家路径仍由 `ledger.c` 直接提交 `GAME_ENDED`。SDL 端随后按原版
  `0x41D89E` 的人类数/胜者类型选择语音、`Panel[112]`、生命周期码 1/2/3/4。
- 终局锁定：`game_dispatch.c`、`turn_flow.c` 和 app 回合更新入口在
  `game_over` 后拒绝新指令/新回合。`turn_status_flow`、命令事件桥和路线事件桥
  现在统一把 `GAME_EVENT_GAME_ENDED` 捕获为 app 结算快照；SDL 在胜者语音期间
  保持原地图作为背景，码 2/3 开始 JUMP 转场时才切换到
  `JUMP.MKF[stage*4+map]`，再用已确认的 `Panel[93]` 和
  `Panel[100+winner_character]` 绘制资源叠加，并保留无资源时的文字回退。原版
  `ref_004808AA` 的 12 条胜者语音（资源
  `1074..1085`）现在由 `character_speech.c` 精确映射，终局快照通过 SDL
  presentation dispatch 接入既有 speech/audio 流，语音对话框也在结算叠加后绘制。
  SDL 在胜者语音完成后才进入 JUMP/继续游戏分支；`Panel[112]` 按原版显示 10 秒
  倒计时，最后一个人类破产的确认路径播放 `Data.mkf[556]` FLC 后再进入加载 UI。
  JUMP 完成位使用原版 `lpBuffer__27[0..3]` 的四地图掩码，不用活动玩家数近似；
  非终局码 2/3 分别回到新游戏/外层菜单，码 1 直接回外层菜单。
- 当四个地图完成位全部置位时，SDL 按 stage 分支尝试播放
  `END%02d.AVI + THANKS.AVI` 或 `END.AVI + OVER.AVI`，随后置终局退出标志；
  文件缺失或非 Windows 时不伪造播放成功，但仍执行原版终局后续退出动作。
  JUMP 背景、Panel 93/角色资源和无资源文字回退仍保留在静态渲染路径。
