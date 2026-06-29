# `0x407AD2` 地图资源初始化

- 地址与目标模块：`0x407AD2`；`data/map_data.c`。
- IDA 结论：2026-07-11 已复核；函数范围 `0x407AD2..0x4080F4`。反编译确认 `MAP.MKF`/`MAPDAT.MKF` 字符串、MAPDAT 缺失 fallback、chunk 24/25/26、`gmap*5+39..43` 和 `gmap+79` 等索引。
- 本地证据：[`../../rich4/asm/rich4.asm`](../../../rich4/asm/rich4.asm) 第 5656 行开始先调用 `0x4080F5`，打开 `MAP.MKF` 并按 stage/map 计算索引读取资源；[`../../rich4/csrc/loadsave.c`](../../../rich4/csrc/loadsave.c) 第 99 行开始还原了 `MAP.MKF`、可选 `MAPDAT.MKF`、索引 `gmap * 2`、`gmap + 16`、24、26 以及 fallback `gmap * 2 + 1`；[`../../rich4/csrc/main.c`](../../../rich4/csrc/main.c) 第 372 行显示新局加载路径调用该函数。
- 置信度：`confirmed`（已迁移的 archive 选择、索引、表字段和 lifetime 范围）。
- 测试链接：[`tests/resource/test_map_data.c`](../../tests/resource/test_map_data.c)、[`tests/resource/test_map_data_fault_injection.c`](../../tests/resource/test_map_data_fault_injection.c)；真实 Map 资源由 `test-original-data` 校验。
- 缺口/冲突：未迁移的原版绘制期全局副作用不属于 `MapData` 的确认范围；不得据此扩大所有权语义。
