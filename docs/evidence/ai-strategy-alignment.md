# AI 原版策略与 SDL 逐项对应

复核日期：2026-10-13。原版为 Rich4.exe v3.11，image base `0x400000`，SHA-256 为 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`。本次通过 IDA Pro MCP 确认输入程序和散列；原版详细条件、常量及反编译见 [AI 策略总表](../../../rich4/docs/ai_strategy.md) 和 [IDA 原始证据](../../../rich4/docs/evidence/ai_strategy_ida.json)。

本轮补充的随机函数、原库存取卡/满手处理、神明投资条件、AI还款及卡窗口寄存器汇编见 [补充 IDA 证据](../../../rich4/docs/evidence/ai_strategy_alignment_ida.json)。

本表对应已知决策条件、候选次序、状态传递和命令接入。领域命令使用零基 ID，下面卡片及道具编号使用原版一基 ID。`confirmed` 表示列出的决策条件有原版证据和针对性测试；完整运行等价性仍为 `partial`，边界见文末。

## 回合、公共参数与随机数

| 原版地址/字段 | 原版行为 | SDL 对应 | 验证 |
|---|---|---|---|
| 玩家 `+21..+26`、十二角色模板 | 控制状态、主动开关、策略等级、借款、现金、股票比例分别保存；托管可改变其中部分 | `GamePlayerState`、`character_template.c`、`ai_settings_flow.c` | `test_ai_settings_flow`、`test_new_game_init`、`test_ai` |
| `0x418C55` | 买股→卖股→特殊融资清理→挂牌市场→卡或道具二选一→移动 | `app/ai_flow.c:rich4_ai_run_preactions_with_projection` | `test_ai_flow`、`test_ai` |
| `0x41E69E`、`0x420E9A` | 难度差≥2拒绝，差1仅1/3通过，其他直接判定 | `ai.c:personality_gate` | `test_ai` |
| `0x40D2D3`、`0x40D31C` | 正关系严格最大优先；部分攻击另有活动、状态清除的随机后备 | `ai.c:relation_preferred_player`、`ai_area_target_player`；被动链相应 helper | `test_ai`、`test_card` |
| `0x456F2D` | 32 位溢出的 `state=1103515245*state+12345`，返回 `(state>>16)&0x7fff` | `rng.c:rich4_rng_original_state_next/range`；AI、银行、拍卖、自动特殊格及相关被动链调用 | `test_ai:original_strategy_alignment` 的种子1向量 `16838,5758,10113,17515,31051` |
| `0x441BAA` | 非零卡槽计数；≤8从首槽开始，>8随机起点环形八项；原始空槽终止；首个通过者结束 | `ai.c:select_card` | `test_ai` |
| `0x447D97` | 有库存的种类按编号入表，跳过时光机；>4随机起点；最多四次，少于四种时重复尝试 | `ai.c:select_tool` | `test_ai:ida_tool_selection_regressions` |
| `0x48BE58` | 卡片预选结果为会跨次保留的 scratch；部分成功分支不重写 | `GameAiSelectionMemory.card_target_raw`，由 `GameSession.ai_selection` 传递 | `test_ai:original_strategy_alignment`、`test_ai_flow` |

上述公共条件为 `confirmed`。有效目标还依赖原版视口对象表、道路占用和动态槽号；不能把地图资产全集直接当成屏幕候选。

## 30 张卡片

以下实现均位于 [ai.c](../../src/core/ai.c)，统一经 `select_card → card_command` 进入专用判定。卡片效果提交到共享 `card.c/card_command.c`；被动卡另走事件链。表中列出策略测试所覆盖的关键规则，完整表达式见原版总表。

| ID/名称 | 原版判定 | SDL 判定入口 | 对齐规则/验证重点 |
|---|---|---|---|
| 1 均富 | `0x41E6FE` | `card_command(card==0)` | 活动玩家现金整数平均>10C且C<3000P；`personalities_and_cards` |
| 2 均贫 | `0x41E779` | `ai_average_poverty_command` | 可见R优先；后备逐编号覆盖，保留最后合格者；`original_card_target_strategies` |
| 3 购地 | `0x41E9E2` | `ai_purchase_command` | 脚下他人已建资产，R必须存在，住宅同组/关系条件，价格<C |
| 4 换地 | `0x41EAE2` | `ai_swap_land_command` | 脚下自有低级资产；同资产类别、基价和等级严格更高；住宅不同名并通过共用购地条件 |
| 5 换屋 | `0x41E6E3` | `card_command(card==4)` | 主动固定拒绝；`original_ai_noop_cards_are_rejected` |
| 6 转向 | `0x41E6E3` | `card_command(card==5)` | 主动固定拒绝；同上 |
| 7 改建 | `0x41ED3E` | `ai_rebuild_command` | 住宅类型/同组条件；自有空设施1级随机1..4；他人特殊设施的等级/关系条件 |
| 8 拍卖 | `0x41EF26` | `ai_auction_command` | 他人≥3级，或R拥有且≥2级；`original_strategy_alignment`覆盖0..3级×关系有无 |
| 9 天使 | `0x41F037` | `ai_angel_command` | 视口同名自有type0且<5级的住宅，≥3块才随机选组 |
| 10 恶魔 | `0x41F1B3` | `ai_demon_command` | 有R与无R分支分别计人数、等级和；无R时自己等级和必须0 |
| 11 怪兽 | `0x41F400` | `ai_monster_command` | 各owner最佳等级/基价；设施优先；同等级同基价保留屏幕表首项 |
| 12 拆除 | `0x41F6A9` | `ai_demolish_command` | 先怪兽；后备特殊住宅、交通工具对应加油站、type16/17动态槽 |
| 13 抢夺 | `0x41F901` | `ai_robbery_command` | R分支难度≥1，其他分支难度=2；严格最高价格，首项决胜；只抢卡 |
| 14 停留 | `0x41FACC` | `ai_stay_command` | 自有建设条件/对手收费条件；单块住宅≥2级沿用旧raw目标；测试0和0x8002缓冲区 |
| 15 冬眠 | `0x41FE4E` | `card_command(card==14)` | eligibility仅`rand()%4==0`；实际效果另由handler判断 |
| 16 梦游 | `0x41FE6F` | `ai_attack_player_command` | 视口普通玩家、无冬眠/复仇；R优先，否则随机 |
| 17 陷害 | `0x41FE6F` | `ai_attack_player_command` | 同梦游目标筛选 |
| 18 复仇 | `0x41E6E3` | 主动拒绝并继续下一卡；`rich4_card_resolve_attack` | 攻击事件被动链；`original_ai_noop_cards_are_rejected`、`test_card` |
| 19 嫁祸 | `0x41E6E3` | 主动拒绝并继续下一卡；收费/特殊事件被动链 | 先选R或随机后备，再判使用门槛；`test_ai`、`test_card`、`test_special_event` |
| 20 免费 | `0x41E6E3` | 主动拒绝并继续下一卡；`rich4_card_resolve_landing_charge` | 费用阈值和神明修正，不统一无条件耗卡；`test_ai`、`test_card` |
| 21 免罪 | `0x41E6E3` | 主动拒绝并继续下一卡；惩罚/魔法屋被动链 | 消耗后压紧原卡槽；`test_ai`、`test_card`、`test_special_event` |
| 22 送神 | `0x41FF77` | `ai_send_god_eligible` | 附身type5/6/7/8/10/15；无神时才看炸弹<13 |
| 23 请神 | `0x41FFF8` | `ai_nearest_god_command` | 当前槽号及选出槽号检查1/2/3/4/12；最近合法未附身神明；槽号与type分开测试 |
| 24 红卡 | `0x420055` | `ai_red_stock_command` | 持股数量×成本严格最大；休市、停牌、涨停过滤 |
| 25 黑卡 | `0x4200EA` | `ai_black_stock_command` | 公司最多者分支后由R分支覆盖；自己持股、停牌、跌停过滤 |
| 26 查税 | `0x4202D2` | `ai_tax_command` | 可见R>30000P优先；其他>50000P逐编号覆盖 |
| 27 涨价 | `0x42040E` | `ai_price_increase_command` | 自有等级和≥7、外部≤3、float比例与double0.66比较；混合表扫描保留ESI写入语义 |
| 28 查封 | `0x42062B` | `ai_seal_command` | 前向六格；住宅同组无自有地且外部等级和≥7，或R的≥3级特殊设施 |
| 29 同盟 | `0x4207CC` | `ai_alliance_command` | 全地图资产严格最多者再过可见/关系/同盟条件；不选第二名 |
| 30 乌龟 | `0x420970` | `ai_card30_direct_gate` | 自己前三格可买建条件；失败按视口普通玩家表顺序检查对手前三格收费条件 |

主体目标条件由 `test_ai:original_card_target_strategies`、`original_strategy_alignment`、`renderer_global_table_projection`、`ai_dynamic_god_target_context` 和 `tortoise_card_ai` 验证。已列条件及视口候选建表规则为 `confirmed`；非法raw目标和长期逐帧完整兼容仍为 `partial`。资源入口核验见 [像素候选表](ai-rng-pixel-compatibility.md)。

涨价卡不是统一最高等级选择：`0x4205F6` 把ESI写入设施比较暂存值。进入函数时ESI继承卡窗口尾游标；住宅分组扫描又把它改为地图扫描索引，包含关系目标触发的提前退出。测试保留“3级设施先于5级设施但仍选首项”的结果。停留卡的旧目标也不会自动改写为自己。

## 13 种道具

实现均位于 `ai.c`，经 `select_tool → tool_command`；放置和效果复用 `tool.c/tool_flow.c`。

| ID/名称 | 原版判定 | SDL 入口 | 对齐规则/验证重点 |
|---|---|---|---|
| 1 机器娃娃 | `0x420EFA` | `ai_machine_doll_direct_gate` | 前四格无分岔；动态type及底层产权/同名收费门槛 |
| 2 路障 | `0x42107F` | `ai_roadblock_tool_command` | 前四格的空路线购地/商店条件；失败检查反向六格与可见表交集中的最高同名收费 |
| 3 地雷 | `0x4213C5` | `ai_route_object_tool_command` | 反向六格与可见空路交集；收容节点优先，其余他人资产随机 |
| 4 定时炸弹 | `0x421574` | `ai_route_object_tool_command` | 同样收容节点优先，其余全部交集道路随机 |
| 5 机车 | `0x421644` | `ai_no_target_tool_eligibility` | 步行且1/4通过 |
| 6 汽车 | `0x421675` | `ai_no_target_tool_eligibility` | 当前交通<2且1/4通过 |
| 7 飞弹 | `0x421717` | `ai_area_tool_command` | 先R再随机后备；必须可见；局部200×200不能伤及自身人物/自有资产 |
| 8 遥控骰子 | `0x421827` | `ai_remote_dice_projected_candidate` | 六步无分岔；槽号、资金、慢行、投资条件；原落点序号作为骰值 |
| 9 机器工人 | `0x421BA6` | `ai_robot_command` | 可见自有可升级资产，按当前收费严格最大；不套购地保留金 |
| 10 时光机 | `0x420EDF` | `select_tool` | 候选直接排除；主动判定固定拒绝 |
| 11 传送机 | `0x421CB6` | `ai_transfer_tool_command` | 完整可见空道路表；无主高等级资产、费用门槛；最高等级且同级首项 |
| 12 工程车 | `0x421E20` | `ai_no_target_tool_eligibility` | 交通0..2，`rand()%15<=策略等级`，叠加公共难度条件 |
| 13 核子飞弹 | `0x421E62` | `ai_area_tool_command` | 他人已建住宅后设施；最多十次有放回抽中心；局部440×440检查自身人物与数量/等级比例 |

验证：`test_ai:ida_tool_selection_regressions`、`route_tool_target_window`、`robot_tool_target_policy`、`area_and_transfer_tool_targets`、`vehicle_tool_ai_gates`、`remote_dice_ai_gates`、`machine_doll_route_gate`。候选、门槛、raw结果协议及像素对象来源规则为 `confirmed`；完整动态场景与长期逐帧对照仍为 `partial`。

## 移动与落地资产

| 原版地址 | 原版行为 | SDL 对应/验证 |
|---|---|---|
| `0x40C05C`、`0x40B221`、`0x40B343` | 四个合法邻接槽分别参与选择，不合并重复边；前后窗口保留禁止边、上一步、分岔及回头规则 | `route_movement.c`、`ai_collect_original_route_window`；`test_route_movement`、`test_ai` |
| `0x4221C0` | 正常交通1/2默认2/3骰；前五格U≥2且E≤1改1；汽车U=0且E>2随机2/3；炸弹<15改1，否则不预看；已有遥控结果不覆盖 | `rich4_ai_select_dice_count`、`app/ai_flow.c:rich4_ai_prepare_move`；`original_dice_count_strategy`、`test_ai_flow` |
| `0x418C55`、道具完成后移动 | 在普通pre-roll和道具效果完成路径均执行骰数准备 | `ai_flow.c`、`platform/sdl1/game_loop_ai.c`；`test_ai_flow`、`test_game_dispatch`验证AI可在PREROLL提交，移动中拒绝 |
| `0x41982D`、`0x40FA61` | 购买留`min(trunc(F*0.05),7000)*P`，必须严格超出；附神槽12禁购买，槽7/8/15阻止投资；建设不套购地保留金 | `rich4_ai_purchase_allowed`、`god.h`、route executor；`test_property`、`test_facility`、`test_route_event_executor` |
| `0x40B455`、`0x40B110` | 自有可升级住宅后设施，按收费严格最大；空设施随机1..4型 | `ai.c:select_upgrade`、`route_event_executor_decisions.c`；`test_ai`、`test_route_event_executor` |
| `0x41AAF0`等 | 自营建筑公司最多两次施工，他营一次；达到上限停止 | route executor property/company链；`test_route_event_executor` |
| `0x44101D` | 研究所选零基level-1最高可用工具，5天生产 | route executor research链；`test_route_event_executor`、`test_facility` |
| `0x41D1A9 → 0x41D839` | 公司增发限制最多1000股，AI保留`trunc(F*0.3)*P`现金 | route executor company issue链；`test_route_event_executor` |

## 股票、资金、商店与拍卖

| 范围/地址 | SDL 对应 | 原版条件与验证 |
|---|---|---|
| 买股 `0x42BF03` | `ai_stock_policy.c:rich4_ai_select_stock_buy` | 1/3入口、仓位预算、≥15天贷款门槛、独立/公司银行门槛、停牌/涨停过滤；分降序后按排名抽取，首个通过者数量为0也结束；`test_ai` |
| 独立股买分 | `independent_buy_score` | 2.5/0.6基础价、24/6历史均值与信号；double常量比较；`stock_buy_sell_and_closed` |
| 公司股买分 | `company_buy_score` | 正业务额分档、正本期额控制权评分、公司值0.85/0.7阈值；亏损不进入正业务分支；`company_stock_buy_and_loan_gate`、`original_strategy_alignment` |
| 独立股卖分 `0x42C79F` | `independent_sell_score` | 最低价初值10000；`8*m>1.25*c`；比例项分别截断；流动资金30000/16000P；紧急总分×2 |
| 公司股卖分 | `company_sell_score` | 全体持股/自己持股；日窗口、亏损、公司值倍数、owner和控制权；紧急只+1 |
| 连续卖股 | `rich4_ai_select_stock_sell`、`GameAiSelectionMemory.stock_distress`、`ai_flow.c` | 贷款≤6天且资金<本金进入；到1.1倍本金才退出，跨命令保留紧急模式；`test_ai`、`test_ai_flow`验证实际卖股到账及拒绝退出 |
| 银行 `0x4379C9`、`0x436668` | `bank.c:rich4_bank_run_ai_transfer/run_ai` | 单精度现金比例、月初/月末修正、含±25%边界；B>2L或到期≤6天且资金≥1.1L全还；无贷款1/10或资金<30000借款；`test_bank` |
| 特殊融资 `0x436B0A` | `rich4_bank_settle_special_finance` | 最后有效银行owner；其他活动玩家清融资；owner覆盖差额；`test_bank`、`test_route_event_executor` |
| 挂牌 `0x4284BE` | `ai_flow.c:ai_market_preactions`、共享ledger | 1/15创建、1/3调价、1/4购买；重复卡双循环权重、七槽顺序、股票整数商、资产严格估值门槛 |
| 商店 `0x42E931` | `route_event_executor_rewards.c`、`route_event_executor_shop.c` | 卖卡首轮难度按槽索引；点券不足100卖最便宜卡及多余工具；卡/工具分预算；卡降价序，工具固定序；`test_route_event_executor`、`test_shop_flow` |
| 拍卖 `0x43BDE5`、`0x439F0D`、`0x43A2DD` | `auction.c`、`app/auction_flow.c` | 起价、活动/拘留过滤、两随机估值上限、物价双乘原式、100..10000五档及调用模式压档；`test_auction`、`test_auction_flow` |

这些已列分支为 `confirmed`。股票、挂牌、银行均通过typed命令/共享领域规则应用；候选选择不是通用最大收益求解。

## 特殊地点与被动链

| 原版入口 | SDL 对应 | 对齐规则/验证 |
|---|---|---|
| 彩票 `0x4315CC` | `run_ai_lottery_purchase` | 现金严格>1000且有空号，空号集合随机一次；`test_route_event_executor` |
| 监狱/医院 `0x43D304`、`0x43E9A4` | `route_event_executor_confinement.c` | 有收容对象再抽1/2；策略0/1/2决定普通/四名特殊人物候选；先抽目标再判>30或≥700点券，不重选；`test_route_event_executor` |
| 魔法屋 `0x43380A` | `route_event_executor_step_special.c`、`special_event_magic.c` | 重抽非空条件组；含自己动作6；否则0..10且6改7；免罪消耗压紧库存；`test_special_event`、`test_magic_house_flow` |
| 三小游戏 `0x415215`、`0x4154DC`、`0x4155FC` | `apply_automatic_point_minigame` | 50..69点，然后另一随机数选择角色发言；`test_route_event_executor`、`test_route_event_flow` |
| 旅馆、购物中心、保险转盘 | `roulette_flow.c`、`hotel_flow.c`、`insurance_flow.c` | 自动停止时点和减速落格，不按回报选格；`test_roulette_flow`、`test_hotel_flow`、`test_insurance_flow` |
| 命运取卡 `0x44C3B7 → 0x441E77` | `special_event_commit.c:fortune_take_cards` | 每名活动对手原卡槽随机取一张；按ID移除首个重复项后压紧；满手先退首个最低价格卡再接收；`test_special_event:fortune_random_card_inventory` |
| 免费/嫁祸/免罪/复仇 `0x44476A`等 | `card.c`及特殊事件防御链 | 按费用、关系、随机门槛与攻击类型分派；不主动选用这四张卡；`test_card`、`test_special_event` |
| 支付/破产 `0x41D2C6`、`0x40CD87` | `ledger.c`、`victory.c`、破产流程 | 资金优先序、清理和遗留资产结算是共享游戏规则；`test_bank`、`test_card`、`test_victory`、回放测试 |

## 验证及剩余边界

构建采用仓库的 `python scripts/make_minimal.py`，隔离目录为 `BUILD_DIR=build/ai-original-align`、`BIN_DIR=bin/ai-original-align`。2026-10-13最终源码验证结果：

| 执行目标 | 结果 |
|---|---|
| `test-unit test-ai-landing test-auction test-auction-flow` | 全部通过 |
| `test-replay sdl` | 全部通过，含真实地图初始化/自动回合/资产巡游/目标资产结局和存读档回放 |
| `git diff --check`（识别CRLF行尾） | 通过 |

真实地图回放以一帧资源代用品推进现有拘留/神明动画、发言和提示队列；它验证事务及阶段衔接，不代替画面比对。资产目标可在其他玩家仍拘留时结束；全玩家访问和收费覆盖由独立资产巡游回放验证。特殊格入口音按动作2..16校验，短巡游奖励格按10..13校验。

完整兼容性为 `partial`，仍有这些明确边界：

1. 原版主线程共享运行库随机字；SDL 活动局以 `ai_rng_state` 统一游戏及表现消费者，落实启动/介绍结束/每天股票前播种、动态对象先于成交量初始化、时光机保留实时随机状态。相同开局种子仍不能代替相同定时输入及表现消耗序列的长期逐帧对照。
2. 全局资源锚点表、道路表及局部爆炸表的来源、准入、同点 OR/覆盖和扫描顺序已逐项核对；八方向投影网格6728格逐值一致。SDL 候选收集入口测试覆盖全部类别、缺失资源和边界裁剪。无法映射的 raw 结果仍返回 deferred；纯 core 人工状态未提供完整对象表时的回退不代表屏幕投影等价。详见 [随机流与像素候选表](ai-rng-pixel-compatibility.md)。
3. 停留卡沿用合法旧raw目标；`0x443F80` 先检查raw非零，raw=0时取消。非零raw的低byte为0时，`0x40D293` 返回-1，后续可能越界；低byte的八个人物位包括四名普通及四名特殊人物。SDL对无效目标提交NONE并由领域校验拒绝，不复制越界访问。此边界不具备字节级行为等价性。
4. `0x48BE58`是运行期scratch。SDL session保留它，但现有存档格式不序列化该scratch；加载时重置。旧缓冲区影响的异常路径不构成跨存档轨迹保证。
5. 本表确认决策及命令链。全部地图、长期整局对照和所有动画像素尚不构成此次测试结论。
6. 新闻15存在已知原版BUG：`0x448BE2(15)`接受任一已建住宅/设施，而`0x44A453(0)`只列已建住宅并执行`rand()%count`。仅设施已建时住宅候选为0并除零。按用户指定，SDL 的新闻15资格判断和候选均只计已建住宅，没有住宅时跳过该新闻。这是明确的兼容例外，见 [特殊事件证据](m3-8-special-events.md)。

更细的对象表与特殊格证据见 [基础AI](m3-10-basic-ai.md)、[AI落地专项](ai-landing-alignment.md)、[拍卖](43bde5-auction-system.md)、[魔法屋](43380a-magic-house.md)。
