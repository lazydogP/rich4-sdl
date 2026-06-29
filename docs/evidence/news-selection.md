# 新闻抽取与目标选择：原版证据和 SDL 对应

## 地址、范围与 IDA 结论

- 模块：`Rich4.exe` V3.11，image base `0x400000`。
- SHA256：`5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`。
- 最近复核：2026-10-22，IDA Pro MCP；当前 IDB 的 module、输入路径与上述固定参考一致。
- 置信度：`confirmed`（排列、资格、随机候选、排名、区域建筑/人物目标、29 的被动目标链）；本记录不替代股票每日行情、税款账务和所有动画的专项验证。
- 原版完整目录：[新闻格行为](../../../rich4/docs/news.md)。
- 原始证据：[59 个函数及 36 项 callback](../../../rich4/docs/evidence/news-selection-ida.json)。JSON 的 `functions[地址].asm` 保留逐条指令地址，`code` 保留反编译，`handlers`、`title_classes`、`strings` 保留原表。

## 原版规则与实现位置

| 原版证据 | 原版规则 | SDL 对应 |
|---|---|---|
| `0x448B81 / 0x499090` | 升序未使用 ID 中取第 `rand()%remaining` 项，36 次随机；最后 `%1` 仍消费随机字 | `special_event.c:shuffle`、`rng.c` |
| `0x44B718..0x44B7E1 / 0x4990E0` | 共用排列与游标，跳过也推进，末尾回零不洗牌 | `rich4_special_event_select`、`begin_news` |
| `0x448C99 / 0x448CAB`、`0x448ECA..0x44913C` | 0..3 只看普通玩家 4 槽占用，特殊人物不使事件合格；自然待释放 `0x80` 和新闻已清占用的 `0x80` 不同 | `news_confinement_target`、`news_prepare_effect`；`confinement_actor_type` 区分待释放占用和新闻已释放的槽 |
| `0x449175..0x4491DD` | 4 在 phase 1 才抽已建 H+B 中心，phase 0 不消费本次目标随机数 | `prepare_news_phase1`；app `prepare_news_area_view` 在 2400ms 页面结束且正文语音结束后准备 |
| `0x4492C5..0x44931F / 0x449759..0x4497B7` | 5 从已建 H+B 合并抽取，7 从无主 H+B 合并抽取 | `choose_property`；住宅在前、设施在后，不先随机分类 |
| `0x4494E0 / 0x44A220 / 0x44A6E0` | 6/14/18 从全部 H+B 抽中心；住宅按原名称扩展、设施仅一项，效果在 phase 1 | `land_group` 来自地图原始名称；`special_event_commit.c`、`scale_property_price` |
| `0x4498B3 / 0x449A8A / 0x449B9C` | 8/9 数 H+B 地块，10 数总股数；严格比较，平局保留较小玩家槽 | `player_property_total`、`player_stock_total`、`extreme_player` |
| `0x409DE7 / 0x40A45C(100)` | 4/20 取聚焦视口中央 `[120,320)` 方区的资源锚点，同像素 raw 按位 OR | 平台复用 `game_map_collect_ai_objects`，HUD Y 减 40；`GameNewsAreaAnchor` 传递借用事实，core `apply_news_area` 选目标 |
| `0x40AC7B` | 4 选区内 H/B 清除和人物；20 仅选区内 H/B 降级；C 不被选作破坏目标 | `apply_news_area`、`special_event_commit.c`；没有世界坐标方框后备近似 |
| `0x44A91E / 0x44AC99` | 19/21 不筛建成或 owner，选空地不重抽 | `choose_property(0,0,0)`；单地清除/降级 |
| `0x44A5D6 / 0x44A657` | 16/17 比较完整交通 byte 的零/非零，所有匹配活动玩家都停留 | `news_prepare_effect` |
| `0x44B0D1 / 0x44B1A3` | 27 全 12 股抽取，停牌字段写 15；28 从已停牌股中抽取 | `news_prepare_effect`；保存股票索引用于页面名称，不另抽 |
| `0x448E64..0x448E99 / 0x40D73F` | 29 资格只需一家公司经营者活动且旅馆/出国/监狱/医院四字段全空 | `news_eligible(29)`、`company_operator` |
| `0x44B285..0x44B29E` | 29 候选只要求有经营者，不重复资格中的离场筛选，也不要求有效股票链接；按公司记录入池，重复经营者保留权重 | `news_prepare_effect(29)` 保存公司和经营者，候选容量按公司数量上限 |
| `0x44B349 / 0x441210` | 29 先免罪、再嫁祸，最后实际代受者入狱；不递归代受者的卡 | 复用 `fortune_target`，app/SDL 的被动输入和页面以经营者而非新闻触发者为 actor |
| `0x44B374..0x44B5F4` | 30..34 全部 C 抽取，不按类型或股票链接过滤 | 按公司索引选择，修改 `company_period_business/total_business`，股票有效才写关联行情参数；33 是扣 10000 |
| `0x448E9C / 0x44B618` | 35 只筛有符号本期盈亏 `+0x28>10000`，不是发行估值 | `company_period_business`；负值和等于 10000 排除，无股票链接的公司仍在池中 |
| `0x475EB4 / 0x475ED8` | 分类边界为 0..5、6..13、14..15、16..17、18..21、22..35 | `special_event_text.c:news_title_index`；分类只展示，不影响抽取 |

原版辅助来源：[主汇编](../../../rich4/asm/rich4.asm)、
[新闻汇编](../../../rich4/asm/rich4_news.asm)、[逆向 C](../../../rich4/csrc/news.c)。
字段语义另见 [设施与公司](../../../rich4/docs/facility_company.md#12-公司记录与董事长)。

## 目标保存、提交与分层

- 房产目标保留 16 位索引、设施位、原 owner 高字节。4 的目标到 phase 1 才保存，随后同步 app 的演出目标和受害者 mask。
- 0..3 保存本次实际收容目标 mask，页面不能从状态已修改后的天数重新猜测头像集合。
- 27/28 保存股票索引；30..35 保存公司索引；29 同时保存公司索引和原经营者。正文使用实际股票/公司名称，29 两个 `%s` 分别使用公司名和经营者名。
- 原版区域对象来源复用已有渲染队列筛选，保留资源存在、overlay 排除、动态附着排除和八方向投影。平台提供坐标、raw ID 及借用的地图；core 合并同像素标识，按屏幕行优先确定建筑、人物和放置道具，app 接续神明/狗的配对重生。
- 区域事实缺失时提交被拒绝，不回退世界坐标近似。app 在状态副本上应用区域效果并提交路线命令，失败不发布部分破坏；成功后同步目标快照。已有 `GameState` 和存档布局不增加区域缓存或借用指针。
- 序列、游标、目标和随机字继续经过已有快照；目标预备重复调用不重复消费随机数。

## callback 的结算与动态对象

原版详情见[新闻行为第 9 节](../../../rich4/docs/news.md#9-金额报价与清除字段)，
补充 MCP 原文见[效果证据](../../../rich4/docs/evidence/news-effects-ida.json)。

| 原版地址 | 原版规则 | SDL 对应 |
|---|---|---|
| `0x4498B3..0x449C7B` | 8..10 在 phase 0 保存获奖者与金额，phase 1 发现金 | `special_event_player_amount`、`special_event_commit.c` 的新闻分支 |
| `0x41D407/0x41D40F/0x41D418` | 现金、存款及收入按 32 位 ADD 回绕；phase-0 金额乘法同样保留低 32 位 | `add_money` 使用无符号中间值；页面及奖励反应读取保存金额 |
| `0x402F99/0x402FA1` | 运行时读档重新以当前计时播种，保留新闻排列与游标 | 游戏内、菜单读档均调用 `rich4_game_save_resume`；仅解码存档不消费或重播随机流 |
| `0x449C7C/0x449DE6/0x44A029` | 11..13 保存完整税额，phase 1 按玩家顺序付款，遇结束标志停止 | 准备只写金额；提交调用 `rich4_ledger_pay`，不将不足余额作为税额上限；其他玩家破产不使路线触发者被标为出局 |
| `0x449DE6` | 地价税以 H/B 原始价格、建设价与等级求和，截税率后乘 P | 扫描地产字段，忽略资产缓存和住宅特殊形态估值 |
| `0x44A029/0x41D2C6` | 股票税市值每项存回 float32；实际付款计损失与彩票奖金池 | float32 累加器，公用账务入口负责现金/存款、损失、奖池、破产和结束事件 |
| `0x4494E0/0x44A220` | double 1.3/0.7，x87 中间乘积，CHP 截断后写 word | `original_product` 用整数分段运算保留 x87 64 位有效位，避免 ARM long double 精度差异；地产写回 uint16_t |
| `0x429040` | 24/25 更新所有股票，30..35 更新有效关联股票，现价与历史立即变化 | `rich4_stock_apply_forced_quote`，不推进历史游标 |
| `0x44B0D1` | 27 停牌时恢复参考价，覆盖历史上一格 | 直接写 `f20=f16` 和游标前一格；仍保存所选股票 ID |
| `0x40AB4A/0x40AC7B` | 清住宅时清 `+0x30` | `clear_property` 清 `land_runtime_value` |
| `0x40A45C/0x40E14D` | 同像素 OR，按行优先清动态对象、归还工具、配对重生消费 RNG | `news_area_next_raw` 共用于 core 和 app；core 归还放置工具，app 按同一顺序清占用及重生；缺地图时拒绝需要重生的提交 |

app 在提交前保留动态对象数组，路线命令失败时恢复，状态与 RNG 仍从副本发布。
地图指针只在本次同步调用中借用，不进入状态、快照或存档。

## 用户指定的兼容例外：ID 15

本次保留 `news_eligible(15)` 和 `choose_property(...,built_only=1,land_only=1)`：
资格和候选均只考虑等级非零的住宅。没有住宅时跳过新闻 15，不消耗其目标随机数。
不复制原版“设施可使资格成立，住宅候选却为零”的除零 BUG，不把设施加入实际候选。
依据：[原版专项](../../../rich4/docs/evidence/news15-stay-card-ida.json)、
[已有兼容约定](m3-8-special-events.md#新闻15原版已知的零住宅候选-bug)。

## 验证

- `python scripts/make_minimal.py test-news-selection`：固定种子原排列、0..3 普通/特殊收容与待释放标志、4 的 phase-1 随机消费、像素区间/同像素 OR、20 受损对象、同名住宅、股数平局、29 的资格/候选差异及免罪/AI嫁祸/真人取消、13 家公司不被 12 股截断、35 正负盈亏与严格门槛、27/28 和公司名称快照、36 项新闻目录及 ID 15 原有回归。
- `test-news-selection` 还覆盖奖励/税款延后结算、原始地产税基与物价、股票税 float32 精度、其他玩家破产、结束后停止征税、队列满回滚、即时行情/历史回绕、地价 word 回绕与原生 x87 数值参照、动态工具归还和附着炸弹解除。app 用例验证动态对象重生顺序、缺地图拒绝和提交失败回滚。
- 奖励边界覆盖 8/9/10 的现金与收入回绕、23 的存款与收入回绕、贷款排除和 phase-1 不重复支付，以及金额乘法回绕为负数后继续结算；反应读取保存金额，并覆盖门槛乘法回绕后的台词随机消费。
- `test-save-load-replay` 验证运行时读档以当前计时重新播种，新闻 27 保留原排列及游标，但以新随机流选择股票；同时覆盖计时回绕和对已解码存档的重复读取。
- `tests/unit/news_selection_cases.inc` 是 `test_special_event.c` 的专项用例，Makefile 声明包含文件依赖。
- `tests/unit/test_special_presentation_flow.c`：新闻 29 的触发者和经营者不同，真人嫁祸可选择触发者、不能选择自己；免罪无收容；4 在页面结束后选中心并将投影目标传入提交。
- `python scripts/make_minimal.py test-ai-pixel-candidates`：8 个方向的原锚点坐标、资源存在和同像素 raw 表；新闻 4/20 复用该来源，缺失资源的邻近建筑不进入受影响集合。
- `python scripts/make_minimal.py test-fortune-selection-sdl`：原版 MKF 被动面板、鼠标输入、新闻 29 的经营者目标排除和单人确认，兼顾共享命运路径。
- `test_special_event_text` 验证标题边界和两个独立 `%s` 名称的有界展开。

## 边界

本次资格/候选核查保留合法地图、存档和玩家槽的既有约束，不仿真非法 raw ID 访问越界。
事件队列容量属于 SDL 内部交付条件；交付失败时保留可重试状态，不丢弃演出或发布部分结算。
本记录覆盖抽取、候选和下述 callback 状态结算；完整逐帧演出及像素结果仍需独立核对。
