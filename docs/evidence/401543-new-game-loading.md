# 新局退场与 loading 的边界

原版 V3.11，2026-10-15 用 IDA MCP 复核 `0x404E44`、`0x401543`、
`0x454EDC`；调用顺序另见 `../rich4/asm/rich4.asm` 的
`0x401CC8..0x401CF0` 和 `../rich4/asm/rich4_init_new_game.asm`。

1. `0x406DE7` 进入选人回调 `0x404E44`。回调状态 `n2_2 == 2`
   以 50 ms 定时器更新面板/人物的退场坐标，继续绘制选人资源。
   人物离开后投递键盘消息结束回调；左/右鼠标松开或按键也能结束。
2. 回调结束后 `0x406DE7` 关闭 JUMP archive，完成新局状态初始化，
   在 `0x4074C9` 起释放选人临时对象并返回。
3. `WinMain` 在 `0x401CDC` 调用 `0x401543`。该函数通过
   `0x48A0E4`（Data.mkf handle）读取资源 `601`，复制 614400 字节到
   后备画面，解锁并提交显示，释放读出的临时内存。
4. `0x401543` 最后调用 `0x454EDC`，后者停止当前音乐并清除音乐状态；
   两者均没有固定 sleep。
5. `WinMain` 然后调用 `0x407AD2` 加载地图，再执行 `0x4190CF`、
   `0x4291D6` 初始化，最后执行 `0x415872` 的开局演出。

SDL 中 `RICH4_SCENE_INTRO` 指选人面板退场；真正的地图开局演出是
游戏场景中的 `session.presentation.intro`。退场期间资源范围仍是选人，
完成回调先提交 Data[601]，然后装载游戏资源并初始化 session。
原版开局演出资源的索引和释放边界见 [0x415872](415872-intro.md)。

回归入口：`python scripts/make_minimal.py test-new-game-loading` 和
`python scripts/make_minimal.py test-miyoo-qemu --progress`。QEMU 从标题
通过物理 A 键选择四个人物并点击 OK，保留退场截图，检查 loading 提交
先于游戏资源读取，且退场期间无游戏资源读取，最后到达地图场景。
