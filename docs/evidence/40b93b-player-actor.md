# `0x40B93B` 玩家 actor 状态资源选择

- 地址与目标模块：`0x40B93B`；`view/game_view_model.c`。
- IDA 结论：2026-07-11 已复核；函数范围 `0x40B93B..0x40BF92`，反编译确认普通玩家 `<4`、特殊槽 `4..7`、记录步长 `0x68/0x34` 及状态位驱动的资源装载分支。
- 本地证据：[`../../rich4/asm/rich4.asm`](../../../rich4/asm/rich4.asm) 第 10757 行开始区分玩家索引 `<4`，以普通玩家记录步长 `0x68`、渲染记录步长 `0x34` 访问角色 `+19`、状态 `+21`，并依据 `0x40`、`0x30` 等位选择/加载 actor 资源；[`../../rich4/csrc/loadsave.c`](../../../rich4/csrc/loadsave.c) 第 240、258 行在加载恢复路径调用它。
- 置信度：`partial`（明确分层）。`0x40` frozen/不可见判定、普通交通 `traffic*3 + standing/moving/action` 和地图方向换算已由调用点及资源布局确认；parts 13/14、16/17、18 的具体状态语义与特殊玩家 `>=4` 完整资源路径仍为 partial。
- 测试链接：[`tests/unit/test_game_view_model.c`](../../tests/unit/test_game_view_model.c)；选择逻辑位于 [`src/view/game_view_model.c`](../../src/view/game_view_model.c)，每个 `GameActorView` 显式携带 `part_confidence`。
- 迁移状态：HUD 与普通玩家 actor renderer 已读取 ViewModel；未确认 part 不升级为 confirmed，特殊玩家 actor 将在后续 renderer 拆分中继续复核。
