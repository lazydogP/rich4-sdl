# Rich4-SDL:降低 LLM 上下文成本的改造 /goal

日期:2026-09-01。本文是当前唯一的重构目标文档。

目标:减少理解和修改一个完整功能所需的文件跳转、无语义转发和重复状态表示,不改变可观察行为。

调用:`/goal 按 docs/llm-context-simplification-lean.md 依次完成 S0–S8。`

## 1. 定义

只用下面三条定义判断,不另设标准。术语在本文之外没有别的含义。

- **无语义转发**:函数体只有一条语句,该语句调用另一个函数并原样传递全部参数,不做校验、转换、副作用管理或平台调用。
- **展示入口**:被渲染或覆盖层绘制直接调用,且不写入领域状态的函数。
- **职责归属**:一组状态字段属于一个文件族,当且仅当只有该文件族写入这些字段,且该族对外只有一个入口头文件。

检查器只能识别单条调用、参数同名同序的语法候选,不能自行判断边界价值。平台释放、回调适配和测试后端切换等合理边界须在 `FORWARDER_EXCEPTIONS` 中按“文件、函数、被调用函数”逐项记录理由；未登记的候选一律失败。扫描覆盖 `src/` 中全部 C/H 文件及所有条件编译分支,不把候选总数解释为单个构建的转发数量。

## 2. 事实基线（S0，改造前）

2026-09-01 实测 `src/**/*.c` 与 `src/**/*.h`,含未跟踪源码,不含 `.inc`、测试、工具与生成目录。本节固定改造前口径；当前工作区的实测结果统一记录在第 7 节。

| 项目 | 实测值 | 用途 |
| --- | ---: | --- |
| C/H 文件与总行数 | 535 / 65,024 | 趋势观察,不设终值 |
| 少于 100 行 / 少于 20 行的文件 | 340 / 72 | 筛选合并候选 |
| `typedef struct *Context` | 100 个,其中 20 个字段数 ≤ 2 | 定位纯打包类型 |
| SDL 层不超过 50 行的 C 文件 | 42 个 | 定位无独立语义的包装 |
| `game_loop_*` / `runtime_*` 文件 | 124 / 40 | 改善功能入口定位 |
| `route_event_executor*` 文件 | 15 个 | S7 归并范围 |
| 字段别名宏 | 19 个裸标识符 `#define` | S1 清除目标 |
| `SceneOps` 表 | 5 表 × 5 槽 = 25 指针,9 个不同函数,仅 `handle_command` 各异 | S4 抽象收益上限 |
| `GameEvent` | 136 字节,其中 `payload` 40 字节;默认 `SCALAR` 与顶层字段同内容 | S6 重复度 |

每帧在 `game_preview_frame.c` 构建的展示类型(当前工具链实测 `sizeof`):

| 类型 | 字节 | 是否按值嵌入领域聚合 |
| --- | ---: | --- |
| `Rich4GameEventPresentationSnapshot` | 168,616 | 是:`GameState` + `GamePresentationState` |
| `GameConfinementRenderView` | 80,432 | 是:`GameState`,其余 21 个标量字段仅 88 字节 |
| `GameOverlayRenderView` | 21,368 | 否 |
| `GameSceneView` | 14,152 | 否,含 `GameViewModel` 12,824 |
| `GamePointMinigameRenderView` | 916 | 否 |
| `GameIntroRenderView` | 48 | 否 |

合计约 279 KiB,其中前两项占 87%。`GameIntroRenderView` 证明窄展示类型在本项目已有可行先例。参照:`GameState` 80,344、`GamePresentationState` 88,272、`GameSession` 355,232 字节。

以上为源码层事实。删除按值复制不等于同等幅度的实测内存流量下降,性能结论需相同构建配置的实测,且桌面数据不外推到 handheld。

## 3. 闸门

S0 的交付物。替换 `tests/unit/test_file_line_budget.c` 与 `tests/unit/test_renderer_viewmodel_boundary.c` 的判据。每项一个确定值。

| 闸门 | 判据 | 当前值 | 目标 |
| --- | --- | ---: | --- |
| 未审核转发函数 | 扫描 `src/` 的语法候选,逐项核对边界例外 | 0（9 个已审核边界） | 0 |
| 展示入口类型 | 参数类型不得按值嵌入 `GameState`/`GamePresentationState`/`GameSession`,含传递嵌套 | 0 | 0 |
| 头文件裸标识符 `#define` | 定义体为成员路径的无参宏计数 | 0 | 0 |
| 依赖方向 | `core` 不引用 `app`/`platform`/SDL;`app` 不引用 `platform`/SDL | 已成立 | 保持 |

展示入口检查实际类型闭包,不匹配类型名字符串——改名不通过。行数改为评审提示,取消 600/300 硬顶及 legacy 只降不升策略。除上表四项外不新增指标,不建立评分框架。

转发闸门不设总数预算。新增候选必须删除冗余包装或说明具体边界价值并登记精确例外；失败输出列出文件、行号、函数及被调用函数。删除或改变已登记的转发后必须同步移除过期例外,不能用减少一个边界函数换取新增业务包装。例外不得留空理由,也不按目录整体豁免。`test-context-simplification` 运行检查器回归测试和源码检查,并纳入 `test-architecture`。

## 4. 必须保留

1. `core` 的玩法规则与确定性状态转换、`app` 的交互流程语义,二者不依赖 SDL 类型、头文件或运行时。`data`/`resource`/`view` 的现有隔离同样保留。
2. 资源 borrow/own 关系、释放顺序、缓存上限与失败路径。不通过复制句柄或放宽所有权检查来消除参数。
3. 路线事件顺序、等待/恢复、turn token、拒绝过期输入、队列容量、失败原子性。库存、金钱、结算与随机数调用顺序不因整理而改变。
4. 鼠标按下/抬起、键盘、手柄的既有差异,modal 优先级,语音打断,音乐恢复,同帧先后次序。
5. 真正用于事务回滚或演出前状态的历史快照,不属于删除目标。改为借用只读对象时须证明生命周期覆盖消费全程。
6. 表达业务意图的命令与事件构造器,以及真有多实现的接口(atlas / FreeType)。不用宏或通用参数替换可搜索的语义接口。
7. 不引入 ECS、事件总线、服务定位器、插件框架或代码生成 DSL。不替换 SDL、构建系统或资源格式。
8. 遵守 `AGENTS.md`。行为有疑问先查原版证据,不推测、不补造未知玩法。

## 5. 阶段

每阶段的退出条件即验收项,本文不另设完成清单。每次只改一个职责族,结构搬迁与语义变化分开提交。构建引用随阶段同步更新。

### S0 建立基线与闸门

记录构建与测试现状,区分已有失败、新增失败与环境缺失。固定五条任务链:菜单进入新游戏并返回;商店买卖与取消;银行交互结束后恢复路线;掷骰到移动落地;事件覆盖层绘制。每条记录输入、期望行为、实际入口函数。按第 3 节实现四个闸门,已知的 2 处展示入口违规登记为 S2 债务,禁止新增,不静默关闭检查。

**退出条件:** 基线可复现,五条任务链可定位;四个闸门可运行且能拦截"改名后仍内嵌完整状态"的反例;合理合并不再因行数被拒。

### S1 清除字段别名宏

`src/core/route_event_executor_internal.h` 的 19 个宏改为显式 `executor->state.<field>`。逐调用点区分已显式访问、其他结构成员与局部标识符,禁止全局字符串替换。保留一个名称绑定回归检查:包含该头文件后,同名局部变量与无关结构成员仍正常编译。本阶段不合并 executor 文件、不改等待/恢复状态机。

**退出条件:** 19 个宏移除,绑定检查通过,执行器行为与基线一致。

### S2 收窄展示输入

先处理 `GameConfinementRenderView`:它已有 21 个正确作用域的标量字段,删除内嵌的 `GameState`,改为补齐实际读取的字段。再逐消费者迁移 `Rich4GameEventPresentationSnapshot`,从依赖最少、同帧消费的覆盖层开始。所有消费者使用同一套输入模式,不给每个字段新建 View 类型。地图投影、可见性、HUD、骰子等有计算意义的 ViewModel 保留。

**退出条件:** 展示入口类型闸门为 0 违规;每帧不再复制完整 `GameState`/`GamePresentationState`;历史快照、借用生命周期与既有演出显示正确。

### S3 收敛 runtime 与帧准备

处理 `game_loop_frame_begin/end/present.*`、`runtime_loop_state_bootstrap.*`、`runtime_config_bootstrap.*` 及 `runtime_main.c` 中的 64 处打包。删除只包一个指针或仅服务一次转发的中间 Context;允许 SDL 私有的长期 runtime 聚合。`game_session_reset` 并入 session 生命周期实现,保留明确的 reset 操作与全部调用点。清理"函数已搬到某处"的过期注释,保留原版地址与行为解释。

**退出条件:** 帧准备不再需要四个子 Context 加外壳;不存在 `context->cursor->cursor` 这类无语义层级;启动、失败清理、关闭与帧顺序不变。

### S4 单一场景状态与直接分发

处理 `scene.*`、`scene_frame_flow.*`、`game_loop_scene_update.*`。收敛为一个场景状态字段、一处转换规则、一处平台帧分发。保留 options 返回场景与合法转换判断。移除不承载实际行为的 `SceneView`/revision/计数与 `AppCommand`→`SceneCommand` 重复转换;若存在真实消费者,先迁移消费者。游戏更新阶段规则只在 app 一处决定。

**退出条件:** 当前场景只有一个权威字段;没有两套覆盖相同职责的帧回调;进入、返回、退出与 modal 优先级回归通过。

### S5 以商店为试点收敛 adapter

贯通 `game_loop_confirm_shop.c`、`game_loop_shop_adapter.c`、`app/shop_flow.c`、`core/route_event_executor_shop.c`,同时检查鼠标、取消、更新与绘制入口。删除只改名的包装。仍需即时语音查询、光标消费或音乐恢复的适配保留并测试调用顺序;没有证据可合并的排空时点保留。保持 app 产出音频事件、platform 执行的边界,不为消除局部队列引入通用消息框架。注意 `tests/unit/test_shop_flow.c` 仅 35 行,只覆盖命中与切页,交易、取消、路线恢复需按风险补测。

**退出条件:** 商店任务链无纯改名中转;买卖、不足额、取消、返回路线与音频顺序可验证;`app`/`core` 无 SDL。

### S6 事件单一权威表示

盘点 `game_event.*`、`game_event_payload.*`、`game_event_typed.c`、`game_event_payload_request_accessors.c` 的生产者与消费者,确认是否存在存档、回放或外部 ABI 依赖。保留类型标签与事件队列,采用语义明确的 payload,不同时保留顶层通用字段与同内容镜像。按事件族小步迁移;临时兼容接口在本阶段关闭时删除。保留独立语义断言:事件次序、主体/目标、数量、拒绝原因、容量溢出、失败不部分提交。

**退出条件:** 不再有 `payload_sync` 常态同步与有效性回退两套读取路径;队列与领域行为不变。

### S7 按职责整理 route、接口与构建

按第 1 节的职责归属定义归并 `route_event_executor*`(15 个文件)与 `app/route_*`,明确调度事实、执行等待、展示状态各由谁写入,减少派生字段镜像。不预定目标文件数。同一职责族共享一个对外头文件,内部 helper 尽量 static,不建总头文件。整理 Makefile 的重复对象列表与测试链接规则,复用现有清单方式。

**退出条件:** 五条任务链的入口、状态与测试能按功能找到;无旧转发与字段宏残留;删除或合并的源文件不再参与编译;desktop/handheld 后端选择正确。

### S8 回归与关闭

重做五条任务链的前后对照,对保留的薄接口说明价值。执行第 6 节矩阵,固定随机种子。清除过渡兼容层与失效引用。交付简短的变更、验证、前后指标与剩余风险说明,写入本文而不新建进度文档。

**退出条件:** 四个闸门达标;五条任务链成本对照完成且无未解释的新失败。

## 6. 验证

Windows 构建器 `.local/toolchain/mingw64/bin/mingw32-make.exe`,在项目根执行。使用新的构建输出目录或强制重编译,避免历史产物干扰。

| 范围 | 入口 | 要证明的内容 |
| --- | --- | --- |
| 核心与架构 | `check-core-headers`、`test-architecture` | 核心独立、依赖方向、四个闸门 |
| 规则/流程/资源 | `test` | 单元、资源、characterization、replay 基线 |
| 场景与输入 | `test_scene.c`、`test_scene_input_flow.c`、`test_sdl1_input_adapter.c` | 合法转换、返回场景、输入归一化 |
| 商店与路线 | `test_shop_flow.c`、`test_route_event_executor.c`、`test_route_event_executor_replay.c` | 完整交互、等待/恢复、失败不部分提交 |
| 事件与显示 | `test_game_event_payload.c`、`test-render` | 事件语义不变、展示输入范围正确 |
| 两种 profile | `sdl`、`sdl-handheld` | 从新产物构建,字体与音频后端选择正确 |
| 真实资源 | `test-original-data` | 有数据时验证 MKF/FLC/地图;SKIP 不算通过 |
| 内存与运行 | `test-sanitize`、桌面实际运行 | 受影响资源与借用生命周期 |

handheld profile 在 Windows 编译通过不等于 Miyoo 交叉编译或真机运行通过,二者分开报告。冒烟覆盖:启动/退出与失败清理、场景返回、掷骰到落地、商店与银行退出恢复、鼠标与键盘/手柄优先级、语音打断与音乐恢复、事件演出前后显示。缺少资源、设备或工具链的项如实标记未验证,不以闸门达标替代行为验收。

## 7. 收口记录 (2026-09-01)

本次工作区已完成 S0–S8 的源码级退出条件。S0 是基线与闸门阶段,不是代码债务；S3、S5、S7、S8 已在本轮补齐。玩法规则、路线事件顺序、资源所有权、音频排空顺序与 SDL 后端选择未改变。`route_event_executor*` 仍保留 15 个文件,因为它们各自承载真实执行语义,没有为减少文件数强行合并。

| 阶段 | 状态 | 依据 |
| --- | --- | --- |
| S0 | 达成 | 基线、四项闸门与反例测试可复现；`test_file_line_budget.c` 调用上下文闸门,渲染测试调用展示边界闸门,架构测试包含依赖方向、对象清单与失败/恢复 fixture。 |
| S1 | 达成 | 删除 `route_event_executor_internal.h` 的 19 个字段路径宏,所有执行器调用点改为 `executor->state.<field>`；名称绑定回归置于 `tests/unit/test_file_line_budget.c` 并通过。 |
| S2 | 达成 | `Rich4GameEventPresentationSnapshot` 168,616 → 16 字节(改为同帧只读借用指针)；`GameConfinementRenderView` 80,432 → 120 字节(改为窄字段)；展示入口类型违规 2 → 0。 |
| S3 | 达成 | 帧与预览的一次性 Context 已摊平,`game_session_reset` 已并入 session 生命周期；字面与正则口径的三级指针链 33 → 0；`*Context` 100 → 64,字段数 ≤2 者 20 → 0；过期 runtime 归属注释已清理。 |
| S4 | 达成 | `Rich4SceneView`、revision/计数与 `AppCommand`→`SceneCommand` 重复转换已移除；当前场景只由 `scene_machine.current` 承载；options 返回、场景输入与场景测试通过。 |
| S5 | 达成 | 商店路径删除纯改名包装并直接调用领域接口；生产代码栈上一次性 `Rich4AudioEventQueue` 47 → 0,统一使用 `GameSession.audio_events`；无语义转发 66 → 6,保留的 6 个均有 ABI、回调或关闭边界理由。 |
| S6 | 达成 | `GameEvent` 删除顶层标量/领域字段与 payload 镜像标签,由 tagged payload 单一承载；`payload_sync` 全仓库 0 次出现；结构 136 → 100 字节,队列 4364 → 3212 字节。 |
| S7 | 达成 | route dispatch 的 8 个 current 镜像字段删除并改为按需解析；状态写入职责已登记如下；构建对象清单与 desktop/handheld 选择均同步，15 个 executor 文件保持语义拆分。 |
| S8 | 达成(源码与本机构建范围) | 五条任务链已有前后阅读范围/跳转和中间层成本对照；干净全量测试、desktop/handheld 构建与原始资源扫描通过；sanitizer、显示设备和 Miyoo 真机另列为环境验证。 |

### 五条任务链成本对照

“阅读范围”按从当前平台入口到领域行为终点追踪出的最小实现文件集合统计,排除头文件、测试、通用资源表与完整传递闭包；“跳转”是集合内相邻实现文件之间的跨文件跳转。S0 与当前的文件集合基本不变,这是保留真实模块职责的结果；本次降低的是集合内的无语义中转、一次性聚合和重复状态,不以合并文件制造虚假下降。

| 任务链 | S0 阅读范围(文件/跳转) | 当前阅读范围(文件/跳转) | 已降低的上下文成本 |
| --- | ---: | ---: | --- |
| 菜单→新游戏→返回 | 9 / 8 | 9 / 8 | 帧/设置/过渡的一次性 Context 4 → 0；入口仍为 `game_handle_confirm_input`、`game_handle_menu_input`、`rich4_app_new_game_setup_pointer_up`、`game_handle_cancel_scene_input`。 |
| 商店买卖与取消 | 8 / 7 | 8 / 7 | shop 纯改名包装 3 → 0；一次性音频队列 8 → 0；`rich4_shop_hit`、`rich4_shop_finish` 与路线提交仍保持原有验证入口。 |
| 银行结束→路线恢复 | 8 / 7 | 8 / 7 | route current 对象/属性镜像 8 → 0；银行拒绝、完成、过期输入仍由 `rich4_route_event_executor_finish_bank_rejection` 与 `rich4_route_event_executor_resume_*` 处理。 |
| 掷骰→移动→落地 | 10 / 9 | 10 / 9 | dice 的 4 个纯展示/状态薄包装 → 0；turn movement 的 resume handoff 保持显式；固定序列由 `rich4_app_update_turn_movement`、`rich4_route_event_executor_step` 验证。 |
| 事件覆盖层绘制 | 6 / 5 | 6 / 5 | `Rich4GameEventPresentationSnapshot` 168,616 → 16 字节；同帧借用生命周期明确；绘制仍从 `draw_game_preview_adapter` 进入。 |

上述文件/跳转统计是可复核的 review scope,不是运行时调用次数；不把同文件内的函数调用误报为文件跳转。五条链的输入、预期行为与测试覆盖如下:

| 任务链 | 输入/期望 | 入口与验证 |
| --- | --- | --- |
| 菜单→新游戏→返回 | 确认菜单项,完成设置,取消返回；场景合法且返回目标不变 | `game_handle_confirm_input`、`game_handle_menu_input`、`rich4_app_new_game_setup_pointer_up`、`game_handle_cancel_scene_input`；`test_new_game_setup_flow`、`test_scene_input_flow`、`test_scene` |
| 商店买卖与取消 | 命中商品/分页,余额不足不提交,取消后回到路线；音频顺序不变 | `rich4_shop_hit`、`rich4_shop_finish`、`rich4_route_event_executor_submit`；`test_shop_flow`、`test_route_event_executor` |
| 银行结束→路线恢复 | 银行拒绝或完成后只恢复合法等待态,过期输入不推进 | `rich4_route_event_executor_finish_bank_rejection`、`rich4_route_event_executor_resume_*`；`test_bank_interaction_flow`、`test_route_event_executor_replay` |
| 掷骰→移动→落地 | 固定输入/随机序列产生同样移动、落地和路线事件 | `rich4_game_dispatch_command`、`rich4_app_update_turn_movement`、`rich4_route_event_executor_step`；`test_game_dispatch`、`test_route_movement`、`test_first_turn_replay` |
| 事件覆盖层绘制 | 同帧构建展示输入,绘制读取借用快照,不按值嵌入完整领域状态 | `draw_game_preview_adapter`、`rich4_app_build_game_event_presentation_snapshot`、`draw_*_presentation`；`test_game_presentation`、`test-render`、展示入口闸门 |

### 状态归属与前后指标

route executor 的职责归属按“逻辑写入族”登记,而不是按文件数量登记:

| 状态 | 当前唯一写入职责 | 例外/消费方式 |
| --- | --- | --- |
| `dispatch.phase/index/pass_count/total_count/current_ordinal/next_phase/next_index/handled_count` | `src/app/route_flow.c` 负责路线游标、计数与阶段推进 | lifecycle 只初始化 phase/index/next sentinel；`src/app/route_event_flow.c` 在结束时标记 `DONE`。 |
| `dispatch.resume_movement` | `src/app/turn_movement_flow.c` 设置 handoff | `src/app/route_event_flow.c` 在接管后清除；这是显式的跨职责交接,不是镜像。 |
| current object/property/event 镜像 | 已删除 | `route_flow.c` 按 dispatch index 或 STOP fact 按需解析 `RouteEvent`,不再保存第二份当前事件。 |
| `turn_token`、execution/wait/presentation/magic/pending/lottery/bank protocol、`last_result` | `src/core/route_event_executor*.c` 文件族 | app 只读取需要的结果或状态；队列只负责存储,不成为第二状态机。 |

前后指标均使用同一检查器与同一构建配置；S0 列固定为改造前 HEAD(`08d2661`)口径:

| 指标 | S0 | 当前 |
| --- | ---: | ---: |
| `src/` 生产 `.c/.h` 文件 | 535 | 534 |
| 生产源码总行数 | 65,024 | 63,997 |
| 少于 100 行 / 少于 20 行文件 | 340 / 72 | 341 / 71 |
| SDL 层不超过 50 行的 C 文件 | 42 | 43 |
| 无语义转发(检查器口径) | 66 | 6 |
| 成员路径别名宏 | 19 | 0 |
| 展示入口类型违规 | 2 | 0 |
| 依赖方向债务 | 0 | 0 |
| `Rich4GameEventPresentationSnapshot` | 168,616 字节 | 16 字节 |
| `GameConfinementRenderView` | 80,432 字节 | 120 字节 |
| `GameEvent` / `GameEventQueue` | 136 / 4364 字节 | 100 / 3212 字节 |
| 生产代码栈上一次性 `Rich4AudioEventQueue` | 47 | 0 |
| `*Context` typedef | 100 | 64 |
| `*Context` 字段数 ≤2 | 20 | 0 |
| `context->a->b->` 三级链 | 33 | 0 |
| route current 镜像字段 | 8 | 0 |
| `game_loop_*` / `runtime_*` 文件 | 124 / 40 | 124 / 40 |
| `route_event_executor*` 文件 | 15 | 15 |

转发闸门当前输出为 `9 reviewed boundary forwarders; 0 unreviewed forwarders; 0 member-path aliases`。上表的 66 → 6 是 S5/S7 重构时的语法候选快照；当前验收使用第 3 节的逐项边界例外,不再使用总数预算。

### 验证结果

- 干净 `mingw32-make test` 通过,包括单元、流程、场景、商店、路线、事件、资源、characterization、replay 与样例 MKF 检查；新增 `stage-sdl-runtime` 构建依赖自动放置 Windows SDL/工具链 DLL,因此 SDL 运行测试不依赖历史产物。
- `mingw32-make test-architecture`、`check-core-headers`、`test-resource`、`test-replay`、`test-render` 通过；架构、SDL 对象清单、guard fixture、上下文闸门、展示边界闸门均通过。当前清单为 156 个 SDL 源文件、156 条唯一源规则、165 个可达对象。
- `mingw32-make sdl` 与 `mingw32-make sdl-handheld` 均通过,分别生成 `bin/rich4_sdl` 和 `bin/handheld/rich4_sdl`；`mingw32-make test-original-data` 通过,实际扫描 V3.11 Data/Jump/Panel/Map。
- `git diff --check` 无空白错误,仅报告工作区 LF/CRLF 转换提示。
- `test-sanitize` 未能在当前 TDM-GCC 环境执行:链接器缺少 `-lasan` 与 `-lubsan`;这不是源码测试失败,需要带 sanitizer runtime 的工具链补跑。
- 未执行带显示设备的桌面交互冒烟,也未执行 Miyoo 真机/交叉工具链验证；handheld Windows 构建不替代真机验证。

## 8. 收口证据、保留薄接口与剩余风险

本节替代原来的“未达成项”清单,记录已经落地的文件族与有意保留的边界:

| 改造族 | 已落地内容 | 复核入口 |
| --- | --- | --- |
| S3 runtime/帧准备 | 直接字段与直接参数取代一次性子 Context；预览帧改为一个直接聚合；session reset 并入 `game_session.c`。 | `src/platform/sdl1/game_loop_preview_update.{c,h}`、`runtime_initial_scene.c`、`runtime_cursor.c`、`runtime_input_bootstrap.c`、`runtime_input_config.c`、`src/app/game_session.c` |
| S5 adapter/音频 | 商店及相关 platform adapter 使用 `GameSession.audio_events`,并统一按 borrow/flush 借用归还(见下节)；保留光标消费、音乐恢复和即时语音的真实副作用。 | `src/app/audio_event.{c,h}`、`game_loop_audio_events.{c,h}`、`game_loop_shop_adapter.c`、`src/app/shop_flow.c`、`tests/architecture/check_context_simplification.py` |
| S7 route/构建 | 删除 dispatch current 镜像；按唯一写入族整理注释和调用；Makefile 复用对象清单并为 SDL 测试提供 runtime staging。 | `src/core/route_event_executor_state.h`、`src/app/route_flow.c`、`Makefile`、`tests/architecture/check_sdl_object_manifest.py` |
| S8 闸门/回归 | 上下文简化与 renderer boundary 检查器纳入测试；五条链、全量测试、两种 profile、原始资源扫描完成。 | `tests/architecture/check_context_simplification.py`、`check_renderer_viewmodel_boundary.py`、第 7 节验证结果 |

当前登记的 9 个语法候选均有具体边界用途,精确身份与理由以检查器中的 `FORWARDER_EXCEPTIONS` 为准:

1. `src/platform/sdl1/audio_stub.c:rich4_audio_shutdown` 实现无 mixer 后端的音频关闭接口并释放其拥有的内存。
2. `src/platform/sdl1/game_loop_scene_update.c:update_fallback_menu_scene`、`update_fallback_options_menu_scene`、`update_fallback_options_game_scene` 将有类型的渲染入口适配到场景服务的 `void *` 回调。
3. `src/platform/sdl1/game_loop_update.c:update_game_session` 是 SDL 帧更新入口,向 session update adapter 传递平台输入。
4. `src/platform/sdl1/test_adapter/diff_adapter.c:r4diff_active`、`r4diff_ticks` 隔离差分测试 API 与 lockstep 的激活状态、时钟实现。
5. `src/platform/sdl1/test_adapter/diff_adapter.h:r4diff_poll`、`r4diff_localtime` 在普通构建中保留 SDL 输入与宿主日历时间,测试构建使用另一分支实现。

`main` 当前同时设置日志缓冲,不属于单条调用候选,因此不登记例外。

### 音频交接队列的归属纪律

把一次性音频队列从栈上搬到 `GameSession.audio_events` 消除了 47 处栈缓冲,但也把它变成了 46 处调用点共用的可变字段:每处各自 `init`,谁都没有声明所有权。只要出现嵌套区段,内层 `init` 就会静默清掉外层尚未交给混音器的事件,而这一约束当时只存在于人工核查里。

排空必须紧跟每次领域调用,不能延到帧尾——第 4 节要求保留同帧先后次序,推迟会改变队列事件与音乐恢复、语音打断等非队列音频操作的相对顺序。因此三段式结构本身是正确的,要修的只是所有权。项目没有 `assert` 约定,所以做成结构性正确而不是运行时断言:

- `rich4_app_session_audio_borrow(session)` 借出队列,只有最外层借用会清空它;
- `rich4_app_session_audio_release(session)` 归还,只有最外层归还才返回队列、表示调用方必须排空;
- `game_session_audio_flush(session, audio)` 是平台层唯一排空入口,内部调用 release 后把事件按发出顺序交给混音器。

非嵌套(当前全部调用点)行为与改造前逐字一致;若将来出现嵌套,内层不再清空、也不提前消费外层事件,而是把排空推给外层,顺序仍为发出顺序,不会丢事件。`GameSession.audio_events` 的 `borrow_depth` 是唯一新增字段。

| 指标 | 搬迁后 | 现在 |
| --- | ---: | ---: |
| 直接对共享队列 `init` 的调用点 | 46 | 0 |
| 队列排空循环 | 13 | 2 |
| 每文件私有排空 helper | 6 | 0 |
| 直接触碰 `session->audio_events` 的生产代码 | 46 | 2(仅借用/归还两个访问器) |
| 借用与归还配平 | 无机制 | 47 处借用逐一配平 |

保留的 3 处 `rich4_audio_event_queue_init` 各有理由:`audio_event.c` 是定义;`route_event_flow.c:188` 是 `rich4_app_route_event_step` 对 out 参数队列的初始化契约(`test_route_event_flow.c` 传入未初始化的局部队列并依赖它);`runtime_loop_state_bootstrap.c` 是会话启动时的一次性归零。`game_emit_session_audio_events` 也改为借用,因此 `GamePreviewFrameContext` 去掉了 `audio_events` 字段。

生命周期注意事项:展示快照中的指针只保证覆盖 `draw_game_preview_adapter` 当前帧的同步消费,调用方不得跨帧保存；若未来出现持久化消费者,需重新建立所有权或历史快照。缺 sanitizer runtime、显示设备与真实 Miyoo 环境属于外部验证残留,不构成当前源码闸门或本机构建失败。

另有一处有意保留的低收益接口:`src/app/scene.c` 的 `scene_update` 仍为空实现,5 张 `Rich4SceneOps` 表继续共享该槽位。它是现有场景表 ABI 的统一占位,没有足够行为证据支持删除,因此不在本次重构中冒险修改。
