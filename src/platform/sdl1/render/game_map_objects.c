#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <string.h>
#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "core/character_template.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

static void game_main_view_center_world(const GameViewModel *view,
                                        int *out_x,
                                        int *out_y)
{
    if (out_x != NULL) *out_x = view != NULL ? view->viewport.center_world_x : 0;
    if (out_y != NULL) *out_y = view != NULL ? view->viewport.center_world_y : 0;
}

static int game_map_object_visible_tile_window(const GameViewModel *view,
                                               int world_x,
                                               int world_y)
{
    int center_x;
    int center_y;
    int tile_x;
    int tile_y;

    game_main_view_center_world(view, &center_x, &center_y);
    tile_x = (world_x >> 5) - (center_x >> 5) + 0x0e;
    tile_y = (world_y >> 5) - (center_y >> 5) + 0x0e;
    return tile_x >= 0 && tile_x <= 0x1c && tile_y >= 0 && tile_y <= 0x1c;
}

static void draw_game_road_node_display_objects(SDL_Surface *screen,
                                                const GamePreviewAssets *assets,
                                                const NewGameSettings *settings,
                                                const GameViewModel *view)
{
    int background_index;
    int node;
    int chunk_count;

    if (screen == NULL || assets == NULL || settings == NULL || view == NULL ||
        !view->active) {
        return;
    }
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        assets->map.maps[background_index].route_count <= 1 ||
        !assets->map.has_map_common_graph[0]) {
        return;
    }
    chunk_count = (int)assets->map.map_common_spr[0].num_chunks;
    if (chunk_count <= 0) {
        return;
    }

    for (node = 1; (size_t)node < assets->map.maps[background_index].route_count; node++) {
        int display_chunk = assets->map.maps[background_index].routes[node].display_chunk;
        int world_x;
        int world_y;
        int x;
        int y;
        int chunk;

        if (display_chunk <= 0) {
            continue;
        }
        world_x = assets->map.maps[background_index].routes[node].x;
        world_y = assets->map.maps[background_index].routes[node].y;
        if (!game_map_object_visible_tile_window(view, world_x, world_y)) {
            continue;
        }
        rich4_game_viewport_project(&view->viewport, world_x, world_y, &x, &y);
        chunk = display_chunk - 1;
        if (chunk < 0 || chunk >= chunk_count) {
            continue;
        }
        draw_spr_smp_center(screen,
                            assets->map.map_common_resource[0].data,
                            assets->map.map_common_resource[0].size,
                            &assets->map.map_common_spr[0],
                            (size_t)chunk,
                            x,
                            y);
    }
}

static void draw_game_map_parity_overlays(SDL_Surface *screen,
                                          const GamePreviewAssets *assets,
                                          int background_index,
                                          const GameViewModel *view)
{
    const MapData *map;
    int direction;
    size_t i;

    if (screen == NULL || assets == NULL || view == NULL ||
        background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        !assets->map.has_map_common_graph[2] ||
        assets->map.map_common_spr[2].num_chunks < 4) return;
    map = &assets->map.maps[background_index];
    direction = view->viewport.direction & 7;
    for (i = 1; i < map->land_count && i <= (size_t)view->land_count; i++) {
        const MapLandRecord *record = &map->lands[i];
        Rich4SprSmpChunk chunk;
        int x;
        int y;
        int rotated;
        if (view->lands[i - 1].price_status == 0 ||
            !game_map_object_visible_tile_window(view, record->x, record->y)) continue;
        rotated = rich4_map_rotated_variant(record->variant, direction);
        if (rich4_spr_smp_get_chunk(assets->map.map_common_data[2],
                                    assets->map.map_common_size[2],
                                    &assets->map.map_common_spr[2],
                                    (size_t)(rotated & 1),
                                    &chunk) != RICH4_SPR_SMP_OK) continue;
        rich4_game_viewport_project(&view->viewport, record->x, record->y, &x, &y);
        draw_game_map_parity_mask(screen, assets->map.map_common_data[2], &chunk,
                                  x, y, view->lands[i - 1].price_status & 1);
    }
    for (i = 1; i < map->facility_count && i <= (size_t)view->facility_count; i++) {
        const MapFacilityRecord *record = &map->facilities[i];
        Rich4SprSmpChunk chunk;
        int x;
        int y;
        int rotated;
        if (view->facilities[i - 1].price_status == 0 ||
            !game_map_object_visible_tile_window(view, record->x, record->y)) continue;
        rotated = rich4_map_rotated_variant(record->variant, direction);
        if (rich4_spr_smp_get_chunk(assets->map.map_common_data[2],
                                    assets->map.map_common_size[2],
                                    &assets->map.map_common_spr[2],
                                    (size_t)(2 + (rotated & 1)),
                                    &chunk) != RICH4_SPR_SMP_OK) continue;
        rich4_game_viewport_project(&view->viewport, record->x, record->y, &x, &y);
        draw_game_map_parity_mask(screen, assets->map.map_common_data[2], &chunk,
                                  x, y, view->facilities[i - 1].price_status & 1);
    }
}

void draw_game_map_objects(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const NewGameSettings *settings,
                           const GameViewModel *view,
                           const GameMapDynamicObjectState *dynamic_objects)
{
#define GAME_MAP_RENDER_ACTOR_SPRITE (-2)
#define GAME_MAP_RENDER_ACTOR_OVERLAY_SPRITE (-3)
#define GAME_MAP_RENDER_SPECIAL_ACTOR_SPRITE (-4)
#define GAME_MAP_RENDER_TOOL_ROBOT_SPRITE (-5)
#define GAME_MAP_RENDER_SPECIAL_ACTOR_OVERLAY_SPRITE (-6)
#define GAME_MAP_RENDER_DYNAMIC_BASE (-100)
    int background_index;
    int i;
    MapRenderObjectInput inputs[GAME_MAP_RENDER_QUEUE_CAPACITY];
    GameVisibleMapObject visible[GAME_MAP_RENDER_QUEUE_CAPACITY];
    int input_count = 0;
    int visible_count = 0;

    if (screen == NULL || assets == NULL || settings == NULL || view == NULL ||
        !view->active) {
        return;
    }
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT) {
        return;
    }
    memset(inputs, 0, sizeof(inputs));
    draw_game_road_node_display_objects(screen, assets, settings, view);

    /* 0x40865B collects normal actors before map tables and gives their
     * low-word depth key a 0x0d current-player or 0x0c other-player bias.
     * Keeping actors in this queue is required for building/property
     * occlusion; drawing every actor after every map object is observably
     * different from the original. */
    for (i = 0; i < view->actor_count && input_count < GAME_MAP_RENDER_QUEUE_CAPACITY; i++) {
        const GameActorView *actor = &view->actors[i];
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = GAME_MAP_RENDER_ACTOR_SPRITE;
        inputs[input_count].object_id = 0x8000 | (1 << i);
        inputs[input_count].owner = -1;
        inputs[input_count].variant = 0;
        inputs[input_count].world_x = actor->world_x;
        inputs[input_count].world_y = actor->world_y;
        inputs[input_count].sort_bias = i == view->hud.current_player ? 0x0d : 0x0c;
        inputs[input_count].drawable = actor->visible && actor->world_x != 0;
        input_count++;
        if (actor->overlay_part >= 0 && input_count < GAME_MAP_RENDER_QUEUE_CAPACITY) {
            inputs[input_count].asset_index = i;
            inputs[input_count].sprite = GAME_MAP_RENDER_ACTOR_OVERLAY_SPRITE;
            inputs[input_count].object_id = 0x8000 | (1 << i);
            inputs[input_count].owner = -1;
            inputs[input_count].world_x = actor->world_x;
            inputs[input_count].world_y = actor->world_y;
            inputs[input_count].sort_bias = i == view->hud.current_player ? 0x0f : 0x0e;
            inputs[input_count].drawable = actor->visible && actor->world_x != 0;
            input_count++;
        }
    }
    /* 0x408919..0x408B7C collects fixed/special actors after ordinary
     * players.  Rescuable slots 0..3 use Data.MKF[380..395], a 0x08 depth
     * bias, and object bits 0x10..0x80. */
    for (i = 0; i < view->special_actor_count &&
                input_count < GAME_MAP_RENDER_QUEUE_CAPACITY; i++) {
        const GameSpecialActorView *actor = &view->special_actors[i];
        int part = actor->resource_part;
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = GAME_MAP_RENDER_SPECIAL_ACTOR_SPRITE;
        inputs[input_count].object_id = 0x8000 | (0x10 << i);
        inputs[input_count].owner = -1;
        inputs[input_count].world_x = actor->world_x;
        inputs[input_count].world_y = actor->world_y;
        inputs[input_count].sort_bias =
            view->current_actor == i + GAME_MAX_PLAYERS ? 0x0d : 0x08;
        inputs[input_count].drawable = actor->visible &&
            part >= 0 && part < GAME_SPECIAL_ACTOR_MODEL_RESOURCES &&
            game_asset_ready(assets, &assets->character.has_map_special_actor[i][part]);
        input_count++;
        if (actor->overlay_chunk >= 0 &&
            input_count < GAME_MAP_RENDER_QUEUE_CAPACITY) {
            inputs[input_count].asset_index = i;
            inputs[input_count].sprite =
                GAME_MAP_RENDER_SPECIAL_ACTOR_OVERLAY_SPRITE;
            inputs[input_count].object_id = 0x8000 | (0x10 << i);
            inputs[input_count].owner = -1;
            inputs[input_count].variant = actor->overlay_chunk;
            inputs[input_count].world_x = actor->world_x;
            inputs[input_count].world_y = actor->world_y;
            inputs[input_count].sort_bias =
                (view->current_actor == i + GAME_MAX_PLAYERS)
                    ? 0x0f : 0x0e;
            inputs[input_count].drawable = actor->visible &&
                assets->map.has_map_common_graph[1];
            input_count++;
        }
    }
    if (view->tool_robot.visible &&
        input_count < GAME_MAP_RENDER_QUEUE_CAPACITY) {
        inputs[input_count].asset_index = 0;
        inputs[input_count].sprite = GAME_MAP_RENDER_TOOL_ROBOT_SPRITE;
        inputs[input_count].object_id = 0x8100;
        inputs[input_count].owner = -1;
        inputs[input_count].variant = view->tool_robot.animation_frame;
        inputs[input_count].world_x = view->tool_robot.world_x;
        inputs[input_count].world_y = view->tool_robot.world_y;
        inputs[input_count].sort_bias = 0x0d;
        inputs[input_count].drawable =
            game_asset_ready(assets, &assets->character.has_tool_robot[view->tool_robot.resource_part]);
        input_count++;
    }
    if (dynamic_objects != NULL) {
        const MapData *map = &assets->map.maps[background_index];
        for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT &&
                    input_count < GAME_MAP_RENDER_QUEUE_CAPACITY; i++) {
            if (rich4_map_dynamic_build_input(&dynamic_objects[i], i,
                                              &view->viewport,
                                              view->actors, view->actor_count,
                                              map->routes, map->route_count,
                                              NULL, &inputs[input_count])) {
                inputs[input_count].sprite = GAME_MAP_RENDER_DYNAMIC_BASE -
                                             inputs[input_count].sprite;
                input_count++;
            }
        }
    }
    {
        const MapData *map = &assets->map.maps[background_index];
        for (i = 0; i < view->land_count &&
                    (size_t)(i + 1) < map->land_count &&
                    input_count < GAME_MAP_RENDER_QUEUE_CAPACITY; i++) {
            const MapLandRecord *record = &map->lands[i + 1];
            int owner = view->lands[i].owner;
            int level = view->lands[i].level;
            MapPropertyRenderStyle style;
            int owner_character = owner >= 0 && owner < view->actor_count
                ? view->actors[owner].character : 0;
            int sprite;
            if (!rich4_map_property_render_style(
                    level, owner, view->actor_count, owner_character,
                    record->variant, view->viewport.direction, &style) ||
                !style.visible) continue;
            sprite = style.use_owner_marker
                ? GAME_MAP_OBJECT_SPRITE_COMMON_25
                : (view->lands[i].type == 0
                    ? game_map_view_sprite_for_level(background_index, level)
                    : game_map_overlay_sprite(background_index));
            if (sprite != GAME_MAP_OBJECT_SPRITE_COMMON_25 &&
                (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES)) continue;
            inputs[input_count].asset_index = i;
            inputs[input_count].sprite = sprite;
            inputs[input_count].object_id = 0x7d1 + i;
            inputs[input_count].owner = style.palette_owner;
            inputs[input_count].variant = style.variant;
            inputs[input_count].world_x = record->x;
            inputs[input_count].world_y = record->y;
            inputs[input_count].drawable = sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25
                ? assets->map.has_map_common_graph[1]
                : assets->map.has_map_object_sprite[sprite];
            input_count++;
        }
        for (i = 0; i < view->facility_count &&
                    (size_t)(i + 1) < map->facility_count &&
                    input_count < GAME_MAP_RENDER_QUEUE_CAPACITY; i++) {
            const MapFacilityRecord *record = &map->facilities[i + 1];
            int owner = view->facilities[i].owner;
            int level = view->facilities[i].level;
            MapPropertyRenderStyle style;
            int owner_character = owner >= 0 && owner < view->actor_count
                ? view->actors[owner].character : 0;
            int sprite;
            if (!rich4_map_property_render_style(
                    level, owner, view->actor_count, owner_character,
                    record->variant, view->viewport.direction, &style) ||
                !style.visible) continue;
            sprite = style.use_owner_marker
                ? GAME_MAP_OBJECT_SPRITE_COMMON_25
                : game_map_facility_sprite(background_index,
                                           view->facilities[i].type,
                                           level);
            if (sprite != GAME_MAP_OBJECT_SPRITE_COMMON_25 &&
                (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES)) continue;
            inputs[input_count].asset_index = i;
            inputs[input_count].sprite = sprite;
            inputs[input_count].object_id = 0xfa1 + i;
            inputs[input_count].owner = style.palette_owner;
            inputs[input_count].variant = style.variant;
            inputs[input_count].world_x = record->x;
            inputs[input_count].world_y = record->y;
            inputs[input_count].drawable = sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25
                ? assets->map.has_map_common_graph[1]
                : assets->map.has_map_object_sprite[sprite];
            input_count++;
        }
    }
    for (i = 0; i < assets->map.map_object_count[background_index] &&
                input_count < GAME_MAP_RENDER_QUEUE_CAPACITY; i++) {
        int sprite = assets->map.map_object_sprite[background_index][i];
        int world_x = assets->map.map_object_x[background_index][i];
        int world_y = assets->map.map_object_y[background_index][i];
        int variant;
        int variant_owner;
        int drawable = 1;

        if (assets->map.map_object_id[background_index][i] >= 0x7d1 &&
            assets->map.map_object_id[background_index][i] < 0x1770) {
            /* Property objects are rebuilt from authoritative session state
             * above.  The asset list contains only the map's initial owner
             * and level snapshot. */
            continue;
        }
        if (sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25) {
            if (!assets->map.has_map_common_graph[1]) {
                drawable = 0;
            }
        } else if (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES ||
                   !assets->map.has_map_object_sprite[sprite]) {
            drawable = 0;
        }
        variant = assets->map.map_object_chunk[background_index][i];
        variant_owner = assets->map.map_object_variant_owner[background_index][i];
        if (variant_owner > 0 && variant_owner <= view->actor_count) {
            variant = view->actors[variant_owner - 1].character;
        } else if (assets->map.map_object_id[background_index][i] >= 0x7d0) {
            variant = rich4_map_rotated_variant(
                assets->map.map_object_variant_base[background_index][i],
                view->viewport.direction);
        }
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = sprite;
        inputs[input_count].object_id = assets->map.map_object_id[background_index][i];
        inputs[input_count].owner = assets->map.map_object_owner[background_index][i];
        /* 0x409660/0x409822: company +0x18 selects palette entry 255. */
        if (inputs[input_count].object_id >= 6001 &&
            inputs[input_count].object_id < 8000) {
            int company = inputs[input_count].object_id - 6001;
            if (company < view->company_count)
                inputs[input_count].owner = view->companies[company].owner + 1;
        }
        inputs[input_count].variant = variant;
        inputs[input_count].world_x = world_x;
        inputs[input_count].world_y = world_y;
        inputs[input_count].sort_bias = 0;
        inputs[input_count].drawable = drawable;
        input_count++;
    }
    /* 0x40829D draws these bit masks immediately while collecting land and
     * facility records, before the unified queue is finally painted. */
    draw_game_map_parity_overlays(screen, assets, background_index, view);
    visible_count = rich4_map_collect_visible(inputs, input_count, &view->viewport,
                                              visible, GAME_MAP_RENDER_QUEUE_CAPACITY);

    for (i = 0; i < visible_count; i++) {
        int sprite = visible[i].sprite;
        int chunk = visible[i].variant;
        const unsigned char *data;
        size_t size;
        const Rich4SprSmpInfo *spr;
        int chunk_count;
        if (sprite == GAME_MAP_RENDER_ACTOR_SPRITE) {
            int player_index = visible[i].asset_index;
            if (!draw_game_player_model(screen,
                                        assets,
                                        view,
                                        player_index,
                                        visible[i].x,
                                        visible[i].y,
                                        0)) {
                SDL_LockSurface(screen);
                fill_rect(screen, visible[i].x - 7, visible[i].y - 7,
                          14, 14, game_player_color(player_index));
                stroke_rect(screen, visible[i].x - 7, visible[i].y - 7,
                            14, 14, 0x0000);
                SDL_UnlockSurface(screen);
            }
            continue;
        }
        if (sprite == GAME_MAP_RENDER_ACTOR_OVERLAY_SPRITE) {
            int player_index = visible[i].asset_index;
            const GameActorView *actor = &view->actors[player_index];
            draw_game_player_model_part(screen,
                                        assets,
                                        actor->character,
                                        actor->overlay_part,
                                        actor->visual_direction,
                                        actor->moving,
                                        actor->walk_frame,
                                        visible[i].x,
                                        visible[i].y);
            continue;
        }
        if (sprite == GAME_MAP_RENDER_SPECIAL_ACTOR_SPRITE) {
            int actor_index = visible[i].asset_index;
            const GameSpecialActorView *actor =
                &view->special_actors[actor_index];
            int part = actor->resource_part;
            int chunks = (int)assets->character.map_special_actor_spr
                [actor_index][part].num_chunks;
            int frames_per_direction = chunks >> 3;
            if (frames_per_direction <= 0) frames_per_direction = 1;
            chunk = actor->visual_direction * frames_per_direction +
                    wrap_index(actor->animation_frame,
                               frames_per_direction);
            draw_spr_smp_center(
                screen,
                assets->character.map_special_actor_data[actor_index][part],
                assets->character.map_special_actor_size[actor_index][part],
                &assets->character.map_special_actor_spr[actor_index][part],
                (size_t)wrap_index(chunk, chunks),
                visible[i].x, visible[i].y);
            continue;
        }
        if (sprite == GAME_MAP_RENDER_SPECIAL_ACTOR_OVERLAY_SPRITE) {
            int chunks;
            if (!assets->map.has_map_common_graph[1]) continue;
            chunks = (int)assets->map.map_common_spr[1].num_chunks;
            if (chunks <= 0) continue;
            chunk = wrap_index(visible[i].variant, chunks);
            draw_spr_smp_center(
                screen, assets->map.map_common_data[1],
                assets->map.map_common_size[1],
                &assets->map.map_common_spr[1], (size_t)chunk,
                visible[i].x, visible[i].y);
            continue;
        }
        if (sprite == GAME_MAP_RENDER_TOOL_ROBOT_SPRITE) {
            int part = view->tool_robot.resource_part;
            int chunks = (int)assets->character.tool_robot_spr[part].num_chunks;
            int frames_per_direction = chunks >= 8 ? chunks / 8 : 1;
            int robot_chunk = view->tool_robot.visual_direction *
                frames_per_direction;
            if (part == 1 && frames_per_direction > 1)
                robot_chunk += wrap_index(
                    view->tool_robot.animation_frame,
                    frames_per_direction);
            if (chunks > 0)
                draw_spr_smp_center(
                    screen, assets->character.tool_robot[part].data,
                    assets->character.tool_robot[part].size,
                    &assets->character.tool_robot_spr[part],
                    (size_t)wrap_index(robot_chunk, chunks),
                    visible[i].x, visible[i].y);
            continue;
        }
        if (sprite < GAME_MAP_RENDER_DYNAMIC_BASE) {
            int type = GAME_MAP_RENDER_DYNAMIC_BASE - sprite;
            int status_index = type - 1;
            if (status_index >= 0 &&
                status_index < GAME_DATA_MAP_STATUS_RESOURCE_COUNT &&
                assets->map.has_data_map_status[status_index]) {
                int chunks = (int)assets->map.data_map_status_spr[status_index].num_chunks;
                if (chunks > 0) {
                    chunk = wrap_index(chunk, chunks);
                    draw_spr_smp_center(screen,
                                        assets->map.data_map_status_data[status_index],
                                        assets->map.data_map_status_size[status_index],
                                        &assets->map.data_map_status_spr[status_index],
                                        (size_t)chunk,
                                        visible[i].x,
                                        visible[i].y);
                }
                /* 0x4098BD: only attached type-18 queue entries carry
                 * both flags. Grounded and flying bombs have no label. */
                if (type == 18 && dynamic_objects != NULL &&
                    (visible[i].object_id & 0xc000) == 0xc000 &&
                    visible[i].asset_index >= 0 &&
                    visible[i].asset_index < GAME_MAP_DYNAMIC_OBJECT_COUNT) {
                    char value[16];
                    snprintf(value, sizeof(value), "%u",
                             (unsigned int)dynamic_objects[visible[i].asset_index].countdown);
                    draw_big5_text(screen, visible[i].x, visible[i].y - 60,
                                   value, 16, 0xf79e, 0x1082, 11, 2);
                }
            }
            continue;
        }
        if (sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25) {
            data = assets->map.map_common_resource[1].data;
            size = assets->map.map_common_resource[1].size;
            spr = &assets->map.map_common_spr[1];
        } else {
            data = assets->map.map_object_resource[sprite].data;
            size = assets->map.map_object_resource[sprite].size;
            spr = &assets->map.map_object_spr[sprite];
        }
        chunk_count = (int)spr->num_chunks;
        if (chunk_count <= 0) {
            continue;
        }
        if (chunk < 0 || chunk >= chunk_count) {
            chunk = wrap_index(chunk, chunk_count);
        }
        draw_spr_smp_center_owner(screen,
                                  data,
                                  size,
                                  spr,
                                  (size_t)chunk,
                                  visible[i].x,
                                  visible[i].y,
                                  visible[i].owner,
                                  visible[i].owner > 0 &&
                                      visible[i].owner <= view->actor_count
                                      ? rich4_map_character_palette_color(
                                            rich4_character_template(
                                                view->actors[visible[i].owner - 1].character)->color)
                                      : 0,
                                  0);
    }
    draw_game_property_flash(screen, assets, settings, view, dynamic_objects);
#undef GAME_MAP_RENDER_QUEUE_CAPACITY
#undef GAME_MAP_RENDER_DYNAMIC_BASE
#undef GAME_MAP_RENDER_SPECIAL_ACTOR_OVERLAY_SPRITE
#undef GAME_MAP_RENDER_SPECIAL_ACTOR_SPRITE
#undef GAME_MAP_RENDER_TOOL_ROBOT_SPRITE
#undef GAME_MAP_RENDER_ACTOR_OVERLAY_SPRITE
#undef GAME_MAP_RENDER_ACTOR_SPRITE
}
