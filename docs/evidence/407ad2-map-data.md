# `0x407AD2` MAP/MAPDAT typed data

IDA 证据：

- `0x407ADB` 打开 `MAP.MKF`。
- `0x407BD1..C14` 尝试打开 `MAPDAT.MKF`；存在时读取 `gmap = stage*4+map`，缺失时读取 `MAP[2*gmap+1]`。
- `0x407C3F..CA6` 读取 40 字节 header 的五组 count/relative offset。
- 五张表的记录宽依次为 route 40、land 52、facility 56、auxiliary 52、special 28；原版以索引 1..count 消费，索引 0 保留。
- `0x407ECE..F5F` 从后两类表的 `+32` / `+26` 读取 sprite resource id，仅在需要时加载 `MAP[sprite+38]`。

实现：

- `data/map_data.[ch]` 提供 MAPDAT 优先/MAP fallback 选择和无 SDL parser。
- `MapData` 只保存 source archive/resource id、typed records 和每条记录的完整原始字节，不保存 SDL surface 或资源裸指针。
- route 节点内嵌 `MapObjectRef`；land/facility 显式保存 owner/type/level/variant/price；两类特殊表保留已确认的 sprite/type/variant 和完整 raw record。
- `main_sdl.c` 使用 selection API 选择一次 archive/resource，并保存八份 typed `MapData`。路线查询、移动、对象引用、道路对象绘制及地产/设施会话初始化均直接消费 MapData；旧 route/land/facility 数组和旧 raw parser 已删除。
- renderer 使用显式 `Rich4ResourceHandle` 读取 map common/object/ground/overview/character 字节；`data/size` 兼容别名只存在于加载与格式 probe 阶段，不再被绘制路径消费。

真实 V3.11 资源没有独立 Mapdat.mkf，因此验证了 MAP fallback 的八个资源 `1,3,5,7,9,11,13,15`。解析后的含保留项 entry 数：

| gmap | routes | lands | facilities | auxiliary | specials |
|---:|---:|---:|---:|---:|---:|
| 0 | 104 | 51 | 5 | 4 | 22 |
| 1 | 145 | 74 | 9 | 5 | 27 |
| 2 | 111 | 50 | 6 | 7 | 17 |
| 3 | 119 | 56 | 9 | 7 | 17 |
| 4 | 136 | 48 | 6 | 4 | 3 |
| 5 | 136 | 61 | 4 | 13 | 144 |
| 6 | 142 | 56 | 7 | 4 | 155 |
| 7 | 102 | 1 | 21 | 8 | 80 |

`tests/resource/test_map_data.c` 以合成 Mapdat 记录覆盖优先选择、fallback resource 公式、五表字段、raw 未知字节保留和坏 offset/截断失败。`mkf_validate_map_data` 与 `make test-original-data` 覆盖真实 fallback 资源。
