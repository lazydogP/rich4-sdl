# M2 non-visual closure

## Setup callback timing and interruption

- Original anchor: `0x404E44`; local reference `../rich4/asm/rich4_init_new_game_callback.asm`.
- Stage 1 increments its callback counter and compares the previous value with
  `10`, so auto-fill/transition work is due on the eleventh 100 ms callback.
- Stage 2 handles `WM_KEYDOWN`, `WM_LBUTTONUP`, and `WM_RBUTTONUP` by killing
  its timer and posting completion result `1`. The SDL path maps those to any
  key down and left/right mouse-button up; controller button down is the
  portable input equivalent.
- Implementation: `src/app/new_game_setup_flow.[ch]`, assembled by
  `src/platform/sdl1/runtime_main.c`.
- Test: `tests/unit/test_new_game_setup_flow.c` covers all eight background
  indices, all 36 setup-character resource combinations, the exact counter
  edge, natural completion, and every accepted SDL interruption family.

## First-turn dispatcher mapping

- Original anchor: `0x40C912` result consumed by the `0x418C55` first-landing
  path.
- M2 mapping is exhaustive: `0` action hint, `1` input refresh, `2/5`
  automatic pre-action, and `-1/3/4/>5` return-only. An automatic status block
  re-marks dirty; otherwise automatic flow enters preroll.
- Implementation: `src/core/first_turn_dispatch.[ch]`; the SDL runtime consumes
  the returned plan instead of duplicating the result switch.
- Test: `tests/unit/test_first_turn_dispatch.c` covers every named result
  family and both automatic status outcomes.

## Map fallback/lifetime and actor selection

- `tests/resource/test_map_data.c` covers MAPDAT-preferred and MAP fallback
  indices for all eight stage/map combinations plus corrupt range rejection.
- `tests/resource/test_map_data_fault_injection.c` covers every parser
  allocation failure and verifies empty/free-safe output.
- `tests/resource/test_resource_store.c` repeatedly opens, borrows, destroys,
  and recreates archive/cache lifetime groups; sanitizer builds use the same
  loop as the leak/use-after-free gate.
- `tests/unit/test_game_view_model.c` covers the confirmed actor selection
  precedence and combinations: frozen/hidden, sleepwalk, special road,
  delayed/disappearing/prison/hospital/sleeping, all four traffic methods, and
  method-3 overlay. Unconfirmed part semantics remain explicitly post-M2 and
  retain `partial` evidence confidence.
- `tests/unit/test_map_renderer.c` covers all four inclusive culling edges,
  both outside edges, and all 64 object variant/base rotation combinations.

## Boundary conclusion

All non-visual behavior named by the bounded M2 done boundaries now has an SDL
implementation and an automated test. Literal Win32 callback/global storage,
unconfirmed actor semantics, AVI decoding, status producers/effects, economic
handlers, special-player actions, and final victory UI remain outside M2.
