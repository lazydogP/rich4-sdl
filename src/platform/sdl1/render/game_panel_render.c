#include "platform/sdl1/game_lazy_assets.h"
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>

#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "resource/flc_animation.h"
#include "resource/flc_player.h"
#include "platform/sdl1/render/game_flc_cache.h"
#include "resource/spr_smp.h"
#include "view/flc_compositor.h"
#include "view/view_constants.h"

/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c. */

int draw_game_panel_chunk(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          int resource_index,
                          size_t chunk_index,
                          int x,
                          int y,
                          int transparent_zero)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || assets == NULL ||
        resource_index < 0 ||
        resource_index >= GAME_PANEL_RESOURCE_COUNT ||
        !game_asset_ready(assets, &assets->ui.has_panel[resource_index])) {
        return 0;
    }
    if (rich4_spr_smp_get_chunk(assets->ui.panel_data[resource_index],
                                    assets->ui.panel_size[resource_index],
                                    &assets->ui.panel_spr[resource_index],
                                    chunk_index,
                                    &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    if (memcmp(assets->ui.panel_spr[resource_index].signature, "SPR", 4) == 0) {
        return draw_spr_chunk_at(screen,
                                 assets->ui.panel_data[resource_index],
                                 assets->ui.panel_size[resource_index],
                                 &assets->ui.panel_spr[resource_index],
                                 &chunk,
                                 x + chunk.x,
                                 y + chunk.y,
                                 transparent_zero);
    }
    return draw_chunk_topleft(screen,
                              assets->ui.panel_data[resource_index],
                              &chunk,
                              x,
                              y,
                              transparent_zero);
}

int draw_game_panel_chunk_at(SDL_Surface *screen,
                             const GamePreviewAssets *assets,
                             int resource_index,
                             size_t chunk_index,
                             int center_x,
                             int center_y,
                             int transparent_zero)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || assets == NULL ||
        resource_index < 0 ||
        resource_index >= GAME_PANEL_RESOURCE_COUNT ||
        !game_asset_ready(assets, &assets->ui.has_panel[resource_index])) {
        return 0;
    }
    if (rich4_spr_smp_get_chunk(assets->ui.panel_data[resource_index],
                                    assets->ui.panel_size[resource_index],
                                    &assets->ui.panel_spr[resource_index],
                                    chunk_index,
                                    &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    if (memcmp(assets->ui.panel_spr[resource_index].signature, "SPR", 4) == 0) {
        return draw_spr_chunk_at(screen,
                                 assets->ui.panel_data[resource_index],
                                 assets->ui.panel_size[resource_index],
                                 &assets->ui.panel_spr[resource_index],
                                 &chunk,
                                 center_x,
                                 center_y,
                                 transparent_zero);
    }
    return draw_chunk_at(screen,
                         assets->ui.panel_data[resource_index],
                         &chunk,
                         center_x,
                         center_y,
                         transparent_zero);
}

int draw_confinement_panel_chunk(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 int resource,
                                 size_t chunk_index,
                                 int anchor_x,
                                 int anchor_y)
{
    Rich4SprSmpChunk chunk;
    if (screen == NULL || assets == NULL ||
        resource < 0 || resource >= GAME_PANEL_RESOURCE_COUNT ||
        !game_asset_ready(assets, &assets->ui.has_panel[resource]) ||
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[resource], assets->ui.panel_size[resource],
            &assets->ui.panel_spr[resource], chunk_index,
            &chunk) != RICH4_SPR_SMP_OK)
        return 0;
    /*
     * sub_456418 treats SMP x/y as the hotspot measured from the caller's
     * anchor.  The negative offsets on Panel[63] character chunks and the
     * large positive offsets on Panel[64] release actors both require this.
     */
    return draw_chunk_topleft(
        screen, assets->ui.panel_data[resource], &chunk,
        anchor_x - chunk.x, anchor_y - chunk.y, 1);
}

static uint16_t map_flc_rgb565_pixel(SDL_Surface *screen, uint16_t pixel)
{
    if (screen->format->Rmask == 0xf800 && screen->format->Gmask == 0x07e0 &&
        screen->format->Bmask == 0x001f && screen->format->Amask == 0) return pixel;
    Uint8 r = (Uint8)(((pixel >> 11) & 0x1f) * 255 / 31);
    Uint8 g = (Uint8)(((pixel >> 5) & 0x3f) * 255 / 63);
    Uint8 b = (Uint8)((pixel & 0x1f) * 255 / 31);
    return (uint16_t)SDL_MapRGB(screen->format, r, g, b);
}

int draw_game_graph_frame_step_data(SDL_Surface *screen,
                                    const unsigned char *data,
                                    size_t size,
                                    int frame_index,
                                    int x,
                                    int y,
                                    const unsigned char *source_pixels,
                                    int source_pitch)
{
    Rich4FlcHeader header;
    Rich4FlcPlayer local_player, *player;
    Rich4FlcError error = RICH4_FLC_OK;
    size_t count;
    uint16_t *pixels = NULL;
    unsigned char *indices = NULL, *dirty = NULL;
    int left, top, right, bottom, local_y;
    if (screen == NULL || data == NULL || source_pixels == NULL ||
        rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) return 0;
    if (frame_index < 0) frame_index = 0;
    if (frame_index >= header.frame_count) frame_index = header.frame_count - 1;
    if (frame_index < 0) return 0;
    count = (size_t)header.width * header.height;
    player = game_flc_cache_decode(data, size, frame_index);
    if (player == NULL) {
        /* Unleased caller buffers keep their immediate-call lifetime. */
        pixels = calloc(count, sizeof(*pixels));
        indices = calloc(count, 1);
        dirty = calloc(count, 1);
        if (pixels == NULL || indices == NULL || dirty == NULL) {
            free(pixels); free(indices); free(dirty); return 0;
        }
        player = &local_player;
        error = rich4_flc_player_init(player, data, size, pixels, indices, dirty, count, 0);
        if (error == RICH4_FLC_OK)
            error = rich4_flc_player_decode_to(player, frame_index, NULL);
    }
    left = x < 0 ? -x : 0;
    top = y < 0 ? -y : 0;
    right = header.width < SCREEN_W - x ? header.width : SCREEN_W - x;
    bottom = header.height < SCREEN_H - y ? header.height : SCREEN_H - y;
    if (error == RICH4_FLC_OK) {
        for (local_y = top; local_y < bottom; local_y++) {
            size_t offset = (size_t)local_y * header.width;
            uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels +
                                        (y + local_y) * screen->pitch);
            const uint16_t *source_row = (const uint16_t *)(const void *)(
                source_pixels + (y + local_y) * source_pitch);
            int local_x;
            for (local_x = left; local_x < right; local_x++) {
                /* Index zero copies this draw's background, never a cached
                 * background. Retained state consists only of FLC pixels. */
                row[x + local_x] = player->indices[offset + local_x] == 0
                    ? source_row[x + local_x]
                    : map_flc_rgb565_pixel(screen, player->pixels[offset + local_x]);
            }
        }
    }
    free(pixels); free(indices); free(dirty);
    return error == RICH4_FLC_OK;
}

int draw_game_panel_graph_frame_step(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     int resource_index,
                                     int frame_index,
                                     int x,
                                     int y,
                                     const unsigned char *source_pixels,
                                     int source_pitch)
{
    if (assets == NULL ||
        resource_index < 0 ||
        resource_index >= GAME_PANEL_RESOURCE_COUNT ||
        !game_asset_ready(assets, &assets->ui.has_panel_graph[resource_index])) {
        return 0;
    }
    return draw_game_graph_frame_step_data(screen,
                                           assets->ui.panel_data[resource_index],
                                           assets->ui.panel_size[resource_index],
                                           frame_index,
                                           x,
                                           y,
                                           source_pixels,
                                           source_pitch);
}

int draw_game_panel_graph_frame(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                int resource_index,
                                int frame_index,
                                int x,
                                int y)
{
    int ok;
    if (screen == NULL) return 0;
    SDL_LockSurface(screen);
    ok = draw_game_panel_graph_frame_step(screen, assets, resource_index,
        frame_index, x, y, screen->pixels, screen->pitch);
    SDL_UnlockSurface(screen);
    return ok;
}

int draw_game_graph_frame_data(SDL_Surface *screen,
                               const unsigned char *data,
                               size_t size,
                               int frame_index,
                               int x,
                               int y)
{
    int ok;
    if (screen == NULL) return 0;
    SDL_LockSurface(screen);
    ok = draw_game_graph_frame_step_data(screen, data, size, frame_index,
        x, y, screen->pixels, screen->pitch);
    SDL_UnlockSurface(screen);
    return ok;
}
