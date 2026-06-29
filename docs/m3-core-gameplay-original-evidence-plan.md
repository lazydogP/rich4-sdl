# M3 核心玩法进度

最近增量核对：2026-10-12（魔法屋 replay 演出续接及后续终局阻塞定位）。下方全阶段表及长篇缺口描述含 2026-09-26 历史快照，应以较新的专项增量为准；本次没有重新验收全部 M3。

**M3 尚未完成：目前还有 1 项实现缺口、1 项验收缺口，另有未定范围的动态演出待取证。** 缺口按实现与验收类别统计；已实现子集、原版 no-op、输入不足的拒绝和等待展示不计为整项未实现。

## 2026-10-12 架构与股票取整增量

本次使用 IDA Pro MCP 与 `rich4/asm/rich4.asm` 核对；保留工作区原有修改，未放宽架构规则。

- **三处平台状态写入已移入 app：** `tool_animation_flow` 在 Data[526] 成功入队时记录小捣蛋动画开始（`0x40F381`）；在队列完成时恢复公司金额提示计时（公司演出/金额窗口见 `0x41982D → 0x41AD99/0x41AEB4`）。救护车关闭动画只跳过 Data[524]，仍保留位置同步、住院语音和理赔续接（`0x43ED2E..0x43EE04`）。`insurance_flow` 负责待赔通知的入队与计时，按 `0x44BA63` 先显示 2000ms 再付款；满队列拒绝不会改写旧通知。
- **AI 股票取整已对齐：** `0x457DBC` 的控制字高字节 0x1F 表示向零截断。持仓市值按每只股票逐项截断并保留累计值经 float 暂存；买入数量及独立股卖出比例评分同步修正。详细地址及边界回归见 [AI 股票取整专项](evidence/m3-10-basic-ai.md#2026-10-12-股票取整专项)。此项不再列为待实现。
- **额外架构阻塞：** 三处写入消除后，完整目标暴露原有 `sale_draw_panel_anchor` 纯转发包装超预算。已删除包装，调用处直接使用同一 `draw_confinement_panel_chunk`，参数、资源和绘制语义不变；没有调整上限。
- **测试可移植性：** 狗咬住院 SDL 测试原写死 `/tmp`，Windows 无该目录时存档往返失败；改用 Make 已创建的 `build/` 临时文件，成功后删除。SALE 回归同样改用 `build/` 临时目录和 Windows `_mkdir`；相关 SDL 回归目标补齐 `stage-sdl-runtime`，避免隔离 BIN_DIR 缺 DLL 导致 Windows 装载停滞。

本次验证使用隔离目录 `BUILD_DIR=build/m3-stock-architecture BIN_DIR=build/m3-stock-architecture/bin`，命令入口均为 `python scripts/make_minimal.py`：

- **通过：** `test-architecture test-original-data`（三处平台写入零违规、semantic forwarders 回到上限 6、真实 V3.11 扫描通过），以及 `test-ai-landing test-worker-animation test-company-landing test-dog-encounter test-god-sdl test-options-sale test-save-load-replay test-render tools sdl test-sdl-smoke`。新增的股票、app 动画及保险边界测试均包含在这些目标中。smoke 的菜单/游戏摘要仍为 `21f26f63/f8719841` 和 `03ce8e1f/021f4318`。
- **全量仍失败：** `-k test` 的 `test-engineering-attack` 在 `tests/unit/test_engineering_attack_flow.c:223` 断言 `property_turn_end_ticks == 8` 失败（此前公司专项已记录）；`test-replay` 在 `tests/replay/test_real_map_bootstrap.c:2164` 断言 `route_facts.count > 0` 失败。本次没有修改这两个测试或其路线/移动实现，也没有放宽断言；全量单测/replay 不能记为通过。由于聚合目标中断，不据其它定向通过推断全部单测执行成功。
- 首轮 `test-original-data` 在 Windows 隔离 BIN_DIR 因混用路径分隔符被 cmd 解析成 `build` 命令；Makefile 已将扫描器路径统一为带引号的 Windows 路径，独立复验通过。
- SDL 专项使用原始资源的自动回归，不代表真人交互、原版逐像素/逐帧对照或 Linux/sanitizer 平台验收。

旧表中“EA2=3 工程车拆屋未实现”和“完全没有 typed lifecycle/JUMP”不能继续作为当前结论：前者已有 2026-10-05 完成记录，后者在 `game_session`/`bankruptcy_flow` 与 SDL `game_loop_preview_update` 已有 request、派发和 JUMP 路径。本次不据此宣称所有 producer/清理/重建及原版画面时序完成验收。特殊 raw 映射、完整 AI 候选等价、存读档剩余时序、完整交互与平台矩阵继续开放，M3 尚未完成。

## 2026-10-12 两处失败的原版时序修正

前节的两处失败记录为修复前快照；本轮仅修改测试驱动及断言，未提前执行游戏效果。

- **土地公与工程车：** IDA 与 `rich4/asm/rich4.asm` 的 `0x41982D` 公共尾部确认先调用 `0x40F381`，再调用 `0x448A7E`，最后返回 `0x88`，由 `0x418E7F` 写入 EA5。土地公在 `0x40F82F` 修改所有权后，仍须完成 `0x40F878` 的 1500 ms 提示与 `0x40F8AB` 的台词。测试接入 app 提示回调及语音状态机，覆盖 1499/1500 ms、音频仍在播放/播放结束边界；普通地产路径最终为 8 tick，`0x418F59/0x418F78` 的返回路径仍为 0 tick，两者都不拆除已归自己的建筑。
- **真实地图落点：** `0x40D7C4` 在步数减至零后，下一次 timer 才处理 `0x41B42D`，经过五次停步递减再进入 `0x418E7F`。bootstrap 循环改为等待离开 MOVE，并检查零剩余步数及 ROUTE_EVENTS 状态。另经 IDA 复核 `0x4198C3`：动作 0 / raw 0 的空格直接返回；因此按实际地图数据精确检查 0 或 1 条事件，并检查非空事件为最终节点的 STOP，而非要求每个落点必有事件。
- **验证结果：** 经 `python scripts/make_minimal.py BUILD_DIR=build/m3-stock-architecture BIN_DIR=build/m3-stock-architecture/bin` 运行，`test-engineering-regression` 和全量 `test-unit`（含架构检查）通过；`test-replay` 已越过原 bootstrap 断言，但在后续 `run_real_map_route_action_smoke` 的 `calls < 512` 暴露另一处阻塞。诊断为 action 16（魔法屋）等待续接，现有 replay drain 未驱动 `magic_house` 演出；该问题独立于本轮两处修复，完整 replay 仍未通过。

## 2026-10-12 魔法屋 replay 续接修正

- **原版证据：** IDA Pro MCP `0x43380A` 的 `0x4339BD → 0x4339CE` 与 `rich4/asm/rich4_magic_house.asm` 一致：AI 先显示条件/动作提示 1500 ms，再调用 `0x431CAA`，效果完成后返回落点流程。动作 1 还会逐目标执行三次命运。详见 [魔法屋续接证据](evidence/43380a-magic-house.md#2026-10-12-真实地图-replay-续接)。
- **修复：** replay 演出 drain 接入已有 `rich4_magic_house_update`，转接返回的特殊事件展示；增加 action 16 进入 AI_NOTICE/等待态及完成后清除等待的断言。没有跳过提示、强清等待或改写游戏实现。
- **继续运行暴露的测试驱动遗漏：** 空路格无执行器时按 app 边界返回 FINISHED（原版 `0x4198C3`）；回合返回补齐 `0x418F59/0x418F78` 的神明/工程车演出等待，而非要求 complete 调用立即变为 IDLE。这些修正仅在 headless replay 驱动内，保持现有 app 状态转换。
- **验证：** 同一隔离目录 `build/m3-stock-architecture` 经 `scripts/make_minimal.py` 运行 `test-magic-house` 与 `test-save-load-replay` 通过。完整 `test-replay` 的 first-turn、route-executor、full-game 通过；真实地图已越过 action 16 和普通资产长走，但 `run_real_map_asset_target_walk` 的胜利原因断言仍失败。诊断运行显示 453 回合/181 天后 winner=0、reason=3（LAST_PLAYER）、资产 73982、目标 60000；测试要求 TARGET_ASSETS。已删除临时诊断输出，未放宽终局断言。需继续核对淘汰与资产目标判定顺序及该场景条件；后续长走/自然 AI 和双运行一致性未到达，不能记录整组 replay 通过。

前节“魔法屋 action 16 阻塞”为修复前记录；当前剩余 replay 阻塞以上述终局断言为准。M3 仍未完成。

## 范围与依据

M3 覆盖地产/设施经济、破产与胜负、卡片/道具、新闻/特殊事件/持续状态、股票、基础 AI 和可用存档。完成目标仍是：真实地图可从新局运行到终局，常用交互可达，存读档不改变后续结果。

本次 MCP `survey_binary` 确认：`assets/original/Rich4.exe.i64`，32-bit x86，base `0x400000`，SHA-256 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`。下列关键地址均在本次会话读取反编译或指针表；不以文档中的“已完成”替代实现检查。

## 全阶段状态

| 步骤 | 当前可确认范围 | 剩余项 |
| --- | --- | --- |
| M3.0 协议 | typed domain、命令/事件、拒绝边界已建立 | 存读档枚举不代表执行器已实现，见 4–6 |
| M3.1 路线执行器 | PASS/STOP、交互暂停/恢复、unknown 安全结束已接入 | 保留未知 raw/action 的显式拒绝，不据兜底枚举推断缺失玩法 |
| M3.2 账本/破产 | 付款、破产清理、市场挂牌交易和 SALE 交互已接入 | Panel 73/74 原版像素/时序对照见 8 |
| M3.3 普通地产 | 购买、升级、同组收费、关联双收款已实现 | 场景验收见 8 |
| M3.4 设施/公司 | 建设、研究、轮盘、交通收费及公司关联已接入 | 场景验收见 8 |
| M3.5 卡片 | 30 项目录，26 项主动 handler，18–21 为原版被动卡/主动失败 | AI 策略见 3；原版表现验收见 8 |
| M3.6 道具 | 13 项 handler；时光机由 session 快照恢复后消费 | AI 策略见 3；表现验收见 8 |
| M3.7 神明/收容/状态 | 倒计时、医院/监狱、动态池、附身/收集、释放与已证实演出已接入 | 表现验收见 8；额外演出仍待取证 |
| M3.8 特殊事件 | 新闻 36 项、命运 37 项、魔法屋 12 条件/12 动作及路线 action 1–16 有实现/覆盖 | 表现验收见 8 |
| M3.9 股票 | 买卖、日更、历史、公司股、估值、周日/地图节日休市已接入 | 日历 Panel 变体与完整场景验收见 8 |
| M3.10 AI | 股票、市场挂牌、购买/升级、卡片/道具 typed 子集、特殊融资追缴与自然移动可运行 | **部分实现**，见 3 |
| M3.11 存读档 | 固定格式整局磁盘存读档、五个手动槽、自动槽、Data.MKF[520] 槽位面板及日界自动保存已接入；异地图档经地图资源和 domain 数量校验后同步切换场景设置；演出语音快照可在读档后重绑；存档往返、AI 市场分叉回放、破损文件回归和真实地图手动槽 1 保存/读取均已通过 | 跨地图实际场景切换及其余模态真人验收归入 8 |
| M3.12 最终验收 | 2026-10-12 全量单测（含架构检查）及工程车回归已有通过记录；魔法屋 replay 续接已修复，完整 replay 另阻塞于资产目标长走的胜利原因断言 | **仍部分完成**；修复全量回归后，继续 SDL 真人交互、原版同状态对照与平台矩阵，见 8 |

## 本轮完成项

- **M3.2 市场挂牌可用链路：** `GameState.market_listings` 保存每名玩家 7 个 12-byte 挂牌；typed 命令/账本处理创建、撤销、成交和 AI 调价。SALE 工具栏现打开 app 交互并可选择库存、输入报价/数量、浏览、购买和撤单；SDL 通过 `Panel.mkf[73/74]` 资源绘制可交互面板。文字列表与原版逐像素布局/时序未对齐，继续归入验收缺口 8。
- **M3.10 AI 市场策略：** 自动阶段按 `0x4284BE` 接入 1/15 挂牌、1/3 调价、1/4 购买、原始卡/道具候选和报价表，以及股票/资产购买条件；挂牌、调价、购买的存档分叉回放已覆盖，详见 M3.11 存档回放验收。
- **M3.10 AI 全局表投影接线：** SDL 当前地图视图把可见对象 raw ID 与屏幕中心传给 core；Y 坐标按原版窗口原点校正 `-40`。core 对同一 440×440 像素的 raw ID 做 OR，并以行优先顺序提供给 `0x421717`、`0x420970` 的普通玩家候选，以及机器工人 `0x421BA6` 的 land/facility raw ID 扫描。原表的玩家可见掩码也供 `0x41E779`、`0x41FACC`、`0x4202D2`、`0x4207CC` 对应 typed 目标分支使用；卡片 `0x41FE6F` 的随机候选在 SDL 表可用时按压平后玩家位顺序构造。IDA 复核 `0x41E779` 与 `0x4202D2` 后，卡片 2/26 的可见集合由表掩码建立，但其回退目标扫描按玩家编号递增并持续覆盖目标，因此最后一个满足门槛的玩家获选；core 已按此语义移除提前退出。IDA 同时确认卡片 `0x41FACC` 先以全屏表形成在场 bitset，再按玩家编号顺序检查普通玩家目标；core 保持这一第二阶段的顺序。完整反编译 `0x41FACC`、`0x41FE6F` 又确认这两条 handler 不接受特殊人物高位；core 已移除其特殊人物兜底命令。`0x41FE6F` 当前按 raw 像素首次出现顺序去重候选；IDA `0x40829D` 证明 actor overlay 的 raw ID 为 0 且与主体同屏幕中心，像素 OR 后去重不会改变候选权重。机器工人表存在时不从全地图 typed 状态补回表中没有的地产。另按 `0x40829D`/`0x409DE7` 修正 SDL AI 专用来源：鼠标命中队列会为未建、无主地产/设施合成遮罩项；对应原版队列项没有资源指针，`0x409DE7` 会跳过，AI 收集器现已过滤这类 hit-only 项；动态池项也按其 `DATA.MKF[396..415]` 资源可用性和 attached-owner 字段应用资格过滤。完整反编译 `0x40829D` 又确认没有独立工具机器人来源；其 raw `0x8100` 由动态池第 0 槽可能产生，AI 收集器已移除额外合成的机器人对象，但场景渲染路径保留不变。`renderer_global_table_projection` 回归确认：路线 raw ID 中有玩家候选但 SDL 表中缺少时不能凭路线状态复活候选；将目标 bit 加回表后区域飞弹可选中目标。普通/特殊 actor 资源选择、其他特殊对象来源及所有消费点的完整等价仍未证明，见缺口 3。
- **M3.10 最近动态神明候选边界：** 对照 `0x444D1A → 0x40A45C(-1)` 和类型谓词 `0x40EA62`，候选仅接受动态类型 `1..10、12、15`，排除已附身对象；指令先对平方距离开方，再与 `10000.0` 严格比较，距离上界为 `<10000`。SDL AI 投影和 typed helper 均用等价的平方距离上界 `100000000`；投影表可用时还按 raw 表坐标同格 OR 和行优先顺序决胜等距候选。真人卡片 panel 现在从当前 SDL 视口传入同一全局表；缺表 typed 回退按动态槽序且跳过 `node==0` 的未活动池槽，因为 `0x444D1A` 仅遍历 `0x40A45C(-1)` 可见 raw 表项，而 `0x409DE7` 只投影符合资格的活动渲染队列项。调用链包含 AI `0x41FFF8` 和卡片执行 `0x444E1A`。
- **M3.10 神仙/恶魔卡全局地产候选：** IDA `0x41F037`、`0x41F1B3` 都从 `0x40A45C(-1)` 的 raw 行序中读取普通地产 ID，再按名称、类型、持有人和等级聚合。SDL AI 现在先应用同像素 raw-ID OR，再按行序解码普通地产候选；神仙卡按可见同名自有地产组去重后抽取，恶魔卡按首个可见同名组累计持有人与等级。原表可用但候选为空时不会扩大到 typed 全地图/路线目标；无表 core-only 状态保留已有 typed viewport 和路线回退。
- **M3.10 其他全局地产候选：** 换地卡 `0x41EAE2`、共享怪兽选择器 `0x41F400`、涨价卡 `0x42040E` 同样消费 `0x40A45C(-1)`。SDL raw 表可用时，core 先按坐标同像素 OR 和全屏 row-major 顺序解码 land/facility ID，再沿用各 handler 的 typed 筛选；表存在但没有候选时不回退扩大搜索。只有无全局表的 core-only 状态保留 typed viewport/路线回退。
- **M3.10 拆除卡目标 fallback：** IDA `0x41F6A9` 先复用 `0x41F400`，失败后按 `0x40A45C(-1)` 行序检查地产、设施及高位动态 slot。地产要求配置字段非零、目标为他人已建地产且 owner 有至少 4 处已建地产；设施要求配置低两位非零、目标为他人所有的 type 3、level 1 设施；动态 slot 16–45 中 type 16 只接受他人地产/设施，type 17 只接受当前 AI 的地产/设施，未发现 type 18 分支。字段映射由 `0x406DE7`、`0x41DDA9`、`0x421E20` 对照到 `is_ai`/`ai_personality`，已建地产计数对应 `0x41970F`。这些候选在同一 raw 行序中交由 typed placement 与公共 inspector；缺表 core-only 状态保留 route-backed 回退，表存在但无目标时不扩展到 typed 全表。隔离 SDL 构建 `build/m3-ai-review12` 通过，未运行测试。
- **M3.11 自有格式整局存读档：** [game_save.c](../src/app/game_save.c) 和生成 codec 逐字段写入定宽小端数据，格式 v3 含槽号、长度和 CRC；临时文件 flush 后原子替换。读档先完整解码到临时 session，再校验并一次性替换；语音文本、银行/收容演出和待播道具语音使用定长文本快照，加载后重绑到 session 内存。股票提示保存稳定编号。
- **M3.11 槽位 UI 与自动保存：** SDL 顶栏 SAVE/LOAD 打开原版 Data.mkf[520] 面板；手动保存为 1–5 槽，读取列出自动槽 0 和手动槽，坏档及资源/计数不匹配的异地图档不列出。读写接入 `saves/SAVE%d.R4S`；共享整轮计数回卷后由 SDL adapter 在启用配置时写槽 0。对应原版 `0x4039C2`、`0x403D74`、`0x404165`、`0x417D65`、`0x418EBD`。
- **M3.11 跨地图加载：** 加载列表允许目标地图已预载、路线/地产/设施/公司计数与存档匹配的异地图档；提交前校验地图，成功后同时替换 session 与 `NewGameSettings.stage/map`，场景下一帧据新设置选用对应 `MapData`。原版 `0x402AC5` 从文件读入 stage/map 后重建地图资源指针并调用 `0x407AD2`，支持按存档地图重建的依据已确认。新 replay 覆盖了异地图 stage/map 序列化身份；场景 adapter 实际切换地图资源仍归验收缺口 8。
- **M3.11 存读档回放验收：** [test_save_load_replay.c](../tests/replay/test_save_load_replay.c) 对交互暂停、半步移动、日界待自动保存、淘汰后、异地图 stage/map 身份分别做 v3 写入/读取往返，逐字节核对 `GameState`、flow 和时光机快照，并确认再次保存后的规范文件一致；读档重绑语音快照且清空瞬态音频队列。另从同一市场存档分叉，实际重放 AI 挂牌、调价、购买，比较命令/事件记录和终态；旧版本、CRC 错误、截断及非法槽号读取均验证失败时目标 session 不变。目标 `test-save-load-replay` 已独立通过。此项关闭验收缺口 7；异地图场景 adapter 的 UI 切换仍归 8。

## 剩余实现缺口（1 项；沿用原工作包编号）

| # | 工作包 | 实际代码缺口与原版依据 |
| --- | --- | --- |
| 3 | AI 完整对象表/候选语义（M3.10） | 普通玩家、地产/设施、动态对象和神明候选已有 SDL raw-ID 投影；静态锚点、常规行走/动作及工程车 EA2=3 已有实现和专项回归。仍需闭合 EA1 替代资源、其余动态资源槽和特殊 raw 的 typed 映射、各 handler 的完整候选顺序/结果映射；世界坐标/相机状态、全局计时相位与消息循环延迟需同状态运行对照。没有安全映射或来源不足的分支继续 deferred，不扩大候选范围。2026-10-12 已关闭股票向零截断差异。typed lifecycle 与 JUMP 派发已存在，所有 producer、回合门控和资源清理/设置重建的完整等价尚未重新审计，不再写成“完全未实现”。详细原表分支与历史推导见 [AI 专项](evidence/m3-10-basic-ai.md)；工程车与 lifecycle 的旧缺口描述按本页较新增量理解。 |

**本轮全局表调用点复核：** IDA 返回 `0x40A45C(-1)` 的 16 个 xref；`0x40AC7B` 是地图状态/持续效果清理，其余 15 个属于玩家掩码、地产/设施、动态对象或神明候选。AI 对应分支已接入 SDL raw 投影：玩家 bitset 包括 `0x41E779`、`0x41F901`、`0x41FACC`、`0x4202D2`、`0x4207CC`，`0x41FE6F` 保留行优先候选去重；地产/设施 raw 扫描包括 `0x41EAE2`、`0x41F037`、`0x41F1B3`、`0x41F400`/`0x41F6A9`、`0x42040E`、`0x421BA6`；`0x420970`/`0x421717` 使用全局玩家掩码和对象窗口，`0x444D1A` 的动态神明选择按同一表序处理。卡片 12 的 `0x41F6A9` 还在 `0x41F400` 无结果后按原表顺序处理玩家配置门槛下的建成地产、一级 3 型设施和动态槽；`0x496B7F` 的 0..2 值与 core 的 `ai_personality` 采用相同的 15 档阈值，`0x496B79 & 3` 映射到 AI 玩家门槛，建成地产数等于 `0x41970F` 的 owner/type 扫描。普通 actor 表项另按 `0x40829D` 的 `x != 0` 与 `+50..+53`/`+21 bit 0x20` gate 对照并接入；`0x41982D`/`0x40D5A5`、`0x40D6BE` 与 `0x418EBD` 已对齐 type-1 设施延迟位的标准设置、到期和暂态清理；`0x40CD87` 另在破产清理时清零整字节。`0x409DE7` 以单个 `(x,y-40)` 锚点格保存符合资格对象的 raw ID，格内按位 OR，再交给 `0x40A45C(-1)` 行优先压平。普通 actor 的常规 EA2=0/1/2 槽与 SDL 静止/移动/动作部件号已逐项对上；视口到 `(x,y-40)` 的静态投影公式已与原版网格、子格系数和原点对齐；移动时长和中间帧公式已按 `0x40C05C` 的单精度时长、float 增量逐次累加及整数截断接入。标准 EA1=0 下，`0x40D7C4` 的 EA2=2 动作帧计数/方向组及结束后立即切至 EA2=1、EA3=0 已与 SDL 掷骰弹窗时的资源槽切换对齐；EA2=1 方向量化与帧游标逐次更新公式本轮已静态对齐；计时器相位和世界坐标/相机状态逐帧等价仍待运行对照。EA1 替代指针、EA2=3 特殊状态和特殊 actor 当前部件/动态 type 的其余动态槽及 raw typed 映射、部分 handler 候选顺序仍未闭合。Map.mkf 静态 sprite 资源指针非空门槛已独立于 SDL 解码标志接入。`0x48BE58`/`0x48BE64` 是结果标量；其 handler-specific raw/ordinal 结果语义已纳入核对。16 个 xref 数量本身不作为未实现项。

**2026-09-25 路线道具候选增量：** IDA 对照 `0x4213C5`（地雷）、`0x421574`（定时炸弹）、`0x40B343` 和 `0x409EF9`（可见路线表）后，路线步向已按 `0x40B343` 更正：六步从上一节点出发并避开当前节点；首轮移动前，SDL 的 `previous_node==0` 时以预选 `next_node` 对应该原始字段。视口可用时按原顺序构建 440×440 路线像素表：路线记录满足 `flags & 0xFFFF00 == 0` 才入表，同像素由较后路线记录覆盖，再按 y/x 行优先压平。地雷与定时炸弹都和六步路径求交；地雷只保留他人持有的原始 land/facility ID，定时炸弹保留全部交集候选；actor 当前收容状态与医院/监狱节点匹配时遵循原版优先分支。其余候选使用原 RNG 模数选择并经过公共 tool inspector；无视口时保留不含可见性和像素碰撞的 typed fallback。

**2026-10-08 AI 道具规则复核：** `0x447D97` 按库存种类构建候选，环形最多判定 4 次，1..3 种候选保留重复尝试。遥控骰子 `0x421827` 检查附身槽编号、慢行、流动资金、投资修正和六步分岔；无主地产要求同名自有数量 >1 并读取 `+28`，自有普通地产读取 `+30` 并保留严格最高等级，无主设施读取 `+34`，自有设施读取固定 `+36`。四类价格比较均为 `2.5*price<cash`，立即结束型候选按路线顺序选择，结果为实际骰值 1..6。机器工人 `0x421BA6` 按全屏表中可升级自有资产的当前等级收费选严格最大值；传送机 `0x421CB6` 按可见道路表选无主且等级至少 3 的最高等级资产，价格读取 `P*地产[+30]` 或 `P*设施[+36]`。13 项具体条件、收容数组和飞弹窗口规则统一见 [原版 AI 道具选择与目标规则](evidence/m3-10-basic-ai.md#原版-ai-道具选择与目标规则)。SDL 实现与原版逐项等价的完成度继续按实现及验证记录核对。

路障 `0x42107F` 已接入 `0x40B221` 的四步分岔检测和直筛、以及无直筛命中后的 `0x40B343` 六步路线/可见路线表评分。直筛复现 route flag、无主地产/设施、现金/存款与价格门槛、投资修正及 `+57` 慢行状态门槛，也处理低字节 action 15 且点券超过 200 的节点；全局分支按可见行序选择自有普通地产组值严格超过 `6000 * price_index` 的最高分目标。状态字段分别对应 `god_investment_modifier` 与 `days_tortoise_walking`。地图原始名称组覆盖所有 land type：直接分支按名称计数，0x419744 等价评分只累计同名 type-0 记录。名称字段不能安全终止时 deferred；无投影 core-only 仍限于已映射的 type-0 名组。静态锚点投影、`0x40C05C` 移动帧数及中间帧 float 累加/整数截断公式已接入；SDL 名义单步间隔已与原版 20 ms multimedia timer × `{6,4,2}` 速度表对齐，进入 MOVE 后首个脉冲等待一个间隔，积压脉冲合并为一次移动更新；全局 timer 相位差及消息循环抖动、世界坐标/相机状态逐状态对应仍待运行态验收。Map.mkf 静态 sprite 的资源指针非空门槛已接入；EA1/EA2 动态资源帧与 SDL 资格/指针选择的逐对象等价、其他对象资格、特殊 raw typed 映射及其他 handler-specific 候选顺序仍未闭合。

## 验收缺口（1 项，不冒充玩法未实现）

| # | 工作包 | 未完成的验收 |
| --- | --- | --- |
| 8 | 完整 SDL 交互与原版逐帧对照（跨 M3.3–M3.9/M3.12） | 全新 SDL binary 的菜单/游戏 smoke 双运行命中既有摘要基线，但仍需真人模态交互到终局及同状态原版像素/时序对照，覆盖轮盘/建设/研究、卡片/道具、医院/监狱/神明、新闻/小游戏/商店/魔法屋、日历 Panel 变体和异地图读档。默认旧 `bin/` 的初次差异来自未随头文件更新的陈旧构建产物，不作为代码回归证据；全新隔离构建已复验摘要。真实地图 replay 仍是无窗口 app/core seam，smoke 摘要与原资源逐帧解码都不证明场景合成与原版一致。最终 Windows/Linux/sanitizer 矩阵待此项补齐后重验。 |

**待取证，不纳入上述剩余缺口计数：** 旧文档泛称的“其他动态对象演出”尚无逐分支调用点/资源/状态清单，不能宣称全部实现，也不能凭此虚构待实现数量。已证实的动态池生命周期、旧神脱离、神明获取、收容切场景与特殊人物梦游 overlay 都已有代码；新增缺口应先给出原版调用点。未知 raw/action 的安全拒绝也不自动等于缺少一个已确认 handler。

## 本次纠正的旧进度

- 股票休市宿主映射已实现：`stock_calendar_events`、`rich4_stock_calendar_is_closed`、`rich4_stock_market_is_closed` 对应 MCP `0x428D01 → 0x4523D5`；删除旧“待映射”说法。
- 设施 `GAME_FACILITY_ACTION_DEFERRED` 已由路线执行器转入轮盘请求；地雷/炸弹 `STOP_DEFERRED_EFFECT` 已提交状态并排队表现；时光机 `RESTORE_REQUIRED` 由 app 消费。各自通过对应执行与表现入口处理。
- 卡片 18–21 的主动失败是原版语义；30 项目录不能写成 30 个主动成功效果。卡/道具目录无 `DEFERRED` 项，但 AI 目标策略仍有独立缺口。
- gmap0 已有自然 AI 的日限终局、BFS 资产竞争和含动态池的 X3 资产目标终局。BFS 目标覆盖路线与 12 回合自然 AI 日限测试须分开描述，不能合称“无目标自然资产长局”。
- 特殊融资追缴现已接入：`0x436B0A(0)` 作为 AI 资金阶段命令，并在核心直接出售股票/资产后执行；银行余额调配触发 `0x436B0A(1)`，真人和 AI 路径均调用共享账本。公司 owner 按地图辅助记录 type 7 扫描；银行优先扣款、差额破产清理和交易事件由 ledger 统一处理。既有 `test_full_game_replay` 的 `ai-maintenance=100` 只调用 `rich4_ledger_spend_cash`，不验证这条新增路径。
- 原版挂牌链 `0x4246C5/0x4247D5/0x4255DA` 已接入 typed 账本；raw type 3 为道具、4 为卡片。SALE 工具栏现在可建单、浏览、撤单和成交，SDL 面板使用原版 Panel 73/74 资源背景。AI `0x4284BE` 的候选、价格表、调价和购买分支已接入，并由存档分叉 replay 覆盖。
- 存档槽位命令域按原版文件编号区分 `0` 自动槽与 `1..5` 五个人工槽（MCP `0x418EBD` 自动写槽 0，`0x403396` 枚举槽 1..5）；SDL 面板、整局读写、整轮自动槽触发和跨地图加载已接入，数据往返、AI 市场 replay、损坏文件回归与真人槽 1 存读档已通过；跨地图切换 UI 仍未验收。
- AI 遥控骰子现将可映射候选的原版步数和优先级送入强制骰值；传送机使用 `0x409EF9` 建立的 440×440 可见路线节点表，按行序选出最高等级的 unowned land/type 3 facility；同等级保留首个候选。缺少 SDL viewport 时保留 typed-coordinate/forward-route core-only fallback；完整 SDL 投影无候选时不扩展搜索。
- IDA 对照 `0x41E779`、`0x4202D2` 纠正了卡片 2/26 的回退选择：可见玩家集合虽由 440×440 表掩码生成，阈值扫描本身仍按玩家编号递增并覆盖目标直至循环结束，最终选中编号最高的合格玩家；AI 扫描全部匹配者并持续更新结果。
- 删除逐日流水账、已失效的“下一步 M3.x”、重复构建宣告与易随结构体改变的状态哈希；专项过程和资源细节继续留在证据文档。

## 验证与证据入口

- 增量构建（2026-09-26）：普通 actor overlay raw 来源、主体资源指针门槛、特殊 actor overlay 过滤、`0x40C05C` float 移动插值、特殊 actor/动态对象及静态 map object 原始资源指针门槛、普通 actor 资源槽条件、传送机全屏可见路线表候选顺序、首个移动调用内部推进首帧和 type-1 设施延迟 actor gate 分别经隔离 SDL build22 至 build33 编译通过；build33 的全新隔离 SDL 构建覆盖普通 actor 掷骰前动作结束后按原版切到 EA2=1 行走资源（含 fallback lane）的修正；本轮未运行测试。computer-use 当前没有 SDL 窗口；尝试用 `sky.launch_app` 启动 build31 时返回 `Computer Use app approval timed out`，SDL 真人交互、原版逐帧对照与完整平台矩阵仍未完成。
- **移动定时脉冲复核：** IDA `0x4020FA` 以 20 ms 周期启动 multimedia timer；回调 `0x401F98` 按配置表 `{6,4,2}` 累计后置位布尔标志，WinMain `0x401D41` 消费时只调用一次 `0x40D7C4`，因此名义移动间隔为 120/80/40 ms，积压脉冲合并。SDL 使用相同 20 ms 常量和速度表；MOVE 首次更新等待一个配置间隔，漏掉的多个间隔只推进一次。定向 movement 单测覆盖首次脉冲延迟和积压脉冲合并并通过。原版 timer 全局相位与 SDL transition anchor 的相位差、消息循环抖动和同状态画面对照仍待运行时验收。
- **本轮隔离验证：** `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review34 BIN_DIR=build/m3-ai-review34/bin test-unit --progress`、`test-replay --progress` 和 `sdl --progress` 均通过；目标 `test_turn_movement_flow` 单测也单独编译运行通过。验证只覆盖 Windows 隔离构建，SDL 真人交互、原版同状态对照及 Linux/sanitizer 矩阵仍未完成。
- **首个移动调用时序复核：** IDA `0x40D7C4` 的 movement 分支在同一更新调用 `0x40C05C`。函数首调用先选目标、初始化时长/帧计数并立即递减帧计数；非末帧累加坐标，单帧边直接写终点并返回。因此“被调用时推进首帧”成立，但调用本身应在进入 MOVE 后的下一个原版 timer 脉冲发生。此前 build30 已有调用内首帧更新，但从骰子结束的下一 SDL 主循环帧就可能提前调用。本轮改用 `turn_state_tick` 加一个配置间隔起算，并将积压脉冲合并为一次更新。timer 全局相位、消息循环延迟、世界坐标/相机和动画帧仍待运行态对照。

- **2026-09-25 AI 表资格与路线道具增量：** 隔离命令 `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review16 BIN_DIR=bin/m3-ai-review16 sdl --progress` 编译通过；本轮未运行测试。已修正 `0x40B343` 路径起点，接入路障 `0x42107F` direct/global-score 分支及地图土地名称组投影，并按 `0x40829D` 的 `+50..+53`、`+21 bit 0x20` gate 更新普通 actor 队列资格；另经 IDA 对上普通角色静止/移动/动作、特殊道路、梦游和冻结资源槽号；build17 对齐原版网格表/子格变换/原点及 `(x,y-40)` raw 行；build18 对齐 `0x40C05C` 的 EA1/`+21 & 0x30` 固定速度分支，隔离 SDL 构建通过；原版 n2_5 门控、EA1/EA2 动态帧状态与指针有效性、角色坐标及移动插值逐状态等价仍未闭合，详见 [m3-10-basic-ai.md](evidence/m3-10-basic-ai.md)。
- **2026-09-25 AI 移动速度目标节点复核：** `0x40C05C` 先把选中的目的节点写入 `word_496B74`，再调用 `0x40B93B` 选资源车道；因此特殊高位路线的 EA1 速度判断基于本步目的节点。SDL 已改为按 `player->next_node` 判断高位路线，其他 `play_state` 与 `[8,12,16,8]` 条件不变。`0x40C05C` 还确认原版用浮点增量累加并逐帧截断坐标；SDL 当前中间帧采用整数比例，逐帧位置/调用时序仍未对齐。隔离 SDL 目标 build19 结果见下方专项记录；本轮未运行测试。桌面清单仅有 `apps=[]`，未能启动或绑定 SDL 窗口，真人交互与原版同状态画面对照仍待外部可用窗口后验收。
- **2026-09-25 移动中的普通角色资源车道：** 同一条 `0x40B93B` 资源选择也读取移动目的节点。SDL view model 已在移动期间按 `player->next_node` 决定特殊道路资源，在静止期间按已提交的当前节点决定；跨入/离开高位路线时使用目的节点对应资源部件。build21 SDL 编译通过；未运行测试。
- **2026-09-25 普通角色 overlay raw 来源：** IDA `0x40889A` 将普通 actor overlay 作为绘制队列项加入，但 `word_48A850` 原始对象 ID 为 0；`0x409DE7` 跳过零 ID。SDL AI 投影现过滤独立 overlay 队列项，显示路径不变。隔离 SDL build22 编译结果见专项证据；未运行测试。
- **2026-09-25 普通 actor raw 资源指针门槛：** `0x40829D` 从 EA1/EA2 槽表选出角色资源指针，`0x409DE7` 只投影指针非空的 raw ID。SDL 角色资源加载器现分别记录 Data.mkf 资源是否存在和 SDL 是否能解析 SPR/SMP；AI 投影按当前角色主体槽的原资源存在性过滤，避免将 SDL 占位绘制误当成原版可见 raw 对象。隔离 SDL build23 编译通过；本轮未运行测试。

- **2026-09-25 特殊 actor overlay raw 来源：** IDA `0x408ACB` 固定/特殊角色的独立 overlay 队列项将 `word_48A850` 写为 0，与普通 actor overlay 一样被 `0x409DE7` 排除。SDL AI raw 投影现同时过滤两种 overlay 项，绘制队列不变。隔离 SDL build24 编译结果见专项证据；未运行测试。
- **2026-09-26 普通 actor 中间帧坐标：** IDA `0x40C05C` 显示原版以距离和选定速度形成单精度时长；`dx/duration`、`dy/duration` 转为 float 增量，写入 float 累加器后每次移动调用加一次，并在投影到角色坐标时截断；到终点时再精确落点。SDL 移动步现复现速度分支、单精度时长、float 增量和逐次累加/转整数，中间帧按浮点增量生成。隔离命令 `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review25 BIN_DIR=bin/m3-ai-review25 sdl --progress` 编译通过；未运行测试。普通 EA1=0/EA2=1 的方向量化和逐次帧游标/分帧公式已静态对齐；原版 timer 全局相位与 SDL `turn_state_tick` 锚点的偏差、消息循环抖动、角色世界坐标/相机状态、EA1/EA2 动态资源/帧状态及同状态动态画面对照仍待运行验收。
- **2026-09-26 特殊 actor/动态对象资源指针门槛：** Data.mkf `380..395` 特殊角色和 `396..415` 动态状态资源现分别记录原始资源提取是否得到非空指针，与 SPR/SMP 解码成功标志分开；AI raw 投影按当前特殊角色部件/动态 type 的原始资源存在性筛选，绘制仍使用解码标志。隔离命令 `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review26 BIN_DIR=bin/m3-ai-review26 sdl --progress` 编译通过；本轮未运行测试。EA1/EA2 动态槽选择及其他特殊对象资格还待逐项核对。
- **2026-09-26 静态 map object 资源指针门槛：** IDA `0x409B18` 对进入原始绘制/表项队列的对象要求 `dword_48A84C[3 * slot] != 0`。SDL Map.mkf 直接对象资源和地图 overlay/view/stage-tile 别名现分别记录 raw archive pointer 是否存在；AI visible 投影按该标记筛选 land/facility 与静态对象，owner marker 按 common resource 指针筛选。实际场景绘制仍依据可解码标志。隔离命令 `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review27 BIN_DIR=bin/m3-ai-review27 sdl --progress` 编译通过；本轮未运行测试。其他特殊显示类别仍待逐项核对。
- **2026-09-26 普通 actor 资源槽条件复核：** IDA `0x40B93B` 逐分支确认选择条件：`+21 == 0` 或 `+21 & 0x40` 取冻结 `+18`；`byte_496B9F` 非零取睡行 `+16/+17` 和动作 `+2`；目的节点 route flag 高位取特殊道路 `+13/+14/+15`；其余按 `+22 & 3`（交通方式）取 `+3*m + {0,1,2}`，方法 3 另加载 `+3` overlay。`+50..+53` 状态阻断只影响 `0x40829D` 队列资格，不由 `0x40B93B` 改选冻结槽。`rich4_game_actor_part` 已更正为整字节/bit 0x40 分支，移除未由原版槽选择函数支持的住院、消失、睡眠状态强制冻结。隔离命令 `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review28 BIN_DIR=bin/m3-ai-review28 sdl --progress` 编译通过；本轮未运行测试。逐帧 render state 更新仍待对照。

- **此前增量验证（2026-09-25）：** 卡片 12 地产/设施 fallback 与动态 slot 统一扫描后，隔离命令 `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review12 BIN_DIR=bin/m3-ai-review12 sdl --progress` 编译通过；本轮未运行测试。最新 computer-use 清单仍为 `apps=[]`；当前运行时未提供 `cua.computer.launch_app` 或 `cua.listWindows`，支持的 UI 通道无法启动/绑定已构建的 SDL 程序，真人 SDL 交互与同状态原版画面对照仍未完成。以下既有隔离验证仍作历史记录：增量目录 `build/` 与 `bin/` 中曾因头文件依赖漏重编而给出误导性失败；独立目录 `build/m3-ai-review2`、`bin/m3-ai-review2` 的 `test-unit`、`test-replay` 和 `sdl` 全部通过，`test_ai` 输出 `original three-profile AI command selector: ok`。当时 dummy SDL 菜单 2 帧及 gmap0 游戏 120 帧的双运行摘要分别命中 menu `21f26f63/f8719841`、game `03ce8e1f/021f4318`，`test-save-load-replay` 也通过（seed=161）。这些旧结果不替代尚未完成的真人交互、原版同状态画面对照及 Windows/Linux/sanitizer 最终矩阵。

本轮 MCP 核对 `0x409DE7` 动态池资格筛选/按坐标 OR、`0x40A45C(-1)` 全屏行优先压平、`0x409EF9` route 静态对象投影、`0x40A0B1` 玩家/资产局部投影，并按 16 个 xref 检查消费函数。`0x444D1A` 的动态神明类型集合与附身过滤已闭合；本次指令复核将距离界更正为 sqrt(dx²+dy²) < 10000.0（此前误按平方距离 10000 解读）。SDL AI 表可用时又按坐标同格 OR 和行优先扫描复刻等距 tie-break，真人卡片 panel 现共享该 raw 投影；仅缺表 typed 回退仍按槽序。机器工人 `0x421BA6` 现在读取 SDL raw 表并沿用行序/同格 OR，将 land/facility ID 映射为 typed 目标。卡片 9/10 的 `0x41F037`/`0x41F1B3` 也改为消费同一 raw 行序中的普通地产候选，保留原表同像素 OR 后的名称分组顺序。普通玩家在场掩码及卡片 `0x41FE6F` 当前按 raw 像素行优先顺序构造去重候选；本轮复核 `0x40829D` 的主体/overlay 共点规则后，已在 SDL AI 专用收集器排除无主未建地产/设施的合成命中遮罩、缺失 `DATA.MKF[396..415]` 资源的动态项和 attached-owner 动态项，分别对应原版空对象指针与动态池资格筛选。卡片 `0x41FACC` 仅按原表建立可见集合，随后按玩家编号选择目标。完整反编译确认这些 handler 不接受特殊人物位；相关特殊目标扩展已从 core 移除。卡片 2/26 的回退筛选也经反编译校正为保留最高编号的匹配者。另复核完整 `0x40829D` 来源序列，未发现独立工具机器人记录；`0x8100` 属于可能的动态池槽 0 编码，已移除 SDL AI 专用合成的机器人项并保留真实渲染路径。当前 SDL 投影因此接入这些消费分支，但普通/特殊 actor 资源选择、其他特殊对象分类及其他消费分支的 raw→domain/候选序仍属于缺口 3；`map_info_build_visible` 尚未证明等价于原版通用表。

此前隔离构建覆盖 Windows `test-unit`、replay 和 SDL smoke；AI hit-only mask、动态资源/附身资格过滤、移除未证实的独立工具机器人来源、神明 row-major tie-break 和 `<10000` 欧氏距离界修正后，隔离目录 `build/m3-ai-review6` 的 SDL 目标构建通过。本轮将 raw 表接入真人卡片 23 请神符、卡片 4/9/10/11/27 的全局地产候选以及卡片 12 的动态目标后续分支；综合隔离目录 `build/m3-ai-review11`、`bin/m3-ai-review11` 的 SDL 目标构建通过，本轮未运行测试。卡片 14 经反编译确认仍按玩家编号选择。未运行 Linux/ASan/UBSan 全矩阵，不把平台矩阵记为完成。computer-use 的窗口列表未暴露 Rich4 SDL 进程；虽启动后的进程报告窗口响应，UI 自动化未返回目标窗口对象，因此没有进行 SDL 人工模态交互或原版同状态截图对照。

专项入口：[协议](evidence/m3-0-gameplay-protocol.md)、[执行器](evidence/m3-1-route-event-executor.md)、[账本](evidence/m3-2-ledger-bankruptcy.md)、[地产](evidence/m3-3-ordinary-land.md)、[设施](evidence/m3-4-facility-company.md)、[卡片](evidence/m3-5-card-system.md)、[道具](evidence/m3-6-tool-system.md)、[状态](evidence/m3-7-god-confinement-status.md)、[特殊事件](evidence/m3-8-special-events.md)、[股票](evidence/m3-9-stock-system.md)、[AI](evidence/m3-10-basic-ai.md)、[replay](evidence/m3-12-replay.md)。历史文档中的完成结论若与上表冲突，以本次代码/MCP 核对为准。

建议顺序：先推进第 3 项 AI 全对象来源和候选策略；随后完成第 8 项 SDL 真人交互及同状态原版对照。规则变化仍须遵守 [AGENTS.md](../AGENTS.md) 的证据、统一命令/事件和正常/拒绝/边界要求。原版 `SAVE*.DAT` 兼容导入是单独能力，不作为当前自有格式的隐含承诺。


**2026-09-26 原版 `n2_5` 全局转场派发与 SDL 接线边界：** IDA 将该 1-byte 全局定位在 `0x46CAF8`。WinMain (`0x401B9C`) 在 message-idle loop 的 `0x401DB8` 检查它，且仅在 `byte_46CB01` 置位时处理；非零时先调用 `0x419228`、`0x4019D2` 并清零闩锁，再按 1–4 派发：1 回到外层初始化循环；2 设置选择码 0 并调用 `0x4075C1` 播放 `JUMP.MKF` 转场（退出标志可进一步关机）；3 调用同一转场后重启外层循环；4 调用 `0x403D74(0)`，成功后以动作码 1 重载。主要写入点为 `0x40CD87`（破产/玩家转场，可采用 `0x407842` 返回码或设 1/2/3）、`0x411AA3`（读取 DOS 日期后设 1）和 `0x41D89E`（依结果设 1/2/3）；`0x407842` 的结束界面返回码 4 会进入旧槽位读取。其余赋值来自对话/转场流程。普通 turn/action 门控点包括 `0x40DEFE`、`0x40EAD7`、`0x418C55`、`0x41982D`、`0x41B42D`、`0x41C84F`、`0x449C7C`、`0x449DE6`、`0x44A029`。SDL 已有 `GameState.game_over`/ending presentation 和独立存读档面板，但没有将这些 producer、同一轮抑制门槛、JUMP/外层场景重派发及资源清理连接起来的 typed lifecycle request；这仍是缺口 3，不能简化成 bool，也不能用结局画面或演出优先级代替。

**2026-09-26 EA2=3 触发条件复核：** `0x448A7E` 的两个调用点仍为状态返回 `0x418EBD` 和地产/设施落点处理 `0x41982D`。helper 首先要求玩家活动字节非零、`(byte_496B79[player] & 0x83) == 3`、梦游字节 `byte_496B9F[player] == 0` 且挂载槽 `LOBYTE(dword_496B9A[player]) == 0`；目的路线记录 raw ID 必须落在普通地产 `2001..3999` 或设施 `4001..5999`，目标 owner 不得为当前玩家，目标建成/活动字节须非零。成功后设置普通 actor 的 EA2=3、EA3=0 并置动画刷新闩锁。`0x40D7C4` 的 EA2=3 分支在 EA3 达到 3 时还会调用 `0x40AB4A(raw_id, 2)`：对仍建成的地产/设施清零 level（原记录 +26）和类型字段（+24），保留 owner，并刷新地图对象；因此这不只是一个高亮动画。高亮资源调用使用 `dword_48A0E4` 的 chunk 526，即 `Data.mkf[526]`；SDL 资源加载器已为点数小游戏读取同一资源，但还没有这个 actor 效果的触发/帧/方向状态及结尾地产状态变化。玩家配置 byte 低两位为 3 可由 `is_ai && ai_personality == 2` 表达，因为新局把 `selected_ai=3` 映射为 `ai_personality=2`；但 `0x406DE7` 将该 byte 从 `lpBuffer__38` 写入的直接数据流仍需确认，不能仅凭另一 handler 的 `& 3 != 0` 门槛将其视为已完成映射。收费事件本身不足以作为触发条件；EA2=3 仍列为缺口 3。

**2026-09-26 SDL 真人验收状态复核：** computer-use 当前窗口清单返回 `apps=[]` 且没有浏览器标签页，因此没有可操作的 Rich4 SDL 或原版窗口。本轮没有进行真人交互、同状态像素/时序对照，也没有将验收缺口 8 标为完成。

**2026-09-26 动态神明 typed fallback 活动槽边界：** 单元验收发现池初始化时未活动的 road-god 槽仍保留 type 标签、node 为 0；pure app typed fallback 把它们与 route node 0 坐标一起计为零距离候选。IDA `0x444D1A` 只扫描 `0x40A45C(-1)` 返回的可见 raw ID，`0x409DE7` 只将满足渲染队列资格的活动项写入该表；typed fallback 因而跳过 `node==0`。修正后，隔离构建 `build/m3-ai-review34` 的完整 `test-unit`、SDL 目标和 `test-replay` 均通过。

**2026-09-26 AI 原版候选与回归验收增量：** IDA `0x421827` 的无主地产分支在同名自有记录数大于 1 且 `2.5*地产原始价格<现金` 时立即选择；自有普通地产使用原始建设价格与当前等级筛选。2026-10-08 已同步 core 和回归用例。全量单元测试还暴露旧断言与原版不符：卡片 12 不处理动态 type 18，卡片 16/17 与停留卡仅接受玩家位掩码，骰子弹窗已活动时资源 lane 转为 EA2=1，`0x40B93B` lane 18 由完整 `+21` 字节/bit 0x40 决定而非住院、消失或普通睡眠状态；相应夹具已按 IDA 分支修正。隔离目录 `build/m3-ai-review34` 的 `test-unit`、`test-replay`、SDL 构建全部通过。AI 缺口 3 仍包括 `n2_5` 派发/清理接线、EA1/EA2 动态状态、逐状态移动及候选语义等价；验收缺口 8 仍缺真人 SDL 交互、原版同状态像素/时序比较和 Windows/Linux/消毒器矩阵，故 M3 尚未完成。

**2026-09-26 普通 actor 方向与行走帧公式增量：** IDA `0x407A8C` 将路线节点坐标差交给 `0x454FB4`；其指令先对 Y 取反，再由 `0x454FE1` 按 `(min(abs(dx),abs(dy)) << 11) / max(abs(dx),abs(dy))` 查 `word_48242C`，`0x454FB4` 再按角度高位和 `byte_482414` 映射到八方向。查表实值在索引 848/849/850 分别为 1023/1024/1025；因此 ratio 近似 `848/2048` 的比较不能代替原版整数查表，索引 849 的精确边界还按角度四象限取舍。SDL 已据此修正 `rich4_game_actor_direction_from_delta`，并对 2049 个表索引的两种主轴和四种符号组合（16,392 个向量样例）与原版查表结果逐项核对，未发现差异；原有方向断言中的索引 848 案例已更正为轴向。帧更新方面，`0x40C05C` 每次移动更新将 `byte_498EA3` 加一，并在到达所选资源 `num_chunks >> 3` 时归零；SDL 每次 `update_player_step` 将 `walk_frame` 加一，地图角色绘制以 `walk_frame % (num_chunks / 8)` 取方向分组内帧，`visual_direction` 对应原版 `((8 - mapRotation + playerDirection) & 7)`。本结论限普通 EA1=0/EA2=1 资源 lane 的方向及每次更新帧序列；资源切换/重置相位和运行时画面仍按上文保留验收项。

**本轮编译记录：** 隔离 SDL 编译 `build/m3-ai-review36`（含普通 actor 方向边界与卡片 30 全局 level-5 charge 修正）通过；本轮未运行 unit/replay 测试。

**2026-09-26 卡片 30 全局设施等级边界纠正：** 完整反编译 `0x420970` 的全局候选分支确认：玩家自有、type 非 0/4 的设施只要求 level 非零，在 `0x420DC6` 读取记录 `+36 + 2*level` 并计数，没有 `level < 5` 限制；地图 typed 记录也明确提供 level 0..5 六个 charge 槽。`ai_card30_candidate_gate` 已改为接受 level 1..5，并更新了原有 level-5 回归断言；直线路径的独立 `level < 5` 条件不变。该分支修正和普通方向修正均由隔离 SDL `build/m3-ai-review36` 编译通过，本轮未运行测试；卡片 30 的特殊 raw/完整可见候选仍属于缺口 3。

**2026-09-26 原版 AI 路线窗口边界校正：** `0x40B221` 在无合法前进边时输出前一节点，并用该节点继续更新后续窗口；同一相邻节点若出现在多个方向槽中也保留重复项参与分岔判断和 RNG 抽取。卡片 30（三格）、机器工人道具（四格）和遥控骰子（六格）现复用共同路线窗口实现，修正了终点回退、重复边权重及分岔拒绝前应消耗的 RNG。隔离 SDL 构建 `build/m3-ai-review40` 通过；本轮未运行 unit/replay 测试。专项证据见 [AI 专项](evidence/m3-10-basic-ai.md)。

**2026-09-26 卡片 30 全局分支汇合纠正：** `0x420970` 在 `0x420986` 的路线分岔和 `0x420C3A` 的直线门槛失败都会跳入 `LABEL_17` 普通玩家全局候选扫描；只有直线门槛通过才在 `0x420C50` 以当前玩家为目标返回。SDL 已移除视口自有地产推导自身目标的 fallback，并让直线拒绝也执行全局扫描；无法重建全局候选时保持 deferred。对应既有断言已改为反映原版分支，本轮未运行测试。隔离 SDL 构建 `build/m3-ai-review41` 通过；AI 特殊 raw/全对象表分支仍属缺口 3。

**2026-09-26 原版 `n2_5` 菜单/局重建路径补充：** 对照 `WinMain 0x401B9C`、主菜单 `0x4029FD`、新局初始化 `0x406DE7` 和旧存档读入 `0x403D74 → 0x402AC5` 后，转场码的目的地可再细分：1 清理当前地图运行资源并回主菜单外层；2 播放 `JUMP.MKF` 后以 `0x406DE7(1)` 沿用现有参赛角色/设置重建本局；3 播放 `JUMP.MKF` 后重返主菜单外层；4 打开旧版 `SAVE0.DAT..SAVE5.DAT` 选择器，选中后回外层按加载路径重建地图状态。共用前置清理为 `0x419228`（释放地图相关资源、停 multimedia timer、清初始化标志）及 `0x4019D2`（减少事件/回调编号并恢复绘制模式），之后才清零 `n2_5` 并派发。SDL 已把结束界面空闲 10 秒的码 1 映射为 typed `RICH4_SCENE_COMMAND_ENDING_TIMEOUT` 并返回菜单，也把确认后的码 4 接到自有 R4S 读取面板；目前没有覆盖所有 producer 与清理动作的全局 `n2_5` lifecycle。码 2/3 的 JUMP 转场及设置保留/重建仍未实现；R4S 格式也不能承接旧版 `SAVE*.DAT` 二进制读入。缺口 3 仍需覆盖原码语义、当前新局设置、清理/重建顺序和格式边界；不能把 2/3 合并成普通场景切换，也不能将 4 直接指向未兼容的原版文件。

**2026-09-26 SDL 窗口通道复查：** 当前 computer-use surface 仍返回 `apps=[]`、浏览器标签为空；本轮没有可操作 SDL/原版窗口。缺口 8 的真人交互、同状态像素/时序对照和完整平台矩阵继续未验收。

**2026-09-26 结束画面转场与读档入口增量：** IDA `0x407842 → 0x4018E7 → 0x406B14` 显示结束界面收到鼠标释放或配置确认键后返回码 4；10 秒 timer 到期返回码 1。SDL [input_flow.c](../src/app/input_flow.c) 在 `ending.active` 时允许确认输入；[game_loop_confirm_modals.c](../src/platform/sdl1/game_loop_confirm_modals.c) 将确认接到 [game_loop_save_panel.c](../src/platform/sdl1/game_loop_save_panel.c) 的 `game_save_load_panel_open_after_ending`，为本地 human player 打开现有自有格式 R4S 读取面板。[game_preview_frame.c](../src/platform/sdl1/game_preview_frame.c) 在 ending-only 渲染分支将面板绘制在结束演出上层。取消时把为读档命令授权而暂选的本地玩家恢复为结束前当前玩家；失败读档保留可重试/可取消的终态面板，成功读档由载入 session 替换旧终态。读档面板打开时暂停结束页 idle timer；未确认时 10 秒后发出 typed `RICH4_SCENE_COMMAND_ENDING_TIMEOUT`，scene machine 回到菜单。前一玩家标记是 runtime-only，生成 codec 跳过整个 `save_load` presentation。隔离 SDL 命令 `python scripts/make_minimal.py BUILD_DIR=build/m3-ai-review43 BIN_DIR=build/m3-ai-review43/bin sdl --progress` 编译通过；本轮未运行测试。仍未实现共用资源清理、码 2/3 JUMP 与设置重建、旧版 DAT 导入和完整全局转场 request，缺口 3/8 仍开放。

**2026-09-26 EA2=3 SDL 映射边界：** `0x448A7E` 的 exact predicate 与两个 callers 已逐条复核。此处旧版将其映射为 `is_ai && ai_personality == 2` 的解释有误：2026-10-05 经 `0x4479D2` 确认为 +17 工程车交通字段；此外 SDL 没有 EA3 的更新协议和效果结束时经 core 更新地产/设施 level/type 的 typed 动作。本轮未按所有收费或普通落点触发近似效果；后续实现需先把触发接到两个原版调用语义，并实现方向保存/恢复、资源 Data.mkf[526] 的帧节奏、高亮和末帧状态清理。缺口 3/8 继续开放。

**2026-09-26 后续核对与验证：** IDA `0x406DE7` 的新局初始化在 `0x407219` 读取 `lpBuffer__38`，于 `0x40721f` 写入每名玩家的 `byte_496B79[104*n9]`；返回重建入口在 `0x406f6b` 从保存的 `lpBuffer__15` 恢复该配置。由此关闭上一段所述配置字节直接写入来源的不确定性，但 `0x448A7E` 的 EA2=3 触发、帧状态及末帧地产效果仍未接入。结束画面打开 R4S 读取面板时，临时选择本地玩家和取消时恢复当前玩家的状态写入已从 SDL 适配器移到 app `game_session`，解决 `platform-state-write` 架构检查失败；隔离目录 `build/m3-followup` 的 `test-architecture` 通过。全量 `test-unit`、`test-replay`、SDL 构建仍以本轮最终运行结果为准。M3 仍有实现缺口 3 和验收缺口 8，不能标为完成。

**2026-09-26 本轮最终验证与实机边界：** 隔离目录 `build/m3-followup` 的 `test-unit test-replay sdl` 全部通过；`test_ai.c` 的未使用局部变量警告随后清除，定向 `test_ai` 重新编译运行通过。原 `test-sdl-smoke` 脚本固定运行旧 `bin/rich4_sdl.exe`，与隔离 `BIN_DIR` 不一致，导致旧二进制的游戏序列摘要误报；Makefile 现把目标 `BIN_DIR` 传给脚本，复跑 `test-sdl-smoke` 通过。直接运行新二进制所得菜单摘要 `21f26f63/f8719841`、游戏摘要 `03ce8e1f/021f4318`，均命中基线。Windows UI 通道本次可列出原版 Steam `Rich4` 窗口；新 SDL 构建从项目目录启动后进程响应且有窗口句柄，但该窗口仍未出现在 UI 通道的可操作窗口列表中，已结束测试进程。因此未做完整真人模态操作或同状态逐帧比对。M3 的实现缺口 3 与验收缺口 8 均保持开放。

**2026-09-26 Computer Use 重试：** 清理了无引用的 `.tmp_flc45`、`.tmp_flc559` 空目录和 `.tmp_resource_scan` 中 278 个提取资源临时文件。通过放在隔离构建目录的临时启动器由 Computer Use 会话启动 SDL 后，`list_windows` 首次返回了标题为 `rich4-sdl M1 input preview` 的目标窗口；`get_window_state` 可读取标题栏无障碍树，但两次截图均失败，错误为 `SetIsBorderRequired failed: 不支持此接口 (0x80004002)`。同一截图错误也出现在另一个已列出的 Steam `Rich4` 窗口，表明当前截图通道本身不可用。游戏画布没有可用的无障碍控件，无法据此确认交互坐标或画面结果，因此未将第 8 项标为验收通过。测试进程及临时启动器已清理；隔离构建产物保留用于后续验证。

**2026-09-26 SDL 画面与存读档实机复验：** 上段截图通道限制已由仅在设置 `RICH4_UI_QA_BMP` 且按 F12 时生效的 SDL 合成帧 BMP 导出绕开；Computer Use 继续负责真实窗口点击、按键，BMP 用于观察结果，不需为游戏画布增加无障碍控件。已从菜单选择角色进入真实地图，打开保存/读取面板并完成槽 1 写入和读回。`save_session_valid` 原把地图路线节点数 104 与事件容量 65 混用，现改为地图路线容量 256，且增加 104 节点的往返回归。先前存档 `fopen` 报 `errno=24` 的根因是 `trace_game_start` 在预算耗尽后仍先打开日志再返回，持续泄漏 `FILE*`；调整检查顺序后，存读档恢复普通 `fopen`，实机进程句柄约 400 且不再增长。进程打开文件列表显示各 MKF 仅一份。对照原版 `0x403396/0x403D74/0x404165`，保存背景改用 Data.MKF[520] 的第二块，读取背景用第一块，空槽留空，已有槽绘制日期、地图与角色图标，去掉自造英文 SLOT/DAY/PLAYERS；两种面板的绘制位置按原始坐标修正。原版 DirectDraw 窗口也遇到同一系统截图接口错误，尚无同状态像素对照；跨地图读取及其他模态仍归缺口 8。

**2026-09-26 本次收尾验证：** 全新隔离目录 `build/m3-followup-final` 的 `test-unit test-replay sdl test-sdl-smoke` 全部通过。早先复用 `build/m3-followup` 时 `test_tool_animation_flow` 的断言失败来自头文件变化后未重编的旧对象，全新构建通过。保存面板元数据写入已收进 app 层，架构门槛通过。真人验证用的启动器、BMP、日志和槽 1 测试存档已清理；实现缺口 3 与验收缺口 8 仍开放，M3 尚未完成。

**2026-09-26 原版存读档视觉与时序再核：** `0x403396/0x403D74` 分别从 Data.MKF[520] 绘制保存/读取背景；有效槽位绘制同资源的槽格块（原指针 `+132`，画于 x=129、每行高 72），地图块索引为 `2+4*stage+map`（x=209），人物图标取 Data.MKF[2] 中角色记录 `byte_48A19B[104*i]` 对应块（x=289、361、433、505）。这些小图的静态框线由图块像素提供；悬停时 `0x4039C2/0x40363A` 另以 `0x45620F` 在整行绘制 452×76 和内缩的 450×74 两层黄色矩形，移出后恢复该行底图。存档头前四个日期字节以低字节日、次字节月、高 16 位年布局；年份经 `0x457D61` 十进制转换，月份/日以 `%d/%d` 绘制，无补零；文字锚点 x=165，年和月/日分别位于槽格顶部 +36、+57。只有读取槽 0 会显示 `AUTO`。保存面板在 `0x404165` 先呈现底图、`0x403396` 顺次扫描 1–5 槽并逐有效槽提交画面；点下/双击已悬停行后 `0x4039C2` 同步调用 `0x402FD1(slot)` 写盘、重新扫描并显示该槽，然后向模态消息循环发回选槽码，`0x404165` 在关闭前调用 `0x45285E(500)`，至少保留 500 ms。读取路径 `0x402AC5` 先显示 Data.MKF[601] 载入画面、释放地图运行资源、读回大块全局/玩家/地图状态并修复指针，再以 `0x407AD2` 重取所选 stage/map 的 MAP.MKF 资源和角色/地图渲染资源；`timeGetTime` 自载入画面之后计时，处理不足 1000 ms 时忙等至 1 秒，不重启进程或重新打开全局 Data.MKF。自动存档由 `0x418EBD` 的完整玩家轮转回卷标志触发：先执行 `0x41CF67` 日结、再执行 `0x41C84F`，若配置位 `byte_49715C` 为真则调用 `0x402FD1(0)` 写 `SAVE0.DAT`；缺省 `RICH4.CFG` 时 `0x411E8F` 将此位设为 1。当前 SDL 尚未复刻整行悬停双框、逐槽显示和 500 ms/1000 ms 原版时序，这些归验收缺口 8。

**2026-09-26 单击与日期位置修正：** `0x4039C2` 的 `WM_LBUTTONDOWN (0x201)` 与双击消息共用选槽分支，普通单击按下即保存；此前“需要双击”的表述不准确。`0x44FABC` 的文本模式 2 会在横、纵两轴以给定坐标为中心，故原版年份及月/日的 `x=165`、槽格顶部 `+36/+57` 是文字中心而非左上角。SDL 已改为按下即提交、松开只消费事件，刷新槽位后保留 500 ms；日期和 AUTO 改为中心锚点、16 px 的原版明暗配色与粗体阴影，悬停另画整行两层黄色细框且移开清除。全新隔离目录 `build/m3-save-panel-final` 的 `test-unit test-replay sdl test-sdl-smoke` 通过，`test_game_presentation` 的 499/500 ms 边界再次单独运行通过。原版逐槽提交的可见时序和读取时至少 1000 ms 的载入画面仍待实现与实机对照。

**2026-09-26 Computer Use 复验限制：** 本轮尝试从 Computer Use 启动隔离构建的 SDL 窗口时，工具返回 `Computer Use was not approved to use ui_qa_launcher[]`，因此没有取得可操作的窗口或新的画面截图；未把上述单击、日期位置的代码修正记为实机视觉验收。测试用临时启动器已删除，缺口 8 保持开放。

**2026-09-27 用户截图回归修正：** 截图显示保存槽人物图标颜色异常、日期偏右，且主菜单“加载”没有动作。直接提取 V2.01/V3.11 的 Data.MKF[2] 均证实人物图是 `SMP` 真彩色；问题不在调色板。原版 `0x403396` 对人物、地图和槽格块调用 `0x4563F5 → 0x455B3A`，整块复制零色像素，而 SDL 先前把零色当透明，露出底层蓝色槽格；现对这三类块使用不透明复制，面板背景仍沿 `0x456418 → 0x455C52` 的零色透明路径。日期在主文字后端不可用而落到 5×7 位图备用路径时，备用路径先前忽略居中锚点；现按位图实际宽高执行同一锚点计算。主菜单按钮 1 已接入已有读取面板，加载有效 R4S 槽时校验目标地图、恢复会话并切回游戏场景；Esc/取消关闭面板。隔离目录 `build/menu-save-fix` 的 `test-unit test-replay sdl test-sdl-smoke` 通过，零色复制修正后 `sdl test-sdl-smoke` 再次通过；默认 `bin/rich4_sdl.exe` 也已重新构建。仍缺原版逐槽刷新、读取载入画面最短 1000 ms 和同状态真人画面对照，M3 缺口 8 保持开放。


**2026-10-05 EA2=3 工程车拆屋已接入：** 旧记录中将 `byte_496B79` 解释成 AI 配置的结论作废。入口、两个调用位置、六 tick 起手、EA3=3 建筑清空、逐逻辑 tick FLC、朝向恢复及 EA5 等待已实现；part 12 常驻叠加误用已移除。详见 [工程车拆屋证据与验证](evidence/m3-6-tool-system.md#2026-10-05-工程车拆屋-ea23)。本项的定向回归、12 角色真实资源 SDL 测试和 SDL 构建通过；不据此关闭其它 AI 或真人对照验收缺口。
