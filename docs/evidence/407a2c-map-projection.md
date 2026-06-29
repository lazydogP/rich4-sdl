# `0x407A2C` 地图 sub-tile 投影

- 地址与目标模块：`0x407A2C`；`view/game_view_model.c`。
- IDA 结论：2026-07-11 已复核；函数大小 `0x60`，原型恢复为两个输入及两个输出指针；反编译确认 `& 0x1F`、四项有符号系数和右移 5 的投影公式。
- 本地证据：[`../../rich4/asm/rich4.asm`](../../../rich4/asm/rich4.asm) 第 5598 行开始将两个输入分别与 `0x1f` 相与，使用方向全局 `0x499088` 索引 `0x474910..0x474913` 的有符号字节系数，乘积算术右移 5 后写出两个坐标；同文件从第 6439 行起有多个绘制调用点。
- 置信度：`confirmed`（投影公式边界；viewport 包装独立于该地址）。
- 测试链接：[`tests/unit/test_game_view_model.c`](../../tests/unit/test_game_view_model.c) 覆盖八方向、`&31` 包装与负输入；实现位于 [`src/view/game_view_model.c`](../../src/view/game_view_model.c)。
- 缺口/冲突：viewport 屏幕裁剪是调用层行为，不归因于这个 `0x60` 字节函数。
