# R7.3 FLC renderer/player evidence

## Boundary

- `resource/flc_animation.[ch]` decodes FLC chunks into persistent indexed pixels, RGB565 pixels, and palette state. It no longer interprets palette index 0 as the game background.
- `resource/flc_player.[ch]` owns the current frame, decoded framebuffer/palette lifetime, header delay or an explicit caller override, clock deadline, sequential advance, and replay-based random seek.
- `view/flc_compositor.[ch]` applies the Rich4 call-layer rule that index 0 may take pixels from the saved source framebuffer. SDL only copies the composited RGB565 result to its destination surface.

## Regression coverage

`tests/resource/test_flc_player.c` builds a two-frame FLC whose second LC frame changes only the middle pixel. Seeking directly to frame 1 must replay frame 0 and produce red/green/red; this fails if prior pixel deltas are skipped. The same test covers repeated random seek, palette lifetime, header delay `77`, caller override `10`, player clock advancement, and source-framebuffer composition.

The runtime intro, landing, and dice drawing paths now obtain a complete frame from `Rich4FlcPlayer`; their former outer decode loops were removed. The dice call layer retains its evidenced speed override, while intro and landing use FLC header delays.

`mkf_scan_flc_opcodes` now decodes every detected FLC through `Rich4FlcPlayer::advance`. Therefore `make test-original-data` exercises the real intro/landing archives (`Jump.mkf` and `Data.mkf`) as continuous player streams, in addition to `Panel.mkf`; V3.11 scanning passes.

## Verification

```text
make -B test-resource sdl test-original-data
flc player/compositor: ok
original V3.11 resource scans: ok
```

