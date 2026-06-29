# `0x40829D` 可见对象收集与地图绘制

## 证据范围

- 原版函数：`sub_40829D`，范围 `0x40829D..0x409A23`，大小 `0x1786`，184 个 basic blocks；恢复原型为 `short __cdecl(int view_x, int view_y)`。
- 当前 IDB：V3.11 `Rich4.exe`，32 位 x86，image base `0x400000`，SHA-256 `5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550`。
- IDA Pro MCP：2026-07-16 完整读取 764 行伪代码，并复核 `0x4079F9` 排序比较器和 `0x407A2C` 子 tile 投影；2026-09-25 重读完整反编译，核实统一队列末段没有独立工具机器人来源。
- 本地依据：[`rich4/asm/rich4.asm`](../../../rich4/asm/rich4.asm) 的 `fcn_0040829d`（第 6342..7602 行）；[`rich4/csrc/player_action.c`](../../../rich4/csrc/player_action.c) 第 30 行保留了 `fcn.0040829d(-1, 0)` 调用，并明确原 C 项目尚未还原该长函数。
- SDL 目标模块：`platform/sdl1/sdl_game_renderer.c`；平台无关收集/排序规则位于 [`src/view/map_renderer.c`](../../src/view/map_renderer.c)。

## 函数职责边界

此函数不是单纯的 terrain renderer。一次调用依序完成：

1. 解析视点；必要时初始化当前玩家的首次落点。
2. 绘制当前旋转方向的地面块和道路/节点标记。
3. 收集普通玩家、特殊玩家和固定 46 槽对象池中的活动动态对象。
4. 收集地产、设施以及两类特殊显示表。
5. 为所有需要遮挡排序的对象建立统一记录，按原版 key 排序。
6. 逐项应用 owner palette、选择方向/帧并绘制。
7. 绘制动态对象数值和当前剩余步数，提交显示面并清零两个临时全局。

因此 SDL 若只复现地产/特殊格 visible-list，不能宣称已覆盖 `0x40829D` 的全部原版职责。

## 视点与首次落点初始化

`0x40829D..0x4083A3`：

- `view_x == -1` 时忽略传入的 `view_y`，改用全局视点 `(0x48B2AC, 0x48B2B0)`。
- 当前实体索引 `< 4` 且视点恰为 `(0, 0)` 时，函数调用 `0x40AA0F` 选择一个 road/node：
  - 把节点号写入当前玩家记录；
  - 从该 0x28-byte road 记录的四个候选字段中收集非零值并随机选择一个 next node；
  - 用 `0x407A8C(next, current)` 计算方向；
  - 把视点设为该节点坐标；
  - 写 `0x475114 = current_player + 1`，形成 one-based landing pending；
  - 在节点记录 `+0x24` 的 dword 中置位 `0x100 << current_player`。
- 这里的 `sub_407A8C(next,current)` 只描述首次落地初始化。进入实际行走
  后，`sub_40C05C` 会把旧节点写入 `+14`、目标节点写入 `+12`，同一函数
  因而得到 `target-old` 的正向位移，不能把首次落地的参数语义套到行走动画。
- 最终视点始终写回两个全局，并计算：

  ```text
  viewport_tile_x = view_x >> 5
  viewport_tile_y = view_y >> 5
  sub_407A2C(view_x, view_y, &sub_x, &sub_y)
  screen_origin_x = sub_x + 220
  screen_origin_y = sub_y + 260
  ```

这确认原版主视点中心是 `(220, 260)`，不是 `(260, 220)`。

## 地面与道路标记

### 29×29 地面窗口

`0x4083FA..0x408598` 按当前旋转 `0x499088` 索引 `0x473610` 的预生成遍历表，最多处理 304 个条目，遇到 `-128` sentinel 停止。每项把相对 tile 加到视点 tile，使用 `0x46CCF0..0x46CCF6` 的旋转网格角点表构造目标四边形：

- 遍历表每对有符号字节依次是 `{相对 Y, 相对 X}`；`0x408429..0x40855A` 将首字节装入 EBP，检查 sentinel，并以它索引网格行及地图 Y。第二字节用于网格列及地图 X。
- 合法地图范围是 `72×72`；越界使用固定空白 tile `0x48B2B4`。
- 合法 tile 从 ground index 表 `0x48BAC4` 取 16-bit tile id，再以 `tile_id << 10` 定位到 `0x48BACC` 的 1024-byte tile 数据。
- 最后调用 `0x4557A1` 把 tile 绘到地图 surface。

### road/node 直接标记

`0x4085A4..0x40865B` 遍历 1-based road 表 `0x498E80`，记录宽 `0x28`。仅当记录 `+0x22` 的显示字段非零时绘制：

- 通过共同的 29×29 culling 和 `0x407A2C` 投影求屏幕锚点；
- 图块来自 `0x474949`，chunk 为 `record.+0x22 - 1`；
- 这批标记立即绘制，不进入后续遮挡排序队列。

## 共同 culling 与屏幕投影

除地面遍历外，表对象都使用同一广义流程：

```text
tile_x = (world_x >> 5) - viewport_tile_x + 14
tile_y = (world_y >> 5) - viewport_tile_y + 14
visible iff tile_x and tile_y are both in 0..28

sub_407A2C(world_x, world_y, &object_sub_x, &object_sub_y)
anchor_x = grid_x[rotation][tile_y][tile_x] - object_sub_x + screen_origin_x
anchor_y = grid_y[rotation][tile_y][tile_x] - object_sub_y + screen_origin_y
```

反编译把大多数 tile 临时量恢复成 unsigned，因此源码中常只出现 `<= 0x1C`；负数转换为大 unsigned 后自然失败。动态浮点对象的一个分支则显式检查 `< 0`。

`0x407A2C` 只处理世界坐标低 5 bit，使用当前旋转对应的四个有符号系数：

```text
sub_x = ((x_low * c0) >> 5) + ((y_low * c2) >> 5)
sub_y = ((x_low * c1) >> 5) + ((y_low * c3) >> 5)
```

## 统一可见对象记录

原版使用并行数组，当前数量为 `0x48BAC8`：

| 数组 | 每项含义 |
| --- | --- |
| `0x48A44C` | 32-bit 排序记录：低 word 为深度/类型 key，高 word 为对象槽号 |
| `0x48A84C` | 图形资源指针 |
| `0x48A850` | 对象 id/点击或额外标志 |
| `0x48A852` | owner byte；`0xFF` 表示不改 owner palette |
| `0x48A853` | chunk/方向帧 |
| `0x48A854`, `0x48A856` | 屏幕 X、Y |

普通深度 key 的基本形状是：

```text
key = (slot_index << 16) | u16(screen_y * 16 + type_bias)
```

`type_bias` 用于同一 Y 附近的玩家/叠加物次序。动态飞行对象另有固定低 word `0x7FF0`。高 word 保留原插入槽号，排序后仍可索引并行数组。

## 收集顺序与字段映射

### 普通玩家（`0x40865B..0x408813`）

- 遍历活动玩家；X 坐标 `+0` 非零，且 `movement_state_dword == 0` 或状态 byte `+0x0D` 含 bit `0x20` 时才收集。这里按原式记录 guard，不再为未被本函数解释的状态强加玩法名。
- 资源索引精确为 `player * 13 + 2 * render_state_2 + render_state_1`；方向内帧为 `render_frame + rotated_direction * (resource_part_count / 8)`。
- 玩家记录 `+0x2E` 非零且渲染记录首 byte 尚无 `0x40` 时，先对本次资源执行一次 `0x4555C5`，随后置 `0x40`，形成一次性资源准备 guard。
- 方向帧公式包含当前地图旋转：`(8 - rotation + actor_direction) & 7`，再乘每方向帧数并加当前动画 frame。
- 当前玩家 bias 为 `0x0D`，其他普通玩家为 `0x0C`。
- owner/mask 写入 `0x8000 | (1 << player)` 形状；可选额外 overlay 以 bias `0x0F/0x0E` 作为第二个队列项。
- overlay guard 是玩家记录 `+0x2F`；其资源固定为 `0x496978`，chunk 来自渲染记录 `+4`。这些 byte 在本函数中的机械用途已确定，即使其上层玩法名称不在绘制函数内。

### 特殊玩家（随后 5 槽）

- 扫描原版 5 个 special-player slot，对应运行时实体索引 `4..8`；槽 `+0x0A == 0` 才进入绘制。
- 16-byte 槽字段在本函数中的用途为：`+0/+2` 世界 X/Y，`+4` road/node，`+9` actor 方向，`+0x0A` 禁用 guard，`+0x0C` 一次性资源准备 guard，`+0x0D` overlay guard。方向同样减去地图旋转。
- 当前实体 bias 为 `0x0D`，非当前特殊玩家通常为 `0x08`；SDL
  `GameViewModel.current_actor` 直接保留普通/特殊共用的实体索引，避免把
  特殊回合错误地折叠成 HUD 的普通玩家索引。
- 前四槽会按节点/状态选择普通或特殊资源族，并写 `0x8000 | (0x10 << slot)` 形状的 mask；第 5 槽走普通 actor 资源族且 mask 为 0。
- 槽 `+0x0D`（梦游倒计时）非零时，原版还会加入一个独立的
  `Map.mkf[25]` overlay 队列项；其 chunk 使用该实体渲染状态 `+0x04`
  的动画帧，与主体同坐标，但使用普通角色 overlay 的 `0x0E/0x0F`
  深度偏移。SDL 现在通过 `GameSpecialActorView.overlay_chunk` 将这个
  瞬时状态送入主地图和命中信息的同一资源队列。

### 46 槽动态对象池（约 `0x408C9D..0x4090D1`）

这里的 46 是**实例槽数，不是 46 种 type**。`0x407AD2` 先把 46 个 24-byte 记录清零，再由常量表写入 type，并从 Data archive 装载 20 个视觉资源：

就 `0x40829D` 的统一队列来源而言，此动态池循环是唯一的 46 槽对象来源；函数后续只再遍历地产、设施和两类特殊显示表，未见独立追加工具机器人的队列记录。池槽 0 按通用动态对象规则会编码 raw ID `0x8100`，因此同一个 ID 本身不能证明 SDL 单独绘制的工具机器人属于该表。AI 投影应仅保留真实动态池记录；工具机器人继续由 SDL 场景渲染路径显示，不额外合成一个全局 AI 对象。

```text
resource[type] = DATA.MKF chunk (396 + type - 1), type = 1..20
```

初始槽分区与 `sub_40E033` 的分配范围完全一致：

| type | 槽范围 | 容量 |
| ---: | --- | ---: |
| `1..14` | 每个 type 固定使用槽 `type-1` | 各 1 |
| `15` | `14..15` | 2 |
| `16` | `16..25` | 10 |
| `17` | `26..35` | 10 |
| `18` | `36..45` | 10 |
| `19..20` | 已加载视觉资源，但初始池表未预留槽 | 0 |

因此不能从本函数证据给 type 1..20 杜撰道具/事件名称；原版可观察契约是资源映射和固定容量。type 15..18 是可复用池，type 1..14 是单例槽。

每个 24-byte 记录的字段已由绘制、分配和自由飞行动画调用点闭环：

| 偏移 | 用途 |
| ---: | --- |
| `+0` | type/resource index |
| `+1` | node-bound 方向 |
| `+2` | node/road id；非零时也是活动 guard |
| `+4` | type 18 绘制后的十进制数值；附身神明路径也写入原版的短倒计时字段 |
| `+5` | one-based attached normal-player id |
| `+6` | 自由飞行 active/剩余帧数标志；每次绘制递减，越出窗口清零 |
| `+7` | 自由飞行方向 |
| `+8/+0x0C` | float 世界 X/Y |
| `+0x10/+0x14` | float 每帧 dX/dY |

每项宽 `0x18`，函数区分三种定位方式：

1. `attached_player != 0`：跟随对应普通玩家坐标，并按玩家方向选择两组 anchor offset；类型 18 还有独立 offset/标志。
2. countdown 非零：使用 float X/Y 和每帧速度更新位置；离开 29×29 窗口时清零 countdown，否则本帧递减。
3. 其余：绑定到指定 road/node 的坐标。

三种方式最终都选择上述 `396 + type - 1` 资源、方向 chunk、屏幕坐标和对象 id。type 18 会把 one-based 槽号编码进对象 id 高 byte 并置 `0x40`；绘制循环清除该标志后，从对应记录 `+4` 取无符号 byte，以 GDI 文字路径在 `y-60` 居中绘制十进制数值。

### 地产表 `0x498E84`（`0x4090D1..0x4092F4`）

- 1-based 遍历，记录宽 `0x34`，对象 id 为 `0x7D0 + index`。
- 方向为 `(8 - (rotation + record.+0x1B)) & 7`。
- `+0x1A != 0` 表示有已建显示资源：owner byte 取 `+0x19`，资源由 `+0x18` 决定使用 overlay 或 view bucket。
- `+0x1A == 0` 时 owner byte 为 `0xFF`；若 `+0x19` 有 owner，则用 common resource `0x48AEA8`，chunk 改为 owner 玩家角色，否则资源为空。
- `+0x17` 非零时，还立即绘制一层来自 `0x47494D` 的 parity overlay；它不进入统一排序队列。
- `0x407AD2` 的装载阶段预先登记每张地图五个 view bucket。SDL 现在把全部五级 `Map[gmap*5+39+slot]` 同时注册为运行时对象资源，而不是只注册初始地图记录已使用的等级；因此 0→1 后会从角色标志切换到可绘制建筑，后续升级也不会因资源未登记而消失。

### 设施表 `0x498E88`（`0x4092F4..0x40954A`）

- 1-based 遍历，记录宽 `0x38`，对象 id 为 `0xFA0 + index`。
- 方向和 owner/unbuilt fallback 与地产相同。
- 建成资源由记录 `+0x18` 的类别选择 `0x48AE64`、`0x48AE78` 或 `0x48AE90` bucket，再用 `+0x1A` 作为索引；类别 0/3 使用各自 bucket 的第 0 项。
- `+0x1C` 非零时立即绘制 `0x47494D` 的设施 parity overlay，不进入排序队列。
- SDL 同样把 overlay 与全部 17 个 stage-tile bucket 注册为可绘制对象资源，覆盖设施可达升级等级。

### 第一类特殊显示表 `0x498E7C`（`0x40954A..0x409689`）

- 1-based 遍历，记录宽 `0x34`；仅 `u16(+0x20) != 0` 时收集。
- 资源为 `0x48AE4C[u16(+0x20)]`。
- 对象 id 为 `0x1770 + index`。
- owner byte 取 `+0x18`，方向为 `(8 - (rotation + byte(+0x1B))) & 7`。

### 第二类特殊显示表 `0x498E78`（`0x409689..0x4097CF`）

- 1-based 遍历，记录宽 `0x1C`；仅 `u16(+0x1A) != 0` 时收集。
- 资源为 `0x48AE4C[u16(+0x1A)]`。
- 对象 id 为 `0x1F40 + index`。
- owner 固定 `0xFF`，方向为 `(8 - (rotation + byte(+0x18))) & 7`。

## 原版排序与绘制

`0x4097CF..0x409A1B`：

1. 若队列非空，调用 `0x457E6C(array, count, 4, 0x4079F9)`。
2. 比较器 `0x4079F9` **只把每个 32-bit key 的低 16 bit 当作 signed short 比较**；相等时返回 0，不再比较高 word。因此不能把“高 word 槽号”描述成原版显式 tie-break。
3. `0x457E6C` 是会在 partition 中交换 equal-key 元素的 qsort 风格实现，不是稳定排序；equal-key 对象不保证保持插入顺序。
4. 排序后从 key 高 word 取回原槽号。
5. 资源非空且 owner byte != `0xFF` 时：owner 0 写 palette 值 0；owner 非零则经 `0x4551F0(player_color)` 转换，然后写资源图形数据的 `+0x1FE` palette word。
6. 调用 `0x456770(surface, resource, chunk, x, y)`：它按 SMP 子图 hotspot 定位、按当前 clip rectangle 裁剪，索引 0 透明，其余索引经资源内 16-bit palette 写入 640-pixel-pitch surface。
7. 动态对象的数值标志置位时，在对象上方 `y - 60` 绘制相应十进制数值。
8. 若 action-hint 全局、剩余步数和当前实体状态 guard 同时允许，则把剩余步数转成十进制字符，以 50 px 字宽居中在 `x=245` 附近、`y=400` 绘制数字图块。
9. 解锁/提交显示面，并清零 `0x474930`、`0x474934` 两个临时全局。

地面 helper `0x4557A1` 是四边形纹理 rasterizer：四次 `0x45596A` 建边，逐 scanline 插值两个纹理坐标，以 32×32、8-bit ground tile 查 `0x48B6B4` 的 16-bit palette 后写入 surface。即时 parity overlay 使用的 `0x456C33` 则把 1-bit/byte mask 经过当前像素格式对应的 `word_488EF0` mask OR 到目标像素。`0x44F9D8` 配置 GDI 字体/前景背景色，`0x44FABC` 负责对齐、描边/阴影、GDI surface 取回和最终 blit。由此本函数直接调用的主要绘制 helper 已无未解释的像素职责。

## 与当前 SDL 实现的关系

当前 [`src/view/map_renderer.c`](../../src/view/map_renderer.c) 已覆盖共同 29×29 culling、`0x407A2C` 等价投影、收集→排序→绘制结构、地图旋转方向、owner palette 合成和部分地图对象。`rich4_map_property_render_style` 固定未建有主地产显示 owner 角色标志、建成后选择旋转 variant 并启用 owner palette 的切换规则。测试见 [`tests/unit/test_map_renderer.c`](../../tests/unit/test_map_renderer.c)。

需要修正文档和实现认知的关键点：

- 原比较器只比较 signed low word，且所调用排序器不稳定；“相同 Y 时用原列表序号稳定 tie-break”只能是 SDL 为确定性采用的 characterization，不是原版规则。
- `0x40829D` 还包含 ground、road direct overlay、普通/特殊玩家、动态对象、即时 overlay、数值和步数显示；这些不应被地图对象单测替代。
- 原函数会在 `(0,0)` 视点触发首次落点状态变更，因此它不是纯 renderer。健康重构可以把该副作用移到 core，但必须保持调用前后的可观察顺序。

## 置信度

- `confirmed`：函数范围、视点规则、首次落点副作用、29×29 culling、投影、各表遍历宽度/字段/对象 id、普通/特殊 actor 的精确 guard 与索引公式、46 槽池布局、type→资源映射、三种动态定位方式、收集顺序、排序比较器、主要像素 helper、owner palette 写入和绘制后数字逻辑。
- type 的道具/事件美术名称和部分状态 byte 的玩法文案不存放在本函数或其资源装载表中；本文以地址、位条件、资源公式和可观察副作用定义它们。这些标签缺失不再作为绘制行为的 `partial` 项。
- 本文整体行为置信度升级为 `confirmed`。

后续测试应把 ground、road immediate overlay、五类队列对象、equal-key、owner 0/非零/`0xFF`、两类动态对象数字以及首次 `(0,0)` 落点分别建立 characterization；当前单元测试只覆盖其中的平台无关子集。
