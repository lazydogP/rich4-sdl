# M3.3 普通地产购买、升级与收费

## 地址与目标模块

- 固定模块：`Rich4.exe` V3.11，SHA-256 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`，32-bit x86，image base `0x400000`。
- 主要实现：`src/core/property.[ch]`；路线接入：`route_event_queue.c` 与 `game_dispatch.c`；原始记录导入：`map_data.[ch]` 和 SDL runtime 的 map → `GameState` 适配。
- 原版锚点：普通地产主分支 `0x4198B9..0x41A168`，同组收费 helper `0x419744`，交易状态检查 `0x40FA61`，收费前保护检查 `0x41D559`，AI 购买资金线 `0x41D7D4`。

## 已确认字段和公式

- 52-byte 普通地产记录：名称 `+4..+23`、price status `+0x17`、type `+0x18`、owner `+0x19`、level `+0x1A`、地价 `+0x1C`、每级房价 `+0x1E`、0–5 级收费表 `+0x20..+0x2B`。`MapLandRecord` 现在显式解析收费表；适配层按名称建立稳定的局部 group ID。
- `0x41A013..0x41A154`：购买价为 `(land_price + level × house_price) × price_index`，只比较并扣除 cash；接受后写 owner，不动 bank。
- `0x41990B..0x419A48`：只有 type 0、level `< 5`、现金足够且不在禁止状态时可升级；每次费用是 `house_price × price_index`，成功后 level 加一。
- `0x419744`：住宅收费按同 owner、同名称组内每块地的当前 level 查六项收费表后求和；非住宅普通地产按 owner 的该类地产数 × 2000；最后乘 price index。当前地产 `price status != 0` 时主费用翻倍。
- `0x41D709` 对总费用应用 payer 的 `god_info` 倍率；`0x41D559` 会在 owner 处于旅馆/消失/监狱/医院/睡眠等状态、`god_info == 15` 或 owner 的 `+65` 关联玩家就是 payer 时免收。
- `0x40FA61` 明确把 payer `god_info` 7、8、15 视为交易禁止；`god_info == 12` 在购买报价前直接跳过。
- `0x41D7D4` 的 AI 购买线是 `min(trunc(initial_fund × 0.05), 7000) × price_index`；仅当 `cash + bank - price` 严格大于该保留额时购买。AI 自有住宅升级沿用与真人相同的价格和状态验证入口。

## 命令、事件和事务

- 真人满足条件时，路线执行器发出 purchase/decline 或 upgrade/decline 的 `GAME_EVENT_INTERACTION_REQUESTED`；回复必须携带相同 turn token 和 typed land ID。
- AI 只负责作出是否提交命令的决定，实际购买/升级仍调用 `rich4_land_apply_command`，与真人共享验证和提交逻辑。
- 购买/升级通过账本的 exact-cash 入口扣款。`GAME_PAYMENT_PAYER_CASH_ONLY` 是明确标注的 core-only 事务标记，不冒充 `0x41D2C6` 原始 flag。
- 成功事件顺序固定为 `MONEY_TRANSFERRED → PROPERTY_OWNER_CHANGED/PROPERTY_LEVEL_CHANGED → SCENE_REFRESH_REQUESTED → ROUTE_EVENT_RESOLVED`。
- 他人地产收费先发出 `GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED` 并冻结路线游标；SDL 完成原版音效、同组闪烁、1500ms 金额框和条件对白后调用 `rich4_route_event_executor_resume_charge`，这时才进入 `rich4_land_charge → rich4_ledger_pay`。流动资金不足时严格按 `0x41D2C6 → 0x40CD87` 转移实际余额并立即破产。
- owner `+65` 指向第三名玩家时，收费计划先计算 owner 份额和关联玩家原始份额，再以关联玩家为第二收款人调用 `rich4_ledger_pay_split`；充足余额与不足余额都保证 owner → 关联玩家的事件顺序，且在同一事务中提交。金额窗口显示两名收款人及各自份额，收款语音在主收款人之后继续覆盖关联玩家。
- SDL 收到真人购买/升级交互后停住路线。IDA 复核确认 `0x440BA8` 不使用 `Panel[112]`，而是调用 `0x453A32(220,320)`；`0x453A64` 通过 `0x48A0E4` 读取 `Data[440]` 作为通用是/否按钮资源，归档绑定由 `0x40172E` 确认。SDL 现直接展示该资源，不再自绘地产专用矩形框。
- `0x45367E` 把按钮资源原始矩形的左、右半区分别返回 1、0；左键落在资源外返回无动作，右键/取消键拒绝，明确确认键接受。输入适配器采用同一三态规则，因此点击地图或框外不会提交扣款命令；领域层仍以 typed subject 和 turn token 再验证。
- 成功后从权威 owner/level 状态重建地图标志/建筑：`level == 0` 保留 owner 角色标志，`level > 0` 切换通用建筑并应用 owner palette。所有五级 view 资源及设施升级 bucket 均在地图装载期登记，并播放 `0x4823D2`/`0x4823DA` 对应的 Effect.mkf[49]/[50]。
- 状态、路线队列和事件先在副本中组合；事件整体追加成功后才提交。价格、资产与收费公式使用 64-bit 中间值并拒绝超过 `INT_MAX` 的结果。

## 当前 deferred

- 普通地产的 owner `+65` 双收款分支已闭合；设施 `0xFA1..0x176F` 继续属于 M3.4。`0xFA0` 是原版表基址哨兵，没有复制普通地产公式。

## 测试与置信度

- `tests/unit/test_property.c`：购买接受、现金不足而银行充足、交易禁止、god 12、0→5 级升级、等级上限、同组收费、price-status 翻倍、自己地不收费、神明 1/2、0、3/2 倍、正常付款、流动资金不足破产、AI 保留线、双收款分摊与事件顺序、溢出与事件容量回滚。
- `tests/unit/test_ledger.c`：双收款充足余额、余额不足时按第一收款人优先分配以及破产后的事务结果。
- `tests/unit/test_route_event_executor.c`：真人购买交互与 token、不可购买 road stop 不扣款、AI 自动升级、收费 presentation 暂停、普通收费恢复、双收款恢复、恢复后付款/淘汰且路线正常结束。
- `tests/unit/test_property_flow.c`：双收款收费窗口提取两名收款人与各自份额，并保持既有金额/对白时序。
- `tests/unit/test_character_speech.c`：12 角色资源号/正文表、发言状态抑制、同名计数、关系目标及 2000/5000/9000 阈值。
- `tests/unit/test_sdl1_input_adapter.c`：通用按钮左右半区边界以及资源矩形外的无动作结果。
- `tests/unit/test_map_renderer.c`：`level == 0` 的角色标志、建成后的旋转 variant/owner palette 和四名玩家颜色。
- `tests/replay/test_route_event_executor_replay.c`：真人拒绝购买的无 UI 固定 replay；既有事件枚举数值不变。
- `tests/resource/test_map_data.c`：六项收费表的原始偏移解析。
- Windows `make -B test sdl test-original-data` 全部通过，包含专项地产/账本、unit/resource/fault-injection、固定 replay、SDL 链接和 V3.11 原始资源扫描；`-Wall -Wextra` 无警告。
- WSL `make test-sanitize` 通过包含 `test-property` 的全部 ASan/UBSan 矩阵，无 sanitizer 诊断。
- 置信度：普通单 owner 与第三方关联双收款地产的购买、升级、收费、AI 决策、账本接入、路线恢复和 SDL 表现均为 `confirmed`；设施专属公式继续归入 M3.4。
