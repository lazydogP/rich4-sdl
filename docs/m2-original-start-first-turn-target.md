# M2 Original Start And First Turn Target

This document records the original Rich 4 behavior that M2 should match before
and during the first turn. It is based on IDA Pro MCP analysis of
`Rich4.exe.i64` plus the local `rich4/` decompilation. Do not treat uncertain
items as implemented behavior.

## Goal Boundaries And Progress Slices

Use this file as a bounded target, not as an open-ended research goal. Each work
session must pick exactly one progress slice below, complete its acceptance
checks, and stop. If a finding belongs to a later slice or to the out-of-M2
queue, record it without expanding the current slice.

Progress rules:

- Do not treat every `[~]` or `[ ]` item in this document as part of the current
  goal.
- Do not chase newly discovered event-handler families unless the active slice
  names that handler family.
- A slice is complete when its "done boundary" is true and its "stop boundary"
  has not been crossed.
- If code already satisfies a slice, the valid result is a short evidence update
  or status correction, not more implementation.

| Slice | Goal | Done boundary | Stop boundary |
| --- | --- | --- | --- |
| P0 | Evidence map only | Addresses, local references, and required order are recorded or corrected. | No SDL behavior is changed. |
| P1 | Setup and game-state commit | Setup screen resources, selection/autofill/exit, returning setup reconstruction, and committed player/fund/stock/session defaults match the SDL acceptance items. | Stop before map resource loading. |
| P2 | Map resource load/lifetime | `MAP.MKF`, `MAPDAT.MKF`, common/view/overlay/stage/Data resources, fallback order, and cleanup buckets are implemented and listed. | Stop before visible-object construction changes. |
| P3 | Visible map object drawing | Land, facility, listed-special, and second-special display rows load resources, queue visible objects, sort, cull, apply owner color, and draw with original ids/variants. | Stop before pass/stop event effects. |
| P4 | Intro, landing, actors, Autodesk Animator FLC | Post-map `JUMP.MKF` intro, skipped AVI dispatch record, `Data.mkf[559 + character]` landing, actor resource families, and Autodesk Animator FLC header timing are handled. | Stop before first-turn dispatch decisions. |
| P5 | First-turn dispatch stop points | Dirty-bit entry, landing consumption, `sub_40C912(0)` result shape, human dice/action stop, AI pre-action audit, and status re-mark guards are represented. | Stop before executing card/tool/sale/status-event effects. |
| P6 | Movement route readiness | Dice/move can produce branch choices, ordered pass candidates, stop candidate, and current route-event snapshot with handler family marked deferred. | Stop before applying pass/stop economic or UI effects. |
| P7 | Rotation/day/win readiness | Original-style normal/special-player scan, countdown/high-bit cleanup records, day counter, and modeled win-candidate detection are represented. | Stop before final victory/loss UI or special-player event execution. |
| P8 | Deferred post-M2 queue | Remaining items are explicitly listed as deferred follow-up targets. | Do not implement them under this M2 goal. |

Out-of-M2 for this document:

- Full card, tool, land, stock, fortune, news, magic-house, or special event
  execution.
- Buy/build/sale/transfer economic mutations beyond readiness snapshots.
- Final victory/loss dialogue and end-game UI flow.
- Special-player action/event execution.
- Full AVI decoding/playback backend; M2 only records the original dispatch
  choice and keeps the `JUMP.MKF` fallback visible.
- Literal Windows callback, DirectDraw, timer handle, cursor, or message-loop
  reproduction when the SDL acceptance item already names an SDL equivalent.

When reporting progress, use this shape:

```text
Active slice: Pn - short name
Changed: files/functions touched
Verified: build/test/manual check or reason not run
Stopped before: named stop boundary
Deferred notes: optional, only items outside the active slice
```

## Scope

This target covers:

- New-game setup and airplane/setup screen before a game starts.
- Transition from setup into map loading.
- Original loading and visible rendering of special map tiles.
- Airplane takeoff and parachute intro resources.
- First formal map landing animation.
- First-turn entry into the main map/game loop.

This target does not expand the full card, tool, land, stock, fortune, news, or
special event execution families. Those begin after the first-turn dispatcher
enters their own function groups. This document only covers the original
loading and display of special map objects before and during first-turn map
rendering.

## Evidence Anchors

| Behavior | Original address | Local reference |
| --- | ---: | --- |
| New-game setup init | `0x406DE7` | `rich4/csrc/game_init.c`, `rich4/asm/rich4_init_new_game.asm` |
| Setup/airplane callback | `0x404E44` | `rich4/asm/rich4_init_new_game_callback.asm` |
| Setup character animation load | `0x404D0A` | `rich4/asm/rich4.asm` |
| Map resource load | `0x407AD2` | `rich4/csrc/loadsave.c`, `rich4/asm/rich4.asm` |
| Map resource cleanup | `0x4080F5` | `rich4/asm/rich4.asm` |
| Map visible object render | `0x40829D` | `rich4/asm/rich4.asm` |
| Map object coordinate lookup | `0x40AF12` | `rich4/asm/rich4.asm` |
| Map panels/resources init | `0x4190CF` | `rich4/csrc/main.c`, `rich4/asm/rich4.asm` |
| Post-map airplane/parachute intro | `0x415872` | `rich4/asm/rich4.asm` |
| Main game loop callback | `0x417E26` | `rich4/asm/rich4.asm` |
| First formal map landing | `0x418C55` | `rich4/asm/rich4.asm` |
| Map actor resource state | `0x40B93B` | `rich4/asm/rich4.asm` |
| Autodesk Animator FLC player | `0x45144F`, `0x450CED` | `rich4/asm/rich4.asm` |

## Required Flow Order

The original order is:

1. Main menu selects start/new stage.
2. `init_new_game()` loads `JUMP.MKF` setup resources.
3. `sub_404E44` runs the setup/airplane selection callback.
4. If setup succeeds, `init_new_game()` commits players, funds, stocks, and
   global game state.
5. Main flow calls loading scene, `0x407AD2`, `0x4190CF`, `0x4291D6`,
   `0x415872`.
6. `0x415872` plays the post-map-load airplane/parachute intro from
   `JUMP.MKF`.
7. `start_game_loop(0)` registers `0x417E26` and marks current player state
   dirty with `0x80`.
8. Main idle loop clears that dirty bit and calls `0x418C55`.
9. If `0x475114 != 0`, `0x418C55` plays `Data.mkf[559 + character]` at
   `(0, 40)`.
10. `0x418C55` sets map position/visibility, calls `0x40B93B`, then enters
    first-turn dispatch.

## New-Game Setup Init

`init_new_game()` at `0x406DE7` must be treated as a resource and state setup
routine, not as the whole first-turn flow.

Required behavior:

- Load sound data from `0x46CCD0`.
- Allocate two `0x96000` buffers.
- Load `JUMP.MKF`.
- Read setup background:
  `read_mkf(JUMP.MKF, game_stage * 4 + game_map, 0x48A358, NULL)`.
- Create dark version:
  `fcn_4552B7(0x48A354, 0x48A358, 0x96000, -16)`.
- Allocate temporary graph buffers `440x155` and `192x461`.
- Read `JUMP.MKF[8]`.
- Read `Data.mkf[2]`.
- For fresh start (`a0 == 0`), force `game_map = 0`, clear character selection
  flags, clear temporary player slots, and set default option indices.
- For returning setup (`a0 != 0`), reconstruct setup option indices from
  existing global state and preselect the first player.
- Crop setup UI fragments from `JUMP.MKF[8]`.
- Start music with `playmid_4549CF(0x8001)`.
- Register wait callback `sub_404E44`.

Important resource rule:

- The setup/airplane background is `JUMP.MKF[game_stage * 4 + game_map]`.
- It is not `Data.mkf`.
- It is not `Map.mkf`.

## Setup Callback

`sub_404E44` is a Windows callback with an internal stage variable
`0x48A3AC`.

### Stage 0: Setup UI

On callback initialization (`Msg == 0x401`):

- Store window handle and callback lParam.
- Initialize hover/selection bytes to `0xFF`.
- Copy the dark background buffer to the render buffer.
- Set timer interval to `100ms`.
- If callback argument is zero, initialize the character hover/display state.

On timer:

- Advance background offset `0x48A3C8 += 4`, wrapping at `0x500`.
- Draw the dark background.
- Draw selected player slot animations from `0x48A364[slot]`.
- Advance each selected slot frame counter `0x48A360[slot]`, wrapping by the
  resource frame count stored at resource offset `+4`.
- Draw setup panels into the temporary buffers and invalidate the window.

On map selection:

- Change `0x46CB54`.
- Re-read `JUMP.MKF[game_stage * 4 + 0x46CB54]` into `0x48A358`.
- Rebuild the dark version with `fcn_4552B7(..., -16)`.

### Setup Character Animation

`sub_404D0A(slot, character)` loads setup-stage character animation:

```text
resource = 9 + 3 * character + 0x46CB44
read_mkf(JUMP.MKF, resource, NULL, NULL)
```

This is the setup/airplane-door animation used inside `sub_404E44`.
It must not be confused with the later post-map-load intro resources
`47 + character` and `59 + character`.

### Stage 1: Auto-Fill AI

When OK is clicked and selected characters are not enough:

- The callback switches to stage `1`.
- On the eleventh 100ms callback (the old counter value reaches `10`), pick
  one unused character randomly.
- Mark it selected in `0x4990F4`.
- Call `sub_404D0A(slot, character)`.
- Set the high bit at the slot marker near `0x48A35F + slot * 12` to mark AI.
- Increment selected slot count.

When all slots are filled:

- Kill the old timer.
- Start a new `50ms` timer.
- Switch to stage `2`.
- Play the OK/start sound.

### Stage 2: Setup Exit Animation

Stage `2` draws the final setup/airplane exit sequence:

- Base background is the original bright `JUMP.MKF` background (`0x48A358`).
- Draw the two temporary panels.
- Draw each selected character animation while moving coordinates are updated.
- Mouse click or key input can interrupt.
- On completion or interrupt, post the callback completion message with result
  `1`.

## Committing New Game State

After the callback returns `1`, `init_new_game()` commits the actual game state:

- `num_players = selected_player_count + 2`.
- `current_player = 0`.
- Initial fund comes from `0x46CB94[fund_index]`.
- Clear player card and tool inventories.
- Initialize card/tool global amounts from their tables.
- For each player slot up to 4:
  - Copy character template from the global player template table.
  - Set human/AI marker from the sign/high bit stored in `0x48A35C`.
  - Human players get half cash and half bank deposit.
  - AI players use character template ratio for cash, rest in bank.
  - Set initial traffic method and dice count.
- Copy special player defaults.
- Initialize day limit, win condition, price index, stocks, player stocks, and
  stock-market/rest globals.
- Copy 12 stock records using `game_stage * 4 + game_map`.
- Release temporary setup resources.
- Normalize `0x4990F4`: value `4` becomes `2`, all other entries become `0`.

## Map Loading Before First Turn

After successful `init_new_game()`, main flow calls:

```text
show_loading_scene()
0x407AD2()
0x4190CF()
0x4291D6()
0x415872()
start_game_loop(0)
```

`0x407AD2` loads map resources:

- Load `MAP.MKF`.
- `gmap = game_stage * 4 + game_map`.
- Read main map resource `MAP.MKF[gmap * 2]`.
- Read map graphics/resource block `MAP.MKF[gmap + 16]` twice.
- Read common map resources `MAP.MKF[24]`, `MAP.MKF[26]`.
- Prefer `MAPDAT.MKF[gmap]` if available; otherwise use
  `MAP.MKF[gmap * 2 + 1]`.
- Read five map view resources `MAP.MKF[gmap * 5 + 39 + i]`.
- Read minimap/overlay resource `MAP.MKF[gmap + 79]`.
- Read stage-specific 17 tile/resource entries:
  - stage `0`: `MAP.MKF[87..103]`
  - stage `1`: `MAP.MKF[104 + game_map * 17 + i]`
- Read `Data.mkf[0x205]`, `Data.mkf[0x207]`, and `Data.mkf[396..415]`.

## Original Special Tile Loading And Display

The original separates special tile resource loading from special event
execution.

This M2 target covers:

- Loading the map-data tables and their display resources.
- Building the visible object list for special map objects.
- Sorting and drawing those visible objects with the same field mapping.

This M2 target does not cover:

- News, fortune, magic house, card/tool, or other special event callbacks.
- The effect logic that runs after a player lands on a special map object.

### Map-Data Tables Used By Rendering

`0x407AD2` reads `MAPDAT.MKF[gmap]` if available; otherwise it falls back to
`MAP.MKF[gmap * 2 + 1]`.

The map-data header initializes these table pointers and counts:

| Global | Meaning | Entry size |
| ---: | --- | ---: |
| `0x498E80` | road/node table | `0x28` |
| `0x498E84` | land table | `0x34` |
| `0x498E88` | facility table | `0x38` |
| `0x498E7C` | listed-company/special display table | `0x34` |
| `0x498E78` | second special display table | `0x1C` |
| `0x498E9C` | road/node count | count |
| `0x498E98` | land count | count |
| `0x498E8C` | facility count | count |
| `0x498E90` | `0x498E7C` count | count |
| `0x499074` | `0x498E78` count | count |

`0x40AF12` maps object ids back to table coordinates by id range:

| Object id range | Table |
| ---: | --- |
| `< 0x7D0` | `0x498E80`, road/node table |
| `0x7D0..0xF9F` | `0x498E84`, land table |
| `0xFA0..0x176F` | `0x498E88`, facility table |
| `0x1770..0x1F3F` | `0x498E7C`, listed-company/special display table |
| `0x1F40..0x270F` | `0x498E78`, second special display table |

Implementation rule:

- Do not treat the special-object id as a raw node id.
- `0x1770 + index` and `0x1F40 + index` are original render/lookup ids for
  the two special display tables.

### Special Display Resource Loading

`0x407AD2` first loads common map display resources:

```text
gmap = game_stage * 4 + game_map

0x48AE4C[i] = read_mkf(MAP.MKF, gmap * 5 + 39 + i)  for i = 0..4
0x48AE60    = read_mkf(MAP.MKF, gmap + 79)

if game_stage == 0:
    0x48AE64[i] = read_mkf(MAP.MKF, 87 + i)          for i = 0..16
else:
    0x48AE64[i] = read_mkf(MAP.MKF, 104 + game_map * 17 + i)

0x47494D    = read_mkf(MAP.MKF, 0x1A)
0x48AEA8    = read_mkf(MAP.MKF, 0x19)
```

Then it lazily loads special display resources referenced by map-data table
fields.

For `0x498E7C`, entries are `0x34` bytes and the loop is 1-based:

```text
for index = 1..0x498E90:
    entry = 0x498E7C + index * 0x34
    resource_id = u16(entry + 0x20)
    if resource_id != 0 and 0x48AE4C[resource_id] == NULL:
        0x48AE4C[resource_id] = read_mkf(MAP.MKF, resource_id + 0x26)
```

For `0x498E78`, entries are `0x1C` bytes and the loop is also 1-based:

```text
for index = 1..0x499074:
    entry = 0x498E78 + index * 0x1C
    resource_id = u16(entry + 0x1A)
    if resource_id != 0 and 0x48AE4C[resource_id] == NULL:
        0x48AE4C[resource_id] = read_mkf(MAP.MKF, resource_id + 0x26)
```

Cleanup rule:

- `0x4080F5` frees the `0x48AE4C` resource pointer array during map resource
  cleanup.
- M2 must keep these lazily loaded special resources in the same lifetime group
  as the other map display resources.

### Special Display Object Construction

`0x40829D` builds the visible object list used for map rendering. It processes
land, facility, `0x498E7C`, and `0x498E78` objects before sorting and drawing.

For each visible object, the original does the same broad transform:

```text
world_x = s16(entry + 0)
world_y = s16(entry + 2)

tile_x = (world_x >> 5) - viewport_tile_x + 0x0E
tile_y = (world_y >> 5) - viewport_tile_y + 0x0E

visible only if 0 <= tile_x <= 0x1C and 0 <= tile_y <= 0x1C
```

If visible:

- Call `0x407A2C(world_x, world_y, &out_x, &out_y)`.
- Use the current map rotation byte `0x499088`.
- Use the tile offset tables at `0x46CCF0` and `0x46CCF2`.
- Compute screen position into render-list fields `0x48A854` and `0x48A856`.
- Write a sort key into `0x48A44C[0x48BAC8]`.
- Write image pointer, object id, owner/color byte, direction/variant byte, and
  screen position into the per-object arrays.
- Increment visible object count `0x48BAC8`.

For `0x498E7C` entries, original behavior is:

```text
entry = 0x498E7C + index * 0x34
if u16(entry + 0x20) == 0:
    skip

resource_id = u16(entry + 0x20)
image       = 0x48AE4C[resource_id]
object_id   = 0x1770 + index
owner_byte  = u8(entry + 0x18)
variant     = (8 - (u8(entry + 0x1B) + u8(0x499088))) & 7
```

The render-list writes are:

```text
0x48A84C[list_index] = image
0x48A850[list_index] = object_id
0x48A852[list_index] = owner_byte
0x48A853[list_index] = variant
0x48A854[list_index] = screen_x
0x48A856[list_index] = screen_y
```

For `0x498E78` entries, original behavior is:

```text
entry = 0x498E78 + index * 0x1C
if u16(entry + 0x1A) == 0:
    skip

resource_id = u16(entry + 0x1A)
image       = 0x48AE4C[resource_id]
object_id   = 0x1F40 + index
owner_byte  = 0xFF
variant     = (8 - (u8(entry + 0x18) + u8(0x499088))) & 7
```

The render-list writes are:

```text
0x48A84C[list_index] = image
0x48A850[list_index] = object_id
0x48A852[list_index] = 0xFF
0x48A853[list_index] = variant
0x48A854[list_index] = screen_x
0x48A856[list_index] = screen_y
```

After all visible objects are queued:

- If `0x48BAC8 > 0`, qsort `0x48A44C` with comparator `0x4079F9`.
- Draw in sorted order.
- Skip objects whose `0x48A84C` image pointer is null.
- If `0x48A852 != 0xFF`, write owner/color information into the image frame
  metadata before drawing.
- Draw the selected frame/variant through `0x456770(image, variant, x, y)`.

Implementation rules:

- Do not load special display art from guessed `MAP.MKF` indices.
- Do not draw a special object unless its map-data resource field is nonzero.
- Do not collapse `0x498E7C` and `0x498E78`; they use different entry sizes,
  resource fields, owner bytes, and object id bases.
- Do not replace this with event-trigger logic. Display comes from `0x40829D`;
  event behavior belongs to later dispatcher/event-function targets.

## Post-Map-Load Airplane And Parachute Intro

`0x415872` loads `JUMP.MKF` again after map resources are ready.

Confirmed resource group:

| Resource | Meaning |
| ---: | --- |
| `JUMP.MKF[45]` | Intro base/background group |
| `JUMP.MKF[46]` | Intro animation played before per-player animation |
| `JUMP.MKF[47 + character]` | Per-character airplane/takeoff animation |
| `JUMP.MKF[59 + character]` | Per-character parachute animation after airplane |

The formulas match:

```text
intro_base = 45
takeoff = intro_base + 2 + character
parachute = intro_base + 2 + 12 + character
```

Additional behavior:

- The function may play map-specific AVI:
  - `FLYTW.AVI`
  - `FLYCHINA.AVI`
  - `FLYJP.AVI`
  - `FLYUS.AVI`
- For another path it uses `AIRPLANE.AVI`.
- The verified branch at `0x41591f` uses `AIRPLANE.AVI` when original stage
  global `0x4991b6` is nonzero; otherwise it indexes the four map-specific AVI
  names with original map global `0x4991b8`.
- It draws a window/airplane base, then plays `JUMP.MKF[46]`, then each
  player takeoff animation, then each player parachute animation.
- The next character shown near the door is darkened through
  `fcn_4552B7(..., -20)` style pixel darkening.

Implementation rule:

- This sequence is still `JUMP.MKF`.
- It is distinct from the formal map landing animation in `Data.mkf`.

## First Formal Map Landing

`0x418C55` owns the first formal map landing.

Precondition:

- `0x475114 != 0`
- `current_player < num_players`

Resource:

```text
character = players[current_player].character
resource = Data.mkf[0x22F + character]
         = Data.mkf[559 + character]
```

Playback:

```text
read_mkf(Data.mkf, 559 + character, NULL, NULL)
sub_45144F(resource, 0, 40, 1, -1)
free(resource)
```

After playback:

- Use `0x475114 - 1` as the player index whose landing was pending.
- Read that player's current road/node id from player state offset `+12`.
- Look up the node table at `0x498E80`.
- Copy node X/Y into player pixel coordinates at offsets `+8` and `+10`.
- Copy player template/status byte `+100` into visible/state byte `+21`.
- Call `0x40B93B(player)` to load map actor resources.
- Clear `0x475114`.
- Call `0x41D476(0, 0, 1)` to refresh map/UI.

Important trigger detail:

- `0x475114` is set earlier by path/position setup logic such as `0x40829D`.
- `0x418C55` consumes this pending flag; it does not invent the initial landing
  state by itself.

## Map Actor State Resources

`0x40B93B(player)` loads map actor resources from `Data.mkf`.

For normal players `< 4`, the per-character base is:

```text
base = 128 + character * 21
```

The loaded parts depend on status:

- Normal walking/standing direction resources use direction-based parts under
  the `base` group.
- Sleepwalking, hospital, frozen/winter sleep, traffic, or special map states
  select other parts.
- A common visible-state part appears as `base + 18` in the status table path.

Implementation rule:

- `Data.mkf[128 + character * 21 + part]` is the map actor/status resource
  family.
- It is not the same as `Data.mkf[559 + character]`.
- `Data.mkf[559 + character]` is the first formal landing cutscene.

## First-Turn Entry

`start_game_loop(0)`:

- Registers game-loop callback `0x417E26`.
- Posts message `0x401`.
- Sets current player's `0x498EA0[current_player * 0x34] |= 0x80`.

Main idle loop:

- If current player's `0x498EA0` has `0x80`, clear the bit and call
  `0x418C55`.

After the optional first landing block, `0x418C55` calls:

```text
state = sub_40C912(0)
```

Dispatcher behavior:

| `sub_40C912(0)` result | Behavior |
| ---: | --- |
| `0` | Call `0x418E7F`, set dice/action hint state, set `0x46CAFB = 1` |
| `1` | Call `0x4196F1`, move OS cursor, refresh input state |
| `2` or `5` | Enter AI/automatic pre-action path: sale, card/tool decision, status checks |
| `-1`, `3`, `4`, or `>5` | Return without the above action path |

If the player has hotel, sleepwalk, winter sleep, or related status, the code
may re-mark the player with `0x80` and return so the idle loop processes it
again later.

Turn rotation is handled by `0x418EBD`:

- Increment `current_player`.
- If it reaches `num_players`, switch into special player indices `4..7`.
- After index `8`, wrap back to `0`.
- A full first round is complete only after all active normal players and
  applicable special-player slots have passed through this rotation.

## Autodesk Animator FLC Timing Rules

`0x450CED`, called by `0x45144F`, initializes Autodesk Animator FLC playback:

```text
u16(data + 4)  must equal 0xAF12
u16(data + 6)  frame count
u16(data + 8)  width
u16(data + 10) height
u32(data + 16) frame delay
```

It writes `u32(data + 16)` into `0x48C870`, and `0x45144F` waits on that value
between frames.

Implementation rules:

- Do not use one fixed speed for all Autodesk Animator FLC resources.
- Use each Autodesk Animator FLC resource header delay unless original call flags explicitly
  override it.
- `Data.mkf[128 + character * 21 + part]` map actor resources are SPR/SMP-like
  direction resources, not Autodesk Animator FLC cutscenes.
- Map actor animation advances by state-machine frame selection:
  `chunk = visual_direction * frames_per_direction + frame`.

## M2 Acceptance Status

Status key:

- `[x]` implemented in the SDL code path.
- `[~]` partially implemented or SDL-equivalent but not original-exact.
- `[ ]` not implemented.
- `[SDL]` original Windows callback/API detail intentionally mapped onto the SDL
  event/render loop instead of being reproduced literally.

M2 should be judged by the SDL behavior first: the target is a portable SDL
implementation that preserves original resource choices, ordering, timing, and
visible behavior where practical. Windows callback handles, DirectDraw calls,
and message ids are evidence for ordering/state, not mandatory implementation
interfaces.

This section is an acceptance ledger for the slices above. Update only the
subsection that belongs to the active slice. A partial item may remain partial
when its missing behavior is named by a later slice or by the out-of-M2 queue;
do not use that partial marker by itself as permission to broaden the current
goal.

### Setup And State Commit

- [x] Setup background uses `JUMP.MKF[game_stage * 4 + game_map]`.
- [x] Setup dark background uses a `-16` brightness transform equivalent.
- [x] `[SDL]` Setup callback is represented by SDL state/update/render code:
  character selection, options, map selection, background scrolling, selected
  character animation, and AI auto-fill are implemented; original Windows
  `Msg == 0x401`, timer handles, and callback `lParam` are not reproduced.
- [x] Setup-stage character door animation uses
  `JUMP.MKF[9 + 3 * character + option]`.
- [x] `[SDL]` Stage 1 AI auto-fill follows the original order and the exact
  callback-counter edge: work is due on the eleventh 100ms tick because
  `0x404E44` compares the previous counter value with `10`. Selected setup
  characters use the original pre-commit marker value `4`.
- [x] `[SDL]` Stage 2 setup exit uses the bright `JUMP.MKF` background,
  selected character animation, and a 50ms transition tick. Left/right mouse
  button release or any key press completes the transition with result `1`;
  controller button press is the portable equivalent. Literal Windows message
  posting remains outside M2.
- [x] Returning setup (`a0 != 0`) reconstruction is represented for the SDL
  path: canceling from the map preview rebuilds setup state from the committed
  session fields for stage/map, players, AI flags, initial fund, traffic,
  land-period option, day limit, and win target. It is still not a literal
  reconstruction from the original global addresses and only reflects fields
  currently modeled by `GameSession`.
- [x] New-game commit initializes active SDL players, current player, funds,
  AI flag, traffic method, dice count, start node, visible position, and price
  index; it now also records the original `selected_player_count + 2`
  `num_players` shape, copies the modeled fields from the original 12-entry
  character template table, applies the original per-character AI
  `init_cash_ratio` table while keeping human players at half cash/half bank,
  initializes loan/special-finance fields to zero, initializes SDL session
  fields for card/tool global amounts, zeroed per-player card/tool
  inventories, zeroed original `f76[6]` player-to-player relation balances,
  special-player defaults, day limit, win condition, and zeroed
  player stocks, copies the 12 original stock records for
  `game_stage * 4 + game_map`, and records the post-commit `0x4990F4`
  normalization from setup marker `4` to selected characters `2` and all
  others `0`. It still does
  not expose every original global/rest field.
- [x] Player count is stored through the original
  `num_players = selected_player_count + 2` commit model for the SDL session's
  active normal players.

### Map Loading

- [x] Load `MAP.MKF` and derive `gmap = game_stage * 4 + game_map`.
- [x] Read main map `MAP.MKF[gmap * 2]` as the `GND` tile resource.
- [x] Read map overview/minimap from `MAP.MKF[gmap + 16]` and keep
  `MAP.MKF[gmap + 79]` as a separate overlay resource group.
- [x] Common map resources `MAP.MKF[24]`, `MAP.MKF[25]`, and `MAP.MKF[26]`
  are loaded into the SDL map lifetime group.
- [x] Prefer `MAPDAT.MKF[gmap]` if available; otherwise use
  `MAP.MKF[gmap * 2 + 1]`.
- [x] Five map view resources `MAP.MKF[gmap * 5 + 39 + i]` are loaded as a
  distinct view-resource group.
- [x] Stage-specific 17 tile/resource entries (`87..103` or
  `104 + game_map * 17 + i`) are loaded as a distinct group.
- [x] `Data.mkf[0x205]`, `Data.mkf[0x207]`, and `Data.mkf[396..415]` are
  loaded into SDL map/session resource state.
- [x] Resource cleanup lifetime is handled through `free_game_preview_assets`
  with separate SDL buckets for common, view, overlay, stage-tile, special
  display, and Data map resources.

### Special Display Tables And Drawing

- [x] Map-data road/node table is parsed from the header pointer/count model,
  including node coordinates, next-node links, flags, and the original
  `0x40AF12` object-id range classification used to identify road/node, land,
  facility, listed-company/special, and second-special table indices.
- [x] Land/facility/special table parsing is sufficient for the current visible
  display rows and fixed-asset snapshots, but not exposed as full original
  table pointer structures.
- [x] Built land/facility rows now enter the same SDL visible-object list as
  special display rows, using the original object ids `0x7D0 + index` and
  `0xFA0 + index`, owner byte, map-rotation variant formula, and the verified
  map-view/overlay/stage-tile resource choices. Unbuilt owned land/facility
  rows now use common resource `MAP.MKF[25]` with the owner player's character
  as the displayed chunk, matching the original `0x48AEA8` path.
- [x] Special display resources are read from map-data fields
  `u16(0x498E7C[index] + 0x20)` and `u16(0x498E78[index] + 0x1A)`.
- [x] Special display resources use `MAP.MKF[resource_id + 0x26]`.
- [x] The cache is represented by SDL asset arrays keyed by `resource_id`
  rather than a literal `0x48AE4C[resource_id]` pointer array.
- [x] Special display objects retain original ids `0x1770 + index` and
  `0x1F40 + index`.
- [x] `0x498E7C` and `0x498E78` remain separate: entry size, resource field,
  owner byte, and variant byte differ.
- [x] Objects are culled to the visible `0..0x1C` tile window.
- [x] Screen projection uses SDL's current main-view transform and original
  grid tables. The sub-tile helper is now an exact `0x407A2C` coefficient-table
  equivalent using `0x474910..0x474913`; the remaining SDL wrapper still owns
  viewport centering/origin handling.
- [x] Objects are sorted before drawing using an SDL equivalent of the original
  `0x48A44C` key shape: low word `screen_y << 4`, high word visible-list index,
  compared with the original signed-low-word ordering.
- [x] Variant is computed from map rotation as in the original formulas.
- [x] Owner/color display is applied for SPR resources by replacing the palette
  entry equivalent to original `graph + 0x1FE` before drawing.
- [x] Special display remains rendering-only; special event execution is not
  mixed into this target.

### Airplane, Landing, Actor Resources, And Autodesk Animator FLC

- [x] Post-map-load airplane/takeoff/parachute intro uses `JUMP.MKF[45..70]`
  with the exact formulas above.
- [x] Map-specific AVI dispatch now records the original selected filename:
  stage nonzero selects `AIRPLANE.AVI`, otherwise map `0..3` selects
  `FLYTW.AVI`, `FLYCHINA.AVI`, `FLYJP.AVI`, or `FLYUS.AVI`. SDL still has no
  AVI decode backend, so it marks the movie as skipped and continues with the
  existing `JUMP.MKF` intro; see the AVI backend plan below.
- [x] Intro draws the JUMP scene/window, `JUMP.MKF[46]`, per-player takeoff,
  per-player parachute, and darkened next-character door art.
- [x] Formal map landing uses `Data.mkf[559 + character]` at `(0, 40)`.
- [x] Landing pending state is represented by an SDL session-level one-based
  flag that mirrors `0x475114`: map position setup sets `current_player + 1`,
  landing consumes `pending - 1`, updates that player's node pixel position,
  and clears the flag. It is not stored at the original global address.
- [x] After landing, player visibility, road-node pixel coordinates, and actor
  direction/state are initialized before dice/map action handling.
- [x] Landing completion copies the committed player template/status byte
  equivalent (`+100`, human `1` / AI `2`) into the visible/control state byte
  equivalent (`+21`) before dispatching the first action.
- [x] `0x40B93B` style map actor resources are loaded from
  `Data.mkf[128 + character * 21 + part]`; normal traffic resources now select
  `traffic_method * 3 + standing/moving` before falling back to the old
  standing/walking resources, and traffic method 3 overlays its extra
  `base + 12` resource when available. SDL maps several original status
  selections: sleepwalk uses `base + 16/17`, special road-node state uses
  `base + 13/14` and now overlays `base + 15` like the extra resource read by
  `0x40B93B`, and delayed `+50..+54`, low visible-state `0`, or
  `who_plays & 0x40` frozen/winter-style state can use `base + 18`. Full
  countdown semantics and unconfirmed hospital/frozen/winter edge cases remain
  explicitly post-M2.
- [x] `Data.mkf[128 + character * 21 + part]` actor resources are separate from
  `Data.mkf[559 + character]` landing cutscenes.
- [x] Autodesk Animator FLC playback reads frame count, dimensions, and delay from the resource
  header for intro, landing, and panel graph playback.

### First-Turn Loop

- [x] First-turn entry follows an SDL dirty-bit path:
  intro completion marks `GAME_PLAYER_DIRTY_TURN`, the update loop clears it,
  then enters the landing/action flow. This is SDL-equivalent to
  `start_game_loop(0)` marking `0x80`, but it does not register the original
  Windows callback `0x417E26`.
- [x] `sub_40C912(0)` dispatcher result handling is represented for the first
  landing split: SDL now stores an original-style visible/play-state byte
  (`1` for human, `2` for AI) and computes the normal-player return shape like
  `0x40C912(0)`: nonzero `+50..+53` or `+54` status returns `0`, `+55`
  sleepwalk returns `-1`, and otherwise the play-state byte is returned. Result
  `1` is mapped to an SDL input-refresh flag plus dice-panel stop, while result
  `2` enters the automatic pre-action path. Result `0` has an SDL
  `0x46CAFB`-equivalent action-hint flag for the dice/action panel. Result `5`
  now shares the automatic branch with result `2`, and results `-1`, `3`, `4`,
  and `>5` are recorded as the original return-without-action branch. The
  branch is recorded in session state, but no additional original callback,
  status speech/message display, or Windows refresh behavior is implemented for
  it yet.
- [x] Human dice/action hint behavior now stops after landing and shows the SDL
  dice-status panel; SDL now tracks separate action-hint and input-refresh
  flags for the `0x418E7F` / `0x46CAFB` and `0x4196F1` paths, but it still does
  not reproduce the original Windows cursor positioning and callback refresh
  APIs literally.
- [x] AI/automatic pre-action path enters the current SDL automatic dice/move
  flow after landing. SDL now records a conservative automatic pre-action audit
  for result `2`/`5`: sale, card decision, and tool decision are marked
  deferred, card/tool pool totals are captured from the initialized global
  amounts, and the status guard result is recorded before either re-marking
  dirty or entering dice/move. Sale, card, and tool effects are still not
  executed.
- [x] Re-marking `0x80` for hotel/sleep/sleepwalk-style status loops is
  represented in the SDL automatic path: if the current player has the
  `+50..+53` delayed-status block, `+54` sleep/winter-style byte, or `+55`
  sleepwalk byte set, the SDL path re-marks `GAME_PLAYER_DIRTY_TURN` and
  returns to the idle/update loop. SDL also decrements `+50..+57`,
  `+59`, `+60`, `+61`, and `+62` status bytes when turn rotation selects a normal
  player, setting the high bit when a countdown reaches zero as in `0x41C84F`;
  `+51` disappearing status now follows the original low-6-bit completion
  check, so it marks high-bit pending when `(value - 1) & 0x3f` becomes zero.
  Nonzero `+50..+53` state suppresses `+54/+55/+57` countdown just like the
  original dword block check, while `+56/+59/+60/+61/+62` continue to tick.
  The dirty path now consumes high-bit pending markers in original offset
  order, records the original handler family for `+50` (`0x40D6BE`), `+51`
  (`0x40D4E5`), `+52` (`0x43D7BF`), `+53` (`0x43EE6E`), `+54`
  (`0x40B8D8`/`0x40B93B`), `+55` (`0x40B93B`), and `+61` (`0x40CC1A`), and
  leaves `+54/+55/+57` pending when the `+50..+53` block is still nonzero.
  SDL now restores saved traffic method/dice count for the `+55`
  sleepwalk-ending high-bit event and clears both linked players for the `+61`
  / `god_info` high-bit event. While a non-high-bit `+61` countdown is active,
  SDL also applies the original `0x40DF69` pair before decrementing: it subtracts
  `price_index * 20` from the current player's and linked player's `f76[target]`
  relation-balance slots, only when those slots are nonzero, and clamps them at
  zero. Status producers and the remaining original high-bit event effects
  remain incomplete.
- [x] Original turn rotation now keeps a separate SDL rotation index that scans
  normal players, special-player indices `4..7`, and wrap-to-`0` round
  completion in the `0x418EBD` order. SDL now tracks the special-player
  `0x498e34..37` status bytes and advances/clears their high-bit pending
  markers when an active special slot is scanned; clearing a high-bit
  `0x498e34` marker also clears the special-player actor `0x40` state bit and
  records the `0x40B8D8`/`0x40B93B` handler family, while high-bit
  `0x498e35..37` markers are recorded as clear-only cleanup.
  Wrap-to-`0` now increments SDL `elapsed_days` using the original `0x4990e4`
  counter semantics and records whether it has reached `day_limit`
  (`0x49911c`). It is intentionally not drawn in the price-index HUD row or any
  other unverified location. SDL now also records the `0x41d89e`-style win
  candidate/reason using currently modeled assets (cash, bank, stock market
  value, and land/facility value derived from session-copied
  `0x498e84`/`0x498e88` property rows), but buy/build/sale/transfer event
  handlers that mutate those rows and the final victory/loss UI flow are still
  deferred. Special-player action/event execution is still not implemented.

## Item-By-Item Implementation Audit (2026-07-16)

This audit was made against the current source tree, not against the progress
prose above. `Confirmed for M2` means the bounded SDL acceptance behavior has a
concrete code path and automated coverage. It does not claim literal Win32
interfaces, behavior in the deferred queue, or visual parity that requires
manual comparison. The item ids follow the acceptance-ledger order.

### Setup And State Commit Audit

| ID | Result | Source evidence | Remaining verification or gap |
| --- | --- | --- | --- |
| S1 | Confirmed | `load_game_preview_assets` loads JUMP resources 0-7 and `draw_new_game_setup` selects `stage * 4 + map`. | Visually check all eight backgrounds. |
| S2 | Confirmed | `draw_new_game_setup` passes brightness delta `-16` to the scrolled RGB565 background renderer. | Visual palette comparison remains manual. |
| S3 | Confirmed for M2 | `update_new_game_setup_animation`, `draw_new_game_setup`, and main-loop input handling implement the SDL state machine. | Win32 message, timer-handle, and `lParam` behavior is intentionally outside M2. |
| S4 | Confirmed | `rich4_setup_character_resource` supplies `9 + 3 * character + option`; unit tests exhaust all 36 combinations. | Visual animation parity remains manual. |
| S5 | Confirmed for M2 | `rich4_setup_stage1_tick_due` implements the original eleventh-callback edge; setup update performs unused-character fill and marker `4`. | Literal Win32 timer handles are outside M2. |
| S6 | Confirmed for M2 | Transition state uses bright JUMP art and 50 ms updates; `rich4_setup_transition_should_complete` handles natural completion, mouse release, key press, and controller input. | Literal completion-message posting is outside M2. |
| S7 | Confirmed for M2 | `rebuild_new_game_settings_from_session` restores all M2-modeled setup fields. | Literal reconstruction of unmodeled globals is in the deferred queue. |
| S8 | Confirmed for M2 | `commit_new_game_global_state` initializes the M2-modeled players, money split, templates, pools, stocks, statuses, day/win settings, and session defaults. | Remaining original globals/rest fields are in the deferred queue. |
| S9 | Confirmed | New-game initialization tests cover every valid committed count (2, 3, and 4) and the separate `selected + 2` shape. | One selected slot is pre-commit state and is auto-filled before commit. |

### Map Loading Audit

| ID | Result | Source evidence | Remaining verification or gap |
| --- | --- | --- | --- |
| M1 | Confirmed | `load_game_map_routes` iterates all eight `gmap` values; catalog/selection tests exhaust them and the original-data validator scans the archive. | Visual map parity remains manual. |
| M2 | Confirmed | The same loader reads `gmap * 2` into the ground-resource bucket. | GND rendering remains a visual check. |
| M3 | Confirmed | It reads `gmap + 16` as overview and `gmap + 79` into a separate overlay bucket. | Confirm overlay use for each map. |
| M4 | Confirmed | Resources 24, 25, and 26 are loaded into `map_common_*`; catalog tests cover every slot and ResourceStore tests cover missing archives/resources. | Visual fallback presentation remains manual. |
| M5 | Confirmed | `Mapdat.mkf[gmap]` is attempted first, then `Map.mkf[gmap * 2 + 1]`; tests exhaust both selections for all eight maps. | The local commercial install exercises the MAP fallback branch. |
| M6 | Confirmed | Five resources at `gmap * 5 + 39 + i` are retained in `map_view_*`. | Visual chunk selection remains manual. |
| M7 | Confirmed | Seventeen stage/map resources use `87 + i` or `104 + map * 17 + i`; tests exhaust both stages, four maps, and all 17 slots. | Visual chunk parity remains manual. |
| M8 | Confirmed | `load_new_game_character_assets` loads Data resources `0x205`, `0x207`, and `396..415`; the V3.11 original-data scan passes. | Other commercial resource versions are not part of the fixed V3.11 target. |
| M9 | Confirmed | ResourceStore owns cached archive data; preview cleanup releases aliases and parsed maps. Repeated lifetime and allocation-fault tests pass. | ASan/UBSan remains a Linux build-matrix gate; the local Windows GCC lacks `libasan`/`libubsan`. |

### Display Tables And Drawing Audit

| ID | Result | Source evidence | Remaining verification or gap |
| --- | --- | --- | --- |
| D1 | Confirmed | Typed map parsing and object-reference tests cover record fields, range boundaries, truncated/corrupt tables, and every allocation failure. | Visual interpretation remains manual. |
| D2 | Confirmed for M2 | `MapData` extracts all fields required by M2 drawing and fixed-asset snapshots. | Full economic table mutation models belong to later targets. |
| D3 | Confirmed for M2 | Map renderer input queues built rows and common-25 unbuilt-owner rows. | Resource/chunk parity remains a visual-only check. |
| D4 | Confirmed | Special rows read resource ids from offsets `+0x20` and `+0x1A`; parser tests use the 1-based sentinel-plus-row shape. | Visual sprite identity remains manual. |
| D5 | Confirmed | `load_game_map_object_sprite` maps special ids to `resource_id + 0x26`; ResourceStore tests cover duplicate-key cache reuse and range failure. | Visual sprite identity remains manual. |
| D6 | Confirmed for M2 | ResourceStore-backed aliases provide equivalent caching/lifetime. | The literal original pointer array is outside the SDL target. |
| D7 | Confirmed | Object-reference/parser tests cover family bases and range boundaries for both special families. | Visual sprite identity remains manual. |
| D8 | Confirmed | Parser tests exercise the separate entry sizes, offsets, owner rules, and variant fields for both special tables. | Visual parity remains manual. |
| D9 | Confirmed | Renderer tests cover all four inclusive `0..0x1C` boundaries and reject both outside edges. | Pixel parity remains manual. |
| D10 | Confirmed for M2 | Projection uses the verified coefficient tables and corrected `(220,260)` SDL viewport center. | Pixel parity remains a visual-only check. |
| D11 | Confirmed | `game_visible_object_sort_key` and `compare_game_visible_map_object` implement the signed-low-word ordering. | Test equal-Y/tie cases visually. |
| D12 | Confirmed | `rich4_map_rotated_variant` is used by renderer assembly and tests exhaust all 64 base/direction combinations. | Visual sprite orientation remains manual. |
| D13 | Confirmed | SPR drawing substitutes the owner palette entry before rendering. | Compare colors for every player character. |
| D14 | Confirmed | Route-event code only classifies/defers special events; drawing does not execute their effects. | Economic/UI handlers remain out of M2. |

### Intro, Landing, Actor, And Autodesk Animator FLC Audit

| ID | Result | Source evidence | Remaining verification or gap |
| --- | --- | --- | --- |
| A1 | Confirmed | Intro asset loading loops all 12 characters over JUMP 45, 46, `47 + character`, and `59 + character`; the V3.11 FLC scan passes. | Visual sequence parity remains manual. |
| A2 | Confirmed for M2 | `game_session_prepare_intro_avi` records the verified AVI branch and skipped state. | AVI decoding is explicitly outside M2. |
| A3 | Confirmed | `draw_game_intro_fullscreen` renders scene/window, common animation, takeoff/parachute actors, and darkened next actor. | Full sequence requires visual/manual timing review. |
| A4 | Confirmed | First-landing loader loops all 12 Data resources `0x22f + character` (559-570), and the V3.11 FLC scan passes. | Visual placement/parity remains manual. |
| A5 | Confirmed for M2 | `landing_pending_player` implements the one-based session equivalent and is consumed by `update_game_session`. | Literal original global storage is outside the SDL target. |
| A6 | Confirmed | `start_game_turn_flow` and landing completion initialize node position, visibility, direction, and actor state before action. | Inspect first frame after landing. |
| A7 | Confirmed | Commit/landing flow copies human/AI low play-state; unit and replay tests cover both human and AI first-turn branches. | SDL presentation parity remains manual. |
| A8 | Confirmed for M2 | `rich4_game_actor_part` covers all confirmed M2 actor-selection precedence and combinations with unit tests. | Unconfirmed part semantics and special-state edge cases remain explicitly post-M2 and retain partial evidence confidence. |
| A9 | Confirmed | Actor resources and landing resources use distinct asset arrays and indices. | No known implementation gap. |
| A10 | Confirmed | Intro, landing, actor-action, and panel graph helpers read Autodesk Animator FLC frame metadata; configured speed overrides the verified dice graph delay path. | Compare timing with original recordings. |

### First-Turn Loop Audit

| ID | Result | Source evidence | Remaining verification or gap |
| --- | --- | --- | --- |
| T1 | Confirmed for M2 | `start_game_turn_flow` and `update_game_session` set, clear, and consume `GAME_PLAYER_DIRTY_TURN`. | Original Win32 callback registration is outside M2. |
| T2 | Confirmed for M2 | `rich4_first_turn_dispatch_plan` exhaustively maps every named result family and the update loop consumes that plan. | Speech/message side effects beyond the M2 SDL flags are deferred. |
| T3 | Confirmed for M2 | Human result paths stop on the dice/status panel and store distinct hint/refresh flags. | Literal cursor positioning and Windows refresh calls are outside M2. |
| T4 | Confirmed for M2 | Automatic result 2/5 records the required pre-action audit, then re-marks dirty or enters dice/movement. | Sale/card/tool execution is the named stop boundary. |
| T5 | Confirmed for M2 | Status countdown and pending-event code implements the P7 counters, ordering, high-bit records, and selected cleanup effects. | Producers and remaining handler effects are post-M2. |
| T6 | Confirmed for M2 | Turn rotation scans normal/special slots, updates day state, and records modeled win candidates. | Special-player actions and final victory/loss resolution are the named stop boundary. |

Audit summary:

| Area | Confirmed for M2 | Non-visual gap | Not implemented in M2 boundary |
| --- | ---: | ---: | ---: |
| Setup/state | 9 | 0 | 0 |
| Map loading/lifetime | 9 | 0 | 0 |
| Display tables/drawing | 14 | 0 | 0 |
| Intro/landing/actor/Autodesk Animator FLC | 10 | 0 | 0 |
| First-turn loop | 6 | 0 | 0 |
| **Total acceptance items** | **48** | **0** | **0** |

The zero non-visual gap applies only to the bounded M2 done boundaries. It does
**not** mean the whole game or deferred queue is implemented. Visual parity is
still a separate manual gate. AVI decoding, event economic/UI effects, status
producers/remaining handler internals, special-player actions, and final
win/loss flow remain explicitly post-M2.

## Cross-Platform AVI Backend Plan

The SDL codebase should stay SDL-first: the main loop owns timing, input,
surface/texture presentation, and fallback behavior. AVI playback should be an
optional video backend used only for the original intro movie paths.

Recommended backend:

- Use FFmpeg libraries (`libavformat`, `libavcodec`, `libswscale`, optionally
  `libswresample`) to demux/decode AVI. FFmpeg's official examples show the
  libavformat/libavcodec demux-and-decode API as the intended low-level path:
  https://ffmpeg.org/doxygen/6.1/demux_decode_8c-example.html
- Convert decoded frames to the SDL renderer's target format:
  - Current SDL 1.2 path: convert to RGB565 and blit into the existing
    `SDL_Surface`.
  - Future SDL2 path: upload frames to an `SDL_Texture`, preferably using a YUV
    texture when possible. SDL2 is explicitly cross-platform and provides a
    renderer/texture API with hardware or software backends:
    https://wiki.libsdl.org/SDL2/Introduction
- Keep FFmpeg optional at build time:
  - `HAVE_FFMPEG=1` enables AVI playback.
  - Without FFmpeg, skip AVI and continue with the existing `JUMP.MKF` intro.
  - Package Windows DLLs and Unix shared-library dependencies only in builds
    that enable AVI.
- License guardrails:
  - Link dynamically to FFmpeg libraries.
  - Build/use FFmpeg without `--enable-gpl` and without `--enable-nonfree`.
  - Document the exact FFmpeg source/build used for releases. FFmpeg's official
    legal checklist recommends dynamic linking for LGPL use and avoiding GPL or
    nonfree options: https://www.ffmpeg.org/legal.html

Alternative considered:

- Platform-native AVI APIs are rejected for M2/M3 because they would split
  Windows, Linux, macOS, and Miyoo behavior.
- Converting AVI files to another bundled format is not the primary path because
  this project aims to consume original resources directly. It can remain a
  fallback for platforms where FFmpeg is too heavy.

## SDL Implementation Progress

Status as of 2026-07-16:

- Implemented in `src/platform/sdl1/runtime_main.c` with bounded flow rules in
  `src/app/new_game_setup_flow.c` and `src/core/first_turn_dispatch.c`: setup
  background/resource order, setup
  selection/autofill flow, fixed post-map intro resource base `45`, first
  landing resources `Data.mkf[559..570]`, Autodesk Animator FLC header frame/delay timing for
  intro/landing playback, and an SDL `0x80`-style first-turn dirty flag. The
  first landing dispatcher now stops human players at the dice/action panel and
  routes AI players into the automatic SDL dice/move path. Landing pending state
  now uses a session-level one-based `0x475114` equivalent instead of
  per-player pending landing state.
- Implemented for returning setup reconstruction: canceling from the SDL map
  preview now rebuilds `NewGameSettings` from the committed `GameSession`
  snapshot, preserving stage/map, selected players, AI flags, fund, traffic,
  land-period, day-limit, and win-target options.
- Implemented for new-game commit fidelity: SDL now records the original
  `num_players = selected_player_count + 2` shape, normalizes the committed
  character selection markers like `0x4990F4` (`4 -> 2`, others `0`), and uses
  modeled fields from the original 12-entry character template table, including
  the AI `init_cash_ratio`, for initial cash/bank setup while human players
  remain half cash and half bank. Loan, special-finance, and original `f76[6]`
  player-to-player relation-balance fields are modeled and initialized, but are
  not folded into day-limit/target asset candidates until the original
  total-asset formula is confirmed.
- Implemented for dispatcher state shape: SDL players now carry the original
  low-bit visible/play-state value (`1` human, `2` computer), so
  `sub_40C912(0)` equivalent dispatch no longer has to encode humans as an SDL
  special-case result. The first-turn dispatcher now also records the latest
  raw result and branch family: action-hint (`0`), input-refresh (`1`),
  automatic (`2`/`5`), or return-only (`-1`, `3`, `4`, and `>5`).
- Implemented for automatic pre-action audit state: result `2`/`5` now records
  the player/result, marks sale/card/tool decisions as deferred, captures
  current global card/tool pool totals plus the current player's card/tool
  inventory totals, and records whether the status guard re-marked the player
  dirty. That guard is now limited to the original `0x418E2D` checks:
  nonzero delayed `+50..+53`, `+55` sleepwalk, or `+54` sleep/winter status.
  This does not execute sale, card, or tool effects.
- Implemented for human dispatcher UI state: the SDL dice/action panel now
  records separate session flags for the original action-hint path
  (`0x46CAFB`) and input-refresh path (`0x4196F1`) instead of only toggling
  panel visibility.
- Implemented for map display: `MAPDAT.MKF[gmap]` is preferred over
  `MAP.MKF[gmap * 2 + 1]`; special display resources are read from the two
  map-data table fields and loaded through `MAP.MKF[resource_id + 0x26]`;
  queued objects retain `0x1770 + index` / `0x1F40 + index`, owner, and
  variant-base fields. Built land/facility rows are now also queued as display
  objects with original ids `0x7D0 + index` / `0xFA0 + index`, owner bytes,
  map-rotation variants, and the verified map-view/overlay/stage-tile resource
  choices; unbuilt owned land/facility rows use common `MAP.MKF[25]`
  (`0x48AEA8`) and the owner player's character as the chunk. Road nodes now
  also retain their original object id from the node entry, derive the
  `0x40AF12` table category/index, and record the most recent pass/stop node
  object internally for later event dispatch.
  The main-view sub-tile projection helper now matches original `0x407A2C`
  coefficients from `0x474910..0x474913`; SDL still wraps that exact helper in
  its own viewport centering/origin transform.
  SDL now also derives a pending pass/stop event candidate kind for road-node,
  land, facility, listed-company/special, and second-special object families;
  this is classification only and does not execute the original event handlers.
  Pass candidates are now retained both as the latest pass node and as a
  per-move ordered pass-event sequence, including the same conservative
  property branch/owner/value snapshot used for land/facility stop candidates,
  so later event execution can consume each crossed node in movement order
  instead of only seeing the final pass.
  When movement ends, SDL also prepares a conservative route-event dispatch
  snapshot for the just-moved player: dispatch starts at the first non-empty
  pass event if one exists, otherwise at the stop event if present, otherwise
  marks the movement as having no route event work. This is readiness state
  only; it does not run pass or stop handlers yet. The dispatch snapshot now
  also normalizes the current pending pass/stop event into one current-event
  record (node, object id/type/index, event kind, and property branch/owner/
  value), so later handlers can consume pass and stop events through the same
  state shape. It also records ordered dispatch metadata (`total`, current
  ordinal, next phase, and next pass index), making the intended pass...
  pass...stop...done sequence explicit. A route-event advance helper can now
  promote the recorded next item to the current-event snapshot and recompute the
  following item. The main loop now enters a route-event candidate state after
  movement when pass/stop candidates exist, walks that ordered sequence,
  records the current-event handler family (road, property, stock-special,
  second-special, or unknown), and marks each current-event's economic/UI effect
  as deferred before turn rotation. Deferred events are retained as an ordered
  sequence with phase, ordinal, node, object id/type/index, event kind, and
  property branch/owner/value snapshots; SDL still does not apply pass/stop
  handler economic/UI effects.
  For land/facility stop candidates, SDL now also records the conservative
  property branch state (invalid index, unowned, owned by current player, owned
  by another player, or unsupported family) plus the current owner/value read
  from the session-copied property rows. Before each movement segment, SDL also
  records route branch candidates from the current node's four original
  `route_next` slots, excluding the previous node and preserving distinct valid
  targets, and records which candidate the current automatic path selected.
  Those branch candidates are now retained as a per-move ordered branch
  sequence, including node, previous node, option slots, selected option, and
  pending-interaction flag. Human-controlled movement now pauses on multi-way
  non-backtracking route choices, highlights the candidate nodes on the main
  map and the small map, consumes a click/confirm on one of those candidates,
  writes the selected
  route back to the current player `next_node`, and updates the latest branch
  sequence entry before movement resumes. AI/automatic movement still uses the
  original automatic selected candidate. This implements branch selection only;
  pass/stop event effects are still separate.
- Implemented for original map resource lifetime groups: common resources
  `MAP.MKF[24]`, `[25]`, `[26]`, five per-map view resources
  `MAP.MKF[gmap * 5 + 39 + i]`, `MAP.MKF[gmap + 79]`, stage-specific 17-entry
  resource groups, `Data.mkf[0x207]`, and `Data.mkf[396..415]` are now loaded
  and released with the SDL map/session assets.
- Implemented for original intro AVI dispatch state: SDL records the same
  selected filename branch as `0x41591f` (`AIRPLANE.AVI` for nonzero stage,
  otherwise the `FLYTW/FLYCHINA/FLYJP/FLYUS` map table) and explicitly marks it
  skipped because no cross-platform AVI backend is linked yet. The visible intro
  remains the existing `JUMP.MKF` sequence.
- Implemented for first-turn rotation structure: SDL now keeps an original-style
  rotation index that scans from active normal players into special-player
  indices `4..7` and wraps through `8 -> 0`. Active special-player slots now
  run the `0x498e34..37` status countdown/high-bit cleanup portion of
  `0x41C84F`; the `0x498e34` high-bit cleanup also clears the special-player
  actor `0x40` state bit, records the `0x40B8D8`/`0x40B93B` handler family,
  and captures the original special-player turn index `4..7` plus the actor
  refresh path used by the following `0x40B93B` call. High-bit
  `0x498e35..37` cleanup is recorded as clear-only, while
  special-player event execution remains deferred.
- Implemented for internal day-limit tracking: each original-style `8 -> 0`
  rotation wrap increments SDL `elapsed_days` like original `0x4990e4`, records
  when `elapsed_days >= day_limit` like the `0x49911c <= 0x4990e4` check, and
  leaves the price-index HUD row unchanged. The actual day-limit victory/loss
  flow and any original date/day display location are still deferred.
- Implemented for part of `0x41d89e` win-candidate detection: SDL now scans
  active players, computes the highest currently modeled asset total
  (cash + bank + stock market value + session land/facility value), and records
  the candidate player/reason when either the day limit has been reached or the
  win target is met. The land/facility rows are parsed from original
  `0x498e84`/`0x498e88` fields (`owner`, `type`, `level`, land price,
  per-level/house price), copied into `GameSession`, and recomputed into the
  per-player fixed-asset cache from that session state. Dynamic mutations from
  buy, build, sale, transfer, and event paths, winner dialogue, player-state
  rewrites, and final victory/loss UI flow are still deferred.
- Implemented for automatic-path dirty re-marking: SDL now tracks the original
  delayed-status fields used by `0x418C55` (`+50..+53`, `+54`, `+55`) and
  re-marks `GAME_PLAYER_DIRTY_TURN` instead of entering dice/move when those
  status fields are nonzero. The automatic-path guard is intentionally limited
  to the original `0x418E2D` checks; `+56`, `+57`, `+59`, `+60`, and `+62`
  still tick during turn rotation but do not cause this automatic pre-action
  re-mark path by themselves.
- Implemented for part of `0x40B93B` actor-state selection: SDL chooses
  status-specific `Data.mkf[128 + character * 21 + part]` resources for
  sleepwalk, special road-node state, delayed sleep/status display, and the
  `who_plays & 0x40` frozen/winter-style state before falling back to normal
  traffic-method standing/moving parts, then to legacy standing/walking parts if
  those resources are unavailable. Traffic method 3 also draws the extra
  `base + 12` resource loaded by the original path when that resource exists;
  special road-node state now also draws the original extra `base + 15`
  resource after the `base + 13/14` status part when it is available.
- Implemented for status countdown progress: SDL decrements the `+50..+57`,
  `+59`, `+60`, `+61`, and `+62` status counters when original-style turn
  rotation selects a normal player, converts counters that reach zero into
  `0x80` high-bit pending status markers, and uses the original `+51`
  disappearing rule that tests the low 6 bits after decrement. It skips
  `+54/+55/+57` while any delayed `+50..+53` byte remains nonzero. When a
  countdown reaches a high-bit pending marker, the selected player is marked
  dirty so the SDL dirty path consumes the event.
- Implemented for the `+61` per-turn `0x40DF69` side effect: SDL now models the
  original `player_info +76` `f76[6]` relation-balance slots and, before
  decrementing a non-high-bit `+61` counter, subtracts `price_index * 20` from
  both the current-player-to-linked-player and linked-player-to-current-player
  slots when they are nonzero, clamping at zero like the original helper. The
  producers that populate those balances and any related UI are still deferred.
- Implemented for high-bit status marker cleanup: when the SDL dirty path
  selects a player, it consumes `+50..+53`, then only consumes
  `+54/+55/+57` if the original `+50..+53` dword block is zero, then consumes
  `+56`, `+59`, `+60`, `+61`, and `+62`. SDL records the original handler
  family for each consumed marker (`0x40D6BE`, `0x40D4E5`, `0x43D7BF`,
  `0x43EE6E`, `0x40B8D8`/`0x40B93B`, `0x40B93B`, `0x40CC1A`, or clear-only).
  The `+55` sleepwalk-ending marker now restores the player's saved traffic
  method and dice count before clearing; the `+61` god/alliance marker now
  clears the current player and linked player's `god_info`/`+61` state like
  `0x40CC1A`. The `+54` sleep/winter-ending marker clears the
  `who_plays & 0x40` actor state bit before returning to normal actor
  selection. The remaining original handler internals for those markers are
  still deferred.
- Implemented for special display drawing: the SDL renderer culls to the
  original `0..0x1C` tile window, computes rotation-adjusted variant chunks,
  sorts with an SDL equivalent of the original `0x48A44C` key shape
  (`screen_y << 4` low word plus visible-list index high word), and applies
  owner colors by replacing the SPR palette entry equivalent to original
  `graph + 0x1FE` before drawing.
- Active slice P8 was completed on 2026-07-06 by turning the remaining partial
  items into the explicit deferred post-M2 queue below. No SDL behavior was
  changed for P8.

### Historical Workspace Sync (2026-07-11)

The M2 slices remain complete through P8. The current working tree also contains
the following uncommitted follow-up progress. These items improve the playable
SDL path but do not expand M2 into the deferred event-handler families:

- Added an original-resource options panel using `Data.mkf[3]`, reachable from
  both the main menu and the in-game top panel. It models game speed,
  animation, music and sound levels, autosave, view mode, the original hotkey
  table, apply/cancel behavior, and the menu/game-specific action rows.
- Added 72-byte `RICH4.CFG` read/write support with original-shaped defaults and
  range validation. SDL-only music-bank and selected-track state is persisted
  separately in `RICH4-SDL.CFG`.
- Added two eight-track music banks, `Original` and `MultiverseJourney`, mapped
  to CD tracks 2 through 9. The options panel can switch banks, select and
  preview a track, restore the previous music on cancel, and continue to the
  next track after a one-shot track finishes. Music lookup accepts OGG/WAV and
  case variants.
- Added movement-loop sound loading directly from `Effect.mkf` resources 44,
  45, 46, and 53 for traffic methods 0 through 3, plus resource 47 for the
  modeled sleepwalking family. Playback follows movement start/stop and method
  changes.
- Refined the original dice/action sequence: the player actor's action resource
  runs before the shared `Panel.mkf` dice animation, Autodesk Animator FLC graph timing follows
  the configured game-speed table, and movement advances on the original-style
  20 ms timer cadence with speed-derived delays.
- Corrected the main-map center from `(260, 220)` to the original viewport
  center `(220, 260)`, corrected the minimap world conversion to the original
  `(5696 * coordinate) >> 16` scale, and aligned minimap click conversion with
  the same transform.
- Refined actor and landing presentation, including status/traffic action-part
  selection, first-landing character anchors, actor-following view restoration
  when dice/action begins, and removal of an incorrect route overlay from the
  main terrain view.
- Updated music documentation and Makefile dependencies for the new audio/MKF
  integration. The local font assets were also replaced (`rich4.ttf` removed,
  `rich4.ttc` updated); this is an asset change and is not an M2 behavior claim.

Current workspace report:

```text
Active slice: post-P8 follow-up integration
Changed: Makefile, README.md, assets/fonts, assets/music/README.md,
         src/audio.c, src/audio.h, src/main_sdl.c, src/music.c, src/music.h
Verified: `make`; `make test` (2026-07-11, both passed)
Stopped before: card/tool/sale/property/stock/special-event economic or UI effects
Deferred notes: the Deferred Post-M2 Queue below remains authoritative
```

### Non-Visual Closure Sync (2026-07-16)

- Corrected Stage 1 to the exact eleventh-callback counter edge shown by
  `0x404E44`.
- Stage 2 now completes on left/right mouse-button release, any key press, or
  controller button press, in addition to natural animation completion; every
  path produces the same SDL scene transition result.
- Extracted and exhaustively tested the first-turn `0/1/2/5/return-only`
  dispatcher mapping, including both automatic status outcomes.
- Extended all-eight-map MAPDAT/fallback selection coverage, repeated
  ResourceStore archive/cache lifetime coverage, and confirmed actor-state
  precedence/combination coverage.
- Re-audited the refactored source locations. The bounded M2 acceptance set has
  zero remaining non-visual gaps; visual parity and the deferred post-M2 queue
  remain separate.

```text
Active slice: M2 non-visual closure
Changed: Makefile, src/app/new_game_setup_flow.[ch],
         src/core/first_turn_dispatch.[ch],
         src/platform/sdl1/runtime_main.c, tests, docs/evidence, this document
Verified: make test; make test-render; make tools; make sdl;
          make test-original-data
Stopped before: visual parity work and every deferred post-M2 behavior
Deferred notes: listed in Deferred Post-M2 Queue
```

## Deferred Post-M2 Queue

These are intentionally outside this M2 target. They should be picked up by
later targets as separate bounded slices instead of being folded back into this
document:

- Optional AVI decode/playback backend. M2 only records the original selected
  AVI filename and keeps the `JUMP.MKF` fallback visible.
- Remaining original first-turn dispatcher side effects and literal Windows
  callback/input-refresh details beyond the SDL action-hint/input-refresh
  state records.
- Literal reconstruction from every original setup/global address. M2 keeps the
  SDL session-backed returning-setup reconstruction for modeled fields.
- Status producers, remaining high-bit status event handler internals, and
  `+61` relation-balance producers/UI.
- Card, tool, sale, land, stock, fortune, news, magic-house, and other special
  event execution families.
- Buy, build, sale, transfer, and event mutations for the modeled land/facility
  rows.
- Full day-limit and target-asset victory/loss resolution, including final
  dialogue, player-state rewrites, and end-game UI flow.
- Special-state actor-selection edge cases not already represented by the SDL
  actor resource family selection.
- Branch-choice presentation refinements beyond the current main-map and
  small-map candidate click/confirm path.
- Pass/stop handler economic and UI effects beyond the current
  handler-family/current-event/deferred-effect skeleton.
- Special-player action and event execution.

Latest progress report:

```text
Active slice: P8 - Deferred post-M2 queue
Changed: docs/m2-original-start-first-turn-target.md
Verified: make; make test
Stopped before: implementing any deferred post-M2 behavior
Deferred notes: listed in Deferred Post-M2 Queue
```
