# platform/sdl1/render — 东西在哪

SDL1 表现层的绘制原语与（将来的）分场景渲染模块。

- `blit.c` / `blit.h` — 帧缓冲表面原语：`fill_rect`/`stroke_rect`、图块/精灵
  blit（`draw_chunk_*`、`draw_spr_chunk_*`、`draw_setup_chunk*`）、
  `draw_raw_rgb565_*`。只操作 `SDL_Surface` + 解码后的 SPR/SMP 字节，无游戏状态。

命名约定（R10.4）：分场景渲染文件用 `scene_<name>_render.c`，游戏子域用
`game_<domain>_render.c`。**禁止**屏上 DEBUG/占位 UI（`draw_debug_*`、
`*_placeholder_*`、ASCII `glyph_rows` 调试字体等），见
[docs/evidence/r10-5-debug-ui-removal.md](../../../../docs/evidence/r10-5-debug-ui-removal.md)。
