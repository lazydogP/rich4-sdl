#ifndef RICH4_SDL1_GAME_LAZY_ASSETS_H
#define RICH4_SDL1_GAME_LAZY_ASSETS_H

#include "platform/sdl1/app_asset_types.h"

enum { GAME_LAZY_RECENT_BYTES = 4 * 1024 * 1024 };

/* Bind existing asset slots to their verified archive/index. Only scope 2
 * defers bytes; complete inventory tests and setup/menu retain their policy. */
int game_lazy_assets_init(GamePreviewAssets *assets);
void game_lazy_assets_finish(GamePreviewAssets *assets);
int game_lazy_asset_deferred(const GamePreviewAssets *assets, const void *slot);
int game_asset_ready(const GamePreviewAssets *assets, const int *available);
int game_asset_require_data(const GamePreviewAssets *assets, const void *data_slot);
int game_asset_graph_frames(const GamePreviewAssets *assets, const void *data_slot, size_t size);
int game_asset_graph_delay(const GamePreviewAssets *assets, const void *data_slot, size_t size);
void game_lazy_assets_prepare_intro(GamePreviewAssets *assets);
void game_lazy_assets_begin_frame(GamePreviewAssets *assets);
/* Drop only inactive retained bytes, leaving active leases untouched. */
void game_lazy_assets_clear_recent(GamePreviewAssets *assets);
void game_lazy_assets_free(GamePreviewAssets *assets);
Rich4ResourceHandle game_lazy_asset_handle(const GamePreviewAssets *assets,
                                          const unsigned char *data);

#endif
