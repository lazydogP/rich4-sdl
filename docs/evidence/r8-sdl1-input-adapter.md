# R8.2 SDL1 input adapter evidence

`platform/sdl1/input_adapter.[ch]` defines the platform pointer command boundary and the canonical 640×480 internal coordinate space. Window coordinates are scaled before entering application hit testing; negative and out-of-window values are clamped, and invalid window extents are rejected.

SDL mouse motion, button-down, and button-up paths in `platform/sdl1/runtime_main.c` call the adapter with the event's absolute `x/y` coordinates and consume `Rich4AppCommand`. Motion does not accumulate `xrel/yrel`, so focus and pointer-enter transitions cannot make the virtual cursor drift away from the host pointer. Keyboard, controller, and application-directed virtual-cursor movement is mirrored back with `SDL_WarpMouse` only while SDL reports both mouse focus and input focus; no host pointer position is written while the pointer is outside the window or the window lacks focus.

`tests/unit/test_sdl1_input_adapter.c` covers 1280×720 → 640×480 scaling, native-size identity, both boundaries, button preservation, invalid dimensions, and null output.
