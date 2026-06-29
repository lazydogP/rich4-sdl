# R8.2 audio event boundary evidence

`app/audio_event.[ch]` defines an SDL-free FIFO for scene/domain audio events. Dice, jump, door, and movement state carry semantic identifiers and small parameters; no mixer handle, channel, file path, or SDL type crosses this boundary.

After `update_game_session`, `main_sdl.c` converts the existing pending animation signals and current movement state into queue events. It then drains the queue through `rich4_audio_consume_event`. There are no remaining direct dice/jump/door/movement playback calls in `main_sdl.c`, `core/`, or `app/`. The SDL_mixer implementation remains responsible for mapping semantic events to loaded chunks and movement channels.

The queue has a documented fixed capacity of 32 and fail-visible `dropped` count. `tests/unit/test_audio_event.c` verifies FIFO order, payload preservation, wrap-safe draining, full-queue rejection, dropped accounting, and invalid event rejection.

Menu feedback and music file selection remain platform/application I/O; they do not originate in core and do not expose SDL_mixer to scene or domain modules.

## 水路移动音效

`0x40D9C5` 在普通交通工具音效表 11..14 之外，为特殊道路状态固定选择
槽 15；`sub_40B93B` 在当前路线记录 `+39` 为负数（SDL 对应 route flags
高位 `0x80000000`）时设置该状态并播放 `0x4823C2`。该音效记录的资源号是
`Effect.MKF[47]`，而不是当前交通工具的 44/45/46/53。

SDL 因此把水路作为第五种语义移动音效传入音频层；普通路线仍按交通工具
0..3 选择，水路不再被 `& 3` 截回步行音效。

## 循环与停止时机（IDA 复核）

- `0x4542CE -> 0x4540D8` 先将播放位置设为 0，然后把 flags 原样传给
  DirectSound Play。移动资源 44/45/46/53/47 与机器娃娃 38 均传 1，
  SDL_mixer 对应 loops=-1；没有按交通类型设置额外重复间隔或播放频率。
- `0x40DA28 / 0x40DE04` 为携带者同时启动 `Effect[32]` 的独立循环缓冲区。
  `0x41B6C4` 每到节点递减数字，但不重新播放滴答声。
- `0x40D8EA / 0x40D90E` 在自然结束时停止两个缓冲区；`0x41B7E7`
  转交炸弹只停滴答，`0x41B6D8` 在爆炸表现之前停止滴答。
- 路障 `0x41BD08` 先停移动声并清零步数；下一次 `0x40D7C4` 才走
  `0x40D90E` 停滴答。SDL 的 FORCED_STOP 与后续五 tick 停留阶段保留这个差别。
- 单格 `actor.move_active` 归零不等于整个移动流程结束；ARRIVAL 阶段必须保持
  循环，否则每个格子边界都会重启样本。银行暂停则停止两个循环，恢复后从头播放。

`test-audio-loops` 覆盖真实移动流程中的转交、爆炸、路障和自然停止，并用 SDL
dummy 音频设备验证五种移动样本及炸弹超过样本时长仍在循环、两声独立停止，
以及魔法屋 `Effect[59]` 单次播放。魔法屋原版调用点为 `0x432A6A`。
