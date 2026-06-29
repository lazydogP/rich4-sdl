# Public API ownership and null convention

All public headers under `src/` follow `api_contract.h` version 1.

- `const T *` input is borrowed only for the call. A struct that retains it uses `RICH4_BORROWED_LIFETIME`; the caller must keep it alive for the documented owner lifetime.
- Mutable state and `out_*` point to caller-owned storage and are not retained. Output pointers are required unless declared `RICH4_OPTIONAL_OUT`.
- A fallible API initializes required outputs to zero/null before work whenever an empty representation exists. On failure the caller may run the matching null-safe cleanup.
- `RICH4_OWNED` results transfer ownership on success and name their matching `close`, `free`, or `destroy` operation. Cleanup functions marked `RICH4_NULL_SAFE` accept null.
- Returned error-name strings and queue-current pointers are borrowed static/interior views and must not be freed.
- Caller-provided FLC pixel/index/dirty buffers and RGB565 framebuffers must outlive their decoder/player/render operation; modules never free them.

Ownership table:

| API | Result/lifetime | Release |
| --- | --- | --- |
| `rich4_mkf_open` | owned `Rich4Mkf`; historical exception: a diagnostic handle may also be returned on failure | `rich4_mkf_close` |
| `rich4_mkf_read_resource` | owned byte buffer on success | `free` by caller, or transfer to `ResourceStore` |
| `rich4_resource_store_create` | owned store containing archives/cache bytes | `rich4_resource_store_destroy` |
| `rich4_resource_store_borrow` | borrowed handle valid until store destruction | none |
| `rich4_map_data_parse` | owned arrays in caller `MapData` | `rich4_map_data_free` |
| FLC decoder/player | borrowed encoded data and caller storage retained for decoder/player lifetime | caller owns every buffer |
| SPR/SMP parsers | borrowed data for the call; output descriptors contain offsets only | none |
| scene/audio queues, core and view APIs | caller storage, no retained ownership unless explicitly marked | none |

Test-only fault headers exist only under their corresponding `*_TESTING` define and are not production APIs.

