# R2.3 V3.11 real FLC opcode scan

命令：`bin/mkf_scan_flc_opcodes assets/original/V3.11/<archive>.mkf`。

| archive | FLC resources | decoded frames | observed opcodes |
|---|---:|---:|---|
| Data.mkf | 72 | 1806 | 4, 7, 15, 18 |
| Jump.mkf | 25 | 1056 | 4, 7, 15, 18 |
| Panel.mkf | 8 | 237 | 4, 7, 15, 18 |
| Effect.mkf | 0 | 0 | — |
| Help.mkf | 0 | 0 | — |
| Map.mkf | 0 | 0 | — |
| Speaking.mkf | 0 | 0 | — |

全部 3099 个 frame 解码完成，无 unsupported/truncated 错误。扫描器逐 chunk 输出 resource index、frame index、opcode 和 chunk size，因此可定位任何新增格式；只要任一 FLC 解码失败，汇总中的 `failed_resources` 非零且进程返回失败。

回归锚点 `Data.mkf[559]`：34/34 帧完成；frame 0 位于 offset 2902、size 4968，三个 chunk 为 opcode `18,4,15`。其余 33 帧均为 opcode 7。此前失败来自首个合法 opcode 18 postage-stamp 附属 chunk。

实现策略：

- opcode 18 在 chunk size 已通过 frame 边界校验后跳过，不改变正式 framebuffer；decoder 记录 `last_skipped_opcode` 和累计数量。
- opcode 13 清屏到 palette index 0，并通过 `put_index` 标记整帧 dirty；合成 fixture 覆盖。
- opcode 11 与 opcode 4 分离，6-bit RGB 分量乘 4；V3.11 扫描未观察到 11，但有独立格式回归测试。
- 未知 opcode 仍返回 `RICH4_FLC_ERR_UNSUPPORTED_OPCODE`；非法/截断 chunk 仍失败。

测试：[`tests/resource/test_flc_animation.c`](../../tests/resource/test_flc_animation.c)。

持续门槛：`make test-original-data` 在本地存在 V3.11 商业资源时自动扫描 Data/Jump/Panel，任一 resource/frame 失败即使目标失败；资源不存在时明确 SKIP。

## 2026-07-13 复验

- Windows 强制重建并运行 `test_flc_animation`、`test_flc_player`：通过，且 `-Wall -Wextra` 无警告。
- Windows `test-original-data`：Data/Jump/Panel FLC 扫描与 Map 校验通过，输出 `original V3.11 resource scans: ok`。
- WSL `make test-sanitize`：全部 unit、resource、render 测试通过，无 ASan/UBSan 诊断。
