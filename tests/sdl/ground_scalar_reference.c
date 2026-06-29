/* Frozen scalar rasterizer for pixel-equivalence regression. */
#define draw_game_main_map_view reference_draw_game_main_map_view
#define game_map_view_sprite_for_level reference_game_map_view_sprite_for_level
#define game_map_overlay_sprite reference_game_map_overlay_sprite
#define game_map_stage_tile_sprite reference_game_map_stage_tile_sprite
#define game_map_facility_sprite reference_game_map_facility_sprite
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_map_render.h"
#include "platform/sdl1/render/ground_rasterizer.h"
#include "platform/sdl1/render/ground_visible_table.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"
#include "data/rich4_map_tables.inc"
#define RICH4_MAP_GRID_Y(direction, row, col) \
    rich4_map_grid[(direction)][(row)][(col)][0]
#define RICH4_MAP_GRID_X(direction, row, col) \
    rich4_map_grid[(direction)][(row)][(col)][1]
/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c. This is
 * the last cluster that calls draw_game_player_model /
 * draw_game_player_model_part (platform/sdl1/render/game_minimap_render.c),
 * and the only user of the rich4_map_grid sub-tile projection table. */
int game_map_view_sprite_for_level(int background_index, int level)
{
    if (level <= 0 || level > GAME_MAP_VIEW_RESOURCE_COUNT ||
        background_index < 0 || background_index >= GAME_BACKGROUND_COUNT) {
        return 0;
    }
    return background_index * GAME_MAP_VIEW_RESOURCE_COUNT + level;
}
int game_map_overlay_sprite(int background_index)
{
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT) {
        return 0;
    }
    return background_index + 41;
}
int game_map_stage_tile_sprite(int background_index, int tile_index)
{
    int stage;
    int map;
    int resource_index;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        tile_index < 0 || tile_index >= GAME_MAP_STAGE_TILE_RESOURCE_COUNT) {
        return 0;
    }
    stage = background_index / GAME_MAPS_PER_STAGE;
    map = background_index % GAME_MAPS_PER_STAGE;
    resource_index = stage == 0 ?
        87 + tile_index :
        104 + map * GAME_MAP_STAGE_TILE_RESOURCE_COUNT + tile_index;
    return resource_index - 38;
}
int game_map_facility_sprite(int background_index, int type, int level)
{
    switch (type) {
    case 0:
        return game_map_stage_tile_sprite(background_index, 0);
    case 1:
        return game_map_stage_tile_sprite(background_index, level);
    case 2:
        return game_map_stage_tile_sprite(background_index, 5 + level);
    case 3:
        return game_map_stage_tile_sprite(background_index, 11);
    case 4:
        return game_map_stage_tile_sprite(background_index, 11 + level);
    default:
        return 0;
    }
}
#include "platform/sdl1/render/game_map_objects.h"
#if 0
static void draw_game_ground_tile_legacy(SDL_Surface *screen,
                                  int p00_x,
                                  int p00_y,
                                  int p10_x,
                                  int p10_y,
                                  int p01_x,
                                  int p01_y,
                                  int p11_x,
                                  int p11_y,
                                  const unsigned char *src,
                                  const uint16_t *palette)
{
    int px[4];
    int py[4];
    double tu[4];
    double tv[4];
    int min_y;
    int max_y;
    int y;

    if (screen == NULL || src == NULL || palette == NULL) {
        return;
    }

    px[0] = p00_x; py[0] = p00_y; tu[0] = 0.0;  tv[0] = 0.0;
    px[1] = p10_x; py[1] = p10_y; tu[1] = 31.0; tv[1] = 0.0;
    px[2] = p11_x; py[2] = p11_y; tu[2] = 31.0; tv[2] = 31.0;
    px[3] = p01_x; py[3] = p01_y; tu[3] = 0.0;  tv[3] = 31.0;

    min_y = py[0];
    max_y = py[0];
    for (y = 1; y < 4; y++) {
        if (py[y] < min_y) min_y = py[y];
        if (py[y] > max_y) max_y = py[y];
    }
    if (min_y < 40) min_y = 40;
    if (max_y > 479) max_y = 479;

    for (y = min_y; y <= max_y; y++) {
        double ix[4];
        double iu[4];
        double iv[4];
        int intersections = 0;
        int edge;

        for (edge = 0; edge < 4; edge++) {
            int next = (edge + 1) & 3;
            double scan_y = (double)y + 0.5;
            int y0 = py[edge];
            int y1 = py[next];
            double t;

            if (y0 == y1) {
                continue;
            }
            if (!((scan_y >= (double)y0 && scan_y < (double)y1) ||
                  (scan_y >= (double)y1 && scan_y < (double)y0))) {
                continue;
            }
            t = (scan_y - (double)y0) / (double)(y1 - y0);
            ix[intersections] = (double)px[edge] + ((double)px[next] - (double)px[edge]) * t;
            iu[intersections] = tu[edge] + (tu[next] - tu[edge]) * t;
            iv[intersections] = tv[edge] + (tv[next] - tv[edge]) * t;
            intersections++;
        }

        if (intersections >= 2) {
            int left = 0;
            int right = 1;
            int x0;
            int x1;
            int x;
            uint16_t *row;
            double span;

            if (ix[right] < ix[left]) {
                left = 1;
                right = 0;
            }
            x0 = (int)ix[left];
            if ((double)x0 < ix[left]) x0++;
            x1 = (int)ix[right];
            if ((double)x1 > ix[right]) x1--;
            if (x0 < 0) x0 = 0;
            if (x1 > 439) x1 = 439;
            if (x0 > x1) {
                continue;
            }

            row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
            span = ix[right] - ix[left];
            if (span == 0.0) {
                continue;
            }
            for (x = x0; x <= x1; x++) {
                double t = ((double)x + 0.5 - ix[left]) / span;
                int u = (int)(iu[left] + (iu[right] - iu[left]) * t);
                int v = (int)(iv[left] + (iv[right] - iv[left]) * t);
                uint16_t pixel;

                if (u < 0) u = 0;
                if (u > 31) u = 31;
                if (v < 0) v = 0;
                if (v > 31) v = 31;
                pixel = palette[src[v * 32 + u]];
                row[x] = map_resource_pixel(screen, pixel);
            }
        }
    }
}
#endif

static void game_original_407a2c_subtile_offset(int direction,
                                                int world_x,
                                                int world_y,
                                                int *out_x,
                                                int *out_y)
{
    /* Original 0x407A2C coefficient table begins at 0x474910. */
    static const signed char coeffs[8][4] = {
        { -34,  11, -14, -25 },
        { -34,  -9,  16, -25 },
        { -13, -25,  37,  -9 },
        {  16, -25,  36,  11 },
        {  37,  -9,  16,  26 },
        {  36,  11, -14,  26 },
        {  16,  26, -35,  11 },
        { -14,  26, -35,  -9 }
    };
    int sub_x = world_x & 31;
    int sub_y = world_y & 31;
    const signed char *c = coeffs[direction & 7];
    if (out_x != NULL)
        *out_x = ((sub_x * c[0]) >> 5) + ((sub_y * c[2]) >> 5);
    if (out_y != NULL)
        *out_y = ((sub_x * c[1]) >> 5) + ((sub_y * c[3]) >> 5);
}

int draw_game_main_map_view(SDL_Surface *screen,
                            const GamePreviewAssets *assets,
                            const NewGameSettings *settings,
                            const GameViewModel *view)
{
    static const unsigned char blank_ground_tile[1024] = {0};
    int background_index;
    const unsigned char *data;
    const unsigned char *tile_pixels;
    const uint16_t *palette;
    uint16_t mapped_palette[256];
    int color;
    size_t tile_count;
    int center_x;
    int center_y;
    int center_tile_x;
    int center_tile_y;
    int center_sub_x;
    int center_sub_y;
    int visible_index;
    int rx;
    int ry;
    int direction;
    uint16_t ground_width = 0;
    uint16_t ground_height = 0;

    if (screen == NULL || assets == NULL || settings == NULL || view == NULL ||
        !view->active || view->actor_count <= 0) {
        return 0;
    }
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        !assets->map.has_map_ground[background_index]) {
        return 0;
    }

    data = assets->map.map_ground_resource[background_index].data;
    if (assets->map.map_ground_resource[background_index].size < 0x2a90 + 1024u ||
        memcmp(data, "GND", 3) != 0 ||
        !rich4_byte_reader_le16(
            data, assets->map.map_ground_resource[background_index].size,
            4, &ground_width) ||
        !rich4_byte_reader_le16(
            data, assets->map.map_ground_resource[background_index].size,
            6, &ground_height) ||
        ground_width != 72 || ground_height != 72) {
        return 0;
    }

    center_x = view->viewport.center_world_x;
    center_y = view->viewport.center_world_y;
    center_tile_x = center_x >> 5;
    center_tile_y = center_y >> 5;
    direction = view->viewport.direction & 7;
    game_original_407a2c_subtile_offset(
        direction, center_x, center_y, &center_sub_x, &center_sub_y);
    tile_pixels = data + 0x2a90;
    palette = (const uint16_t *)(const void *)(data + 0x10);
    for (color = 0; color < 256; color++)
        mapped_palette[color] = map_resource_pixel(screen, palette[color]);
    palette = mapped_palette;
    tile_count = (assets->map.map_ground_resource[background_index].size - 0x2a90) / 1024u;

    SDL_LockSurface(screen);
    for (visible_index = 0; visible_index < 304; visible_index++) {
        int tile_x;
        int tile_y;
        int grid_x;
        int grid_y;
        int p00_x;
        int p00_y;
        int p10_x;
        int p10_y;
        int p01_x;
        int p01_y;
        int p11_x;
        int p11_y;
        uint16_t tile_index;
        const unsigned char *src;

        /* Original 0x408429..0x40855A: table pairs are {world Y, world X}. */
        ry = rich4_ground_visible[direction][visible_index][0];
        rx = rich4_ground_visible[direction][visible_index][1];
        if (ry == -128)
            break;

        tile_x = center_tile_x + rx;
        tile_y = center_tile_y + ry;
        grid_x = rx + 14;
        grid_y = ry + 14;

        if (tile_x < 0 || tile_x >= 72 || tile_y < 0 || tile_y >= 72) {
                /* 0x408404 selects the zero-filled 1024-byte 0x48B2B4
                 * tile for out-of-map cells. */
                src = blank_ground_tile;
        } else {
                if (!rich4_byte_reader_le16(
                        data,
                        assets->map.map_ground_resource[background_index].size,
                        0x210u + ((size_t)tile_y * 72u + (size_t)tile_x) * 2u,
                        &tile_index))
                    tile_index = 0;
                src = (size_t)tile_index < tile_count
                    ? tile_pixels + (size_t)tile_index * 1024u
                    : blank_ground_tile;
        }
            /* 0x408484..0x4084FE index the table as [world Y][world X]
             * and add the camera sub-tile offset exactly once.  Keeping
             * the four axes explicit prevents state-dependent projection
             * helpers from transposing or reflecting the ground quad. */
        p00_x = GAME_MAIN_MAP_ORIGIN_X + center_sub_x +
                RICH4_MAP_GRID_X(direction, grid_y, grid_x);
            p00_y = GAME_MAIN_MAP_ORIGIN_Y + center_sub_y +
                RICH4_MAP_GRID_Y(direction, grid_y, grid_x);
            p10_x = GAME_MAIN_MAP_ORIGIN_X + center_sub_x +
                RICH4_MAP_GRID_X(direction, grid_y, grid_x + 1);
            p10_y = GAME_MAIN_MAP_ORIGIN_Y + center_sub_y +
                RICH4_MAP_GRID_Y(direction, grid_y, grid_x + 1);
            p01_x = GAME_MAIN_MAP_ORIGIN_X + center_sub_x +
                RICH4_MAP_GRID_X(direction, grid_y + 1, grid_x);
            p01_y = GAME_MAIN_MAP_ORIGIN_Y + center_sub_y +
                RICH4_MAP_GRID_Y(direction, grid_y + 1, grid_x);
            p11_x = GAME_MAIN_MAP_ORIGIN_X + center_sub_x +
                RICH4_MAP_GRID_X(direction, grid_y + 1, grid_x + 1);
            p11_y = GAME_MAIN_MAP_ORIGIN_Y + center_sub_y +
                RICH4_MAP_GRID_Y(direction, grid_y + 1, grid_x + 1);
        rich4_draw_ground_tile_original(screen,
                                            p00_x,
                                            p00_y,
                                            p10_x,
                                            p10_y,
                                            p01_x,
                                            p01_y,
                                            p11_x,
                                            p11_y,
                                            src,
                                            palette,
                                            NULL);
    }
    SDL_UnlockSurface(screen);
    return 1;
}
