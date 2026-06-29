# Map-object information bubble (`0x417559`)

Original `Rich4.exe` behavior verified against IDA Pro output.

## Original trigger and lifetime

`sub_417E26` handles the main window messages. The map branch calls
`sub_417559(mouse_x, mouse_y)` from `WM_LBUTTONDOWN` (`0x201`, call site
`0x4186BE`), not from `WM_MOUSEMOVE`. A valid object is therefore shown when the
left button is pressed and remains visible while the press is held.

The same window procedure clears it in two places: `WM_MOUSEMOVE` reaches
`sub_417C67` at `0x418B70`, and `WM_LBUTTONUP` reaches `sub_417C67` at
`0x418881`. `sub_417C67` restores the saved map region and resets
`dword_475270`. The `SetTimer(..., 0x1F4, ...)` at `0x4180F3` is installed by
the `0x401` map initialization path; its `WM_TIMER` branch only animates the
map-side cursor when `dword_475270 == 0`, and explicitly returns while this
bubble is active. It is not the bubble's hide timer.

`sub_417559` calls `sub_40A9D7(x, y - 40)` to resolve the map object. A valid
object immediately restores the previous presentation (`sub_417191(0)`), draws
the bubble, and sets `dword_475270 = 1`. Moving away, entering a modal window, or
leaving the map causes the normal scene to be restored, so the bubble is a
transient press overlay rather than a separate dialog.

## Hit testing and object range

`sub_40A9D7` first calls `sub_409B18(0)` and then reads a 16-bit object ID from
the synchronized `440 * 440` hit map at `0x474938`:

```c
id = *(uint16 *)(0x474938 + 2 * (map_x + 440 * map_y));
```

`sub_409B18` rebuilds the hit map from the same projected, depth-sorted object
queue used by map rendering. It uses the non-zero pixels of each SPR/SMP mask,
not a fixed rectangle. The effective ID families are:

| ID | Original object family |
| --- | --- |
| `1..1999` | route nodes |
| `2001..3999` | ordinary land |
| `4001..5999` | facilities |
| `6001..7999` | company/auxiliary map objects |
| `8001..9999` | other static/special map objects |
| `0x8000` family | ordinary actors and special/dynamic actors |

The original validity guard also requires a route node to have a non-empty
route name. Actors are not excluded: ordinary actors, special actors, and the
dynamic map objects emitted by the actor renderer all participate in the same
hit map.

There is one important distinction between rendering and hit testing: the
generic land/facility/auxiliary masks are painted for their whole ID ranges
even when a level-0 unowned land or facility has no render sprite. Route
masks use the local map coordinate of `sub_40A9D7`, namely projected screen
`y - 40`.

## Bubble resource and placement

`sub_407AD2` loads the bubble resource with
`sub_450441(0x48A0E4, 517)`: resource `0x48A0E4` is `Data.mkf`, so this is
`Data.mkf[0x205]`, not a `Panel.mkf` entry. The resource supplies the bubble
dimensions at offsets `+12/+14` and four directional/placement variants at
`+12 + 12 * mode`.

For the local map coordinate `map_y = mouse_y - 40`, the original chooses among
four variants:

* below/right of the cursor when there is not enough room above and the cursor
  is not near the right edge;
* above/right when there is room above;
* below/left near the right edge;
* above/left near the right edge and when there is room above.

The bubble is blitted by `sub_456418`. Text uses the original 16-pixel Big5
style configured by `sub_44F9D8`/`sub_44FABC`; its anchor is offset from the
cursor by `width / 2 + 9`, and its vertical offset is `+42` below or `-96`
above. The original captures the affected region before drawing the overlay
and restores it through `sub_417C67`.

## Text coverage

The `0x417559` branches reproduce the original records:

* routes: route name;
* land: owner, “empty land” or land type, level, and formatted value/toll;
* facilities: owner, level/type, purchase/fee value, fixed `$1000` branch, or
  the selected research/tool label;
* company/auxiliary objects: company name and `餘股:%d` (the original `+0x30`
  available-shares field);
* special static map objects: object label;
* actor IDs: ordinary actor names and the original special/dynamic label tables.

Money formatting follows `sub_452793`: decimal digits with comma separators.
The text and label bytes are taken from the original Big5 string/table data
(`0x463819`, `0x46390c`, `0x475138`, `0x475150`, `0x475164`, `0x47ed6a`, and
`0x47ed76`) and drawn by `sub_44FABC`.

## Facility label strings

The facility label table at `0x475150` contains five pointers into the
original string block `0x463847..0x46386B`: 公  園, 旅  館, 購物中心,
加油站, and 研究所. The gas-station branch at `0x4179F5` reads table
entry `[3]`, whose pointer is `0x46385E` and bytes are
`A5 5B AA 6F AF B8 00` (加油站).
