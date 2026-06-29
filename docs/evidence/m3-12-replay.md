# M3.12 核心 replay 与存读档证据

## 范围

本记录包括 M3.12 不依赖存档的核心规则链，以及本轮补充的 M3.11 存档往返与
AI 市场分叉证据。保存数据往返不等同于 SDL 槽位 UI 或原版画面对照验收。

## 可重复场景

`tests/replay/test_full_game_replay.c` 构造三名玩家的短局状态，并通过公开
`GameCommand -> GameEvent`/核心事务边界依次覆盖：

1. 新局初始化；
2. 核心路线选边、逐步提交和玩家节点更新；
3. 真人买地、升级、收费；
4. 设施购买和建设；
5. AI 经过路线事件自动买地；
6. 卡 14 停留、道具 2 路障；
7. 股票买入、卖出和股票日更；
8. 新闻 phase 0/展示请求/phase 1 提交；
9. 玩家破产、完整轮回与日推进；
10. 资产目标胜利。

另有 `tests/replay/test_real_map_bootstrap.c` 使用原始 V3.11 `Map.mkf` 的
gmap0（104 个路线节点、51 条地产记录、5 条设施记录）装配 `GameSession`，
通过应用层逐帧移动 3 步，并确认 8 个动态对象槽位、玩家节点提交和路线事实收集成功；
同一地图和 seed 会运行两个独立 session，并逐字段比较动态对象摘要、节点链和路线事实。
随后从同一 gmap0 找到可达的 action 13 特殊格，实际执行一次
`presentation request -> resume -> card inventory event -> resolved`，确认真实路线
特殊事件也能进入现有执行器链；本次双回放实际请求 `Data.MKF[536]`、停留时长为
`0`，两次都抽到 card `17`，库存数量为 `1`，卡池余量为 `3`，并收到
`ROUTE_EVENT_APPLIED`。请求先经 `rich4_app_route_event_step` 转为
`RICH4_ROUTE_PRESENTATION_SPECIAL_PLACE`，再由核心恢复接口提交奖励。
缺少原始资源时该 smoke 明确跳过；它不把资源缺失环境误报为通过。

同一测试随后在第二个独立 session 上执行真实地图长流程：只从地图合法后继中选择
普通道路/空节点、已确认 action `0` 的 second-special no-op，以及 action `10..13`
特殊格，避免把未证实的地产或稀有 UI modal 伪装成已完成。每回合仍经
`rich4_app_route_event_step` 和 `rich4_app_complete_route_event_turn` 完成回合。
gmap0 的两次运行均完成 12 回合、4 个完整轮回、4 天推进，并实际处理 5 次特殊格
presentation/resume；测试把 day-limit 设为 4，随后两次都收到唯一终局提交：
`game_over=1`、winner `0`、reason `GAME_WIN_CANDIDATE_DAY_LIMIT`，最终状态摘要为
`7f30d31c`，玩家/节点序列和终局字段逐项一致。也就是说，真实地图已从新局走完
回合轮转、日结、胜负评估并锁定终局；这是一条短配置终局，不宣称自然长局资产竞争。

在此基础上，测试新增一条不 teleport 的连续资产路径：BFS 只使用 gmap0 实际开放边、
并遵守“不能立即返回上一节点”的移动约束，让三个 AI 逐步走到同一块 land `0`
和同一条 facility `0`。每次移动都经过真实 app movement、route facts 和
`rich4_app_route_event_step`；首个到达者自动购买，后续到达者经同一收费/设施轮盘链
产生资产竞争。为隔离本专项的地产/设施证据，测试只清空 presentation dynamic-object
池（神明 acquisition 另有独立 replay），并把每个 AI 配置为现金 `20000`、银行
`500000`，day-limit 为 `300`。两次运行均让三名玩家访问两类目标（mask `7/7`），
产生 land 收费 `311` 次、facility 收费 `50` 次，终局保留 land owner `1`、facility
owner `2`，以 winner `2`、reason `GAME_WIN_CANDIDATE_DAY_LIMIT` 结束，最终摘要
为 `20d1fe02`。因此这里已证明真实地图上的连续地产/设施购买—收费竞争路径；终局仍是
受控 day-limit，而不是声称已经完成以资产目标自然决胜的完整长局。

同一测试另外增加资产目标终局路径：仍从 gmap0 的真实起点逐边移动到 land `0`，
但保留新局的现金/银行分配，关闭 day-limit，并使用新局界面可配置的最低资产目标
`X3`（`initial_fund=20000`，`win_target=60000`）。三名 AI 均访问目标（mask `7`），
并保留真实 runtime 初始化的 8 个 dynamic-object 槽位；目标回放使用固定动态对象
布局 seed `1`，所以正常道路神明落地效果也在同一 app/core 链中运行。路径产生 land
收费 `59` 次，终局仍有 `8` 个动态对象槽位、其中 `2` 个带附身关系；第 `285` 个回合经正常回合轮转触发
`GAME_WIN_CANDIDATE_TARGET_ASSETS`，winner `2`，最新摘要为 `4b177512`，双回放一致。
这闭合了包含动态对象池的真实地图资产目标判定与终局提交；路线仍是为覆盖目标资产而
选择的确定性 BFS 路径；无目标引导的自然 AI 长局由下面的独立回放覆盖。

同一测试还增加了不指定目标节点的自然 AI 回放：三个玩家都走真实的 AI 回合前置动作、
骰子状态机、app movement、route-event executor、presentation/resume 和回合轮转，
不使用 teleport 或目标覆盖 BFS。两次运行均完成 `12` 个 AI 回合、`12` 个 stop、
`27` 个 route fact、`4` 个完整轮回和 `4` 天推进；随后以
`GAME_WIN_CANDIDATE_DAY_LIMIT` 结束，`game_over=1`、winner `2`，最终状态摘要为
`b9239bfd`。因此真实 gmap0 已有无目标 AI 驱动的完整骰子—移动—路线事件—轮转—日限
闭环，并且双回放逐字段一致。

路线事件的买地、设施和收费仍由 `RouteEventExecutor` 暂停/恢复；除短配置核心链外，
真实地图 smoke 还在两个独立 session 中各选取一个合法地产节点和一个合法设施节点，
通过 app route-event step 让 AI 自动购买，结果分别为 gmap0 的 land `0`、facility `2`，
两者 owner 都为玩家 `0`、level 都为 `0`。这条补充路径只证明真实地图上的未拥有地产/设施
落点和自动购买链；另有两次独立的已拥有地产 smoke：预置 land `0` 为玩家 `1` 的
level-1 地产，经同一 app executor 触发收费 presentation/resume，玩家 `0` 现金
`10000 -> 9900`、玩家 `1` 银行 `12000 -> 12100`，结果为 `ROUTE_EVENT_APPLIED`。
已拥有设施 smoke 预置 facility `2` 为玩家 `1` 的 type-1/level-1 设施，实际跑完
自动轮盘，稳定落在 outcome `3`，玩家 `0` 现金 `10000 -> 9850`、玩家 `1` 银行
`12000 -> 12150`，同样完成 route event 且没有人类 interaction modal。两条路径均
在两个独立 session 中逐字段一致。测试故意不调用保存/读取命令或任何快照 API。

此外，测试扫描 gmap0 的实际路线 action `1..16`，对每个存在且可达的 action 选取一个
真实前驱/目标节点，以全 AI 的 `GameSession` 逐帧移动并经 app route-event seam 完成。
本次 gmap0 的 16 个 action 全部存在并通过（mask `0x1fffe`）：包括银行、新闻/命运、
监狱/医院、小游戏、彩票、特殊场所、商店和魔法屋；action `2/3` 实际进入并结束
special-event presentation，action `10..13` 实际完成 special-place resume。每条路径
最终均为 `ROUTE_EVENT_APPLIED`，双 session 摘要为 `05e42f7e`。这是无窗口的 app/core
可达性证明，不宣称 SDL 逐像素等价。

## 等价判定

同一 setup seed `311`、同一目标动态对象布局 seed `1` 运行两次：

- 逐条记录事件类型、玩家、值、flags、typed subject/target 和拒绝码；
- 每个阶段记录完整 `GameState` 的 FNV-1a 摘要；
- 最终对两份事件文本和完整 `GameState` 字节做比较。

任一随机流、事件顺序、状态副作用或终局字段不稳定，测试都会失败。目标已
加入 `Makefile` 的 `test-replay`，与原有首回合 replay 和路线事件 replay
一起运行。

## 当前结果与剩余边界

此前的 Windows `make -B test-replay` 通过，`-Wall -Wextra` 无警告；本轮隔离目录中的
`test-unit`、`test-replay` 和 SDL build 均通过。短配置核心链和
真实 gmap0 启动/多回合应用移动、未拥有地产/设施自动购买、连续地产/设施购买—收费
竞争、已拥有地产收费/设施轮盘、action `1..16` 全家族 route-event 双回放，以及
day-limit、无目标自然 AI 长局与包含真实动态对象池的 `X3` 资产目标终局双回放均可
运行；SDL smoke 现在另外覆盖真实 V3.11 资源 bootstrap、菜单两帧和复用正式新局
initializer 的 gmap0 游戏场景 120 帧，菜单/游戏 framebuffer 摘要分别可双回放稳定复现
（`21f26f63` / `03ce8e1f`；逐帧摘要链 `f8719841` / `021f4318`）。smoke 模式使用固定 RNG 和逐帧推进的逻辑时钟，交互运行仍使用 SDL 实时时钟。
这仍不替代 SDL 资源/逐像素验收。2026-09-25 默认旧 `build/`/`bin/` 增量运行时曾
分别触发停留卡断言和游戏 smoke 摘要失配；检查发现头文件修改没有触发所有依赖对象重编译。
为保留已有构建，使用 `BUILD_DIR=build/m3-ai-review2 BIN_DIR=bin/m3-ai-review2` 隔离构建后，
完整 `test-unit`、`test-replay` 通过，SDL smoke binary 编译通过。新 binary 在 dummy SDL 下菜单 2 帧
和 gmap0 游戏 120 帧各独立运行两次，均匹配原静态摘要：`21f26f63/f8719841` /
`03ce8e1f/021f4318`。原版逐像素等价、真人 SDL 交互到终局和完整平台矩阵仍未验收。

## M3.11 存读档往返和市场 replay（2026-09-25）

新增 `tests/replay/test_save_load_replay.c`，独立目标为
`python scripts/make_minimal.py test-save-load-replay --progress --raw-output`，本轮单独
运行通过；它也由隔离全新构建的 `test-replay` 末项再次执行。输出
`save/load round-trip and AI market replay: ok (seed=161)`。

分别对以下中间状态写槽 2、读回并核验：路线交互暂停且带确认/取消命令、角色半步移动、
日界待自动保存、另一玩家淘汰、异地图 stage/map 身份。各次比较完整 `GameState`、
`GameFlowState`、时光机状态/flow 的字节，并将加载后的 session 重新编码，要求与原存档
规范字节完全一致。当前/时光机演讲语音须重绑到加载 session 自有快照，瞬态音频队列须清空。

AI 市场分叉从完全相同的存档副本继续调用 `rich4_ai_run_preactions` 和正式
`rich4_game_dispatch_command`；固定 seed `161` 实际触发挂牌创建、挂牌调价和股票购买，
逐条比较 AI 命令/核心事件文本，以及最终 `GameState`/flow。坏版本头、坏 CRC、截断文件、
非法槽号均须拒绝，且失败不能改写预填充的输出 session。跨地图的 `stage/map` 序列化身份
已有此往返覆盖，但 SDL adapter 真正切换地图资源的端到端交互仍留在缺口 8。
