# config

Configuration files start in M1 with a small input mapping file.

`input.toml` is a simple TOML-like key/value file parsed by the SDL
validation program. It currently controls keyboard cursor movement,
confirm/cancel keys, joystick confirm/cancel buttons, and virtual cursor
speed.

The default SDL command reads `config/input.toml`. Use this option to test a
different file:

```sh
bin/rich4_sdl --data-dir assets/original --input-config config/input.toml
```

## `RICH4-SDL.CFG`

这是 SDL 客户端专用配置，与原版形状的 `RICH4.CFG` 分开保存。当前的
`edge_scroll_inset` 控制地图目标选择时的边缘滚屏触发带：

```text
edge_scroll_inset=16
```

数值单位是输入适配后的 640×480 逻辑像素，默认 `16`，范围 `0..200`。设为
`0` 恢复原版的精确边界比较。该配置只对可移动视口的地图目标阶段生效；股票、
设施类型、抢夺卡二级面板和其他固定选择阶段不使用内缩带。相机已经到达地图世界
边界时，内缩带不会阻止目标点击。
