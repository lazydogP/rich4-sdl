/* Full replay decoder/compositor reference for pixel-equivalence regression. */
#define draw_game_panel_chunk reference_draw_game_panel_chunk
#define draw_game_panel_chunk_at reference_draw_game_panel_chunk_at
#define draw_confinement_panel_chunk reference_draw_confinement_panel_chunk
#define draw_game_graph_frame_step_data reference_draw_game_graph_frame_step_data
#define draw_game_panel_graph_frame_step reference_draw_game_panel_graph_frame_step
#define draw_game_panel_graph_frame reference_draw_game_panel_graph_frame
#define draw_game_graph_frame_data reference_draw_game_graph_frame_data
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>

#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "resource/flc_animation.h"
#include "resource/flc_player.h"
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
        !assets->ui.has_panel[resource_index]) {
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
        !assets->ui.has_panel[resource_index]) {
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
        !assets->ui.has_panel[resource] ||
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
    /* The FLC decoder already produces RGB565.  On the original RGB565
     * surface the blit path preserves those bits; do not round-trip through
     * 8-bit RGB here, because that hides the exact native-format rule. */
    if (screen->format->Rmask == 0xf800 &&
        screen->format->Gmask == 0x07e0 &&
        screen->format->Bmask == 0x001f &&
        screen->format->Amask == 0)
        return pixel;
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
    Rich4FlcPlayer player;
    Rich4FlcError error;
    size_t pixel_count;
    uint16_t *pixels;
    uint16_t *source;
    uint16_t *composited;
    unsigned char *indices;
    unsigned char *dirty;
    int local_y;

    if (screen == NULL || data == NULL || source_pixels == NULL ||
        rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) {
        return 0;
    }
    if (frame_index < 0) frame_index = 0;
    if (frame_index >= header.frame_count) frame_index = header.frame_count - 1;
    if (frame_index < 0) return 0;
    pixel_count = (size_t)header.width * header.height;
    pixels = (uint16_t *)calloc(pixel_count, sizeof(*pixels));
    source = (uint16_t *)calloc(pixel_count, sizeof(*source));
    composited = (uint16_t *)calloc(pixel_count, sizeof(*composited));
    indices = (unsigned char *)calloc(pixel_count, 1);
    dirty = (unsigned char *)calloc(pixel_count, 1);
    if (pixels == NULL || source == NULL || composited == NULL || indices == NULL || dirty == NULL) {
        free(pixels);
        free(source);
        free(composited);
        free(indices);
        free(dirty);
        return 0;
    }
    for (local_y = 0; local_y < header.height; local_y++) {
        int local_x;
        int source_y = y + local_y;
        if (source_y < 0 || source_y >= SCREEN_H) continue;
        for (local_x = 0; local_x < header.width; local_x++) {
            int source_x = x + local_x;
            const uint16_t *row;
            if (source_x < 0 || source_x >= SCREEN_W) continue;
            row = (const uint16_t *)(const void *)(source_pixels +
                                                   source_y * source_pitch);
            source[(size_t)local_y * header.width + local_x] = row[source_x];
        }
    }
    error = rich4_flc_player_init(&player, data, size, pixels, indices, dirty,
                                  pixel_count, 0);
    if (error != RICH4_FLC_OK) {
        free(pixels);
        free(source);
        free(composited);
        free(indices);
        free(dirty);
        return 0;
    }
    error = rich4_flc_player_seek(&player, frame_index);
    if (error == RICH4_FLC_OK) {
        size_t i;
        rich4_flc_composite_game(indices, pixels, source, composited,
                                 pixel_count, 1);
        for (i = 0; i < pixel_count; i++) {
            int local_x;
            int dst_x;
            int dst_y;
            uint16_t *row;
            local_x = (int)(i % header.width);
            local_y = (int)(i / header.width);
            dst_x = x + local_x;
            dst_y = y + local_y;
            if (dst_x < 0 || dst_x >= SCREEN_W || dst_y < 0 || dst_y >= SCREEN_H) {
                continue;
            }
            row = (uint16_t *)((unsigned char *)screen->pixels + dst_y * screen->pitch);
            row[dst_x] = indices[i] == 0 ? source[i] :
                map_flc_rgb565_pixel(screen, composited[i]);
        }
    }
    free(pixels);
    free(source);
    free(composited);
    free(indices);
    free(dirty);
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
        !assets->ui.has_panel_graph[resource_index]) {
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
    unsigned char *source_pixels;
    size_t source_size;
    int ok = 1;

    if (screen == NULL) {
        return 0;
    }
    if (frame_index < 0) {
        frame_index = 0;
    }
    SDL_LockSurface(screen);
    source_size = (size_t)screen->pitch * (size_t)SCREEN_H;
    source_pixels = (unsigned char *)malloc(source_size);
    if (source_pixels == NULL) {
        SDL_UnlockSurface(screen);
        return 0;
    }
    memcpy(source_pixels, screen->pixels, source_size);
    ok = draw_game_panel_graph_frame_step(screen,
                                          assets,
                                          resource_index,
                                          frame_index,
                                          x,
                                          y,
                                          source_pixels,
                                          screen->pitch);
    free(source_pixels);
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
    Rich4FlcHeader header;
    unsigned char *source_pixels;
    size_t source_size;
    int ok = 1;

    if (screen == NULL ||
        rich4_flc_parse_header(data, size, &header) != RICH4_FLC_OK) {
        return 0;
    }
    if (frame_index < 0) {
        frame_index = 0;
    }
    SDL_LockSurface(screen);
    source_size = (size_t)screen->pitch * (size_t)SCREEN_H;
    source_pixels = (unsigned char *)malloc(source_size);
    if (source_pixels == NULL) {
        SDL_UnlockSurface(screen);
        return 0;
    }
    memcpy(source_pixels, screen->pixels, source_size);
    ok = draw_game_graph_frame_step_data(screen,
                                         data,
                                         size,
                                         frame_index,
                                         x,
                                         y,
                                         source_pixels,
                                         screen->pitch);
    free(source_pixels);
    SDL_UnlockSurface(screen);
    return ok;
}
