# 保险理赔：原版范围与 SDL 分层对应

## 地址、来源和置信度

固定程序为 V3.11 `Rich4.exe`，SHA-256：
`5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`。

本对话于 2026-10-14 使用 IDA Pro MCP 复核模块、散列、`0x44BA63`、
全部六个直接调用点，以及命运事件的跨函数共享跳转。
2026-10-15 的 SDL 对齐继续采用该输出和本地反汇编；本轮 IDB
`not reverified`。出国的外星人模式另由 `0x44C6ED → 0x44C6D8`
共享跳转及 `0x40D375` 汇编核对。

置信度：`partial`。下表的资金、计数、保险通知及收容继续流程有回归覆盖；
所有地图中的完整事件抽取、被动卡交互选择和整条旅行 FLC 不是本记录的验证范围。

原版本地证据：

- `rich4/asm/rich4.asm:67743`：`0x44BA63`，玩家保险字节 `+62`、
  第一家 type-4 公司、2000ms 提示与转账。
- `rich4/asm/rich4.asm:31195`：`0x41D2C6`，公司 `+40/+44`、
  玩家现金 `+28` 和月度收入 `+96`。
- `rich4/asm/rich4.asm:12999`：`0x40D375`，出国／外星人带走，
  初次理赔、累计天数及再次消失的追加分支。
- `rich4/asm/rich4.asm:27747`：旅馆入住及 `0x41A82D` 理赔调用。
- `rich4/asm/rich4.asm:60105`、`:61971`：`0x43D593/0x43EC3F`，
  入狱／住院追加与本次天数理赔。
- `rich4/asm/rich4_fortune.asm:368`、`:1438`、`:1744`：冒贷、
  扣款后的理赔及十项金额事件的共享入口。
- `rich4/asm/rich4_fortune.asm:778`：出国与外星人命运共享调用。
- `rich4/asm/player_info.h:54`：保险字段 `days_assurance`。
- [入狱与送医时序](confinement-entry-timing.md)、
  [旅馆](hotel-gas-station.md)、
  `rich4/docs/facility_company.md`、`rich4/docs/tools.md`。

## 层 1：统一资金和保单条件

`src/core/insurance.c` 对应 `0x44BA63 → 0x41D2C6`。

| 原版规则 | SDL 对应 |
| --- | --- |
| 保险字节非零即满足保单条件，`0x80` 也通过 | 取 `days_assurance & 0xff`，不只看低七位 |
| 按地图公司记录顺序找第一家 type 4 | `rich4_insurance_find_company` |
| 不检查持股、董事长或股票映射是否有效 | 理赔不以 stock index 为付款门槛 |
| 提示 2000ms 后付款，支持释放输入提前返回 | request / settle 两阶段，app notice FIFO 与 skip |
| 赔款加到现金，同时记玩家月度收入 | `cash`、`monthly_income` 做原版 32 位加法 |
| 公司两项账目均扣金额；不按余额削减赔款 | period/total business 做 32 位减法 |
| 理赔不消费保险天数，没有次数或金额累计上限 | 结算只修改资金和公司账目 |

即时 core API 表示完整阻塞函数返回后的状态；运行时 API 先发通知请求，
由 app 在通知返回后结算。结算不再发第二个通知。

## 层 2：按事件天数补偿

金额统一为 `2000 * price_index * 本次事件天数`，不以保期剩余天数或费用计算。

| 原版入口 | 覆盖来源 | SDL 对应 |
| --- | --- | --- |
| `0x41A82D` | 实际入住旅馆，转盘 1..4 天 | `facility.c`、`insurance_flow.c`、`hotel_flow.c` |
| `0x40D425` | 航空公司、强迫出国、被外星人带走 | `confinement_disappear`、`company.c`、`special_event_commit.c` |
| `0x43D749` | 陷害／复仇、魔法屋、违法超贷新闻、坐牢命运 | `confinement.c`、`card.c`、special event 模块 |
| `0x43EDF8` | 恶犬、地雷、定时炸弹、飞弹／核弹、外星人新闻、受伤命运、魔法屋 | `confinement.c`、`tool_flow.c`、special event 模块 |

- 入狱、住院的追加分支仍执行本次天数理赔；剩余天数保留低七位。
- 同类计数为 `0x80` 时也走追加分支，跳过初次反应与入场动画。
- 出国／外星人带走只有 `+51 == 0` 的初次分支理赔和累计 `+66`。
  追加分支写 `(旧计数 & 0x3f) + 本次天数 + 本次模式位`，不再理赔或累计。
- 旅馆、出国、坐牢、住院的累计天数 `+66` 按字节回绕，保险返回后才累计。
- 出国与住旅馆保留交通方式、骰数；受伤调用方另外执行 `0x40CD07`。
- 特殊人物 4..7 没有普通玩家保险流程。

## 层 3：按事件金额补偿

`src/core/special_event_commit.c` 对应下列原版命运文本。
`P` 为物价指数。神明免除则没有该笔理赔；加倍则按加倍后的金额理赔。

| 文本 | SDL resolved ID | 基础金额 | 原版入口 |
| --- | ---: | ---: | --- |
| `#0187` 人头被盗用冒贷 | 2 | `10000P` | `0x44C0E8` |
| `#0199` 行人闯越马路罚款 | 14 | `3000P` | `0x44CD99` |
| `#0200` 未戴安全帽罚款 | 15 | `3000P` | `0x44CF1E` |
| `#0201` 汽车超速罚款 | 16 | `3000P` | `0x44D06D` |
| `#0202` 请所有人吃大餐 | 17 | `6000P` | `0x44D0D6` |
| `#0203` 乱丢垃圾罚款 | 18 | `600P` | `0x44D1A5` |
| `#0204` 小狗乱大小便罚款 | 19 | `1500P` | `0x44D1E0` |
| `#0208` 遗失钱包 | 23 | `1000P` | `0x44D41E` |
| `#0209` 遗失钱包 | 24 | `2000P` | `0x44D462` |
| `#0211` 被倒会 | 26 | `8000P` | `0x44D4E7` |
| `#0215` 付保险金 | 30 | `5000P` | `0x44D5F4` |

冒贷增加现有贷款，首次贷款按 `0x433B7E` 设置并校正 90 天期限；
已有期限不延长。理赔支付现金，贷款仍保留。
其余十项先扣款，付款后仍在局、流程未结束才理赔；保险不提供破产前垫付。
“付保险金”是金额事件，不写保险天数。

## 层 4：受益人和表现继续流程

- 旅馆和航空公司的最终付款人同时承受住宿／出国，检查该人的保险。
- 陷害、嫁祸和复仇的理赔以最终实际受罚者为受益人；嫁祸回施放者的
  入狱天数为 4，通常目标及复仇为 5。
- core 请求收容后，app 完成天数反应、状态提交、可选 FLC、设施同步及
  收容对白，再发保险通知。通知返回才付款及累计本次天数。
- 命运金额事件保留原事件反应对白，随后处理保险通知。
  出国／外星人命运在天数反应后、旅行动画前处理理赔。
- 通知的本次天数随事件保存，累计不依赖结束时反推物价。

对应实现：`game_dispatch.c`、`game_command_flow.c`、`command_event_flow.c`、
`tool_animation_flow.c`、`special_presentation_flow.c`、
`special_presentation_update_flow.c`、`special_event_timeline.c`、
`game_presentation.c`、`insurance_flow.c`。

## 层 5：保期与未覆盖损失

`status_countdown.c` 对应 `0x41C84F`：收容／出国／住宿期间保险继续递减；
1 递减为 `0x80`，下次状态清理置零。保险公司增加天数保留低七位。

普通租金、购物／加油／修车等公司费用、建筑／地产损失、股票损失、
银行卡资金盗取、卡片道具点券损失、车辆价值及梦游冬眠等状态，
不因保险而产生额外赔款。同一事件导致住院时，另按住院天数补偿。

## 测试

- `tests/unit/test_insurance_alignment.c`：11 项金额事件 × 2 个物价 ×
  3 种保期 × 3 种神明结果 × 2 种提交方式，共 396 组；
  另覆盖重复收容、出国模式、受益人、破产、车辆损失、到期字节、
  公司账目回绕、容量失败事务，以及卡片的运行时收容与通知结算。
- `tests/unit/test_insurance.c`、`test_insurance_flow.c`：统一结算与 FIFO。
- `tests/unit/test_special_presentation_flow.c`：真实命运提交保留理赔，
  在事件反应后显示，提示期间现金保持扣款后的值。
- `test_facility.c`、`test_card.c`、`test_tool.c`、`test_confinement_god.c`、
  `test_tool_animation_flow.c`、`test_status_countdown.c`：调用方及原状态边界。
- 命令：`python scripts/make_minimal.py test-insurance test-unit sdl`。
