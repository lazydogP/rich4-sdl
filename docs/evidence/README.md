# 行为证据台账

本目录记录 `rich4-sdl` 中行为、资源映射和渲染规则的来源。实现前先更新对应记录；记录不能替代测试，也不能把推测升级为原版行为。

## 固定参考

- 模块：`Rich4.exe` V3.11
- SHA-256：`5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`
- Image base：`0x400000`
- 架构：32-bit x86

开始新的行为分析时必须通过 IDA Pro MCP 重新确认当前 IDB 的模块和散列。2026-07-13 再次通过 IDA Pro MCP 确认当前 IDB 为 `Rich4.exe`、32 位 x86、image base `0x400000`，SHA-256 与固定参考一致。索引中的置信度按当前专项实现和测试更新；长函数中未迁移或仅 characterization 的分支继续保持 `partial`。

## 记录格式

每个函数簇使用一份 Markdown 文件，并包含：

1. **地址与目标模块**：原版地址、职责边界和预定模块。
2. **IDA 结论**：最近一次 IDA 复核结果；未复核必须直写 `not reverified`，不得从其他来源推断。
3. **本地证据**：`../rich4/asm`、`../rich4/csrc` 或 `../rich4/docs` 的具体文件和行号，以及能直接观察到的事实。
4. **置信度**：只允许 `confirmed`、`partial`、`unknown`。
5. **测试链接**：已有测试或目标文档中规定的待建测试；待建测试必须注明 `planned`。
6. **缺口/冲突**：尚未证明的语义、来源冲突和下一次复核动作。

置信度含义：

- `confirmed`：当前固定 IDB 已复核，且关键控制流/字段或资源索引有测试覆盖。
- `partial`：有反汇编、还原代码或资源证据，但 IDA 复核或完整测试仍缺失。
- `unknown`：现有证据不足以支持行为实现。

## 函数簇索引

| 地址 | 记录 | 当前置信度 |
|---|---|---|
| `0x448B81`、`0x448BE2`、`0x44B6DF`、36 项 callback、`0x40A45C` | [新闻抽取与目标选择](news-selection.md) | `confirmed`（抽取/候选、经营者防御、税款与破产、即时股价、地价字宽和动态对象；ID 15 保留避错兼容例外，逐帧边界见正文） |
| `0x44BA63`、`0x41D2C6`、四类天数补偿及11项金额命运 | [保险理赔与 SDL 分层对应](insurance-claims.md) | `partial`（资金、保期、继续流程有回归；完整事件选择与旅行 FLC 见记录边界） |
| `0x418C55`、30张卡、13种道具、股票/资金/落地/被动链 | [AI 原版策略与 SDL 逐项对应](ai-strategy-alignment.md) | `partial`（已列策略条件有证据和回归；整局随机流、完整像素对象来源及非法raw兼容见边界） |
| `0x407AD2` | [地图资源初始化](407ad2-map-lifetime.md) | `confirmed` |
| `0x407A2C` | [地图 sub-tile 投影](407a2c-map-projection.md) | `confirmed` |
| `0x40829D` | [可见对象与地图绘制](40829d-map-render.md) | `confirmed` |
| `0x415F69`, `0x4166F8`, `0x4169BC`, `0x416E6D`, `0x417E26` | [日月历、小地图与组合画面](417e26-window-layouts.md) | `confirmed` |
| `0x40A4E1`, `0x40A801`, `0x40A9BD`, `0x417D65`, `0x417E26` | [顶部“地图”按钮与全地图模态 UI](40a9bd-small-map-ui.md) | `confirmed` |
| `0x40AF12` | [地图对象解析](40af12-map-object.md) | `confirmed` |
| `0x40B93B` | [玩家 actor 资源状态](40b93b-player-actor.md) | `partial` |
| `0x41A421`, `0x41A4DB`, `0x40D5A5`, `0x40C05C` | [加油站、旅馆收费及入住/退房](hotel-gas-station.md) | `confirmed` |
| `0x40C912` | [玩家行动/阻塞状态](40c912-player-status.md) | `confirmed` |
| `0x415872` | [开局动画流程](415872-intro.md) | `confirmed` |
| `0x418EBD` | [回合轮转](418ebd-turn.md) | `confirmed` |
| `0x41CF67`, `0x439BFA`, `0x42BA97`, `0x431712`, `0x428EC5` | [月结、公司分红、乐透及股票取整](monthly-finance-lottery.md) | `confirmed`（账务与抽号） |
| `0x418C55`, `0x41982D` | [M3.0 玩法协议与稳定 ID](m3-0-gameplay-protocol.md) | `confirmed`（协议边界） |
| `0x418E7F`, `0x41982D` | [M3.1 路线事件执行器](m3-1-route-event-executor.md) | `confirmed`（调度/暂停边界） |
| `0x414858`, `0x414BBC`, `0x414FCD`, `0x42D37F` | [点券小游戏与百货/货栈](special-place-point-games-shop.md) | `confirmed`（action 6/7/8/15 真人 UI、交互、资源与音效） |
| `0x41982D`, `0x44EF41`, `0x44F354..0x44F627`, `0x451985` | [地产弹窗、角色发言、收费演出与代表色](41982d-property-dialog-color-delay.md) | `confirmed`（普通单 owner 表现链） |
| `0x40B93B`, `0x40C78C`, `0x40C912`, `0x41BCEB`, `0x41BE5F`, `0x41D559`, `0x42D37F`, `0x43CF97`, `0x441F73` | [2026-08-09 八项表现与交互修正](2026-08-09-eight-presentation-fixes.md) | `confirmed`（资源、事件顺序、交互和回归测试） |
| `0x40CD87`, `0x41D2C6`, `0x41D709`, `0x4239B9` | [M3.2 账本、清算与破产](m3-2-ledger-bankruptcy.md) | `confirmed`（经济事务边界） |
| `0x419744`, `0x4198B9..0x41A168`, `0x40FA61`, `0x41D7D4` | [M3.3 普通地产](m3-3-ordinary-land.md) | `confirmed`（单 owner 普通地产） |
| `0x41A17F..0x41A9AF`, `0x4239B9`, `0x428CAF` | [M3.4 设施与公司地产](m3-4-facility-company.md) | `partial`（购买/建设/升级/三类收费公式/轮盘/关联已接入；资源演出边界 deferred） |
| `0x41E69E`, `0x441262`, `0x441343`, `0x441BAA`, `0x475D5C` | [M3.5 卡片系统](m3-5-card-system.md) | `partial`（目录/库存/事务及 1、14、30 已确认） |
| `0x420E9A`, `0x447D97`, `0x40E033`, `0x40E14D`, `0x41B42D`, `0x475DD5` | [M3.6 道具系统](m3-6-tool-system.md) | `partial`（13 项目录/handler、库存/放置物/交通事务已接入；剩余边界是 AI 全局目标表） |
| `0x40CC1A`, `0x40E14D`, `0x40E32C`, `0x41C84F`, `0x43D593..0x43D88F`, `0x43EC3F..0x43EF3E`, `0x444C45` | [M3.7 神明、监狱、医院与持续状态](m3-7-god-confinement-status.md) | `partial`（关系、权限、进入/释放、请神目标、动态资源 8/6/1、396..415 RGB555 像素基线与 128..395 角色资源 8 方向契约已确认；剩余为完整 SDL 场景/少数特殊演出的逐帧截图核对） |
| `0x448B81`, `0x448BE2`, `0x44B6DF`, `0x44BAEA`, `0x44BB4B`, `0x44DB81`, `0x431842`, `0x431CAA`, `0x43380A` | [M3.8 新闻、命运、魔法屋与特殊事件](m3-8-special-events.md) | `partial`（两阶段协议、36 新闻/49 命运 handler、魔法屋 12 动作已接入；剩余为资源/逐像素表现边界） |
| `0x431842`, `0x431CAA`, `0x432511`, `0x4325C2`, `0x43380A` | [魔法屋完整行为、资源与交互 UI](43380a-magic-house.md) | `confirmed`（12 条件、12 动作、Panel 18/19/20、语音/音乐/音效） |
| `0x428D01`, `0x428D2A`, `0x428E23`, `0x428EC5`, `0x42915A`, `0x4291D6`, `0x4294D5` | [M3.9 股票交易、日更与资产估值](m3-9-stock-system.md) | `confirmed`（普通买卖、成交量、公司排名、日更/历史/恢复边界） |
| `0x418C55`, `0x40B455`, `0x41E69E`, `0x41FE4E`, `0x420E9A`, `0x42BF03`, `0x42C79F`, `0x441BAA`, `0x447D97` | [M3.10 三档基础 AI](m3-10-basic-ai.md) | `partial`（三档配置、独立/公司股买卖主体、购地/拍卖/冬眠/均贫/红卡/黑卡/查税卡/同盟卡目标、停留/梦游/陷害的特殊人物安全子集、卡/道具门槛及纯命令选择已确认；其余全局目标表 deferred） |
| `0x41D89E` | [胜利条件](41d89e-victory.md) | `confirmed` |
| `0x418EBD`, `0x41CF67`, `0x40CD87`, `0x41D89E` | [M3.12 无存档整局核心 replay](m3-12-replay.md) | `partial`（核心与真实 gmap0 app/AI 回放、SDL 菜单/游戏 smoke 可重复；完整 SDL 逐像素基线与 M3.11 存读档等价仍待完成） |
| `0x450CED`, `0x45144F` | [Autodesk Animator FLC](450ced-45144f-flc.md) | `confirmed` |

M2 非视觉边界的跨函数收口记录见
[M2 non-visual closure](m2-nonvisual-closure.md)；它不会把 `0x40B93B`
尚未确认的 part 语义升级为 `confirmed`。

游戏固定文字、收款与贷款通知、股票和地图选择界面的原版行为见
[原版游戏文字与选择界面](game-text-original-behavior.md)。

整局随机状态、每天重播种、初始化消耗顺序与三类440×440候选表的原版规则见
[整局随机流与 AI 像素候选表](ai-rng-pixel-compatibility.md)。
