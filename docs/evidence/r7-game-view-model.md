# R7.1 GameViewModel

## 平台无关边界

`GameFlowState` 已从含 SDL 的 `app/app_types.h` 移到 `core/game_flow_state.h`，计时字段使用 `uint32_t`。因此 `view/game_view_model.[ch]` 可直接从 `const GameState + const GameFlowState`（以及可选只读 `MapData`）构建 `GameViewModel`，无需 SDL、资源 surface 或 debug trace。

ViewModel 包含：

- `GameViewportModel`：世界中心、方向和 override。
- `GameActorView[4]`：角色、可见性、世界位置、方向、移动帧、primary/fallback/overlay/action part 和证据置信度。
- `GameSpecialActorView[5]`：特殊人物的位置、方向、主体资源 part，以及梦游时独立 `Map.mkf[25]` overlay 的 chunk。
- `current_actor`：原版地图排序使用的普通/特殊共用当前实体索引，不与 HUD 的普通玩家索引混用。
- `GameHudView`：tab、当前玩家、角色、现金、银行、合计和物价指数。
- 剩余步数等纯展示字段。

## 投影与 actor

- `rich4_game_viewport_build` 只负责选择/包装 viewport。
- `rich4_game_viewport_project` 单独实现原版 8 方向 29×29 表格与 `0x407A2C` sub-tile 系数，并为表外点使用原有线性 fallback。
- `rich4_game_actor_part` 集中 frozen、sleepwalk、特殊道路、延迟状态和普通交通 part 优先级。
- `0x40B93B` 证据明确分为 confirmed 与 partial；未确认的 13/14、16/17、18 语义仍标记 `VIEW_EVIDENCE_PARTIAL`。

## Renderer 接入

- HUD renderer 只读取 `GameHudView`，其绘制范围内没有 `session->debug` 访问。
- 普通玩家 actor renderer 从 `GameActorView` 获取 part、方向、action frame 和 overlay，不再自行判定状态优先级。
- 特殊人物的主体与梦游 overlay 都由 `GameSpecialActorView` 投影到统一地图队列；地图信息/像素命中复用同一 overlay 资源和 chunk。
- 主视图世界坐标转换已改用独立 viewport projection API。

## 验证

`tests/unit/test_game_view_model.c` 覆盖 viewport override、中心投影、HUD 快照、方向换算、特殊道路、交通 overlay、sleepwalk 和 frozen part。`core/game_flow_state.h` 纳入无 SDL header 门槛。全量测试、固定 replay、SDL 和原始资源扫描通过，`-Wall -Wextra` 无警告。
