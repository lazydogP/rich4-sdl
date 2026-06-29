# Original AI, bank, sound, minimap and cursor behavior

This note records the evidence used to align the SDL implementation. Addresses
refer to the original executable loaded in IDA Pro; reconstruction labels refer
to files under `rich4/asm`.

## Managed-AI panel

- Entry: `sub_41E345`; callback: `sub_41DDA9`; redraw: `sub_41DB91`; ratio meter:
  `sub_41DA61`. The matching reconstruction is
  `rich4/asm/rich4_ui_ai_settings.asm`.
- The entry loads `Panel.mkf[77]` through the panel archive handle at
  `0x48A05C`. Redraw places its chunk 0 at `(102,62)`.
- The four player hit rows are `x=110..226`, starting at `y=70`, with an
  83-pixel stride. Clicking the selected row toggles managed/unmanaged; clicking
  another row selects it.
- Controls toggle two action bits, choose one of three personalities, adjust the
  cash and stock ratios in ten-percent steps, or drag either ten-segment meter.
- OK copies all five fields back to every player record; Cancel closes without
  committing. Original OK/Cancel hit rectangles are `(479,159)-(519,215)` and
  `(479,247)-(519,303)`.
- Active-player portraits come from each character resource, chunk 12. The two
  meters start at `x=311`, use ten segments with an eight-pixel stride, and are
  drawn at `y=328` and `y=361`.
- The callback contains no `sub_4542CE` call, so internal AI-panel controls do
  not invent button sounds.

## Bank pass/stop timing and panels

- Route action 14 enters `sub_4379C9`, which loads `Panel.mkf[24]` and runs the
  modal callback `sub_436EF8`. The route dispatcher remains blocked until that
  callback returns.
- A bank arrival does not replace that pass panel with the loan page. In
  `sub_41982D`, action 14 first calls `sub_4379C9`; after it returns, and only
  when `byte_46CAF8 == 0`, the same branch calls `sub_436668(raw_object_id)`.
  Thus passing a bank is transfer-only, while stopping on one is ordered as
  transfer first, then the loan/repayment presentation.
- `sub_436EF8` uses the 18 rectangles at `0x475888`: withdraw, deposit, exit,
  amount bar, digits, clear, zero, backspace, maximum, and confirm. The third
  top control is exit, not another account mode.
- A valid mouse-down draws chunk `control + 1`. Withdraw/deposit and the amount
  bar update immediately; keypad, exit, and confirm remain pressed until
  button-up, when their actions run and the background is restored.
- Withdraw/deposit use `ref_482322` (`Effect.mkf[1]`), ordinary bank keys use
  `ref_48234A` (`Effect.mkf[7]`), and the amount bar uses `ref_482352`
  (`Effect.mkf[9]`). Right-button-up closes this panel without a sound call.
- Route action 14 reaches the `0x419884` sound-table lookup only on STOP:
  `byte_475299[14]` is slot 10, whose record contains `Effect.mkf[43]`.
  Passing a bank performs the transfer panel without this entry sound; stopping
  plays `Effect.mkf[43]` once before the ordered transfer/loan presentation,
  and the loan page does not replay it.
- `sub_436668`/`sub_435062` and `sub_43695E`/`sub_436034` are a separate bank
  presentation using `Panel.mkf[23]` and `Panel.mkf[2]` through the handle at
  `0x48A05C`; those resources must not be composited behind the
  `Panel.mkf[24]` transfer panel.
- The original panel contains its own labels and numeric graphics. English
  diagnostics such as `SPECIAL`, `MAX`, or `$cash/$bank/$loan` are not original
  overlays and must not be drawn.

## YES/NO and general button sounds

- In `sub_43FAE4`, entering or changing either YES/NO hover target calls
  `sub_4542CE(ref_48231A,0)`, which maps to `Effect.mkf[0]`.
- YES press uses `ref_482322` / `Effect.mkf[1]`; NO press uses `ref_482332` /
  `Effect.mkf[4]`.
- The original sound structures at `0x48231A`, `0x482322`, `0x48232A`,
  `0x482332`, and `0x48233A` map to Effect resources 0, 1, 2, 4, and 3.
- Hash/size comparison shows `Instsel.wav` and `Instok.wav` are not the same
  audio as Effect resources 0 and 1. SDL therefore loads UI sounds directly
  from `Effect.mkf`.

## Minimap icon and virtual cursor

- The 400x400 modal overview uses character-resource chunk 6 (one-based,
  SDL index 5); the 200x200 sidebar minimap uses the smaller chunk 7
  (one-based, SDL index 6). See `docs/evidence/map-character-icon-sizes.md`.
- `sub_402460(1)` enables and draws the 32x32 software cursor through
  `sub_402250`; `sub_402460(0)` removes it through `sub_40235D`.
- The main game callback hides it at `0x4182D9..0x4182DE` before entering the
  dice/automatic-action path. Human-control case 1 restores it at
  `0x418D99..0x418DB9`, first moving it to the action area with `SetCursorPos`.
- `sub_419703` clears `byte_46CAFD` as the action starts; the main mouse-button
  path checks that flag at `0x418151` and ignores UI hits while it is clear.
  `sub_4196F1` sets it again when human control resumes.
- The executable imports `ShowCursor` and `SetCursorPos`, but not `ClipCursor`,
  `SetCapture`, or `ReleaseCapture`: the pointer is hidden and automatic-turn
  UI input is gated, not physically confined to a screen rectangle.
- Modal interactive callbacks (AI panel, bank, YES/NO) enable the cursor while
  waiting for input and disable it when they close. Automatic turn phases such
  as dice animation, route movement, and noninteractive presentation execute
  with it disabled. SDL mirrors this by hiding the software cursor during an
  active automatic turn flow, while keeping it visible for route interactions.
