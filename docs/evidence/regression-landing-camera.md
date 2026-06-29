# 回归修复：首回合 landing 与相机中心

2026-07-14 用户实机报告：重构后人物落地动画消失，真人局无法继续，主地图视角不再以当前人物为中心。

原因一是 `GameActorFlowState` 迁移后的 idle dirty dispatch 改成先为真人显示操作面板；但 ViewModel 按设计在 `first_turn_landing_pending` 时隐藏面板，而 landing 又只能由该操作进入 turn flow，形成不可见输入死锁。重构前 `update_game_session` 在 dirty turn 上无条件调用 `start_game_turn_flow`，先进入 state 3 landing。

修复将 turn-flow 起始状态抽为无 SDL 的 `core/turn_flow.[ch]`。首回合 pending 自动进入 `GAME_TURN_STATE_LANDING`，落地完成后才由既有 status dispatch 显示真人操作面板或驱动 AI。`test_turn_flow` 覆盖 pending landing、普通 preroll、重复启动拒绝和 UI/view flag 清理；完整 replay 也必须实际经过 landing state。

原因二是 ViewModel 重构把相机中心统一改为 `actor.visual_x/y`。重构前规则是：仅在移动插值期间使用 visual 坐标，静止时重新读取当前玩家 route node 的权威坐标。visual presentation 状态可能在落地、轮转或位置规则更新后短暂陈旧，因此画面偏离人物。

修复后 `rich4_game_viewport_build` 接收只读 `MapData`：无自由视角 override 且角色静止时使用当前 route node `x/y`，移动时继续跟随 visual 插值。单测显式放入陈旧 visual 坐标，验证静止相机仍落在 node，移动相机才使用 visual，并验证中心点投影到 `(220,260)`。
