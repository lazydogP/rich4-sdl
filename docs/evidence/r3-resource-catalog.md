# R3.3 ResourceCatalog

`data/resource_catalog.[ch]` 的每个成功条目都包含 archive、resource index、IDA evidence address 和 `confirmed` 置信度。参数越界或未确认的映射返回 false，并把输出重置为 `unknown`，不会继承上一次有效条目。

默认 catalog：

| family | mapping | evidence | confidence |
|---|---|---:|---|
| Map common 0 | `Map[24]` | `0x407BAB` | confirmed |
| Map common 1 | `Map[25]` | `0x407F77` | confirmed |
| Map common 2 | `Map[26]` | `0x407BBF` | confirmed |
| Map view | `Map[gmap*5+39+slot]`, slot 0..4 | `0x407E3B` | confirmed |
| Map overlay | `Map[gmap+79]` | `0x407E6C` | confirmed |
| stage-0 tile | `Map[87+slot]`, slot 0..16 | `0x407E8E` | confirmed |
| stage-1 tile | `Map[104+map*17+slot]` | `0x407EC1` | confirmed |
| map character | `Map[27+model_resource_id]` | `0x407FC7` | confirmed |
| Panel event family | `Panel[78+slot]`, slot 0..12 | `0x415253..0x415334` | confirmed |
| Panel ending layout | `Panel[93]` | `0x40768C` | confirmed |
| Panel ending character | `Panel[100+character]` | `0x407673` | confirmed |
| Panel dialog 112 | `Panel[112]` | `0x40785B` | confirmed |
| 通用是/否按钮 | `Data[440]` | `0x453A64` | confirmed |
| 通用询问/发言背景 | `Data[517]` | `0x440C14` | confirmed |
| 金贝贝发言图示 | `Data[519]` | `0x44F24B` | confirmed |

`0x440BA8` 本身不读取 `Panel[112]`：它在地图 surface 上绘制问题文字，再调用 `0x453A32(220,320)`；后者于 `0x453A64` 从 `0x48A0E4` 读取 `Data[440]`，以资源原始宽高居中，并把左、右半区分别解释为返回值 1、0。`0x40172E` 明确把 `0x48A0E4` 绑定到 `data.mkf`，而 `0x401752` 才把 `0x48A05C` 绑定到 `panel.mkf`；两项资源属于不同调用链，catalog 不用多属性兜底混同它们。

SDL 当前对 `Panel[0..8]` 的格式探测没有对应 V3.11 IDA 映射证据，故仍是 app 层兼容探测，没有进入默认 catalog。Map common/view/overlay/stage tile/character 与通用确认按钮的运行时索引计算已改用 catalog。

`Data[517].chunk[5]` 是购买与金额消息背景，chunk[6] 是 `player_say` 背景；`Data[519]` 的 `@NN` 分支使用 chunk `NN-1`。完整调用链见 [地产弹窗、角色发言与收费演出](41982d-property-dialog-color-delay.md)。

测试：[`tests/unit/test_resource_catalog.c`](../../tests/unit/test_resource_catalog.c) 覆盖每个族的端点、证据地址、置信度及 unknown 拒绝路径。
