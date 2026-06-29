#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
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
#include "platform/sdl1/runtime_trace.h"
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

static int map_info_visible_tile_window(const GameViewModel *view,
                                        int world_x,
                                        int world_y)
{
    int tile_x;
    int tile_y;
    if (view == NULL) return 0;
    tile_x = (world_x >> 5) - (view->viewport.center_world_x >> 5) + 14;
    tile_y = (world_y >> 5) - (view->viewport.center_world_y >> 5) + 14;
    return tile_x >= 0 && tile_x <= 28 && tile_y >= 0 && tile_y <= 28;
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

int game_map_collect_ai_objects(
    const GamePreviewAssets *assets, int background_index,
    const GameViewModel *view,
    const GameMapDynamicObjectState *dynamic_objects,
    GameVisibleMapObject *out_objects, int capacity)
{
    int count;
    int i;

    if (assets == NULL || view == NULL || !view->active ||
        background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        out_objects == NULL || capacity <= 0)
        return 0;
    count = map_info_build_visible(assets, background_index, view,
                                   dynamic_objects, out_objects, capacity);

    /* 0x409DE7 excludes dynamic-pool entries whose owner byte is nonzero.
     * The SDL pool stores that same byte as attached_player; other high-bit
     * queue entries (actors) are not dynamic pool slots. */
    {
        int kept = 0;
        for (i = 0; i < count; i++) {
            const GameVisibleMapObject *object = &out_objects[i];
            int slot = object->asset_index;
            /* 0x409DE7 admits one anchor per resource-backed queue entry,
             * never a sprite footprint. Window Y includes the 40px HUD. */
            if (object->object_id == 0 || object->x < 0 || object->x >= 440 ||
                object->y < 40 || object->y >= 480)
                continue;
            /* 0x40889A and 0x408ACB append ordinary/special actor overlays
             * with word_48A850 == 0; 0x409DE7 ignores raw-ID-zero entries.
             * The shared SDL draw/hit queue carries the actor ID on both
             * entries, so omit both overlays from AI raw-table projection
             * while leaving them drawable. */
            if (object->sprite == GAME_MAP_INFO_ACTOR_OVERLAY_SPRITE ||
                object->sprite == GAME_MAP_INFO_SPECIAL_ACTOR_OVERLAY_SPRITE)
                continue;
            /* 0x40829D selects this ordinary actor's resource pointer from
             * the EA1/EA2 slot table; 0x409DE7 skips the raw entry when that
             * pointer is null. Keep archive presence separate from SDL's
             * stricter directional-SMP decode result so an unsupported but
             * present original resource does not disappear from AI targets. */
            if (object->sprite == GAME_MAP_INFO_ACTOR_SPRITE) {
                int actor_index = object->asset_index;
                const GameActorView *actor;
                int part;
                if (actor_index < 0 || actor_index >= view->actor_count)
                    continue;
                actor = &view->actors[actor_index];
                part = actor->use_action_part
                    ? actor->action_part : actor->primary_part;
                if (actor->character < 0 ||
                    actor->character >= GAME_JUMP_CHARACTER_COUNT ||
                    part < 0 || part >= GAME_CHARACTER_MODEL_RESOURCES ||
                    !assets->character.map_player_resource_present
                        [actor->character][part])
                    continue;
            }
            if (object->sprite == GAME_MAP_INFO_SPECIAL_ACTOR_SPRITE) {
                int actor_index = object->asset_index;
                const GameSpecialActorView *actor;
                int part;
                if (actor_index < 0 ||
                    actor_index >= view->special_actor_count)
                    continue;
                actor = &view->special_actors[actor_index];
                part = actor->resource_part;
                if (part < 0 ||
                    part >= GAME_SPECIAL_ACTOR_MODEL_RESOURCES ||
                    actor_index >= GAME_SPECIAL_ACTOR_MODEL_COUNT ||
                    !assets->character.map_special_actor_resource_present
                        [actor_index][part])
                    continue;
            }
            /* The map-info hit list synthesizes a generic mask for an
             * unowned level-0 land/facility.  Original 0x409DE7 only puts
             * entries with a non-null object resource into the AI pixel
             * table, and those records have no resource in 0x40829D. */
            if (object->sprite == GAME_MAP_INFO_PROPERTY_MASK_SPRITE)
                continue;
            if (object->sprite < GAME_MAP_INFO_DYNAMIC_BASE) {
                int type = GAME_MAP_INFO_DYNAMIC_BASE - object->sprite;
                int status_index = type - 1;
                if (status_index < 0 ||
                    status_index >= GAME_DATA_MAP_STATUS_RESOURCE_COUNT ||
                    !assets->map.data_map_status_resource_present[status_index])
                    continue;
            }
            if (object->sprite < GAME_MAP_INFO_DYNAMIC_BASE &&
                object->object_id > 0 &&
                (object->object_id & 0x8000) != 0 &&
                dynamic_objects != NULL && slot >= 0 &&
                slot < GAME_MAP_DYNAMIC_OBJECT_COUNT &&
                dynamic_objects[slot].attached_player != 0)
                continue;
            if (kept != i) out_objects[kept] = out_objects[i];
            kept++;
        }
        count = kept;
    }
    return count;
}

/* Resolved nonzero source footprint, matching sub_456A1C. It only borrows
 * pixels during this call; no resource pointers survive their leases. */
typedef struct MapInfoMask {
    const unsigned char *pixels;
    int left, top, width, height, bytes_per_pixel;
} MapInfoMask;

static int map_info_chunk_mask(const unsigned char *data, size_t size,
    const Rich4SprSmpInfo *spr, size_t index, int cx, int cy, MapInfoMask *mask)
{
    Rich4SprSmpChunk chunk;
    size_t required;
    int bytes;
    memset(mask, 0, sizeof(*mask));
    if (data == NULL || spr == NULL ||
        rich4_spr_smp_get_chunk(data, size, spr, index, &chunk) != RICH4_SPR_SMP_OK ||
        chunk.width <= 0 || chunk.height <= 0) return -1;
    if (memcmp(spr->signature, "SPR", 4) == 0) {
        if (spr->start_offset > size || size - spr->start_offset < 512) return -1;
        bytes = 1;
    } else if (memcmp(spr->signature, "SMP", 4) == 0) bytes = 2;
    else return -1;
    required = (size_t)chunk.width * (size_t)chunk.height * (size_t)bytes;
    if (chunk.data_offset > size || chunk.data_size < required ||
        size - chunk.data_offset < required) return -1;
    mask->pixels = data + chunk.data_offset;
    mask->left = cx - chunk.x; mask->top = cy - chunk.y;
    mask->width = chunk.width; mask->height = chunk.height;
    mask->bytes_per_pixel = bytes;
    return 1;
}

static int map_info_mask_pixel(const MapInfoMask *mask, int x, int y)
{
    size_t offset;
    x -= mask->left; y -= mask->top;
    if (x < 0 || y < 0 || x >= mask->width || y >= mask->height) return 0;
    if (mask->pixels == NULL) return 1;
    offset = ((size_t)y * mask->width + x) * mask->bytes_per_pixel;
    return mask->pixels[offset] ||
        (mask->bytes_per_pixel == 2 && mask->pixels[offset + 1]);
}

static int map_info_chunk_pixel(const unsigned char *data, size_t size,
    const Rich4SprSmpInfo *spr, size_t index, int cx, int cy, int x, int y)
{
    MapInfoMask mask;
    if (map_info_chunk_mask(data, size, spr, index, cx, cy, &mask) < 0) return -1;
    return map_info_mask_pixel(&mask, x, y);
}

static int map_info_player_part_mask(const GamePreviewAssets *assets,
                                      const GameActorView *actor,
                                      int part,
                                      int center_x,
                                      int center_y,
                                      MapInfoMask *mask)
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
    return map_info_chunk_mask(
        assets->character.map_player_data[character][part],
        assets->character.map_player_size[character][part],
        &assets->character.map_player_spr[character][part],
        (size_t)chunk_index, center_x, center_y, mask);
}

static int map_info_player_model_mask(const GamePreviewAssets *assets,
                                       const GameActorView *actor,
                                       int center_x,
                                       int center_y,
                                       MapInfoMask *mask)
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
            result = map_info_chunk_mask(
                assets->character.map_player_data[character][actor->action_part],
                assets->character.map_player_size[character][actor->action_part],
                &assets->character.map_player_spr[character][actor->action_part],
                (size_t)action_frame, center_x, center_y, mask);
            if (result >= 0) return result;
        }
    }
    result = map_info_player_part_mask(assets, actor, actor->primary_part,
                                        center_x, center_y, mask);
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
        result = map_info_chunk_mask(
            assets->character.map_player_data[character][1],
            assets->character.map_player_size[character][1],
            &assets->character.map_player_spr[character][1],
            (size_t)chunk_index, center_x, center_y, mask);
        if (result >= 0) return result;
    }
    if (game_asset_ready(assets, &assets->character.has_map_player[character][0]) &&
        assets->character.map_player_spr[character][0].num_chunks >= 8) {
        result = map_info_chunk_mask(
            assets->character.map_player_data[character][0],
            assets->character.map_player_size[character][0],
            &assets->character.map_player_spr[character][0],
            (size_t)visual_direction, center_x, center_y, mask);
        if (result >= 0) return result;
    }
    {
        int direction_group = ((visual_direction + 1) & 7) / 2;
        int frame = actor->walk_frame % 3;
        int part;
        if (frame < 0) frame = 0;
        part = direction_group * 3 + frame;
        result = map_info_player_part_mask(assets, actor, part,
                                            center_x, center_y,
                                            mask);
        if (result >= 0) return result;
    }
    return -1;
}

static int map_info_default_dynamic_type(int slot)
{
    if (slot < 0 || slot >= GAME_MAP_DYNAMIC_OBJECT_COUNT) return 0;
    if (slot < 14) return slot + 1;
    if (slot < 16) return 15;
    if (slot < 26) return 16;
    if (slot < 36) return 17;
    return 18;
}

static int map_info_actual_mask(const GamePreviewAssets *assets,
                                 const GameViewModel *view,
                                 const GameVisibleMapObject *visible,
                                 MapInfoMask *mask)
{
    int sprite;
    int result;

    if (assets == NULL || view == NULL || visible == NULL) return 0;
    sprite = visible->sprite;
    if (sprite == GAME_MAP_INFO_ACTOR_SPRITE) {
        int player = visible->asset_index;
        if (player < 0 || player >= view->actor_count) return 0;
        result = map_info_player_model_mask(
            assets, &view->actors[player], visible->x, visible->y,
            mask);
        if (result >= 0) return result;
        memset(mask, 0, sizeof(*mask));
        mask->left = visible->x - 7; mask->top = visible->y - 7;
        mask->width = mask->height = 14;
        return 1;
    }
    if (sprite == GAME_MAP_INFO_ACTOR_OVERLAY_SPRITE) {
        int player = visible->asset_index;
        if (player < 0 || player >= view->actor_count) return 0;
        result = map_info_player_part_mask(
            assets, &view->actors[player],
            view->actors[player].overlay_part, visible->x, visible->y,
            mask);
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
        result = map_info_chunk_mask(
            assets->character.map_special_actor_data[actor_index][part],
            assets->character.map_special_actor_size[actor_index][part],
            &assets->character.map_special_actor_spr[actor_index][part],
            (size_t)wrap_index(chunk, chunks), visible->x, visible->y,
            mask);
        return result > 0;
    }
    if (sprite == GAME_MAP_INFO_SPECIAL_ACTOR_OVERLAY_SPRITE) {
        int chunks;
        int chunk;
        if (!assets->map.has_map_common_graph[1]) return 0;
        chunks = (int)assets->map.map_common_spr[1].num_chunks;
        if (chunks <= 0) return 0;
        chunk = wrap_index(visible->variant, chunks);
        result = map_info_chunk_mask(
            assets->map.map_common_data[1], assets->map.map_common_size[1],
            &assets->map.map_common_spr[1], (size_t)chunk,
            visible->x, visible->y, mask);
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
        result = map_info_chunk_mask(
            assets->map.data_map_status_data[status_index],
            assets->map.data_map_status_size[status_index],
            &assets->map.data_map_status_spr[status_index],
            (size_t)chunk, visible->x, visible->y, mask);
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
        result = map_info_chunk_mask(data, size, spr, (size_t)chunk,
                                      visible->x, visible->y,
                                      mask);
        return result > 0;
    }
}

static int map_info_generic_mask(const GamePreviewAssets *assets,
                                  const MapData *map,
                                  const GameViewModel *view,
                                  const GameVisibleMapObject *visible,
                                  MapInfoMask *mask)
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
    result = map_info_chunk_mask(data, size, spr, (size_t)chunk,
                                  visible->x, visible->y,
                                  mask);
    return result > 0;
}

static int map_info_actual_pixel(const GamePreviewAssets *assets,
    const GameViewModel *view, const GameVisibleMapObject *object, int x, int y)
{
    MapInfoMask mask;
    return map_info_actual_mask(assets, view, object, &mask) > 0 &&
        map_info_mask_pixel(&mask, x, y);
}

static int map_info_generic_pixel(const GamePreviewAssets *assets, const MapData *map,
    const GameViewModel *view, const GameVisibleMapObject *object, int x, int y)
{
    MapInfoMask mask;
    return map_info_generic_mask(assets, map, view, object, &mask) > 0 &&
        map_info_mask_pixel(&mask, x, y);
}

/* 0x409B18 stamps masks in depth order once. Later opaque pixels replace
 * earlier IDs, including actors which clear a property's flash selection. */
static void map_info_stamp(unsigned char *selection, const MapInfoMask *mask,
                            unsigned char selected)
{
    int x, y, left = mask->left, top = mask->top;
    int right = left + mask->width, bottom = top + mask->height;
    if (left < 0) left = 0;
    if (top < 40) top = 40;
    if (right > 440) right = 440;
    if (bottom > 480) bottom = 480;
    for (y = top; y < bottom; y++)
        for (x = left; x < right; x++)
            if (map_info_mask_pixel(mask, x, y))
                selection[(y - 40) * 440 + x] = selected;
}

static void map_info_flash_mask(unsigned char *selection,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameViewModel *view, const GameMapDynamicObjectState *objects)
{
    GameVisibleMapObject visible[GAME_MAP_INFO_QUEUE_CAPACITY];
    int i, count, background = map_info_background_index(settings);
    const MapData *map = &assets->map.maps[background];
    memset(selection, 0, 440u * 440u);
    count = map_info_build_visible(assets, background, view, objects,
                                   visible, GAME_MAP_INFO_QUEUE_CAPACITY);
    for (i = 0; i < count; i++) {
        MapInfoMask mask;
        int land = visible[i].object_id - 2001;
        unsigned char selected = land >= 0 && land < view->land_count
            ? (unsigned char)(signed char)view->lands[land].flash_adjust : 0;
        if (map_info_generic_mask(assets, map, view, &visible[i], &mask) > 0)
            map_info_stamp(selection, &mask, selected);
        if (map_info_actual_mask(assets, view, &visible[i], &mask) > 0)
            map_info_stamp(selection, &mask, selected);
    }
}

static void map_info_apply_flash(SDL_Surface *screen, const unsigned char *selection,
                                  int override_adjust)
{
    int x, y;
    SDL_LockSurface(screen);
    for (y = 40; y < 480 && y < screen->h; y++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
        for (x = 0; x < 440 && x < screen->w; x++) {
            int adjust = (signed char)selection[(y - 40) * 440 + x];
            if (adjust)
                row[x] = map_surface_pixel_property_flash(screen, row[x],
                    override_adjust ? override_adjust : adjust);
        }
    }
    SDL_UnlockSurface(screen);
}

void draw_game_property_flash(SDL_Surface *screen,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameViewModel *view, const GameMapDynamicObjectState *objects)
{
    unsigned char selection[440 * 440];
    unsigned int started;
    if (!screen || !assets || !settings || !view || !view->active ||
        !view->property_flash_adjust || map_info_background_index(settings) < 0) return;
    started = runtime_profile_now();
    map_info_flash_mask(selection, assets, settings, view, objects);
    runtime_profile_span("render", "property.mask", started, -1, 0);
    started = runtime_profile_now();
    map_info_apply_flash(screen, selection, 0);
    runtime_profile_span("render", "property.flash", started, -1, 0);
}

void reset_game_property_flash_cache(GameRenderCache *cache)
{
    if (!cache) return;
    if (cache->property_flash_base) SDL_FreeSurface(cache->property_flash_base);
    free(cache->property_flash_mask);
    cache->property_flash_base = NULL;
    cache->property_flash_mask = NULL;
}

/* sub_409B18(1) and sub_451985 keep one mask and an unchanged source map
 * for all sixteen frames. The app suspends the underlying turn meanwhile. */
int draw_game_cached_property_flash(SDL_Surface *screen, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const GameViewModel *view,
    const GameMapDynamicObjectState *objects, GameRenderCache *cache)
{
    SDL_Rect rect = {0, 40, 440, 440}, dest = {0, 0, 440, 440};
    unsigned int started;
    if (!cache) return 0;
    if (!view->property_flash_active) {
        reset_game_property_flash_cache(cache);
        return 0;
    }
    if (!cache->property_flash_base) {
        GameViewModel unmodified = *view;
        unmodified.property_flash_adjust = 0;
        cache->property_flash_mask = malloc(440u * 440u);
        cache->property_flash_base = SDL_CreateRGBSurface(SDL_SWSURFACE, 440, 440,
            screen->format->BitsPerPixel, screen->format->Rmask, screen->format->Gmask,
            screen->format->Bmask, screen->format->Amask);
        if (!cache->property_flash_mask || !cache->property_flash_base) {
            reset_game_property_flash_cache(cache); return 0;
        }
        draw_game_map_objects(screen, assets, settings, &unmodified, objects);
        SDL_BlitSurface(screen, &rect, cache->property_flash_base, &dest);
        started = runtime_profile_now();
        map_info_flash_mask(cache->property_flash_mask, assets, settings, view, objects);
        runtime_profile_span("render", "property.mask", started, -1, 0);
    }
    SDL_BlitSurface(cache->property_flash_base, NULL, screen, &rect);
    if (view->property_flash_adjust) {
        started = runtime_profile_now();
        map_info_apply_flash(screen, cache->property_flash_mask, view->property_flash_adjust);
        runtime_profile_span("render", "property.flash", started, -1, 0);
    }
    return 1;
}

int game_map_object_at(const GamePreviewAssets *assets,
                       const NewGameSettings *settings,
                       const GameViewModel *view,
                       const GameMapDynamicObjectState *dynamic_objects,
                       int cursor_x,
                       int cursor_y,
                       GameVisibleMapObject *out_object)
{
    GameVisibleMapObject visible[GAME_MAP_INFO_QUEUE_CAPACITY];
    GameVisibleMapObject candidate;
    const MapData *map;
    int background_index;
    int visible_count;
    int node;
    int found = 0;

    if (out_object != NULL) memset(out_object, 0, sizeof(*out_object));
    if (assets == NULL || settings == NULL || view == NULL || !view->active ||
        cursor_x < 0 || cursor_x >= 440 || cursor_y < 40 || cursor_y >= 480)
        return 0;
    background_index = map_info_background_index(settings);
    if (background_index < 0) return 0;
    map = &assets->map.maps[background_index];
    memset(&candidate, 0, sizeof(candidate));

    /* 0x409B18 paints route-node masks before the unified visible queue. */
    if (assets->map.has_map_common_graph[2] && map->route_count > 1 &&
        assets->map.map_common_spr[2].num_chunks > 4) {
        for (node = 1; (size_t)node < map->route_count; node++) {
            int x;
            int y;
            if (map->routes[node].raw[4] == 0 ||
                (map->routes[node].flags & 0x00ffff00U) != 0 ||
                !map_info_visible_tile_window(view,
                                              map->routes[node].x,
                                              map->routes[node].y)) continue;
            rich4_game_viewport_project(&view->viewport,
                                        map->routes[node].x,
                                        map->routes[node].y, &x, &y);
            if (map_info_chunk_pixel(
                    assets->map.map_common_data[2],
                    assets->map.map_common_size[2],
                    &assets->map.map_common_spr[2], 4, x, y - 40,
                    cursor_x, cursor_y - 40) > 0) {
                candidate.object_id = node;
                candidate.x = x;
                candidate.y = y;
                found = 1;
            }
        }
    }

    visible_count = map_info_build_visible(
        assets, background_index, view, dynamic_objects,
        visible, GAME_MAP_INFO_QUEUE_CAPACITY);
    for (node = 0; node < visible_count; node++) {
        if (map_info_generic_pixel(assets, map, view, &visible[node],
                                   cursor_x, cursor_y) ||
            map_info_actual_pixel(assets, view, &visible[node],
                                  cursor_x, cursor_y)) {
            candidate = visible[node];
            found = 1;
        }
    }
    if (!found || candidate.object_id == 0) return 0;
    if (out_object != NULL) *out_object = candidate;
    return candidate.object_id;
}

static const char *const map_info_land_level_labels[6] = {
    "\xaa\xc5\x20\x20\xa6\x61",
    "\xa5\xad\x20\x20\xa9\xd0",
    "\xa9\xb1\x20\x20\xe7\x45",
    "\xb0\xd3\x20\x20\xb3\xf5",
    "\xb0\xd3\xb7\x7e\xa4\x6a\xbc\xd3",
    "\xbc\xaf\xa4\xd1\xa4\x6a\xbc\xd3"
};

static const char *const map_info_facility_labels[9] = {
    "\xa4\xbd\x20\x20\xb6\xe9",
    "\xae\xc8\x20\x20\xc0\x5d",
    "\xc1\xca\xaa\xab\xa4\xa4\xa4\xdf",
    "\xa5\x5b\xaa\x6f\xaf\xb8", /* 加油站 @ 0x46385E, table 0x475150[3] */
    "\xac\xe3\xa8\x73\xa9\xd2",
    "\xa2\xaf\xaf\xc5",
    "\xa4\x40\xaf\xc5",
    "\xa4\x47\xaf\xc5",
    "\xa4\x54\xaf\xc5"
};

static const char *const map_info_level_labels[8] = {
    "\xa2\xaf\xaf\xc5",
    "\xa4\x40\xaf\xc5",
    "\xa4\x47\xaf\xc5",
    "\xa4\x54\xaf\xc5",
    "\xa5\x7c\xaf\xc5",
    "\xa4\xad\xaf\xc5",
    "\xb9\x4c\xb8\xf4\xb6\x4f",
    "\xa9\xd0\xaf\xb2\xb6\x4f"
};

static const char *const map_info_high_special_labels[9] = {
    "\xa4\x70\xb0\xbd",
    "\xb1\x6a\xb5\x73",
    "\xac\x79\xaa\x5d",
    "\xb6\xa1\xbf\xd2",
    "\xa4\x70\xb0\x5d\xaf\xab",
    "\xa4\x6a\xb0\x5d\xaf\xab",
    "\xa4\x70\xba\xd6\xaf\xab",
    "\xa4\x6a\xba\xd6\xaf\xab",
    "\xa4\x70\xbd\x61\xaf\xab"
};

static const char *const map_info_dynamic_labels[19] = {
    "",
    "\xa4\x70\xb0\x5d\xaf\xab",
    "\xa4\x6a\xb0\x5d\xaf\xab",
    "\xa4\x70\xba\xd6\xaf\xab",
    "\xa4\x6a\xba\xd6\xaf\xab",
    "\xa4\x70\xbd\x61\xaf\xab",
    "\xa4\x6a\xbd\x61\xaf\xab",
    "\xa4\x70\xb0\x49\xaf\xab",
    "\xa4\x6a\xb0\x49\xaf\xab",
    "\xa4\xd1\xa8\xcf",
    "\xb4\x63\xc5\x5d",
    "\xb4\x63\xa4\xfc",
    "\xa4\x67\xa6\x61\xa4\xbd",
    "\xc2\xa7\xaa\xab",
    "\xc4\x5f\xbd\x63",
    "\xa6\xba\xaf\xab",
    "\xb8\xf4\xbb\xd9",
    "\xa6\x61\xb9\x70",
    "\xa9\x77\xae\xc9\xac\xb5\xbc\x75"
};

static void map_info_copy_name(char *out,
                               size_t out_size,
                               const unsigned char *source,
                               size_t source_size)
{
    size_t i;
    if (out == NULL || out_size == 0) return;
    out[0] = '\0';
    if (source == NULL || source_size == 0) return;
    for (i = 0; i + 1 < out_size && i < source_size && source[i] != 0; i++)
        out[i] = (char)source[i];
    out[i] = '\0';
}

static int map_info_names_equal(const unsigned char *left,
                                const unsigned char *right,
                                size_t size)
{
    size_t i;
    if (left == NULL || right == NULL) return 0;
    for (i = 0; i < size; i++) {
        if (left[i] != right[i]) return 0;
        if (left[i] == 0) return 1;
    }
    return 1;
}

static int map_info_bounded_amount(long long amount)
{
    if (amount <= 0) return 0;
    if (amount > 0x7fffffffLL) return 0x7fffffff;
    return (int)amount;
}

static void map_info_money(char *out, size_t out_size, long long amount)
{
    format_hud_number(out, out_size, map_info_bounded_amount(amount), 1);
}

static void map_info_draw_text(SDL_Surface *screen,
                               int x,
                               int y,
                               const char *text)
{
    if (screen == NULL || text == NULL || text[0] == '\0') return;
    draw_big5_text(screen, x, y, text, 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 3, 2);
}

static void map_info_draw_owner(SDL_Surface *screen,
                                const GameViewModel *view,
                                int owner,
                                int x,
                                int y)
{
    if (view == NULL || owner < 0 || owner >= view->actor_count) return;
    map_info_draw_text(screen, x, y,
                       new_game_character_name(view->actors[owner].character));
}

static int map_info_land_group_value(const GameViewModel *view,
                                     const MapData *map,
                                     int owner,
                                     const MapLandRecord *target,
                                     int price_index)
{
    long long sum = 0;
    int i;
    if (view == NULL || map == NULL || target == NULL || owner < 0) return 0;
    for (i = 0; i < view->land_count && (size_t)(i + 1) < map->land_count; i++) {
        const GameLandView *land = &view->lands[i];
        const MapLandRecord *record = &map->lands[i + 1];
        int level;
        if (land->type != 0 || land->owner != owner ||
            !map_info_names_equal(record->raw + 4, target->raw + 4, 20)) continue;
        level = land->level;
        if (level < 0) level = 0;
        if (level > 5) level = 5;
        sum += record->toll_by_level[level];
    }
    return map_info_bounded_amount(
        sum * (price_index > 0 ? price_index : 1));
}

static int map_info_land_type_count(const GameViewModel *view, int owner)
{
    int i;
    int count = 0;
    if (view == NULL || owner < 0) return 0;
    for (i = 0; i < view->land_count; i++)
        if (view->lands[i].owner == owner && view->lands[i].type != 0)
            count++;
    return count;
}

static void map_info_draw_land(SDL_Surface *screen,
                               const GameViewModel *view,
                               const MapData *map,
                               int view_index,
                               int x,
                               int base_y)
{
    const MapLandRecord *record;
    const GameLandView *land;
    char text[GAME_MAP_INFO_TEXT_CAPACITY];
    int level;
    int owner;
    int price_index;
    long long amount;

    if (screen == NULL || view == NULL || map == NULL || view_index < 0 ||
        view_index >= view->land_count ||
        (size_t)(view_index + 1) >= map->land_count) return;
    record = &map->lands[view_index + 1];
    land = &view->lands[view_index];
    owner = land->owner;
    level = land->level;
    if (level < 0) level = 0;
    if (level >= (int)(sizeof(map_info_level_labels) /
                       sizeof(map_info_level_labels[0]))) level = 0;
    price_index = view->hud.price_index > 0 ? view->hud.price_index : 1;
    map_info_draw_owner(screen, view, owner, x, base_y);
    if (land->level == 0) {
        map_info_draw_text(screen, x, base_y + 18,
                           map_info_land_level_labels[0]);
    } else if (land->type != 0) {
        map_info_draw_text(screen, x, base_y + 18,
                           "\xb3\x73\xc2\xea\xa9\xb1");
    } else {
        map_info_draw_text(screen, x, base_y + 18,
                           map_info_land_level_labels[level < 6 ? level : 0]);
    }
    map_info_draw_text(screen, x, base_y + 36, map_info_level_labels[level]);
    if (owner >= 0) {
        if (land->type != 0) {
            amount = 2000LL * price_index;
            map_info_money(text, sizeof(text), amount);
            snprintf(text + strlen(text), sizeof(text) - strlen(text),
                     "\xa1\xd1%d", map_info_land_type_count(view, owner));
        } else {
            amount = map_info_land_group_value(
                view, map, owner, record, price_index);
            map_info_money(text, sizeof(text), amount);
        }
    } else {
        int toll_level = land->level;
        if (toll_level < 0) toll_level = 0;
        if (toll_level > 5) toll_level = 5;
        map_info_money(text, sizeof(text), record->toll_by_level[toll_level]);
    }
    map_info_draw_text(screen, x, base_y + 54, text);
}

static void map_info_draw_facility(SDL_Surface *screen,
                                   const GameViewModel *view,
                                   const MapData *map,
                                   int view_index,
                                   int x,
                                   int base_y)
{
    const MapFacilityRecord *record;
    const GameFacilityView *facility;
    char text[GAME_MAP_INFO_TEXT_CAPACITY];
    int type;
    int level;
    int owner;
    int price_index;

    if (screen == NULL || view == NULL || map == NULL || view_index < 0 ||
        view_index >= view->facility_count ||
        (size_t)(view_index + 1) >= map->facility_count) return;
    record = &map->facilities[view_index + 1];
    facility = &view->facilities[view_index];
    owner = facility->owner;
    type = facility->type;
    level = facility->level;
    if (level < 0) level = 0;
    if (level > 5) level = 5;
    price_index = view->hud.price_index > 0 ? view->hud.price_index : 1;
    map_info_draw_owner(screen, view, owner, x, base_y);
    if (facility->level == 0) {
        map_info_draw_text(screen, x, base_y + 27,
                           map_info_land_level_labels[0]);
        return;
    }
    if (type == 0) {
        if (type < (int)(sizeof(map_info_facility_labels) /
                         sizeof(map_info_facility_labels[0])))
            map_info_draw_text(screen, x, base_y + 27,
                               map_info_facility_labels[type]);
        return;
    }
    if (type == 1 || type == 2) {
        map_info_draw_text(screen, x, base_y + 18,
                           type < 9 ? map_info_facility_labels[type] : "");
        map_info_draw_text(screen, x, base_y + 36, map_info_level_labels[level]);
        map_info_money(text, sizeof(text),
                       (long long)price_index * record->charge_by_level[level]);
        map_info_draw_text(screen, x, base_y + 54, text);
    } else if (type == 3) {
        map_info_draw_text(screen, x, base_y + 27,
                           type < 9 ? map_info_facility_labels[type] : "");
        map_info_money(text, sizeof(text), 1000);
        map_info_draw_text(screen, x, base_y + 54, text);
    } else if (type == 4) {
        map_info_draw_text(screen, x, base_y + 18,
                           type < 9 ? map_info_facility_labels[type] : "");
        map_info_draw_text(screen, x, base_y + 36, map_info_level_labels[level]);
        if (facility->research_turns != 0 &&
            facility->research_selection >= 0 &&
            facility->research_selection <= 5)
            map_info_draw_text(screen, x, base_y + 54,
                               game_tool_big5_name(
                                   7 + facility->research_selection));
    }
}

static int map_info_dynamic_slot_from_id(int object_id)
{
    int high;
    int base_high;
    if ((object_id & 0xff) != 0) return -1;
    high = (object_id >> 8) & 0x7f;
    base_high = high;
    /* Type 18 carries the original 0x4000 marker.  Its coordinate resolver
     * masks that marker before indexing the 46-slot table; undo it here so
     * the tooltip follows the same visible object. */
    if ((object_id & 0x4000) != 0) base_high &= ~0x40;
    if (base_high < 1 || base_high > GAME_MAP_DYNAMIC_OBJECT_COUNT) return -1;
    return base_high - 1;
}

static void map_info_draw_high_bit(SDL_Surface *screen,
                                   const GameViewModel *view,
                                   const GameMapDynamicObjectState *dynamic_objects,
                                   int object_id,
                                   int x,
                                   int y)
{
    unsigned int low = (unsigned int)object_id & 0xffU;
    if (low != 0) {
        int bit = 0;
        unsigned int value = low;
        while ((value & 1U) == 0) {
            value >>= 1;
            bit++;
        }
        if (bit < 4 && view != NULL && bit < view->actor_count) {
            map_info_draw_text(screen, x, y,
                               new_game_character_name(view->actors[bit].character));
        } else if (bit >= 4 && bit - 4 < 9) {
            map_info_draw_text(screen, x, y,
                               map_info_high_special_labels[bit - 4]);
        }
        return;
    }
    {
        int slot = map_info_dynamic_slot_from_id(object_id);
        int type = slot >= 0 && dynamic_objects != NULL
            ? dynamic_objects[slot].type : 0;
        if (slot >= 0 && type == 0) type = map_info_default_dynamic_type(slot);
        if (type >= 1 && type < (int)(sizeof(map_info_dynamic_labels) /
                                      sizeof(map_info_dynamic_labels[0])))
            map_info_draw_text(screen, x, y, map_info_dynamic_labels[type]);
    }
}

static void map_info_draw_object_text(SDL_Surface *screen,
                                      const GameViewModel *view,
                                      const GameMapDynamicObjectState *dynamic_objects,
                                      const MapData *map,
                                      int object_id,
                                      int x,
                                      int base_y)
{
    char name[GAME_MAP_INFO_TEXT_CAPACITY];
    int index;

    if (object_id > 0 && object_id < 2000) {
        if (map != NULL && (size_t)object_id < map->route_count) {
            map_info_copy_name(name, sizeof(name),
                               map->routes[object_id].raw + 4, 20);
            map_info_draw_text(screen, x, base_y + 27, name);
        }
    } else if (object_id > 2000 && object_id < 4000) {
        map_info_draw_land(screen, view, map, object_id - 2001, x, base_y);
    } else if (object_id > 4000 && object_id < 6000) {
        map_info_draw_facility(screen, view, map, object_id - 4001, x, base_y);
    } else if (object_id > 6000 && object_id < 8000) {
        index = object_id - 6001;
        if (map != NULL && view != NULL && index >= 0 &&
            (size_t)(index + 1) < map->auxiliary_count) {
            const MapAuxiliaryRecord *record = &map->auxiliary[index + 1];
            int owner = record->owner_raw != 0 ? record->owner_raw - 1 : -1;
            if (index < view->company_count && view->companies[index].owner >= 0)
                owner = view->companies[index].owner;
            map_info_draw_owner(screen, view, owner, x, base_y);
            map_info_copy_name(name, sizeof(name), record->raw + 4, 20);
            map_info_draw_text(screen, x, base_y + 27, name);
            snprintf(name, sizeof(name),
                     "\xbe\x6c\xaa\xd1:%u", (unsigned int)record->available_shares);
            map_info_draw_text(screen, x, base_y + 54, name);
        }
    } else if (object_id > 8000 && object_id < 10000) {
        index = object_id - 8001;
        if (map != NULL && (size_t)(index + 1) < map->special_count) {
            map_info_copy_name(name, sizeof(name),
                               map->specials[index + 1].raw + 4, 20);
            map_info_draw_text(screen, x, base_y + 27, name);
        }
    } else if ((object_id & 0x8000) != 0) {
        map_info_draw_high_bit(screen, view, dynamic_objects,
                               object_id, x, base_y + 25);
    }
}

void draw_game_map_info(SDL_Surface *screen,
                        const GamePreviewAssets *assets,
                        const NewGameSettings *settings,
                        const GameSceneView *scene)
{
    const GameViewModel *view;
    const MapData *map;
    Rich4SprSmpChunk background;
    int background_index;
    int cursor_x;
    int cursor_y;
    int local_y;
    int above;
    int right;
    int mode;
    int text_x;
    int text_y;
    int width;

    if (screen == NULL || assets == NULL || settings == NULL || scene == NULL ||
        !scene->map_info_active || !scene->use_main_view ||
        !scene->game.active || !assets->map.has_map_control) return;
    background_index = map_info_background_index(settings);
    if (background_index < 0) return;
    view = &scene->game;
    map = &assets->map.maps[background_index];
    cursor_x = scene->map_info_cursor_x;
    cursor_y = scene->map_info_cursor_y;
    local_y = cursor_y - 40;
    if (cursor_x < 0 || cursor_x >= 440 || local_y < 0 || local_y >= 440)
        return;
    if (rich4_spr_smp_get_chunk(
            assets->map.map_control_data, assets->map.map_control_size,
            &assets->map.map_control_spr, 0, &background) != RICH4_SPR_SMP_OK)
        return;
    width = background.width;
    if (width <= 0 || background.height <= 0) return;
    above = local_y >= background.height;
    right = 440 - width < cursor_x;
    mode = (above ? 1 : 0) + (right ? 2 : 0);
    if (rich4_spr_smp_get_chunk(
            assets->map.map_control_data, assets->map.map_control_size,
            &assets->map.map_control_spr, (size_t)mode, &background) !=
        RICH4_SPR_SMP_OK) return;
    if (memcmp(assets->map.map_control_spr.signature, "SPR", 4) == 0)
        draw_spr_chunk_at(screen, assets->map.map_control_data,
                          assets->map.map_control_size,
                          &assets->map.map_control_spr, &background,
                          cursor_x, cursor_y, 1);
    else
        draw_chunk_at(screen, assets->map.map_control_data, &background,
                      cursor_x, cursor_y, 1);
    text_x = right ? cursor_x - (width / 2 + 9) : cursor_x + (width / 2 + 9);
    text_y = cursor_y + (above ? -96 : 42);
    map_info_draw_object_text(screen, view, scene->dynamic_objects, map,
                              scene->map_info_object_id, text_x, text_y);
}
