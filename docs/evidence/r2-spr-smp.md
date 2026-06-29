# R2.2 SPR/SMP 格式层拆分

- 范围：将 SPR/SMP header probe、chunk 表解析和像素解码从 MKF archive API 分离；不修改资源编号或渲染规则。
- 本地格式依据：现有合成样本使用 12 字节 `SMP\0` header、12 字节 chunk entry 和显式 graph offset；原型渲染路径将 SMP graph 解释为小端 RGB555，将 SPR graph 解释为 8 位 palette index，SPR palette 为 256 个小端 16 位颜色。
- 模块：`resource/spr_smp.[ch]` 定义 `Rich4SprSmpInfo`、`Rich4SprSmpChunk`、独立错误域、probe、chunk lookup 和平台无关 RGB555 解码。
- 边界：`mkf.[ch]` 不再声明或实现任何 SPR/SMP 类型/API，只负责 archive、索引、解压和返回资源字节。`spr_smp` 不依赖 MKF、SDL、app 或 view。
- 错误：短/损坏 header 返回 probe false；无效参数返回 `FORMAT`；chunk index、截断表、graph offset/size、palette 和输出容量越界返回 `RANGE`。
- 置信度：`confirmed`（保持既有格式语义的机械拆分）。
- 测试链接：[`tests/resource/test_spr_smp.c`](../../tests/resource/test_spr_smp.c)。

