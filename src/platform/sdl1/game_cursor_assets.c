#include <string.h>

#include <SDL/SDL.h>

#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/spr_smp.h"

/* R10.1d: cursor resource acquisition, lifecycle and drawing moved verbatim
 * out of runtime_main.c. The resource-store handle remains owned by the
 * process-wide store; CursorAssets only keeps its typed view. */

int game_load_cursor_assets(ResourceStore *store,
                            Rich4Mkf *mkf,
                            CursorAssets *assets)
{
    Rich4MkfError err;
    Rich4SprSmpError spr_err;
    Rich4SprSmpChunk chunk;

    memset(assets, 0, sizeof(*assets));
    err = rich4_resource_store_borrow(store, mkf, 0, &assets->resource, NULL);
    if (err != RICH4_MKF_OK) {
        memset(assets, 0, sizeof(*assets));
        return 0;
    }
    assets->data = (unsigned char *)assets->resource.data;
    assets->size = assets->resource.size;
    if (!rich4_spr_smp_probe(assets->data, assets->size, &assets->spr)) {
        memset(assets, 0, sizeof(*assets));
        return 0;
    }
    assets->chunk_index = 41;
    spr_err = rich4_spr_smp_get_chunk(assets->data,
                                      assets->size,
                                      &assets->spr,
                                      assets->chunk_index,
                                      &chunk);
    if (spr_err != RICH4_SPR_SMP_OK) {
        memset(assets, 0, sizeof(*assets));
        return 0;
    }
    assets->available = 1;
    return 1;
}

void game_free_cursor_assets(CursorAssets *assets)
{
    if (assets == NULL) {
        return;
    }
    memset(assets, 0, sizeof(*assets));
}

int game_draw_resource_cursor(SDL_Surface *screen,
                              const CursorAssets *assets,
                              const VirtualCursor *cursor)
{
    Rich4SprSmpChunk chunk;
    Rich4SprSmpError err;

    if (assets == NULL || !assets->available) {
        return 0;
    }
    err = rich4_spr_smp_get_chunk(assets->data,
                                  assets->size,
                                  &assets->spr,
                                  assets->chunk_index,
                                  &chunk);
    if (err != RICH4_SPR_SMP_OK) {
        return 0;
    }
    return draw_chunk_at(screen, assets->data, &chunk, cursor->x, cursor->y, 1);
}
