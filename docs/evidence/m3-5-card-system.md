# M3.5 卡片系统证据与 30 项清单

> 原版卡片行为、主动/被动优先级、收费链，以及静态/动画/可交互 UI 资源的完整清单见
> [`rich4/docs/cards.md`](../../../rich4/docs/cards.md)。该文档已用 IDA Pro MCP 复核。

## 地址与目标模块

- `0x499120`：四名玩家共 60 bytes 卡片槽，每人连续 15 个原版 card ID。
- `0x499198`：30 bytes 公共卡片数量。
- `0x441262`：统计玩家非零卡槽。
- `0x441343`：消费指定卡，删除首个匹配槽、左移并增加公共数量。
- `0x441BAA`：真人/AI 使用卡片总入口；真人面板使用 `Panel.MKF[11]`。
- `0x41E69E`：AI 卡片能力门槛及逐卡 eligibility 分派。
- `0x475328`：30 项 eligibility 函数表；第 30 项与随后表连续。
- `0x475D60`：30 项 handler 指针表的真实首地址；调用表达式使用
  `[card_id*4 + 0x475D5C]`，其中 `0x475D5C` 是表首减 4。
- 目标实现：`src/core/card.[ch]`、`src/core/game_dispatch.c`、`src/core/game_types.h`。

## IDA 结论

2026-07-22 已重新调用 IDA Pro MCP，对固定 V3.11 的原始 `Rich4.exe` 复核入口表、
`0x441F73` 效果窗、`0x445E4D` 地图目标选择器、四个被动 handler 以及
`0x41982D`/`0x4451F0` 收费链。完整证据与资源边界见上方链接；SDL 迁移状态仍以本文件
“当前状态 / 测试”列为准。

## 通用行为

- `_card_table` 每项 8 bytes：名称指针和四个 byte 元数据。`0x41E69E` 对原版 ID `c` 读取表项最后一个 byte（代码寻址为 `[c*8 + _card_table - 1]`），减当前玩家记录 `+23`。差值 `>=2` 返回不可用；差值 `==1` 仅在 `rand()%3 == 0` 时继续；随后调用 `0x475328[c-1]` eligibility。
- 真人选择取消得到 0，不调用 handler。AI 最多从持有卡中取连续/环绕的 8 项候选，逐项调用 `0x41E69E`，首个返回 1 的候选进入同一 `0x475D5C` handler 表。
- 原版 handler 自己决定消费时机；统一核心事务将其等价约束为“效果和事件全部可提交后成功一次消费”。失败、取消或 deferred 不消费。
- 名称来自 `rich4_card_table.c`/`_card_table`（`0x47FDEA`）；使用提示由 `0x441BAA` 的 `0x465305` 格式串和 `0x441F73` 生成；角色语音/文案来自 `_card_strings`（`0x48123A` 起）。下表旧列标题写作“handler 指针槽”，但其中地址实际是调用表达式基址的逐项偏移；真实槽地址应整体加 4，即 `0x475D60 + 4*(ID-1)`。实际 handler 地址和逐项资源见完整文档。

## 30 项目录

| ID | 名称 | 元数据 `数量/价/f6/f7` | 目标 | handler 指针槽 / 符号 | 前置条件与主要副作用 | 当前状态 / 测试 |
|---:|---|---|---|---|---|---|
| 1 | 均富卡 | `1/200/2/2` | 无/全局 | `0x475D60` / `junfuka` | 至少一名活动玩家；现金取整数平均；富者还写 `(cash-average)/100` 的玩家对累计量 | implemented / 现金、关系累计量、AI eligibility 与前置效果顺序均有测试/实现 |
| 2 | 均贫卡 | `2/200/2/2` | 玩家 | `0x475D64` / `junpinka` | 选择其他活动玩家；双方现金取平均 | implemented / `test_card`, SDL 地图选择 |
| 3 | 购地卡 | `4/35/0/1` | 当前地产 | `0x475D68` / `goudika` | 他人地产且现金足；付款并转 owner | implemented / 核心事务 |
| 4 | 换地卡 | `4/25/0/0` | 地产 | `0x475D6C` / `huandika` | 两处地产可交换；交换 owner | implemented / SDL 地图选择 |
| 5 | 换屋卡 | `4/20/0/0` | 地产 | `0x475D70` / `huanwuka` | 两处地产可交换；交换 level | implemented / SDL 地图选择 |
| 6 | 转向卡 | `3/20/0/0` | 角色 | `0x475D74` / `zhuanxiangka` | 有效角色；反转行进方向 | implemented / `test_card` |
| 7 | 改建卡 | `8/15/0/0` | 当前地产 | `0x475D78` / `gaijianka` | 当前住宅/设施已建设；handler 不检查 owner；切换住宅 type 或选择设施 subtype | implemented / 真人 UI 与 `0x41ED3E` AI 当前地产策略均已闭合 |
| 8 | 拍卖卡 | `3/20/0/1` | 当前地产 | `0x475D7C` / `paimaika` | 当前对象为地产；拍卖并可能释放 owner | implemented / 拍卖结果由命令承载 |
| 9 | 天使卡 | `2/160/2/0` | 地产 | `0x475D80` / `tianshika` | 可建设地产；同组升级或设施建设 | implemented / SDL 地图选择 |
| 10 | 恶魔卡 | `1/180/2/2` | 地产 | `0x475D84` / `emoka` | 已建设地产；同组建筑归零并补偿 | implemented / 核心组结算 |
| 11 | 怪兽卡 | `2/60/0/2` | 地产 | `0x475D88` / `guaishouka` | 合法已建设地产；清空 type/level、保留 owner，并按旧等级记录补偿量 | implemented / `0x41F400` AI 目标、`Data.MKF[557]` 与受害者语音均已闭合 |
| 12 | 拆除卡 | `5/15/0/1` | 他人已建设地产或 16/17/18 型动态道具 | `0x475D8C` / `chaichuka` | 复现 `0x0E0C0626` 混合目标选择；地产降一级，动态道具清除并回补公共供给，炸弹附着引用同步解除 | implemented / `test_card`、`test_card_panel_flow`、SDL 地图命中；地产成功后接入 `Data.MKF[529]` 破坏演出与 `Effect.MKF[97]` |
| 13 | 抢夺卡 | `4/25/0/2` | 玩家 | `0x475D90` / `qiangduoka` | 目标持有卡片或道具；原版真人二级面板可转移其中任一项 | implemented / 真人两种转移及 `0x41F901` AI 选人选卡均已闭合 |
| 14 | 停留卡 | `4/20/0/0` | 普通/特殊角色 | `0x475D94` / `tingliuka` | 活动目标；自己写 `0x80`，其他写 1 | implemented / `test_card`, `test_game_dispatch` |
| 15 | 冬眠卡 | `2/100/2/2` | 无/全局 | `0x475D98` / `dongmianka` | 存在可冬眠角色；其他角色冬眠五天和补偿 | implemented / 状态事务 |
| 16 | 梦游卡 | `4/25/0/1` | 角色 | `0x475D9C` / `mengyouka` | 目标未冬眠；梦游、交通恢复值及反制 | implemented / `test_card` 被动优先级 |
| 17 | 陷害卡 | `4/20/0/2` | 玩家 | `0x475DA0` / `xianhaika` | 目标可陷害；监狱及反制链 | implemented / `test_card` 被动优先级 |
| 18 | 复仇卡 | `4/20/0/0` | 被动 | `0x475DA4` / `0x4420D5` | 主动 handler 恒返回 0；仅被动反制 | passive implemented；human active effect/failure/retry UI implemented；core/AI active rejected / `test_card`, `test_card_panel_flow` |
| 19 | 嫁祸卡 | `4/40/0/0` | 被动 | `0x475DA8` / `0x4420D5` | 主动 handler 恒返回 0；仅被动反制 | passive implemented；human active effect/failure/retry UI implemented；core/AI active rejected / `test_card`, `test_property`, `test_card_panel_flow` |
| 20 | 免费卡 | `4/25/0/0` | 被动 | `0x475DAC` / `0x4420D5` | 主动 handler 恒返回 0；仅收费反制 | passive implemented；human active effect/failure/retry UI implemented；core/AI active rejected / `test_card`, `test_property`, `test_card_panel_flow` |
| 21 | 免罪卡 | `4/25/0/0` | 被动 | `0x475DB0` / `0x4420D5` | 主动 handler 恒返回 0；仅被动反制 | passive implemented；human active effect/failure/retry UI implemented；core/AI active rejected / `test_card`, `test_card_panel_flow` |
| 22 | 送神符 | `3/10/0/0` | 自己 | `0x475DB4` / `songshenfu` | 有可送走神明；解除状态 | implemented / `test_card` |
| 23 | 请神符 | `3/20/0/0` | 神明 | `0x475DB8` / `qingshenfu` | 选择地图神明；移动并附身 | implemented / SDL 神明选择 |
| 24 | 红卡 | `3/50/0/0` | 股票 | `0x475DBC` / `hongka` | 选择股票；设置涨停并刷新 | implemented / SDL 股票选择 |
| 25 | 黑卡 | `3/30/0/1` | 股票 | `0x475DC0` / `heika` | 选择股票；设置跌停并结算持仓 | implemented / SDL 股票选择 |
| 26 | 查税卡 | `4/35/0/1` | 玩家 | `0x475DC4` / `chashuika` | 选择可查税玩家；税款与反制 | implemented / 收费链 |
| 27 | 涨价卡 | `3/35/0/0` | 地产 | `0x475DC8` / `zhangjiaka` | 同名地产整组或单个设施写入 `0x50`；红色地块标记 | implemented / SDL 地图选择 |
| 28 | 查封卡 | `3/35/0/1` | 地产 | `0x475DCC` / `chafengka` | 同名地产整组或单个设施写入 `0x51`；蓝色地块标记 | implemented / SDL 地图选择 |
| 29 | 同盟卡 | `2/40/0/0` | 玩家 | `0x475DD0` / `tongmengka` | 选择活动玩家；建立关联 | implemented / SDL 人物选择 |
| 30 | 乌龟卡 | `3/70/0/0` | 普通/特殊角色 | `0x475DD4` / `wuguika` | 活动目标；自己 2 天、其他 3 天 | implemented / `test_card` |

## ID 1、7、11、12、13 的实现前置闭合

逐函数判定、状态字段、取消边界以及静态/动画/可交互 UI 与音效清单已经补入
[`rich4/docs/cards.md` 的 3.1～3.4](../../../rich4/docs/cards.md)。SDL 接入前至少需要闭合：

| ID | 核心/数据前置 | 表现与交互前置 | 当前差异 |
|---:|---|---|---|
| 1 | 活动玩家枚举、64-bit 求和后整数平均、现金批量提交、`0x40DF69` 关系量等价状态、`0x41D433` 财务刷新 | `Data.MKF[571]` 效果窗、Effect 62；无目标 UI 和专用动画 | 已闭合；AI 使用 `average > cash×10 && cash < 3000×price_index` |
| 7 | 当前节点到住宅/设施记录映射；住宅 `type^=1`；设施 subtype 0..4；type 1/住宅及 type 0/3 设施的等级压到 1；成功后地图刷新 | `Data.MKF[577]`；`Data.MKF[517]` chunk 4/5 五类型 UI，hover/确认/取消为 Effect 0/1/4 | 已闭合；AI 仅检查脚下对象，并把设施预选 type 随命令提交 |
| 11 | mask `0x0E0C0506` 目标选择；按旧等级产生 `62×level×price_index` 关系量；清空 type/level 但保留 owner；地图聚焦/刷新 | `Data.MKF[581]`；地图选择 Effect 2/3/4；`Data.MKF[557]` 破坏序列；AI 共享飞向目标效果 | 已闭合；AI 与真人均在动画及 500 ms 等待后播放 owner 的 `_card_strings[character][2][10]` 语音 |
| 12 | mask `0x0E0C0626`；地产目标限他人已建设地产，动态目标限 type 16/17/18；原版清除对象前回补对应公共道具计数，type 18 同步清除附着玩家引用 | `Data.MKF[529]` 破坏序列；动态对象使用统一地图命中编码；Effect 2/3/4 | 已闭合；核心 typed `GAME_DOMAIN_MAP_OBJECT` 事务、公共库存回补、附着炸弹回归、SDL 混合目标选择，以及地产成功后的 Data[529]/Effect[97] 表现阶段均有测试/接线；AI 正常命令桥也会为地产目标启动同一表现状态和音效事件 |
| 13 | mask `0x0E0C0410`、人物 bit 解码、目标 15 卡槽和 15 道具槽、两种库存转移、取消回滚、关系量和刷新 | `Data.MKF[583]`；地图选择 Effect 2/3/4；`Panel.MKF[11]` 上卡下道具二级面板；二级 callback 无已证实专用音效 | 已闭合；AI 只抢卡、不预选道具，并限制为仍在地图上的活动对手 |

### 原版 AI 目标策略（IDA 复核）

- ID 2 / `0x41E779`：先从当前地图仍在场的活动玩家中建立候选。关系推荐玩家若现金低于 `30000 × price_index` 且高于施放者现金两倍则优先；否则按玩家扫描顺序选择现金低于 `50000 × price_index` 且高于施放者现金三倍的目标。当前 AI map context 已提供在场集合并提交同一 `GAME_COMMAND_USE_CARD`。
- `0x40D2D3` 的“推荐玩家”是其他活动玩家中 `relation_balance` 最大且严格大于 0
  的一人；相同值保留先扫描者，没有正值则无推荐目标。
- ID 7 / `0x41ED3E`：只检查 AI 当前脚下对象。住宅必须自有；非零 type 还要求
  同组存在另一块自有地。type 0 必须为 1 级；`GOOD_CHILD` 可直接使用，其他性格仅在
  同组不存在另一块自有或无主地时使用。设施若为自有 type 0、1 级，随机预选
  type 1..4；否则必须是他人非零 type，等级至少 3，或 owner 是推荐玩家且等级至少 2，
  并预选 type 0。
- ID 11 / `0x41F400`：只收集他人且等级至少 3 的住宅/设施；每个 owner、每类对象保留
  “等级最高，同级基础价最高”的一项。推荐玩家存在时先取其设施、再取其住宅；后备扫描
  活动对手时，设施门槛为 3、住宅门槛为 4，并在最终结果中固定设施优先于住宅。
- ID 13 / `0x41F901`：只考虑仍出现在地图上的其他活动玩家。推荐玩家在图时，优先抢其
  `f7 >= 1` 的最高价卡；没有该目标时，在全部候选中只接受 `f7 == 2` 的全局最高价卡。
  AI 不走道具分支。
- ID 30 / `0x420970`：前三个前向路线对象的 typed 直线路径中，至少两个可计价地产/设施
  通过原版价值/现金、现金加银行和投资修正门槛时，AI 选择自身作为乌龟卡目标；分岔会按
  原版全局普通玩家候选顺序逐个尝试三节点路线，typed 价值/数量足够时生成玩家目标；
  特殊对象和无法由 `GameState` 表达的全局表分支仍返回 `DEFERRED_TARGET`。

### 怪兽卡受害者语音

`0x443917` 在清除建筑并播放 `Data.MKF[557]` 后等待 500 ms；owner 非零时以 speech
type 1 调用 `_card_strings[owner_character][2][10]`。12 名角色字符串中的 `#dddd`
语音资源号依次为 `0467, 0519, 0571, 0623, 0675, 0727, 0779, 0831, 0883, 0935,
0987, 1039`；原始 Big5 字节已逐项保存在 `src/data/character_speech.c`，SDL 同时按
该表显示文本并播放 Speaking 资源。

效果窗是 handler 前置表现：`0x441BAA` 在目标选择之前读取
`Data.MKF[card_id+570]`、播放 Effect 62 并停留 1500 ms，所以之后取消/非法仍已显示卡图。
SDL 真人卡片栏对全部 30 张卡统一按此前置顺序实现；ID 18-21 在
1500 ms 后按原版空 handler 失败并回到卡片栏。成功提交后不重复显示卡图。

`0x441F73` 使用位于 `0x441204` 的固定 graph 模板：宽 165、高 256，
把 MKF 返回指针直接写入像素字段。因此 `Data.MKF[571..600]` 是无四字节
宽高头的 RGB555 裸图。展示顺序是 `Data.MKF[517].chunk[5]`（锚点
`220,129`）、16 px Big5 标题、卡图左上角 `(138,200)`，随后
`Effect.MKF[62]` 和 1500 ms 保持。SDL 的奖励卡与主动卡调用点已统一走这条
共享绘制路径，不再各自把首个像素误解析为尺寸。

2026-10-05 IDA Pro MCP 复核：`0x44205C` 调用 `0x4563F5 → 0x455B3A`
完整复制卡图，零值像素为黑色，不能透明跳过；SDL 卡图已改为不透明绘制。
提示框走 `0x456418 → 0x455C52`，仍以零值透明绘制。`0x40808F` 确认
`0x48BAD8` 来自 Data 句柄 `0x48A0E4` 的资源 517；chunk 5 为 249×170，
偏移 (123,101)，在锚点 (220,129) 绘制。V3.11 的全部 30 张卡图均含零值像素。

## 测试链接

- `tests/unit/test_card.c`：30 项目录/地址、公共池、15 槽容量、取卡、左移消费、均富/均贫、转向、停留、乌龟，以及免罪→嫁祸→主效果→复仇和免费→嫁祸两条链的事务/回滚。
- `tests/unit/test_game_dispatch.c`：真人 `UseCard` 统一分派与成功消费。
- `tests/unit/test_property.c`：地产收费接入免费卡和嫁祸卡，覆盖免单与改写实际付款人。
- `tests/unit/test_new_game_init.c`：目录初始池及玩家模板 `+23` skill 导入。
- `tests/unit/test_ledger.c`：破产清空完整 15 槽。
- `tests/unit/test_ai.c`：ID 7/11/13 的当前地产、推荐 owner、设施优先和在图受害者目标，
  以及 ID 30 乌龟卡的直线路径、自身目标、经济门槛和分岔 deferred 边界。
- `tests/unit/test_character_speech.c`：怪兽受害者语音表边界以及 `#dddd`/`@08` 解码。

## SDL 接入

- `0x44542D`/`0x445593` 覆盖住宅 `+0x17` 或设施 `+0x1C`：涨价为
  `0x50`，查封为 `0x51`。住宅按原始名称匹配整组，不限制屋主或建筑
  子类型；设施只处理目标一块，查封研究所还清除 `+0x1E` 研究进度。
- `0x407BB7` 从已打开的 `Map.mkf` 读取资源 26，保存在 `0x47494D`。
  `0x4092D5`/`0x409520` 在建筑排序绘制之前叠加地块底面：住宅取
  旋转方向最低位的 chunk，设施再加二；状态最低位选择红/蓝像素 OR
  掩码（RGB565 为 `0xF800`/`0x003F`）。
- `0x41CF67` 日结在终局检查、股票与月结之后，对上述状态高半字节
  减 `0x10`，归零时清除整个状态。因此标记和效果都在第五次日结后
  消失，普通玩家之间的回合切换不递减。
- 差值为 1 的 AI `rand()%3` 门槛使用可注入、可持久化 RNG；卡片目标通过
  `GameAiMapContext` 注入当前地产及在图玩家集合。
- 顶栏卡片按钮打开 `Panel.MKF[11]`；槽位命中仍使用原版 `19/135` 起点、`80×56` 步长与 5×3 布局。右键取消，空槽不变；18-21 仍可被真人点击，显示卡片效果窗后主动失败并回到卡片栏，不消费。
- 人物/地产/神明走地图投影命中；红黑卡使用股票二级列表；抢夺卡复用卡片栏，
  显示目标上方 15 个卡槽和下方 15 个道具槽。
- 原版在 handler 前读取 `Data.MKF[card_id+570]`，播放 `Effect.MKF[62]` 并保持
  1500 ms；地图合法/非法/取消分别使用 Effect 2/3/4。全部 30 张卡的真人使用流程已按此前置顺序接入。
