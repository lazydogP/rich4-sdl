/* Frozen pre-optimization scalar pixel reference, for QA comparisons only. */
#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

#define GAME_MAP_INFO_ACTOR_SPRITE (-2)
#define GAME_MAP_INFO_ACTOR_OVERLAY_SPRITE (-3)
#define GAME_MAP_INFO_SPECIAL_ACTOR_SPRITE (-4)
#define GAME_MAP_INFO_SPECIAL_ACTOR_OVERLAY_SPRITE (-5)
#define GAME_MAP_INFO_PROPERTY_MASK_SPRITE (-6)
#define GAME_MAP_INFO_DYNAMIC_BASE (-100)
#define GAME_MAP_INFO_QUEUE_CAPACITY \
    (GAME_MAX_MAP_OBJECTS + 2 * GAME_MAX_MAP_PROPERTIES + \
     2 * GAME_MAX_PLAYERS + 2 * GAME_SPECIAL_ACTOR_MODEL_COUNT + \
     GAME_MAP_DYNAMIC_OBJECT_COUNT + 1)
#define GAME_MAP_INFO_TEXT_CAPACITY 256

static int map_info_background_index(const NewGameSettings *settings)
{
    int index;
    if (settings == NULL) return -1;
    index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    return index >= 0 && index < GAME_BACKGROUND_COUNT ? index : -1;
}

/* This is the same input sequence consumed by draw_game_map_objects.  The
 * original hit table is built beside that queue (0x40829D/0x409B18), so the
 * tooltip must query the resource pixels of these exact visible objects. */
static int map_info_build_visible(const GamePreviewAssets *assets,
                                  int background_index,
                                  const GameViewModel *view,
                                  const GameMapDynamicObjectState *dynamic_objects,
                                  GameVisibleMapObject *out_visible,
                                  int capacity)
{
    MapRenderObjectInput inputs[GAME_MAP_INFO_QUEUE_CAPACITY];
    const MapData *map;
    int input_count = 0;
    int i;

    if (assets == NULL || view == NULL || out_visible == NULL ||
        capacity <= 0 || !view->active || background_index < 0 ||
        background_index >= GAME_BACKGROUND_COUNT) return 0;
    map = &assets->map.maps[background_index];
    memset(inputs, 0, sizeof(inputs));

    /* 0x40865B: ordinary actors and their independent overlay entries. */
    for (i = 0; i < view->actor_count &&
                input_count < GAME_MAP_INFO_QUEUE_CAPACITY; i++) {
        const GameActorView *actor = &view->actors[i];
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = GAME_MAP_INFO_ACTOR_SPRITE;
        inputs[input_count].object_id = 0x8000 | (1 << i);
        inputs[input_count].owner = -1;
        inputs[input_count].world_x = actor->world_x;
        inputs[input_count].world_y = actor->world_y;
        inputs[input_count].sort_bias = i == view->hud.current_player ? 0x0d : 0x0c;
        inputs[input_count].drawable = actor->visible && actor->world_x != 0;
        input_count++;
        if (actor->overlay_part >= 0 &&
            input_count < GAME_MAP_INFO_QUEUE_CAPACITY) {
            inputs[input_count].asset_index = i;
            inputs[input_count].sprite = GAME_MAP_INFO_ACTOR_OVERLAY_SPRITE;
            inputs[input_count].object_id = 0x8000 | (1 << i);
            inputs[input_count].owner = -1;
            inputs[input_count].world_x = actor->world_x;
            inputs[input_count].world_y = actor->world_y;
            inputs[input_count].sort_bias = i == view->hud.current_player ? 0x0f : 0x0e;
            inputs[input_count].drawable = actor->visible && actor->world_x != 0;
            input_count++;
        }
    }

    /* 0x408919: the four fixed/special actor slots. */
    for (i = 0; i < view->special_actor_count &&
                input_count < GAME_MAP_INFO_QUEUE_CAPACITY; i++) {
        const GameSpecialActorView *actor = &view->special_actors[i];
        int part = actor->resource_part;
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = GAME_MAP_INFO_SPECIAL_ACTOR_SPRITE;
        inputs[input_count].object_id = 0x8000 | (0x10 << i);
        inputs[input_count].owner = -1;
        inputs[input_count].world_x = actor->world_x;
        inputs[input_count].world_y = actor->world_y;
        inputs[input_count].sort_bias =
            view->current_actor == i + GAME_MAX_PLAYERS ? 0x0d : 0x08;
        inputs[input_count].drawable = actor->visible &&
            part >= 0 && part < GAME_SPECIAL_ACTOR_MODEL_RESOURCES &&
            i < GAME_SPECIAL_ACTOR_MODEL_COUNT &&
            assets->character.map_special_actor_resource_present[i][part];
        input_count++;
        if (actor->overlay_chunk >= 0 &&
            input_count < GAME_MAP_INFO_QUEUE_CAPACITY) {
            inputs[input_count].asset_index = i;
            inputs[input_count].sprite =
                GAME_MAP_INFO_SPECIAL_ACTOR_OVERLAY_SPRITE;
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

    if (dynamic_objects != NULL) {
        for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT &&
                    input_count < GAME_MAP_INFO_QUEUE_CAPACITY; i++) {
            if (rich4_map_dynamic_build_input(
                    &dynamic_objects[i], i, &view->viewport,
                    view->actors, view->actor_count,
                    map->routes, map->route_count, NULL,
                    &inputs[input_count])) {
                inputs[input_count].sprite = GAME_MAP_INFO_DYNAMIC_BASE -
                    inputs[input_count].sprite;
                input_count++;
            }
        }
    }

    /* The authoritative session view rebuilds land/facility entries rather
     * than trusting the map preview's initial owner/level snapshot. */
    for (i = 0; i < view->land_count &&
                (size_t)(i + 1) < map->land_count &&
                input_count < GAME_MAP_INFO_QUEUE_CAPACITY; i++) {
        const MapLandRecord *record = &map->lands[i + 1];
        int owner = view->lands[i].owner;
        int level = view->lands[i].level;
        int owner_character = owner >= 0 && owner < view->actor_count
            ? view->actors[owner].character : 0;
        MapPropertyRenderStyle style;
        int sprite;
        if (!rich4_map_property_render_style(
                level, owner, view->actor_count, owner_character,
                record->variant, view->viewport.direction, &style)) continue;
        /* sub_40829D keeps every land record in the hit-test queue, even
         * when level==0 and owner==-1 leaves it without a render sprite.
         * sub_409B18 then paints the common property mask for that ID. */
        if (!style.visible) {
            sprite = GAME_MAP_INFO_PROPERTY_MASK_SPRITE;
            style.variant = 0;
            style.palette_owner = -1;
        } else {
            sprite = style.use_owner_marker
                ? GAME_MAP_OBJECT_SPRITE_COMMON_25
                : (view->lands[i].type == 0
                    ? game_map_view_sprite_for_level(background_index, level)
                    : game_map_overlay_sprite(background_index));
            if (sprite != GAME_MAP_OBJECT_SPRITE_COMMON_25 &&
                (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES)) continue;
        }
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = sprite;
        inputs[input_count].object_id = 0x7d1 + i;
        inputs[input_count].owner = style.palette_owner;
        inputs[input_count].variant = style.variant;
        inputs[input_count].world_x = record->x;
        inputs[input_count].world_y = record->y;
        inputs[input_count].drawable = sprite == GAME_MAP_INFO_PROPERTY_MASK_SPRITE
            ? assets->map.has_map_common_graph[2]
            : (sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25
                ? assets->map.has_map_common[1]
                : assets->map.map_object_resource_present[sprite]);
        input_count++;
    }
    for (i = 0; i < view->facility_count &&
                (size_t)(i + 1) < map->facility_count &&
                input_count < GAME_MAP_INFO_QUEUE_CAPACITY; i++) {
        const MapFacilityRecord *record = &map->facilities[i + 1];
        int owner = view->facilities[i].owner;
        int level = view->facilities[i].level;
        int owner_character = owner >= 0 && owner < view->actor_count
            ? view->actors[owner].character : 0;
        MapPropertyRenderStyle style;
        int sprite;
        if (!rich4_map_property_render_style(
                level, owner, view->actor_count, owner_character,
                record->variant, view->viewport.direction, &style)) continue;
        /* Facilities use the same unconditional generic masks as land in
         * the original hit table; an unowned level-0 record is hit-only. */
        if (!style.visible) {
            sprite = GAME_MAP_INFO_PROPERTY_MASK_SPRITE;
            style.variant = 0;
            style.palette_owner = -1;
        } else {
            sprite = style.use_owner_marker
                ? GAME_MAP_OBJECT_SPRITE_COMMON_25
                : game_map_facility_sprite(background_index,
                                           view->facilities[i].type, level);
            if (sprite != GAME_MAP_OBJECT_SPRITE_COMMON_25 &&
                (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES)) continue;
        }
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = sprite;
        inputs[input_count].object_id = 0xfa1 + i;
        inputs[input_count].owner = style.palette_owner;
        inputs[input_count].variant = style.variant;
        inputs[input_count].world_x = record->x;
        inputs[input_count].world_y = record->y;
        inputs[input_count].drawable = sprite == GAME_MAP_INFO_PROPERTY_MASK_SPRITE
            ? assets->map.has_map_common_graph[2]
            : (sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25
                ? assets->map.has_map_common[1]
                : assets->map.map_object_resource_present[sprite]);
        input_count++;
    }

    /* Static map objects include auxiliary companies and special objects;
     * only the [2001, 6000) property range is replaced above. */
    for (i = 0; i < assets->map.map_object_count[background_index] &&
                input_count < GAME_MAP_INFO_QUEUE_CAPACITY; i++) {
        int sprite = assets->map.map_object_sprite[background_index][i];
        int object_id = assets->map.map_object_id[background_index][i];
        int variant;
        int variant_owner;
        int drawable = 1;
        if (object_id >= 0x7d1 && object_id < 0x1770) continue;
        if (sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25)
            drawable = assets->map.has_map_common[1];
        else if (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES)
            drawable = 0;
        else
            /* AI raw-table eligibility follows the original non-null
             * resource pointer, not SDL's SPR/SMP decode result. The actual
             * scene renderer still uses has_map_object_sprite. */
            drawable = assets->map.map_object_resource_present[sprite];
        variant = assets->map.map_object_chunk[background_index][i];
        variant_owner = assets->map.map_object_variant_owner[background_index][i];
        if (variant_owner > 0 && variant_owner <= view->actor_count) {
            variant = view->actors[variant_owner - 1].character;
        } else if (object_id >= 0x7d0) {
            variant = rich4_map_rotated_variant(
                assets->map.map_object_variant_base[background_index][i],
                view->viewport.direction);
        }
        inputs[input_count].asset_index = i;
        inputs[input_count].sprite = sprite;
        inputs[input_count].object_id = object_id;
        inputs[input_count].owner = assets->map.map_object_owner[background_index][i];
        inputs[input_count].variant = variant;
        inputs[input_count].world_x = assets->map.map_object_x[background_index][i];
        inputs[input_count].world_y = assets->map.map_object_y[background_index][i];
        inputs[input_count].drawable = drawable;
        input_count++;
    }

    return rich4_map_collect_visible(inputs, input_count, &view->viewport,
                                     out_visible, capacity);
}

/* Returns -1 for an unusable resource/chunk, 0 for a transparent/outside
 * pixel, and 1 for a nonzero source pixel.  This is the occupancy equivalent
 * of draw_spr_chunk_at/draw_chunk_at and deliberately uses the source's
 * nonzero rule from sub_456A1C. */
static int map_info_chunk_pixel(const unsigned char *data,
                                size_t size,
                                const Rich4SprSmpInfo *spr,
                                size_t chunk_index,
                                int center_x,
                                int center_y,
                                int pixel_x,
                                int pixel_y)
{
    Rich4SprSmpChunk chunk;
    size_t pixels;
    size_t required;
    int local_x;
    int local_y;

    if (data == NULL || spr == NULL ||
        rich4_spr_smp_get_chunk(data, size, spr, chunk_index, &chunk) !=
            RICH4_SPR_SMP_OK || chunk.width <= 0 || chunk.height <= 0)
        return -1;
    pixels = (size_t)chunk.width * (size_t)chunk.height;
    if (memcmp(spr->signature, "SPR", 4) == 0) {
        if (spr->start_offset > size || size - spr->start_offset < 512)
            return -1;
        required = pixels;
    } else if (memcmp(spr->signature, "SMP", 4) == 0) {
        required = pixels * 2;
    } else {
        return -1;
    }
    if ((size_t)chunk.data_size < required || chunk.data_offset > size ||
        size - chunk.data_offset < required) return -1;
    local_x = pixel_x - (center_x - chunk.x);
    local_y = pixel_y - (center_y - chunk.y);
    if (local_x < 0 || local_x >= chunk.width ||
        local_y < 0 || local_y >= chunk.height) return 0;
    if (memcmp(spr->signature, "SPR", 4) == 0) {
        return data[chunk.data_offset +
                    (size_t)local_y * (size_t)chunk.width +
                    (size_t)local_x] != 0;
    }
    {
        uint16_t value;
        if (!rich4_byte_reader_le16(
                data, size,
                chunk.data_offset + 2 *
                    ((size_t)local_y * (size_t)chunk.width +
                     (size_t)local_x), &value)) return -1;
        return value != 0;
    }
}

static int map_info_player_part_pixel(const GamePreviewAssets *assets,
                                      const GameActorView *actor,
                                      int part,
                                      int center_x,
                                      int center_y,
                                      int pixel_x,
                                      int pixel_y)
{
    int character;
    int frames;
    int chunk_index = 0;

    if (assets == NULL || actor == NULL) return -1;
    character = actor->character;
    if (character < 0 || character >= GAME_JUMP_CHARACTER_COUNT ||
        part < 0 || part >= GAME_CHARACTER_MODEL_RESOURCES ||
        !game_asset_ready(assets, &assets->character.has_map_player[character][part])) return -1;
    frames = (int)assets->character.map_player_spr[character][part].num_chunks;
    if (frames <= 0) return -1;
    if (frames >= 8) {
        int frames_per_direction = frames / 8;
        if (frames_per_direction <= 0) frames_per_direction = 1;
        chunk_index = actor->visual_direction * frames_per_direction;
        if (actor->moving) chunk_index += actor->walk_frame % frames_per_direction;
    }
    if (chunk_index < 0 || chunk_index >= frames) chunk_index = 0;
    return map_info_chunk_pixel(
        assets->character.map_player_data[character][part],
        assets->character.map_player_size[character][part],
        &assets->character.map_player_spr[character][part],
        (size_t)chunk_index, center_x, center_y, pixel_x, pixel_y);
}

static int map_info_player_model_pixel(const GamePreviewAssets *assets,
                                       const GameActorView *actor,
                                       int center_x,
                                       int center_y,
                                       int pixel_x,
                                       int pixel_y)
{
    int character;
    int visual_direction;
    int result;

    if (assets == NULL || actor == NULL) return -1;
    character = actor->character;
    if (character < 0 || character >= GAME_JUMP_CHARACTER_COUNT) return -1;
    visual_direction = actor->visual_direction;
    if (actor->use_action_part && actor->action_part >= 0 &&
        actor->action_part < GAME_CHARACTER_MODEL_RESOURCES &&
        game_asset_ready(assets, &assets->character.has_map_player[character][actor->action_part])) {
        int chunks = (int)assets->character.map_player_spr[
            character][actor->action_part].num_chunks;
        int action_frames = chunks >= 8 ? chunks / 8 : chunks;
        int action_frame = actor->action_frame;
        if (action_frames > 0) {
            if (actor->action_holds_last_frame)
                action_frame = action_frames - 1;
            if (action_frame < 0) action_frame = 0;
            else if (action_frame >= action_frames)
                action_frame = action_frames - 1;
            if (chunks >= 8) action_frame += visual_direction * action_frames;
            result = map_info_chunk_pixel(
                assets->character.map_player_data[character][actor->action_part],
                assets->character.map_player_size[character][actor->action_part],
                &assets->character.map_player_spr[character][actor->action_part],
                (size_t)action_frame, center_x, center_y, pixel_x, pixel_y);
            if (result >= 0) return result;
        }
    }
    result = map_info_player_part_pixel(assets, actor, actor->primary_part,
                                        center_x, center_y, pixel_x, pixel_y);
    if (result >= 0) return result;
    if (actor->moving && game_asset_ready(assets, &assets->character.has_map_player[character][1]) &&
        assets->character.map_player_spr[character][1].num_chunks >= 8) {
        int frames_per_direction = (int)assets->character.map_player_spr[
            character][1].num_chunks / 8;
        int chunk_index;
        if (frames_per_direction <= 0) frames_per_direction = 1;
        chunk_index = visual_direction * frames_per_direction +
            actor->walk_frame % frames_per_direction;
        if (chunk_index < 0)
            chunk_index = visual_direction * frames_per_direction;
        result = map_info_chunk_pixel(
            assets->character.map_player_data[character][1],
            assets->character.map_player_size[character][1],
            &assets->character.map_player_spr[character][1],
            (size_t)chunk_index, center_x, center_y, pixel_x, pixel_y);
        if (result >= 0) return result;
    }
    if (game_asset_ready(assets, &assets->character.has_map_player[character][0]) &&
        assets->character.map_player_spr[character][0].num_chunks >= 8) {
        result = map_info_chunk_pixel(
            assets->character.map_player_data[character][0],
            assets->character.map_player_size[character][0],
            &assets->character.map_player_spr[character][0],
            (size_t)visual_direction, center_x, center_y, pixel_x, pixel_y);
        if (result >= 0) return result;
    }
    {
        int direction_group = ((visual_direction + 1) & 7) / 2;
        int frame = actor->walk_frame % 3;
        int part;
        if (frame < 0) frame = 0;
        part = direction_group * 3 + frame;
        result = map_info_player_part_pixel(assets, actor, part,
                                            center_x, center_y,
                                            pixel_x, pixel_y);
        if (result >= 0) return result;
    }
    return -1;
}

static int map_info_actual_pixel(const GamePreviewAssets *assets,
                                 const GameViewModel *view,
                                 const GameVisibleMapObject *visible,
                                 int pixel_x,
                                 int pixel_y)
{
    int sprite;
    int result;

    if (assets == NULL || view == NULL || visible == NULL) return 0;
    sprite = visible->sprite;
    if (sprite == GAME_MAP_INFO_ACTOR_SPRITE) {
        int player = visible->asset_index;
        if (player < 0 || player >= view->actor_count) return 0;
        result = map_info_player_model_pixel(
            assets, &view->actors[player], visible->x, visible->y,
            pixel_x, pixel_y);
        if (result >= 0) return result;
        return pixel_x >= visible->x - 7 && pixel_x < visible->x + 7 &&
               pixel_y >= visible->y - 7 && pixel_y < visible->y + 7;
    }
    if (sprite == GAME_MAP_INFO_ACTOR_OVERLAY_SPRITE) {
        int player = visible->asset_index;
        if (player < 0 || player >= view->actor_count) return 0;
        result = map_info_player_part_pixel(
            assets, &view->actors[player],
            view->actors[player].overlay_part, visible->x, visible->y,
            pixel_x, pixel_y);
        return result > 0;
    }
    if (sprite == GAME_MAP_INFO_SPECIAL_ACTOR_SPRITE) {
        int actor_index = visible->asset_index;
        const GameSpecialActorView *actor;
        int part;
        int chunks;
        int frames_per_direction;
        int chunk;
        if (actor_index < 0 || actor_index >= view->special_actor_count ||
            actor_index >= GAME_SPECIAL_ACTOR_MODEL_COUNT) return 0;
        actor = &view->special_actors[actor_index];
        part = actor->resource_part;
        if (part < 0 || part >= GAME_SPECIAL_ACTOR_MODEL_RESOURCES ||
            !game_asset_ready(assets, &assets->character.has_map_special_actor[actor_index][part])) return 0;
        chunks = (int)assets->character.map_special_actor_spr[
            actor_index][part].num_chunks;
        if (chunks <= 0) return 0;
        frames_per_direction = chunks >> 3;
        if (frames_per_direction <= 0) frames_per_direction = 1;
        chunk = actor->visual_direction * frames_per_direction +
            wrap_index(actor->animation_frame, frames_per_direction);
        result = map_info_chunk_pixel(
            assets->character.map_special_actor_data[actor_index][part],
            assets->character.map_special_actor_size[actor_index][part],
            &assets->character.map_special_actor_spr[actor_index][part],
            (size_t)wrap_index(chunk, chunks), visible->x, visible->y,
            pixel_x, pixel_y);
        return result > 0;
    }
    if (sprite == GAME_MAP_INFO_SPECIAL_ACTOR_OVERLAY_SPRITE) {
        int chunks;
        int chunk;
        if (!assets->map.has_map_common_graph[1]) return 0;
        chunks = (int)assets->map.map_common_spr[1].num_chunks;
        if (chunks <= 0) return 0;
        chunk = wrap_index(visible->variant, chunks);
        result = map_info_chunk_pixel(
            assets->map.map_common_data[1], assets->map.map_common_size[1],
            &assets->map.map_common_spr[1], (size_t)chunk,
            visible->x, visible->y, pixel_x, pixel_y);
        return result > 0;
    }
    if (sprite == GAME_MAP_INFO_PROPERTY_MASK_SPRITE) return 0;
    if (sprite < GAME_MAP_INFO_DYNAMIC_BASE) {
        int type = GAME_MAP_INFO_DYNAMIC_BASE - sprite;
        int status_index = type - 1;
        int chunks;
        int chunk;
        if (status_index < 0 || status_index >= GAME_DATA_MAP_STATUS_RESOURCE_COUNT ||
            !assets->map.has_data_map_status[status_index]) return 0;
        chunks = (int)assets->map.data_map_status_spr[status_index].num_chunks;
        if (chunks <= 0) return 0;
        chunk = wrap_index(visible->variant, chunks);
        result = map_info_chunk_pixel(
            assets->map.data_map_status_data[status_index],
            assets->map.data_map_status_size[status_index],
            &assets->map.data_map_status_spr[status_index],
            (size_t)chunk, visible->x, visible->y, pixel_x, pixel_y);
        return result > 0;
    }
    {
        const unsigned char *data;
        size_t size;
        const Rich4SprSmpInfo *spr;
        int chunks;
        int chunk;
        if (sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25) {
            if (!assets->map.has_map_common_graph[1]) return 0;
            data = assets->map.map_common_data[1];
            size = assets->map.map_common_size[1];
            spr = &assets->map.map_common_spr[1];
        } else {
            if (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES ||
                !assets->map.has_map_object_sprite[sprite]) return 0;
            data = assets->map.map_object_data[sprite];
            size = assets->map.map_object_size[sprite];
            spr = &assets->map.map_object_spr[sprite];
        }
        chunks = (int)spr->num_chunks;
        if (chunks <= 0) return 0;
        chunk = wrap_index(visible->variant, chunks);
        result = map_info_chunk_pixel(data, size, spr, (size_t)chunk,
                                      visible->x, visible->y,
                                      pixel_x, pixel_y);
        return result > 0;
    }
}

static int map_info_generic_pixel(const GamePreviewAssets *assets,
                                  const MapData *map,
                                  const GameViewModel *view,
                                  const GameVisibleMapObject *visible,
                                  int pixel_x,
                                  int pixel_y)
{
    int object_id;
    int record_index;
    int variant;
    int chunk;
    const unsigned char *data;
    size_t size;
    const Rich4SprSmpInfo *spr;
    int result;

    if (assets == NULL || map == NULL || view == NULL || visible == NULL ||
        !assets->map.has_map_common_graph[2]) return 0;
    object_id = visible->object_id;
    if (object_id > 2000 && object_id < 4000) {
        record_index = object_id - 2000;
        if (record_index <= 0 || (size_t)record_index >= map->land_count) return 0;
        variant = map->lands[record_index].variant;
        chunk = rich4_map_rotated_variant(
            variant, view->viewport.direction) & 1;
    } else if (object_id > 4000 && object_id < 8000) {
        if (object_id < 6000) {
            record_index = object_id - 4000;
            if (record_index <= 0 || (size_t)record_index >= map->facility_count) return 0;
            variant = map->facilities[record_index].variant;
        } else {
            record_index = object_id - 6000;
            if (record_index <= 0 || (size_t)record_index >= map->auxiliary_count) return 0;
            variant = map->auxiliary[record_index].variant;
        }
        chunk = 2 + (rich4_map_rotated_variant(
            variant, view->viewport.direction) & 1);
    } else {
        return 0;
    }
    data = assets->map.map_common_data[2];
    size = assets->map.map_common_size[2];
    spr = &assets->map.map_common_spr[2];
    result = map_info_chunk_pixel(data, size, spr, (size_t)chunk,
                                  visible->x, visible->y - 40,
                                  pixel_x, pixel_y - 40);
    return result > 0;
}

/* 0x451985/0x4554FC transforms the final pixels selected by 0x409B18,
 * including the ground footprint, while excluding occluding actors. */
void scalar_draw_game_property_flash(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const NewGameSettings *settings,
                              const GameViewModel *view,
                              const GameMapDynamicObjectState *dynamic_objects)
{
    GameVisibleMapObject visible[GAME_MAP_INFO_QUEUE_CAPACITY];
    const MapData *map;
    int background, count, i, x, y;
    int left = 440, top = 480, right = 0, bottom = 40;
    if (screen == NULL || assets == NULL || settings == NULL || view == NULL ||
        !view->active || view->property_flash_adjust == 0) return;
    background = map_info_background_index(settings);
    if (background < 0) return;
    map = &assets->map.maps[background];
    count = map_info_build_visible(assets, background, view, dynamic_objects,
                                    visible, GAME_MAP_INFO_QUEUE_CAPACITY);
    for (i = 0; i < count; i++) {
        int land = visible[i].object_id - 2001;
        int pass;
        if (land < 0 || land >= view->land_count ||
            !view->lands[land].flash_adjust) continue;
        for (pass = 0; pass < 2; pass++) {
            const unsigned char *data;
            const Rich4SprSmpInfo *spr;
            size_t size;
            int chunk;
            Rich4SprSmpChunk bounds;
            if (pass == 0) {
                if (!assets->map.has_map_common_graph[2] ||
                    (size_t)(land + 1) >= map->land_count) continue;
                data = assets->map.map_common_data[2];
                size = assets->map.map_common_size[2];
                spr = &assets->map.map_common_spr[2];
                chunk = rich4_map_rotated_variant(
                    map->lands[land + 1].variant, view->viewport.direction) & 1;
            } else if (visible[i].sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25) {
                data = assets->map.map_common_data[1];
                size = assets->map.map_common_size[1];
                spr = &assets->map.map_common_spr[1];
                chunk = visible[i].variant;
            } else {
                int sprite = visible[i].sprite;
                if (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES) continue;
                data = assets->map.map_object_data[sprite];
                size = assets->map.map_object_size[sprite];
                spr = &assets->map.map_object_spr[sprite];
                chunk = visible[i].variant;
            }
            if (spr->num_chunks == 0 || rich4_spr_smp_get_chunk(
                    data, size, spr, (size_t)wrap_index(chunk, (int)spr->num_chunks),
                    &bounds) != RICH4_SPR_SMP_OK) continue;
            x = visible[i].x - bounds.x;
            y = visible[i].y - bounds.y;
            if (x < left) left = x;
            if (y < top) top = y;
            if (x + bounds.width > right) right = x + bounds.width;
            if (y + bounds.height > bottom) bottom = y + bounds.height;
        }
    }
    if (left < 0) left = 0;
    if (top < 40) top = 40;
    if (right > 440) right = 440;
    if (bottom > 480) bottom = 480;
    if (right > screen->w) right = screen->w;
    if (bottom > screen->h) bottom = screen->h;
    SDL_LockSurface(screen);
    for (y = top; y < bottom; y++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
        for (x = left; x < right; x++) {
            for (i = count - 1; i >= 0; i--) {
                int land;
                if (!map_info_generic_pixel(assets, map, view, &visible[i], x, y) &&
                    !map_info_actual_pixel(assets, view, &visible[i], x, y)) continue;
                land = visible[i].object_id - 2001;
                if (land >= 0 && land < view->land_count &&
                    view->lands[land].flash_adjust)
                    row[x] = map_surface_pixel_property_flash(
                        screen, row[x], view->lands[land].flash_adjust);
                break;
            }
        }
    }
    SDL_UnlockSurface(screen);
}
