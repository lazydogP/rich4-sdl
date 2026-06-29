# R10.2 统一字体后端抽象（去 GDI）

## 背景

原版 Rich4 的文本绘制（`0x44F9D8` 建字体、`0x44FABC` 排版、`0x44FA74`
调 `CreateFontA`）依赖 Windows GDI 系统 TrueType（「細明體」/PMingLiU）与
Big5 编码，通过 512×200 的 16bpp DirectDraw 位图光栅化后按「非零像素即文字」
拷回。移植到 SDL1 后，Windows 运行时仍走 GDI，非 Windows 走 SDL_ttf，无字体
时回退 7×5 ASCII 点阵（中文乱码）。

R10.2 的目标是去掉 GDI 依赖、抽象平台无关的字体后端，并为掌机准备零字体引擎
的预烘焙位图路径。**这属于移植层的表现实现，不改变任何已确认的原版领域行为**：
排版规则（粗体/阴影/描边/锚点/竖排步长）仍复用 `text_rules.c`
（`0x44FABC` 语义，见 `docs/evidence/44f9d8-44fabc-text-rendering.md`）。

## 变更

- `src/platform/sdl1/text/text_backend.h`：平台无关接口 `Rich4TextBackend`
  （`measure` / `render` / `shutdown`），面向裸 RGB565 帧缓冲描述
  `Rich4TextTarget` 而非 `SDL_Surface`，以便未来非 SDL 掌机目标复用。
- `text_backend_freetype.c`：桌面/Windows 后端，基于 SDL_ttf(FreeType)。
  `TTF_RenderUTF8_Blended` 逐字形合成到 RGB565；按像素高度缓存 `TTF_Font`
  句柄（上限 128 档）限制内存与重复打开成本；Big5→UTF-8 走捆绑的 CP950
  表（`big5_utf8.c`，对应原版 `0x44FA74` charset `0x88`），不依赖 SDL_iconv
  或系统码页 API。
  **不触碰任何 OS 字体 API（无 GDI）**。缺 SDL_ttf 时退化为返回 0 的桩，
  由调用方回退其它后端。
- `font_atlas.{h,c}`：紧凑、显式小端、无引擎依赖的预烘焙位图字集格式
  （magic `R4FA`，A8 覆盖度），供 baker/掌机后端/测试共用。
- `text_backend_atlas.c`：掌机后端，消费预烘焙图集，运行时**零字体引擎依赖**，
  排版仍复用 `text_rules`。
- `tools/font_bake.c`：构建期工具。扫描给定 Big5 文本源（如
  `src/data/special_event_text.c`、`src/data/character_speech.c` 及导出的
  MKF 文本）收集 ASCII 与 DBCS 码点，用授权 CJK TrueType 经 SDL_ttf 光栅化，
  产出上述图集。
- `runtime_main.c`：`draw_big5_text_with_spacing` 的 GDI 分支与
  `rich4_gdi_draw_text` 均改置于 `#if defined(_WIN32) && defined(RICH4_TEXT_GDI)`
  之后；默认所有平台（含 Windows）走 SDL_ttf/FreeType 路径。GDI 宏保留一个
  发布周期用于跨平台中文观感校验，`gdi32` 仅作为 SDL1 自身链接依赖保留。

## 字符集与字体授权证据（R10.2c）

- **字符集来源**：`tools/font_bake` 的码点集合完全来自命令行给定的游戏文本源
  文件，不臆造字符。示例（本机验证）：
  `font_bake <font> 16 build/rich4_font.r4atlas src/data/special_event_text.c src/data/character_speech.c`
  → 91 个字形、12427 字节 A8 覆盖度、13903 字节图集。
- **字体授权**：仓库**不**内置任何 TrueType 字体。桌面后端在运行时按序查找
  `assets/fonts/rich4.ttf|ttc` 等用户自备字体，或系统 CJK 字体路径；
  掌机图集须由使用者用**自有授权**的 CJK 字体经 `font_bake` 生成。原版
  「細明體」为 Windows 系统字体，不随本项目分发。发布打包时选用的具体字体及其
  授权须在此文件补记。

## 测试

- `tests/unit/test_text_backend.c`：接口分派与空指针安全（`test_text_backend`）。
- `tests/unit/test_font_atlas.c`：图集序列化/解析/二分查找 + 掌机后端渲染的
  golden 像素断言（`test_font_atlas`），无需真实字体。
- `text_rules` 既有 `tests/unit/test_sdl1_text_rules.c` 继续守卫排版语义。

## 验证

- `make sdl`（默认，去 GDI）与 `-DRICH4_TEXT_GDI`（遗留 GDI 路径）均编译通过。
- `make test-replay test-render` 全绿（均为无头/视图层，不含 GDI 文本像素 golden，
  故去 GDI 不影响既有回归）。
