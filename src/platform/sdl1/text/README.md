# platform/sdl1/text — 东西在哪

去 Windows GDI 的统一字体后端。检索时优先看
[docs/evidence/r10-2-text-backend.md](../../../../docs/evidence/r10-2-text-backend.md)。

- `text_backend.h` / `text_backend.c` — 平台无关接口 `Rich4TextBackend`
  （`measure`/`render`/`shutdown`），面向裸 RGB565 帧缓冲 `Rich4TextTarget`。
- `text_backend_freetype.c` — 桌面/Windows 后端（SDL_ttf/FreeType + 字形缓存）。
- `big5_utf8.c` — 原版 charset 0x88（CP950）到 UTF-8 的跨平台映射，不依赖 iconv。
- `font_atlas.c` / `font_atlas.h` — 预烘焙位图字集格式（`R4FA`，A8）。
- `text_backend_atlas.c` — 掌机后端，消费预烘焙图集，运行时零字体引擎依赖。
- 排版规则复用上一级目录的 `text_rules.c`（原版 `0x44FABC` 语义）。
- 构建期烘焙工具：[tools/font_bake.c](../../../../tools/font_bake.c)。
