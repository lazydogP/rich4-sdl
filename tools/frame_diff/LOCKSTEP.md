# 第二阶段：双端锁步与画面对比

这是显式启用的测试适配器。构建和运行都在隔离目录，原版文件不修改，正常 SDL 构建不包含控制通道。仅设置 `R4DIFF_LINK` 的测试实例接入控制器。

## 运行

在仓库根目录执行，Python 需安装 NumPy，编译器沿用第一阶段的 x86 MinGW 和项目 x64 SDL 工具链：

```powershell
python scripts/make_minimal.py frame-diff frame-diff-sdl test-frame-diff
python tools/frame_diff/lockstep.py --data-dir assets/original --dxwnd-dir C:/Users/lazydog/Downloads/dxwnd_v2_05_68_build --out .local/frame-diff-runs/my-setup --scenario setup --duration-ms 1200
```

输出目录必须不存在。场景有 `menu`、`options`、`setup`、`newgame`。`newgame` 包含选择一名人物并确认，实际会检测到随机调用时刻差异。退出码 0 表示所测检查点通过，2 表示差异或失败，细分原因见 `result.json`。工具不据此声称整局行为一致。

`--script file.json` 可替代内置输入。文件为数组，每项指定 `ms`、`kind`，鼠标指定逻辑 `x/y`，键盘指定 Windows 虚拟键 `key`。kind 为移动 1、左键按下 2、左键释放 3、按键按下 4、按键释放 5。时间严格递增，一个时间戳最多一个输入；0 ms 留给启动屏障。支持方向键、A–Z、数字、Enter、Esc、空格、退格和 Tab。例子见 [keyboard-wrap.json](scenarios/keyboard-wrap.json)。同刻多个输入和右键/滚轮尚未实现，输入校验会拒绝。

```powershell
python tools/frame_diff/lockstep.py --data-dir assets/original --dxwnd-dir C:/Users/lazydog/Downloads/dxwnd_v2_05_68_build --out .local/frame-diff-runs/my-wrap --script tools/frame_diff/scenarios/keyboard-wrap.json --origin 0xffffff00 --duration-ms 400
```

`--origin` 指定双方初始 32 位 uptime，因此也控制启动种子。日历固定 2000-01-01，从零秒起算；单次虚拟时长限定 1–20000 ms。原版初始化保留 `OVER.AVI` 光盘检查，省略启动 AVI，双方配置关闭音乐和音效。这是测试 fixture，不能当作音频/视频验证。原版 `0x4502FE` 独占打开 MKF，所以双方使用内容相同的独立文件副本，不能共享文件或硬链接。

## 同步契约

* 协议见 `lockstep_wire.h`：固定 uint32、小端序、256 字节头、最多 4096 条轨迹、最新 640×480 RGB16 画布、32 个完整绘制提交槽。没有跨进程本地指针。溢出和无效格式停止采集，不丢帧继续判等。
* 每条 STEP 指定递增 epoch 和绝对虚拟毫秒。`GetTickCount/timeGetTime` 及 SDL 游戏时钟只读 `origin + ms`，读取次数、绘制耗时、控制器编码 PNG 和 IPC 等待不会推进时间。QPC 仍用于真实耗时诊断。
* 原版在已验证消息泵的空队列处交还控制，允许空 Peek 的后继逻辑执行一次。控制器取双方下一截止时间、下一输入时间和结束时间的最小值推进，最大空闲步长 20 ms。双方响应必须匹配控制器发出的 epoch、毫秒和角色。
* 原版 WinMM 回调在独立工作线程执行；主线程等待回调完成，之后派发窗口 timer。窗口 timer 保留替换、取消、至少 10 ms 间隔及队列合并语义。输入先入虚拟队列，再执行到期 WinMM 回调，再入队窗口 timer。同一时间的执行顺序固定；这是受控测试调度，不模拟 Windows 所有抢占排列。
* 原版虚拟键经过原有 WH_KEYBOARD 回调，虚拟鼠标用 640×480 坐标直达游戏 WndProc，避免 DxWnd 再次缩放。丢弃宿主鼠标、键盘和 timer 消息；SDL 使用脚本事件和受控按键状态。原版原生 paint/lifecycle 仍用于窗口维护：测试期间不要调整/遮挡窗口来触发额外重绘；任意宿主窗口事件扰动尚不属于确定性保证。
* 双方正常执行原有 LCG。记录 seed/rand 的前后状态、结果、虚拟时刻与逻辑线程，逐条核对，另行校验原版公式。原版保存调用 RVA；SDL 当前保存统一观察接缝 site=0。只有首次播种前的未规定初值不比较；之后状态不强制修正，额外调用也不会跳过。

## 采集与判定

原版成功更新主表面的 Blt/BltFast/Flip/Unlock/ReleaseDC 后读取真实 RGB16 表面；SDL 在 `rich4_sdl1_present` 转换为显示格式之前采集同一逻辑画布。所有同刻提交保存为 `original|sdl/NNNNN-submit-NN.rgb565`，最终屏障画面保存为 `NNNNN.rgb565`，格式和每帧哈希记录在 JSON。

比较器按相同虚拟时刻的最终画面比较，无像素容差，不错帧搜索、不自动同步 RNG。报告含改变像素数、包围框、原图、SDL 图、差异热图和透明叠加滑块。原版多个局部提交与 SDL 一次 present 不做强行一一对应；同刻短暂中间画面的跨端等价性尚未判定，但原始提交全部保留，重复运行检查包含它们。

判定顺序：采集/协议有效性 → 输入回执 → RNG 轨迹 → 已验证场景 ID → 像素。像素差异可以继续收集；结构差异默认停止。`--continue-after-diff` 只用于诊断，会保留首个结构差异，绝不恢复 PASS。不同场景、玩家/动画等完整状态导出尚未实现，当前状态比较只是场景 ID。

`UNSUPPORTED` 表示未知消息泵回调、未实现 timer 类型、嵌套同步 SDL delay 等覆盖边界。`INVALID_CAPTURE` 表示损坏、溢出、失败的图形调用等；`HANG` 是 15 秒内无安全点，`CRASH` 是进程意外结束。端内等待上限 30 秒，回调 5 秒，原版启动器有 300 秒总看门狗；停止时只回收本轮创建的进程。遇到未知路径保留最后快照，不把冻结时钟下的忙等判为通过。

当前原版已验证的场景回调：菜单 `0x40257A`、选项 `0x4103A3`、新局设置 `0x404E44`。整局、嵌套子面板、音频完成、视频、任意忙等和事务 RNG 提交/回滚等仍需扩展取证与适配；`strict_equivalence` 因此始终为 false。

## 验证工具

对相同场景分别指定 `--delay-ms 0`、`50`、`200`，控制器会在给原版发命令后延迟给 SDL 发同一命令。比较每端自身的轨迹和原始提交，不能拿两端本来就不同的画面作为重复性基准：

```powershell
python tools/frame_diff/repeat.py .local/frame-diff-runs/phase2-setup-delay0 .local/frame-diff-runs/phase2-setup-delay50 .local/frame-diff-runs/phase2-setup-delay200 --out .local/frame-diff-runs/phase2-repeat.json
python scripts/make_minimal.py test-frame-diff test-frame-pacing test-frame-diff-inactive
```

重复性检查校验原始文件长度/哈希、二进制/资源、脚本、时间起点、归一化 DxWnd 配置、轨迹及所有中间提交。`test-frame-diff-inactive` 对比普通构建和已编译但未连接控制器的 SDL 构建。2026-10-22 实测与已知差异见 [验证记录](../../docs/evidence/lockstep-phase2-2026-10-22.md)。
