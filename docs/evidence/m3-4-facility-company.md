# M3.4 设施与公司地产证据

## 地址与目标模块

- `0x41982D`：路线对象总分派；`0xFA0..0x176F` 进入 56-byte 设施族。
- `0x41A17F..0x41A9AF`：设施自有建设/升级、他人设施收费和无主购买。
- `0x4239B9`：玩家资产扫描，设施价值为基价加等级乘每级价。
- `0x428CAF`：12 支地图股票与 52-byte 公司/辅助地产记录关联。
- `0x40CD87`：破产时释放设施 owner 并清设施记录 `+0x34`。
- 目标实现：`src/core/facility.[ch]`、`src/data/map_data.[ch]`、`src/core/ledger.c`、`src/core/route_event_queue.c`。

## IDA 结论

本步没有重新调用 IDA Pro MCP；依据固定 V3.11 `rich4/asm/rich4.asm` 的对应反汇编复核。因此整体置信度记为 `partial`，已实现的字段、购买、建设、升级、费用公式、轮盘暂停/恢复、估值和公司链接分支有逐条本地证据与测试；原版随机交互的资源/演出细节没有提升为 confirmed。

## 本地证据

`rich4/asm/rich4.asm` 中可直接观察到：

- `0x41A86B..0x41A998`：无主设施检查梦游和 `god_info == 12`，价格是记录 word `+0x22 * price_index`，只检查/扣现金，接受后写 `owner = current_player + 1`。
- `0x41A17F` 后自有设施 level 0 建设以同一基价计费；真人调用 `0x440AAC` 选择 1–4 型，AI 使用 `rand()%4+1`。level 非零时以 `+0x24 * price_index` 计费并递增等级。
- `ref_00474940` 字节序列为 `01 05 05 01 05`；按 subtype 索引，1/2/3/4 型最大等级为 5/5/1/5。
- 他人设施收费仅进入 subtype 1、2、3。1/2 型从 `+0x24 + level*2` 读取基数并调用 `0x44090E`；当前 `rich4_facility_resolve_landing`、路线轮盘请求和 phase-2 continuation 已接入该公式/结果边界。`0x43F7C6` 的 `0x475D0C` 结果表也已按原始 4×12 字节布局接入：`0xFF` 是无效落点，不能被当作收费结果；具体 `rand()%12` 交互的 Panel/Data 演出仍由 SDL 边界承接。3 型已按原版 `500 * price_index * dice_total * (1 << (traffic_method - 1))` 接入，步行者不收费；现在先发 typed 收费演出事件并暂停路线，金额窗口结束后才进入统一账本/清算恢复路径，剩余仍是资源/音效细节。
- `0x4239B9` 对每个 owner 匹配的 56-byte 设施累计 `word(+0x22) + level * word(+0x24)`。
- `0x428CAF` 只扫描 `ref_00498e7c` 的 52-byte 记录；比较记录 byte `+0x19` 与股票索引 0–11，并把 one-based 记录序号写入股票结构 `+4`。这证明公司身份与 56-byte 设施 owner 是两套数据。
- `0x40CD87` 对设施记录清 byte `+0x19` 和 dword `+0x34`，与普通地产清理分支分开。
- `0x41982D` 的设施公共尾部在 owner、开放状态、type 4、level>0 且状态 nibble 清零时调用 `0x44101D`。该函数真人从 `Panel.MKF[11]` 的研究工具图标（chunks `10..14`）中选择，写入研究项与 5 回合计时；AI 直接选择最高项，同样写入 5 回合。

## 输入、输出和副作用

- 输入：设施 ID、actor、owner/type/level、价格指数、现金、交易状态，以及建设命令中的 subtype 1–4。
- 输出：购买/建设/升级动作、设施 subtype 选择、收费轮盘请求或明确的 deferred。
- 副作用：现金账本、owner/type/level、设施价值、`+0x34` 释放、场景刷新事件；公司链接由地图 auxiliary 表确定性重建，不随设施 owner 改写。

## 测试链接

- `tests/unit/test_facility.c`：购买、建设、升级上限、价值公式、subtype 费用公式和溢出原子回滚。
- `tests/unit/test_route_event_executor.c`：设施 subtype 选择、非 owner 设施轮盘请求及暂停/恢复。
- `tests/unit/test_route_event_executor.c`：type 4 研究路线的 AI 最高项选择、真人选择、5 回合状态和事件提交。
- `tests/unit/test_route_flow.c`：研究选择面板、选择数量、确认/取消命令边界。
- `tests/resource/test_map_data.c`：56-byte 尾字段/分级值表与 52-byte `stock_index` 独立解析，公司链接重建。
- `tests/unit/test_ledger.c`：破产释放设施 owner/value/`+0x34`。
- `tests/unit/test_game_command.c`：设施建设 subtype 命令边界。
- 全矩阵：Windows `make -B test sdl test-original-data`；WSL `make test-sanitize`。

## 缺口与冲突

- subtype 1/2 的收费不是纯公式，而是 `0x44090E -> 0x43F7C6` 随机交互的返回值乘设施等级表；`0x475D0C` 的四行各 12 槽已按原始字节接入，非法 `0xFF` 槽会继续轮盘减速而不会提交结果。轮盘状态现在也按 `0x43F7C6` 的 `0x24` ms 外层时钟、阶段 1..5/每阶段 3 次循环以及有效落点后的 `0x28` 次保持实现；具体 Panel/Data 资源的动态帧序与逐像素差异仍 deferred。subtype 3 的交通倍率、骰子金额和步行无收费已在 core 应用，收费前的 1500ms 金额窗口和恢复/破产边界也已接入，剩余仅为资源/音效表现核对。
- 轮盘的第一帧与音频边界已按同一调用链收紧：`0x44090E` 先显示 `Panel[68+mode]` chunk 2，首个 `0x24` ms tick 后才显示 `slot+2`；进入有效落点时只发一次 `ROULETTE_STOP`，恢复路线不重复停止循环音。SDL 的 chunk 0/1（旋转/停止）和 `(220,320)/(265,230)` 锚点保持原版调用顺序。
- 软件光标：2026-10-13 用 IDA Pro MCP 与 `rich4/asm/rich4.asm` 核对 `0x44090E`、`0x43F7C6`、`0x43F127`。航空、旅馆、购物中心、保险四个 mode 在旋转、减速、结果停留时均隐藏；掷骰入口 `0x4182DE` / `0x401271` 已调用 `0x402460(0)`，转盘不重新启用光标。真人仍可松开左右鼠标键或键盘键停止，判定不依赖坐标。SDL 的 `rich4_app_input_display_visible` 只控制绘制，保留输入处理；`test_app_input_flow` 覆盖四个 mode、手动/自动、各阶段及退出后恢复。
- mode 1/2 的轮盘说明文字现在也按原版闭合：从设施记录 `+0x19` 取 owner，按 `0x452946` 移除角色名中的 ASCII 空格，再使用 `0x465210/0x465224` 的 Big5 格式串；公司 type 1/4 的 mode 0/3 也分别接入 `0x465272/0x46525C` 的原版 Big5 格式。公司辅助记录 `+0x04` 的 16-byte 动态名称已在地图加载时复制到临时 `GamePresentationState.map_company_names`，轮盘优先使用该字符串，资源缺失/合成测试状态才回退确定性股票名；它不进入 `GameState`、存档或回放字段。
- 保险通知沿用同一名称来源：有地图辅助记录时优先显示 `map_company_names[company]`，无地图字符串的合成展示才回退股票静态名；这只改变 SDL 展示输入，不改变保险计数或账务。
- level 0 建设的核心选择/命令入口已实现，AI 使用原版四选一范围；真人选择器已使用 `Data.MKF[0x205]` chunks `4/5`、原版五项 Big5 名称、`0x43FAE4` 的 `50..389/286..353` 命中区和按下态；剩余仅为最终逐像素截图对比。
- type 4 研究已接入路线执行器、真人选择面板和 AI 最高项选择；研究项写入 5 回合 typed 状态，并由既有 `facility_tick_research` 在完成时发放工具。当前 SDL 选择器已使用原版 `Data.MKF[0x205]`（十进制 `517`）chunk 7 五格底板、`Panel.MKF[11]` chunks `10..14`、76 像素步长命中区、`0x44101D` 的 Big5 提示，以及 `0x451B9E` 的左键按下态；剩余仅为最终逐像素截图对比。
- 当前工程尚无完整存档格式。公司关联已作为稳定零基字段保存在 `GameState`，且可由原始地图确定性重建；真正的磁盘存读档验证归 M3.11，不能在本步虚构格式。


## 旅馆住宿状态与人物移动

旅馆收费和入住证据见 [加油站与旅馆](hotel-gas-station.md)。`0x40D5A5` 对本人入住设置
`+0x15 bit 0x20`，`0x40C05C` 在入场半程后清该标记；住宿计数仍非零，渲染器因此隐藏人物。
`0x40D6BE` 退房设置 `0x10`，退出移动在半程后清住宿计数，最终 `0x418EBD` 恢复方向并清暂态位。
`0x40CD87` 清整个 player state byte 属破产处理。

## 保险公司落地、托管与弹窗

固定原版为 V3.11 Rich4.exe，SHA-256
`5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`。
2026-10-17 经 IDA Pro MCP 与 `rich4/asm/rich4.asm` 复核。

- `0x41AB6D -> 0x41B067`：无董事长只进入认股，不转盘、不加保险、不收保费；
  没有首次访问标记，认股后也不回头投保。
- `0x41A9FE/0x41AC3C`：董事长免费保险，其他人按基础费乘天数乘物价缴费。
  不直接检查踩地者持股；天数先写 `(旧值+d)&0x7F` 到踩地者 `+0x3E`。
  认股与经营权变化均在收费之后，不追溯退费；代付不转移保险。
- `0x43F7C6`：电脑、托管、梦游自动走第 40 tick 停止分支；每 tick 至少 36 ms。
  十二格结果为 `5,-1,3,-1,30,-1,20,-1,15,-1,10,-1`；真人释放输入停止，右键不撤销保险。
- `0x41AE37/0x41AEB4`：原始保费非零才显示公司名、董事长、金额及“保险费”1500 ms。
  `0x41D709` 直接检查玩家神明字节，调整金额后才尝试免费卡、嫁祸卡。
- `0x444A60`：真人先确认；自动玩家用 `(rand()%3000+3000)*P` 与现金判断。
  使用时通过 `0x441F73` 显示 Data[590] 卡面及“使用免费卡”1500 ms，再耗卡、发言。
- `0x44476A`：真人先显示 Data[589] 卡面及“某某／嫁祸卡生效！”1500 ms，再确认或选择目标。
  自动玩家先按关系值／随机候选选目标，再以 `(rand()%4000+4000)*P` 判断使用；
  成功时显示同样的卡面，再显示“嫁祸给某某！”1500 ms，之后耗卡、双方发言。
  两张卡的调用门槛均为费用达到 `2000P` 或超过现金加存款；主动用卡开关不参与。
- `0x440E1A/0x43FF56`：多目标使用 Data[518] 的 `候选数-2` 帧与 Data[2] 头像；
  头像左上角为 `(220-frame.x+12,320-frame.y+12)`，横向间隔 80，命中高度 72。
- `0x40FBB8`：从玩家 0 起找首个活动、非付款人、神明原始字节为 14/15 的玩家。
  提示“死神显灵／由某某赔偿保险费”1500 ms；不检查同盟字段。
- `0x41B022`：上述演出返回后才扣款，现金优先、存款补足，收入进入公司两项业务额。
  代付者破产不把踩地者判为破产，保险不重复加入。
- `0x41D1A9/0x41D839`：非梦游、仍在局且有发行股份时才认购。
  发行价为公司价值整数除 10000；上限为现金可买数、1000、发行余额的最小值。
  不以交易所上市状态或股票目标仓位比例为门槛。自动玩家保留 `trunc(F*0.3)*P` 现金；
  只有真人显示认股确认和数量输入。所有控制方式取得经营权均显示 1500 ms 结果提示。
- `0x440BA8/0x453544/0x452C02`：确认框锚点 `(220,320)`；数量窗初始 `(256,144)`、128×192，
  用 Panel[21] 图和 Panel[22] byte mask。支持数字、退格、清零、最大值、Enter、滑杆、右键取消；
  拖动原点限于 `x=0..512,y=0..288`。SPR 与 SMP 按实际资源格式绘制。
- `0x409660/0x409822`：建筑调色板第 255 项为实时董事长角色颜色；
  `0x40A723..0x40A7F3` 的小地图标记使用 Data[517] 的 `24+(variant&1)` word mask。

对应代码：`company.c`、`route_event_executor_landing.c`、`route_event_executor_resume.c`、
`property_flow.c`、`roulette_flow.c`、`special_presentation_support_flow.c`、
SDL 事件弹窗、输入适配及共享数量面板。

回归：`test_property_flow` 覆盖真人／电脑／托管、零持股／已有股份、卡面期间不付款、
取消不耗卡、死神代付、神明只调整一次和代付者破产；`test_route_event_executor` 覆盖无主认股、
保留现金、零保费及董事长分支。`test_insurance_building_sdl` 读取原版资源检查卡面像素、头像命中区域、
数字键盘按键、拖动与完整面板绘制。命令：
`python scripts/make_minimal.py test-company-landing test-insurance-building-sdl test-unit test-save-load-replay sdl`。
