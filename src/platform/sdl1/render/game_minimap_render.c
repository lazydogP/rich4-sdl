#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <string.h>
#include <SDL/SDL.h>
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "core/character_template.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_route_api.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"
/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c.
 * GAME_SMALL_MAP_* / GAME_ORIGINAL_SMALL_MAP_SCALE live in
 * view/view_constants.h (shared with runtime_main.c's open_small_map_panel). */

enum {
    GAME_CHARACTER_MEDIUM_MAP_ICON_CHUNK = 5,
    GAME_CHARACTER_SMALL_MAP_ICON_CHUNK = 6
};

static int draw_game_map_player_head(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     int character,
                                     size_t chunk_index,
                                     int center_x,
                                     int center_y)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || assets == NULL ||
        character < 0 || character >= 12 ||
        !assets->character.has_map_character[character]) {
        return 0;
    }
    if (rich4_spr_smp_get_chunk(assets->character.map_character_resource[character].data,
                                    assets->character.map_character_resource[character].size,
                                    &assets->character.map_character_spr[character],
                                    chunk_index,
                                    &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    if (memcmp(assets->character.map_character_spr[character].signature, "SPR", 4) == 0) {
        return draw_spr_chunk_at(screen,
                                 assets->character.map_character_resource[character].data,
                                 assets->character.map_character_resource[character].size,
                                 &assets->character.map_character_spr[character],
                                 &chunk,
                                 center_x,
                                 center_y,
                                 1);
    }
    return draw_chunk_at(screen,
                         assets->character.map_character_resource[character].data,
                         &chunk,
                         center_x,
                         center_y,
                         1);
}
static int game_world_to_small_map(int value)
{
    /* sub_40A4E1 uses 11392/65536 for the 400x400 overview. */
    return (GAME_ORIGINAL_SMALL_MAP_SCALE * value) >> 16;
}

static int draw_game_small_map_background(
    SDL_Surface *screen, const GamePreviewAssets *assets, int background_index)
{
    Rich4SprSmpChunk chunk;
    if (screen == NULL || assets == NULL ||
        background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        !assets->map.has_map_overview[background_index] ||
        assets->map.map_overview_spr[background_index].num_chunks <= 1 ||
        rich4_spr_smp_get_chunk(
            assets->map.map_overview_resource[background_index].data,
            assets->map.map_overview_resource[background_index].size,
            &assets->map.map_overview_spr[background_index], 1,
            &chunk) != RICH4_SPR_SMP_OK ||
        chunk.width != GAME_SMALL_MAP_WIDTH ||
        chunk.height != GAME_SMALL_MAP_HEIGHT)
        return 0;
    if (memcmp(assets->map.map_overview_spr[background_index].signature,
               "SPR", 4) == 0)
        return draw_spr_chunk_at(
            screen,
            assets->map.map_overview_resource[background_index].data,
            assets->map.map_overview_resource[background_index].size,
            &assets->map.map_overview_spr[background_index],
            &chunk,
            GAME_SMALL_MAP_LEFT + chunk.x,
            GAME_SMALL_MAP_TOP + chunk.y,
            0);
    return draw_chunk_topleft(
        screen,
        assets->map.map_overview_resource[background_index].data,
        &chunk, GAME_SMALL_MAP_LEFT, GAME_SMALL_MAP_TOP, 0);
}

static int draw_game_small_map_owner_marker(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int chunk_index, int anchor_x, int anchor_y, int character)
{
    Rich4SprSmpChunk chunk;
    const unsigned char *data;
    size_t size;
    size_t pixel_count;
    uint16_t color;
    int dst_x;
    int dst_y;
    int x;
    int y;
    int is_spr;

    if (screen == NULL || assets == NULL || !assets->map.has_map_control ||
        character < 0 || character >= 12 ||
        chunk_index < 0 ||
        (size_t)chunk_index >= assets->map.map_control_spr.num_chunks)
        return 0;
    data = assets->map.map_control_data;
    size = assets->map.map_control_size;
    if (rich4_spr_smp_get_chunk(
            data, size, &assets->map.map_control_spr,
            (size_t)chunk_index, &chunk) != RICH4_SPR_SMP_OK ||
        chunk.width <= 0 || chunk.height <= 0)
        return 0;
    pixel_count = (size_t)chunk.width * (size_t)chunk.height;
    is_spr = memcmp(assets->map.map_control_spr.signature, "SPR", 4) == 0;
    if ((size_t)chunk.data_offset > size ||
        (is_spr
             ? pixel_count > size - (size_t)chunk.data_offset
             : pixel_count > (size - (size_t)chunk.data_offset) / 2u))
        return 0;

    color = map_resource_pixel(
        screen,
        rich4_map_character_palette_color(
            rich4_character_template(character)->color));
    dst_x = anchor_x - chunk.x;
    dst_y = anchor_y - chunk.y;
    SDL_LockSurface(screen);
    for (y = 0; y < chunk.height && dst_y + y < SCREEN_H; y++) {
        uint16_t *row;
        if (dst_y + y < 0) continue;
        row = (uint16_t *)((unsigned char *)screen->pixels +
                          (dst_y + y) * screen->pitch);
        for (x = 0; x < chunk.width && dst_x + x < SCREEN_W; x++) {
            int occupied;
            if (dst_x + x < 0) continue;
            if (is_spr)
                occupied =
                    data[chunk.data_offset + (size_t)y * chunk.width + x] != 0;
            else {
                uint16_t pixel_value = 0;
                occupied = rich4_byte_reader_le16(
                    data, size,
                    (size_t)chunk.data_offset +
                        2u * ((size_t)y * chunk.width + (size_t)x),
                    &pixel_value) && pixel_value != 0;
            }
            if (occupied) row[dst_x + x] = color;
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_game_small_map_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameViewModel *view,
    const SmallMapPanelState *panel)
{
    const MapData *map;
    int background_index;
    int i;

    if (screen == NULL || assets == NULL || settings == NULL || view == NULL ||
        panel == NULL || !panel->active || panel->saved_background == NULL ||
        !view->active)
        return 0;
    if (SDL_BlitSurface(panel->saved_background, NULL, screen, NULL) != 0)
        return 0;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (!draw_game_small_map_background(screen, assets, background_index))
        return 0;
    map = &assets->map.maps[background_index];

    for (i = 0; i < view->land_count &&
                (size_t)(i + 1) < map->land_count; i++) {
        const MapLandRecord *record = &map->lands[i + 1];
        int owner = view->lands[i].owner;
        if (owner < 0 || owner >= view->actor_count) continue;
        draw_game_small_map_owner_marker(
            screen, assets,
            GAME_SMALL_MAP_LAND_MARKER_FIRST + (record->variant & 1),
            GAME_SMALL_MAP_LEFT + game_world_to_small_map(record->x),
            GAME_SMALL_MAP_TOP + game_world_to_small_map(record->y),
            view->actors[owner].character);
    }
    for (i = 0; i < view->facility_count &&
                (size_t)(i + 1) < map->facility_count; i++) {
        const MapFacilityRecord *record = &map->facilities[i + 1];
        int owner = view->facilities[i].owner;
        if (owner < 0 || owner >= view->actor_count) continue;
        draw_game_small_map_owner_marker(
            screen, assets,
            GAME_SMALL_MAP_FACILITY_MARKER_FIRST + (record->variant & 1),
            GAME_SMALL_MAP_LEFT + game_world_to_small_map(record->x),
            GAME_SMALL_MAP_TOP + game_world_to_small_map(record->y),
            view->actors[owner].character);
    }
    for (i = 0; i < view->company_count &&
                (size_t)(i + 1) < map->auxiliary_count; i++) {
        const MapAuxiliaryRecord *record = &map->auxiliary[i + 1];
        int owner = view->companies[i].owner;
        if (owner < 0 || owner >= view->actor_count) continue;
        draw_game_small_map_owner_marker(
            screen, assets,
            GAME_SMALL_MAP_FACILITY_MARKER_FIRST + (record->variant & 1),
            GAME_SMALL_MAP_LEFT + game_world_to_small_map(record->x),
            GAME_SMALL_MAP_TOP + game_world_to_small_map(record->y),
            view->actors[owner].character);
    }
    for (i = 0; i < view->actor_count; i++) {
        const GameActorView *actor = &view->actors[i];
        /* 0x40A8A1 skips a player while player +8 xpos is zero.  SDL has
         * already assigned that player a logical start node, so world_x is
         * not a compatible sentinel until first landing has completed. */
        if (actor->first_turn_landing_pending || actor->world_x == 0)
            continue;
        /* 0x40A91A passes resource +0x48: header + descriptor 5. */
        draw_game_map_player_head(
            screen, assets, actor->character,
            GAME_CHARACTER_MEDIUM_MAP_ICON_CHUNK,
            GAME_SMALL_MAP_LEFT + game_world_to_small_map(actor->world_x),
            GAME_SMALL_MAP_TOP + game_world_to_small_map(actor->world_y));
    }
    return 1;
}

static int game_player_model_part(const GameActorView *actor)
{
    int visual_direction;
    int direction_group;
    int frame;

    if (actor == NULL) {
        return 0;
    }
    visual_direction = actor->visual_direction;
    direction_group = ((visual_direction + 1) & 7) / 2;
    frame = actor->walk_frame % 3;
    if (frame < 0) {
        frame = 0;
    }
    return direction_group * 3 + frame;
}

int draw_game_player_model_part(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                int character,
                                int part,
                                int visual_direction,
                                int moving,
                                int walk_frame,
                                int center_x,
                                int center_y)
{
    int frames;
    int chunk_index = 0;

    if (screen == NULL || assets == NULL ||
        character < 0 || character >= 12 ||
        part < 0 || part >= GAME_CHARACTER_MODEL_RESOURCES ||
        !game_asset_ready(assets, &assets->character.has_map_player[character][part])) {
        return 0;
    }
    frames = (int)assets->character.map_player_spr[character][part].num_chunks;
    if (frames <= 0) {
        return 0;
    }
    if (frames >= 8) {
        int frames_per_direction = frames / 8;
        if (frames_per_direction <= 0) {
            frames_per_direction = 1;
        }
        chunk_index = visual_direction * frames_per_direction;
        if (moving) {
            chunk_index += walk_frame % frames_per_direction;
        }
    }
    if (chunk_index < 0 || chunk_index >= frames) {
        chunk_index = 0;
    }
    return draw_spr_smp_center(screen,
                               assets->character.map_player_data[character][part],
                               assets->character.map_player_size[character][part],
                               &assets->character.map_player_spr[character][part],
                               (size_t)chunk_index,
                               center_x,
                               center_y);
}

int draw_game_player_model(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const GameViewModel *view,
                           int player_index,
                           int center_x,
                           int center_y,
                           int include_overlay)
{
    const GameActorView *actor;
    int character;
    int part;
    int visual_direction;

    if (screen == NULL || assets == NULL || view == NULL ||
        player_index < 0 || player_index >= view->actor_count) {
        return 0;
    }
    actor = &view->actors[player_index];
    character = actor->character;
    if (character < 0 || character >= 12) {
        return 0;
    }
    visual_direction = actor->visual_direction;
    if (actor->use_action_part) {
        int action_part = actor->action_part;
        int chunks = game_asset_ready(assets, &assets->character.has_map_player[character][action_part])
            ? (int)assets->character.map_player_spr[character][action_part].num_chunks : 0;
        int action_frames = chunks >= 8 ? chunks / 8 : chunks;
        int action_frame = actor->action_frame;

        if (action_frames > 0) {
            /* In 0x40D7C4, state 2 advances this Data.mkf action until its
             * final frame, then calls 0x419572 for the shared Panel.mkf dice
             * animation. The actor action is not reset at that transition. */
            if (actor->action_holds_last_frame) {
                action_frame = action_frames - 1;
            }
            if (action_frame < 0) {
                action_frame = 0;
            } else if (action_frame >= action_frames) {
                action_frame = action_frames - 1;
            }
            if (assets->character.map_player_spr[character][action_part].num_chunks >= 8) {
                action_frame += visual_direction * action_frames;
            }
            if (draw_spr_smp_center(screen,
                                    assets->character.map_player_data[character][action_part],
                                    assets->character.map_player_size[character][action_part],
                                    &assets->character.map_player_spr[character][action_part],
                                    (size_t)action_frame,
                                    center_x,
                                    center_y)) {
                return 1;
            }
        }
    }
    part = actor->primary_part;
    if (draw_game_player_model_part(screen,
                                    assets,
                                    character,
                                    part,
                                    visual_direction,
                                    actor->moving,
                                    actor->walk_frame,
                                    center_x,
                                    center_y)) {
        /* In 0x40B93B, parts 13, 14, and 15 are alternate primary
         * resources selected by the actor state.  Part 15 is not a water
         * overlay; compositing it here made the water movement look like the
         * dice-throw action. */
        if (include_overlay && actor->overlay_part >= 0) {
            draw_game_player_model_part(screen,
                                        assets,
                                        character,
                                        actor->overlay_part,
                                        visual_direction,
                                        actor->moving,
                                        actor->walk_frame,
                                        center_x,
                                        center_y);
        }
        return 1;
    }
    if (actor->moving &&
        game_asset_ready(assets, &assets->character.has_map_player[character][1]) &&
        assets->character.map_player_spr[character][1].num_chunks >= 8) {
        int chunk_index;
        int frames_per_direction = (int)assets->character.map_player_spr[character][1].num_chunks / 8;
        if (frames_per_direction <= 0) {
            frames_per_direction = 1;
        }
        chunk_index = visual_direction * frames_per_direction +
            (actor->walk_frame % frames_per_direction);
        if (chunk_index < 0) {
            chunk_index = visual_direction * frames_per_direction;
        }
        return draw_spr_smp_center(screen,
                                   assets->character.map_player_data[character][1],
                                   assets->character.map_player_size[character][1],
                                   &assets->character.map_player_spr[character][1],
                                   (size_t)chunk_index,
                                   center_x,
                                   center_y);
    }
    if (game_asset_ready(assets, &assets->character.has_map_player[character][0]) &&
        assets->character.map_player_spr[character][0].num_chunks >= 8) {
        return draw_spr_smp_center(screen,
                                   assets->character.map_player_data[character][0],
                                   assets->character.map_player_size[character][0],
                                   &assets->character.map_player_spr[character][0],
                                   (size_t)visual_direction,
                                   center_x,
                                   center_y);
    }
    part = game_player_model_part(actor);
    if (part < 0 || part >= GAME_CHARACTER_MODEL_RESOURCES ||
        !game_asset_ready(assets, &assets->character.has_map_player[character][part])) {
        return 0;
    }
    return draw_spr_smp_center(screen,
                               assets->character.map_player_data[character][part],
                               assets->character.map_player_size[character][part],
                               &assets->character.map_player_spr[character][part],
                               0,
                               center_x,
                               center_y);
}

/* 0x40A723..0x40A7F3 -> 0x456384: stamp every nonzero word of the
 * Data.MKF[517] silhouette in the current chairman's character color. */
static void draw_overview_marker(SDL_Surface *screen,
    const GamePreviewAssets *assets, const GameViewModel *view,
    int owner, int world_x, int world_y, size_t frame, int minimap_y)
{
    Rich4SprSmpChunk chunk;
    int x, y, left, top;
    uint16_t color;
    const unsigned char *pixels;
    if (!game_asset_ready(assets, &assets->ui.has_common_dialog) || owner < 0 || owner >= view->actor_count ||
        rich4_spr_smp_get_chunk(assets->ui.common_dialog_data,
            assets->ui.common_dialog_size, &assets->ui.common_dialog_spr,
            frame, &chunk) != RICH4_SPR_SMP_OK ||
        chunk.data_size < (uint32_t)(chunk.width * chunk.height * 2)) return;
    pixels = assets->ui.common_dialog_data + chunk.data_offset;
    color = rich4_character_rgb565_color(rich4_character_template(
        view->actors[owner].character)->color);
    left = game_world_to_minimap_x(world_x) - chunk.x;
    top = game_world_to_minimap_y_at(world_y, minimap_y) - chunk.y;
    SDL_LockSurface(screen);
    for (y = 0; y < chunk.height; y++) {
        int dy = top + y;
        if (dy < minimap_y || dy >= minimap_y + GAME_MINIMAP_H ||
            dy < 0 || dy >= screen->h) continue;
        for (x = 0; x < chunk.width; x++) {
            int dx = left + x;
            int offset = 2 * (y * chunk.width + x);
            if (dx >= GAME_MINIMAP_X && dx < GAME_MINIMAP_X + GAME_MINIMAP_W &&
                dx >= 0 && dx < screen->w && (pixels[offset] || pixels[offset + 1]))
                ((uint16_t *)((unsigned char *)screen->pixels + dy * screen->pitch))[dx] = color;
        }
    }
    SDL_UnlockSurface(screen);
}

static void draw_company_overview(SDL_Surface *screen,
    const GamePreviewAssets *assets, const GameViewModel *view,
    int background_index, int minimap_y)
{
    const MapData *map = &assets->map.maps[background_index];
    int i;
    for (i = 0; i < view->company_count && (size_t)(i + 1) < map->auxiliary_count; i++) {
        const MapAuxiliaryRecord *record = &map->auxiliary[i + 1];
        draw_overview_marker(screen, assets, view, view->companies[i].owner,
            record->x, record->y, 24 + (record->variant & 1), minimap_y);
    }
}

#include "platform/sdl1/render/game_minimap_controls.h"
int draw_game_minimap_panel(SDL_Surface *screen,
                            const GamePreviewAssets *assets,
                            const NewGameSettings *settings,
                            const GameViewModel *view,
                            int view_mode)
{
    int background_index;
    Rich4SprSmpChunk chunk;
    int i;
    int minimap_y;

    if (screen == NULL || assets == NULL || settings == NULL || view == NULL ||
        !view->active || view_mode == 0) {
        return 0;
    }
    minimap_y = view_mode == 2 ? 80 : 280;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT) {
        return 0;
    }

    if (assets->map.has_map_overview[background_index] &&
        rich4_spr_smp_get_chunk(assets->map.map_overview_resource[background_index].data,
                                    assets->map.map_overview_resource[background_index].size,
                                    &assets->map.map_overview_spr[background_index],
                                    0,
                                    &chunk) == RICH4_SPR_SMP_OK) {
        draw_chunk_topleft(screen,
                           assets->map.map_overview_resource[background_index].data,
                           &chunk,
                           GAME_MINIMAP_X,
                           minimap_y,
                           0);
    }

    /* sub_40A4E1 builds the cached overview once, then stamps current
     * ownership over it.  Keep these markers live so purchases/upgrades are
     * reflected without rebuilding the background resource. */
    for (i = 0; i < view->land_count &&
                (size_t)(i + 1) < assets->map.maps[background_index].land_count; i++) {
        const MapLandRecord *record = &assets->map.maps[background_index].lands[i + 1];
        draw_overview_marker(screen, assets, view, view->lands[i].owner,
            record->x, record->y, 22 + (record->variant & 1), minimap_y);
    }
    for (i = 0; i < view->facility_count &&
                (size_t)(i + 1) < assets->map.maps[background_index].facility_count; i++) {
        const MapFacilityRecord *record = &assets->map.maps[background_index].facilities[i + 1];
        draw_overview_marker(screen, assets, view, view->facilities[i].owner,
            record->x, record->y, 24 + (record->variant & 1), minimap_y);
    }
    draw_company_overview(screen, assets, view, background_index, minimap_y);
    draw_game_minimap_rotate_buttons(screen, assets, view, minimap_y);

    for (i = 0; i < view->actor_count; i++) {
        int world_x;
        int world_y;
        int x;
        int y;
        if (view->actors[i].first_turn_landing_pending)
            continue;
        world_x = view->actors[i].minimap_world_x;
        world_y = view->actors[i].minimap_world_y;
        x = game_world_to_minimap_x(world_x);
        y = game_world_to_minimap_y_at(world_y, minimap_y);
        if (x < GAME_MINIMAP_X || x >= GAME_MINIMAP_X + GAME_MINIMAP_W ||
            y < minimap_y || y >= minimap_y + GAME_MINIMAP_H) {
            continue;
        }
        /* 0x416FFB passes resource +0x54: header + descriptor 6. */
        draw_game_map_player_head(
                screen, assets, view->actors[i].character,
                GAME_CHARACTER_SMALL_MAP_ICON_CHUNK, x, y);
    }

    SDL_LockSurface(screen);
    {
        int current = view->hud.current_player;
        int white_visible = current >= 0 && current < view->actor_count &&
            !view->actors[current].first_turn_landing_pending &&
            view->actors[current].minimap_world_x != 0;
        int white_world_x;
        int white_world_y;
        int red_world_x = view->viewport.center_world_x;
        int red_world_y = view->viewport.center_world_y;
        int white_x;
        int white_y;
        int red_x;
        int red_y;
        int frame_w = 30;
        int frame_h = 30;

        white_world_x = view->actors[current].minimap_world_x;
        white_world_y = view->actors[current].minimap_world_y;
        if (view->current_actor >= 4 && view->current_actor < 8) {
            const GameSpecialActorView *actor =
                &view->special_actors[view->current_actor - 4];
            white_visible = actor->visible;
            white_world_x = actor->world_x;
            white_world_y = actor->world_y;
        } else if (view->current_actor == 8) {
            white_visible = view->tool_robot.visible;
            white_world_x = view->tool_robot.world_x;
            white_world_y = view->tool_robot.world_y;
        }
        red_x = game_world_to_minimap_x(red_world_x);
        red_y = game_world_to_minimap_y_at(red_world_y, minimap_y);
        white_x = game_world_to_minimap_x(white_world_x);
        white_y = game_world_to_minimap_y_at(white_world_y, minimap_y);
        if (white_visible)
            stroke_rect_thin(screen, white_x - frame_w / 2, white_y - frame_h / 2, frame_w, frame_h, 0xffff);
        if (view->viewport.has_override &&
            (red_x != white_x || red_y != white_y)) {
            stroke_rect_thin(screen, red_x - frame_w / 2, red_y - frame_h / 2,
                             frame_w, frame_h, 0xf800);
        }
    }
    SDL_UnlockSurface(screen);
    return 1;
}
