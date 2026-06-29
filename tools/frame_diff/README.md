# 原版观测工具：第一阶段

这是原版 Rich4 + DxWnd 的独立调试工具。它负责启动、原生时序观测、随机数轨迹和 DirectDraw 原始像素采集；不改 SDL 运行逻辑，不提供第二阶段的虚拟时间或双进程锁步。`OBSERVED` 表示本次记录通过已实现的完整性检查，不等于逐帧一致性通过。

## 构建与运行

在仓库根目录执行，需要 Windows、Python 3 和支持 `-m32` 的 MinGW GCC。本机验证使用 `D:\TDM-GCC-64\bin\gcc.exe`；可用 `R4DIFF_CC` 指定。NumPy 可选，用于加速 PNG 转换。

```powershell
$env:R4DIFF_CC = 'D:\TDM-GCC-64\bin\gcc.exe'
python scripts/make_minimal.py frame-diff test-frame-diff

python tools/frame_diff/r4diff.py probe `
  --data-dir assets/original `
  --dxwnd-dir C:/Users/lazydog/Downloads/dxwnd_v2_05_68_build `
  --out .local/frame-diff-runs/my-setup `
  --seconds 16 --scenario setup --capture full --set renderer0=5

python tools/frame_diff/r4diff.py report .local/frame-diff-runs/my-setup
```

输出目录必须不存在。工具复制游戏资源和 DxWnd 到输出目录，以仓库的 `RICH4.CFG` 为初始配置，记录运行前文件哈希。源游戏和源 DxWnd 配置不写入。脚本操作自己的游戏窗口，会暂时移动系统鼠标并请求焦点；运行时避免手动操作游戏。结束后恢复鼠标位置。

游戏 EXE 和 DxWnd DLL 使用 SHA-256 白名单，分别为：

```text
Rich4.exe  5a90aee28ee5f7a5c3ba5cb935c9e55751a529c25fcd91208748a66293569550
dxwnd.dll  b0dfd9b439595c2079b303fb15e7393b2ec22a00dfea7b24d3a4f86418b229e1
```

当前支持本机 DxWnd 2.05.68。其他版本需重新验证 hook 链，不能仅替换白名单。已有 `dxwnd.exe` 时拒绝启动，避免共享目标配置冲突。启动器在 EXE 入口断点暂停，依次装入 DxWnd、探针，再继续游戏；运行副本强制启用 `INJECTSUSPENDED`。游戏放入关闭即终止的 Job Object，启动、远程调用、退出均设超时，只终止本次创建的进程。

参数：

| 参数 | 用途 |
| --- | --- |
| `--scenario idle` | 不操作游戏，可手动测试 |
| `--scenario menu` | 跳过启动视频、移动光标、打开选项 |
| `--scenario setup` | 跳过启动视频、移动光标、进入新局设置 |
| `--scenario game` | 在 setup 后选择角色并确认，继续开场流程 |
| `--capture none` | 只记录事件/耗时，不读回画面；定位性能优先使用 |
| `--capture full` | 保存支持路径的全部主表面观测；用于图像检查 |
| `--capture sample --interval 200` | 减少完整表面快照；局部补丁仍保存，合成预览可能含过期区域 |
| `--seconds 1..300` | 原版恢复运行后的观测时长 |
| `--set renderer0=5` | 只修改运行副本的数值字段，可重复传入 |
| `--profile PATH` | 从指定 DxWnd 配置的 target 0 创建运行副本 |

输入脚本按实际时间发送消息，启动耗时和随机种子仍是原生值。它是场景触发工具，不能用于证明不同运行具有相同逻辑序列。`game` 不保证在固定秒数内进入地图；以输出画面为准。

## 证据与比较

每次运行生成：

| 文件 | 内容 |
| --- | --- |
| `manifest.json` | 输入资源、EXE/DLL/探针哈希、配置、参数、启动结果 |
| `launcher.log` | PID、装载/退出结果、脚本输入实际时间 |
| `modules.tsv` | 游戏进程模块路径和实际加载地址 |
| `events.csv` | QPC 微秒、耗时、线程、调用者 RVA、对象、返回值和参数 |
| `frame-*.r4f` | 16 位原始像素，含尺寸、mask、目标区域、表面地址和时间 |
| `frame-*.png` | 640×480 主表面的观测预览；不包含桌面缩放后的图像 |
| `summary.json` / `report.html` | 调用统计、错误、随机数校验、图像索引 |

读取 [本机测量记录](../../docs/evidence/original-probe-phase1-2026-10-22.md)。重复性能对照时，保持资源、时长、场景、探针相同，关闭像素采集，只改变 renderer，并按 3、5、5、3 顺序运行。随后：

```powershell
python tools/frame_diff/r4diff.py compare `
  .local/frame-diff-runs/ab-r3-01 .local/frame-diff-runs/ab-r5-01 `
  .local/frame-diff-runs/ab-r5-02 .local/frame-diff-runs/ab-r3-02 `
  --start 10 --end 16 --out .local/frame-diff-runs/comparison.json
```

该比较聚合新局设置路径的 BltFast、光标移除/重画 Unlock，以及取出的 WM_TIMER 消息间隔。它不是图像差异比较器。不同线程的重叠耗时不能相加；报告的 inclusive 与 exclusive 分开列出。零微秒调用计入样本数，p95 使用 nearest rank。

## Hook 范围与采集语义

原版函数依据见 [IDA 证据](../../docs/evidence/differential-testing-hook-points-ida.json)。所有补丁先核对入口字节或原始 IAT 操作数；原版文件不修改。

* 在游戏调用点拦截 `DirectDrawCreate`，保留 DxWnd 的 IAT；对接口 1 的共享 vtable 保存原始方法后包装 `CreateSurface`、`WaitForVerticalBlank`、`Lock/Unlock`、`Blt/BltFast/Flip`、`GetDC/ReleaseDC` 等。
* 在已验证的原版 CALL/JMP 操作数上接入 `GetTickCount`、`timeGetTime`、`timeSetEvent/timeKillEvent`、`SetTimer/KillTimer`、`PeekMessageA`。不替换游戏 IAT，避免 DxWnd 延迟重挂钩将探针当作原函数而递归。
* `0x456F2D rand`、`0x456F50 srand` 使用 x86 trampoline，记录线程、调用点、返回值和状态前后值。报告验证 `next = state * 1103515245 + 12345 (mod 2^32)`、`result = (next >> 16) & 32767`，检查每线程轨迹连续性及 reseed。
* 包装 `0x45285E`、`0x4528B9` 和 `0x4543C4` 的等待函数，保留实际时钟、消息消费和原有定时器线程。未经过的函数不会凭安装成功宣称已经动态验证。
* 时钟读数按线程、API、返回的毫秒合并；`b` 为距离上次输出的调用数。它适合观察忙等，不是逐次时钟回放日志；结束时尚未变化的尾部计数不输出。

已有主表面 Lock 时，在原版 Unlock 前拷贝原始像素。成功的无色键 BltFast 从已知源表面读回对应矩形，重建目的表面；这样避免为每次 BltFast 额外 Lock/Unlock 主表面。其他 Blt、Flip、主表面 ReleaseDC 使用额外主表面读回，并记录 `surface` 事件（`a=99`）；BltFast 源读回为 `a=98`。这些额外调用有开销，所以性能结论必须用 `capture none` 对照。

这不是逻辑帧边界。一次画面更新可能包含光标擦除、中间 blit、光标重画，多线程原生调度也可能穿插。`full` 只表示不主动抽样；`sample` 合成结果不能用于像素判等。现阶段只支持正 pitch、16 位 RGB、640×480 主画布，其他格式、色键、未知接口产生 coverage error。AVI/独立视频窗口不承诺捕获。

探针使用固定事件队列、8 个帧槽和异步文件线程。溢出、缺失 END、启动不完整、帧文件/事件数量不符、图形 HRESULT 失败、RNG 校验失败会标记 `INVALID_CAPTURE`，`probe/report` 命令返回 7；启动器错误使用其他非零退出码。这类运行仍可保留计时证据，但不能作为有效像素基准。未知接口和容量上限不属于受支持测试路径（128 个累计表面记录、64 次多媒体 timer 注册）；当前验证场景未触及上限。

上述观察模式没有完整记录调色板、DirectDraw 新接口、未列入的输入 API 或所有系统时间源，也不为原版线程强制排序。第二阶段锁步使用下面的独立入口，不能把观察模式的 QPC 时间戳当作同步依据。

## 第二阶段锁步入口

双端虚拟时钟、输入、随机轨迹与画面对比使用 `lockstep.py`，构建为 `frame-diff frame-diff-sdl`。具体命令、同步契约、报告与支持范围见 [LOCKSTEP.md](LOCKSTEP.md)。第一阶段 `r4diff.py probe` 继续保留真实时钟诊断语义。

## CSV 与原始帧格式

CSV 表头：`us,duration_us,type,thread,caller,object,result,a,b,c,d`。`caller/object` 为十六进制，其余十进制。类型编号在 `probe.h`，名称在 `r4diff.py`。`rng/seed` 的 `a/b/c` 是状态前值、后值、入口参数；`wait.object` 为等待类型 2/3/4，`wait.a` 为入口参数。`timer_set` 的 `a` 是请求间隔；通过 caller 区分 WinMM 和窗口 timer，不能用 `d` 单独区分。

`.r4f` 头部为 16 个 little-endian uint32，共 64 字节：magic `0x31463452`、序号、时间低/高位、宽、高、bpp、有效行字节数、R/G/B mask、目标表面地址、事件类型、目标 x/y、线程号。后面紧跟去除 pitch padding 的像素行。PNG 只按 mask 位移扩展，例如 RGB565 最大值为 `(248,252,248)`，原始哈希不依赖 PNG。

探针 error 的主要编号：20/21 像素格式/尺寸，30 timer 类型/容量，40 内部调用深度，41 入口字节，42 操作数 hook 被覆盖，43 CALL/JMP 不匹配，50 主表面读回，51/53 未支持的 QI，52 表面容量，54 多个 DD 对象，55/56 图形 hook 被覆盖，57 DirectDrawCreate 字节，58 vtable 写入，59 不支持的 BltFast 源/flags，60 源表面读回。完整错误与负 HRESULT 在 JSON 和 HTML 中保留。
