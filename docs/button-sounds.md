# Button sound behavior

This note records the Rich 4 button sound calls that are relevant to the SDL
port's currently implemented menu and new-game setup screens.

## Sound entries

Original code loads button sounds from `Effect.mkf` through `sub_4542CE`.
The SDL port now reads those same MKF resources directly; `Instsel.wav` and
`Instok.wav` are different files and are not substitutes for these entries.

| Original entry | Effect index | Meaning |
| --- | ---: | --- |
| `ref_0048231a` | 0 | selection / hover-change sound |
| `ref_00482322` (`snd0`) | 1 | OK / accepted action sound |
| `ref_00482332` | 4 | NO / cancel sound |
| `dword_46CCD0` | 5 | selected-character outbound transition |
| `ref_0048233a` (`snd1`) | 3 | result / card-tool effect sound |
| `ref_0048234a` | 7 | bank keypad / exit / confirm key sound |
| `ref_00482352` | 9 | bank amount-bar sound |

References:

- `rich4/csrc/main.c`: main menu callback.
- `rich4/asm/rich4_ui_main.asm`: same main menu callback in assembly.
- `rich4/asm/rich4_init_new_game_callback.asm`: new-game setup callback.
- `rich4/csrc/cards_ui.c`: names `snd0 = {1, NULL}` and `snd1 = {3, NULL}`.

## Main menu

The original menu has five button hotspots: start, load, options, exit, and
new stage.

- `WM_MOUSEMOVE`: when the cursor enters a button or moves from one button to a
  different button, play `ref_0048231a` (`Effect.mkf[0]`). Moving within the same
  button does not replay the sound. Moving outside buttons does not play sound.
- `WM_LBUTTONDOWN`: if the current button status is valid, play
  `ref_00482322` (`Effect.mkf[1]`) before dispatching the button action. Clicking
  outside buttons does not play sound.

## New-Game Setup

The setup callback divides controls into character portraits, OK/BACK buttons,
six setting buttons, four map buttons, and an expanded setting dropdown.

- Character portrait hover (`WM_MOUSEMOVE` over the portrait grid): play
  `ref_0048231a` only when the hover character changes and that character is
  not already selected. Hovering a selected character, moving within the same
  character, or moving outside the grid does not play sound.
- Character portrait click (`WM_LBUTTONDOWN`): selecting or unselecting a valid
  hovered character plays `ref_00482322`.
- OK and BACK buttons (`WM_LBUTTONDOWN`): the cases share code that indexes
  `ref_0048231a + 8 * (control + 1)`. Therefore OK uses
  `ref_0048232a` (`Effect.mkf[2]`) and BACK/EXIT uses
  `ref_00482332` (`Effect.mkf[4]`). OK may then start the auto-fill flow or
  finish setup; BACK exits the screen.
- Setting buttons (`WM_LBUTTONDOWN` on the six setup values): pressing a
  setting button plays `ref_0048231a` and opens the dropdown.
- Expanded dropdown hover (`WM_MOUSEMOVE` while a dropdown is open): changing
  the highlighted option does not call `fcn_004542ce`; it only updates the
  highlight.
- Expanded dropdown close (`WM_LBUTTONUP`): choosing or dismissing an option
  does not play a button sound in the callback.
- Map buttons (`WM_LBUTTONDOWN`): changing to a different map plays
  `ref_00482322`. Clicking the already selected map does not play sound.
- Auto-fill AI during OK flow: each random AI character that the timer selects
  plays `ref_00482322`.
- Once the selected characters have assembled and setup timer state 2 begins,
  `0x405D08` calls `sub_4542CE(dword_46CCD0, 0)`. The first field of that
  sound record is 5, so the outbound transition plays `Effect.mkf[5]` once.
  It is not an extra OK sound and is not repeated on each transition frame.

## Options

- The bottom Cancel button uses `ref_00482332` (`Effect.mkf[4]`).
- The bottom OK button uses `ref_0048232a` (`Effect.mkf[2]`).
- Closing the panel through the right-button/cancel-message path does not add a
  button sound.

## YES / NO and bank panels

- `sub_43FAE4` plays `Effect.mkf[0]` when hover changes to either YES or NO.
- Pressing YES plays `Effect.mkf[1]`; pressing NO plays `Effect.mkf[4]`.
- Route-action-14 callback `sub_436EF8` uses `Effect.mkf[1]` for withdraw and
  deposit, `Effect.mkf[7]` for keypad/exit/confirm, and `Effect.mkf[9]` for the
  amount bar. Valid controls draw chunk `control + 1`; keypad/exit/confirm keep
  that pressed frame until button-up. Right-button-up closes without a sound.

## In-game top-toolbar map button

- `sub_417E26` plays `Effect.mkf[0]` when the pointer changes to the map
  toolbar slot. Moving inside the same slot does not replay it.
- Left-button-down on the map slot plays `Effect.mkf[1]` and stores toolbar
  command 5. Left-button-up dispatches the stored command and opens
  `sub_40A9BD`; opening does not play a second sound.
- The full-map callback `sub_40A801` ignores left-button input. Right-button-up
  closes the modal without any effect call, so there is no cancel/close sound.
- Full resource and UI evidence is recorded in
  `docs/evidence/40a9bd-small-map-ui.md`.
