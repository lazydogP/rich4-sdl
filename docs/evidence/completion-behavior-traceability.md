# 完成定义：行为追溯与 unknown 边界

`behavior-traceability.tsv` 是当前生产行为的机器可读索引。它区分原版确认行为、格式机械实现、架构边界、兼容 characterization，以及含有明确 partial 的混合实现；后两类不得在文档或代码中冒充完整原版语义。

运行时仍保留的菜单背景、缺失资源提示、调试文字等 fallback 是 SDL 原型兼容路径，不是原版 UI 复刻。`0x40B93B` 未完整确认的 actor part 仍保持 `partial`；`0x40829D` 的原版比较器和对象收集/绘制行为已经确认，但 SDL 的同 Y 稳定 tie 仍明确属于 characterization，尚未迁移的地面、actor、动态对象等职责也不能由现有地图对象单测代替。已迁移的 `+50..+62` 状态事务均返回 `APPLIED`；`STATUS_EFFECT_UNIMPLEMENTED_CONFIRMED` 只作为旧接口/审计守卫保留，不代表当前生产状态分支仍 deferred。资源目录未确认映射返回 `unknown`；未知 FLC 画面 opcode 返回 `RICH4_FLC_ERR_UNSUPPORTED_OPCODE`。

`test_behavior_traceability` 验证每个台账模块、证据文件和测试文件存在，分类只使用批准集合，生产行为源文件都有台账行，并检查上述关键 unknown/unimplemented 守卫仍存在。它不把“文件存在”当成 IDA 语义证明；语义证据仍由链接记录、专项测试和原始资源扫描共同承担。

完成复验（2026-07-13）：机器台账共 27 行，Windows 与 Linux sanitizer 守卫均通过。Windows 强制重建 `test test-render tools sdl test-original-data`，V3.11 Data/Jump/Panel FLC 和 Map 扫描通过，C99 `-Wall -Wextra` 无警告。由此确认当前声称为原版的实现都有地址或 `rich4` 文件入口；仍不完整的语义保持 mixed/partial、characterization、unknown 或 `UNIMPLEMENTED_CONFIRMED`，兼容 fallback 均有可诊断日志且不计作原版复刻。

首回合 replay 已改为无 SDL 的真实模块集成路径，不再直接伪造六段完成事件：测试实际驱动 `SceneMachine` 的 Menu→NewGame→Intro→Game、`new_game_init`、human/AI 共用的 `GameCommand` dispatcher、固定 MSVCRT RNG、route movement 和 turn rotation。种子 311 两次均得到真人 5 步、轮转 AI 后 2 步的逐字节相同 trace；Windows 与 ASan/UBSan 均通过。测试见 [`tests/replay/test_first_turn_replay.c`](../../tests/replay/test_first_turn_replay.c)。
