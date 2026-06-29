#include <stdlib.h>
#include <string.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/render/game_map_render.h"
#include "platform/sdl1/render/game_flc_cache.h"
#include "platform/sdl1/game_lazy_assets.h"

/* The store owns decoded bytes. Runtime scene packs own explicit leases;
 * the complete asset-audit mode retains permanent borrows. */

static int game_preview_resource_needed(const GamePreviewAssets *assets,
                                        Rich4Mkf *archive, size_t index)
{
    if (assets->load_scope == 0) return 1;
    if (archive == assets->data_archive) {
        if (assets->load_scope == 1) return index == 2 || index == 520;
        if (assets->load_scope != 2)
            return index == 2 || index == 3 || index == 440 ||
                   index == 0x205 || index == 520;
        if (index >= 128 && index < 380)
            return (assets->active_characters & (1u << ((index - 128) / 21))) != 0;
        if (index >= 559 && index < 571)
            return (assets->active_characters & (1u << (index - 559))) != 0;
    }
    if (archive == assets->jump_archive) {
        if (assets->load_scope != 2) return assets->load_scope == 3 && index <= 44;
        if (index < 8) return (int)index == assets->active_background;
        if (index >= 9 && index <= 44)
            return (assets->active_characters & (1u << ((index - 9) / 3))) != 0;
        /* 0x415872 reads only the chosen players' two intro FLC groups. */
        if (index >= 47 && index <= 70)
            return (assets->active_characters & (1u << ((index - 47) % 12))) != 0;
    }
    if (archive == assets->panel_archive && assets->load_scope != 2) return 0;
    if (archive == assets->map_archive) {
        if (index < 16 && (index & 1u)) return 1;
        if (assets->load_scope != 2) return 0;
        if (index < 16) return (int)(index / 2) == assets->active_background;
        if (index >= 27 && index < 39)
            return (assets->active_characters & (1u << (index - 27))) != 0;
    }
    return 1;
}

Rich4MkfError game_preview_borrow_resource(GamePreviewAssets *assets,
                                           Rich4Mkf *mkf,
                                           size_t resource_index,
                                           unsigned char **out_data,
                                           size_t *out_size,
                                           Rich4MkfResourceInfo *out_info)
{
    Rich4ResourceHandle handle;
    Rich4ResourceHandle *grown;
    Rich4MkfError err;
    size_t next_capacity;

    if (out_data != NULL) {
        *out_data = NULL;
    }
    if (out_size != NULL) {
        *out_size = 0;
    }
    if (assets == NULL || assets->resource_store == NULL || out_data == NULL || out_size == NULL) {
        return RICH4_MKF_ERR_FORMAT;
    }
    if (!game_preview_resource_needed(assets, mkf, resource_index))
        return RICH4_MKF_ERR_RANGE;
    if (game_lazy_asset_deferred(assets, out_data))
        return RICH4_MKF_ERR_RANGE;
    err = (assets->load_scope ? rich4_resource_store_acquire : rich4_resource_store_borrow)(assets->resource_store,
                                      mkf,
                                      resource_index,
                                      &handle,
                                      out_info);
    if (err != RICH4_MKF_OK) {
        return err;
    }
    if (assets->borrowed_resource_count == assets->borrowed_resource_capacity) {
        next_capacity = assets->borrowed_resource_capacity == 0
            ? 32
            : assets->borrowed_resource_capacity * 2;
        if (next_capacity < assets->borrowed_resource_capacity ||
            next_capacity > (size_t)-1 / sizeof(*grown)) {
            if (assets->load_scope) rich4_resource_store_release(assets->resource_store, &handle);
            return RICH4_MKF_ERR_MEMORY;
        }
        grown = (Rich4ResourceHandle *)realloc(assets->borrowed_resources,
                                               next_capacity * sizeof(*grown));
        if (grown == NULL) {
            if (assets->load_scope) rich4_resource_store_release(assets->resource_store, &handle);
            return RICH4_MKF_ERR_MEMORY;
        }
        assets->borrowed_resources = grown;
        assets->borrowed_resource_capacity = next_capacity;
    }
    assets->borrowed_resources[assets->borrowed_resource_count++] = handle;
    /* Borrowed presentation views are read-only and never own this cache. */
    *out_data = (unsigned char *)handle.data;
    *out_size = handle.size;
    return RICH4_MKF_OK;
}

void game_preview_release_borrowed_view(const void *data)
{
    (void)data;
}

Rich4ResourceHandle game_preview_handle_for_borrowed_view(const GamePreviewAssets *assets,
                                                  const unsigned char *data)
{
    size_t i;
    if (assets != NULL && data != NULL) {
        for (i = 0; i < assets->borrowed_resource_count; i++) {
            if (assets->borrowed_resources[i].data == data) return assets->borrowed_resources[i];
        }
    }
    return game_lazy_asset_handle(assets, data);
}

int game_load_raw_mkf_resource(GamePreviewAssets *assets,
                               Rich4Mkf *mkf,
                               size_t resource_index,
                               unsigned char **out_data,
                               size_t *out_size)
{
    unsigned char *data = NULL;
    size_t size = 0;

    if (mkf == NULL || out_data == NULL || out_size == NULL) {
        return 0;
    }
    if (game_preview_borrow_resource(assets,
                                     mkf,
                                     resource_index,
                                     &data,
                                     &size,
                                     NULL) == RICH4_MKF_OK &&
        data != NULL && size > 0) {
        *out_data = data;
        *out_size = size;
        return 1;
    }
    game_preview_release_borrowed_view(data);
    return 0;
}

void game_free_preview_assets(GamePreviewAssets *assets)
{
    size_t i;
    if (assets == NULL) {
        return;
    }
    /* Derived decoders borrow these bytes: retire before the final lease. */
    game_flc_cache_clear();
    game_lazy_assets_free(assets);
    game_reset_ground_render_cache();
    for (i = 0; i < GAME_BACKGROUND_COUNT; i++) {
        rich4_map_data_free(&assets->map.maps[i]);
    }
    if (assets->load_scope) {
        for (i = 0; i < assets->borrowed_resource_count; i++)
            rich4_resource_store_release(assets->resource_store, &assets->borrowed_resources[i]);
    }
    /* Tool-robot bytes are ResourceStore borrows, just like every other
     * preview byte.  ResourceStore is destroyed after this object. */
    free(assets->borrowed_resources);
    memset(assets, 0, sizeof(*assets));
}
