#include <stdio.h>
#include <string.h>

#include "core/game_constants.h"
#include "data/resource_catalog.h"
#include "data/map_data.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/runtime_options_types.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"

/* Map preview bytes are borrowed from GamePreviewAssets' ResourceStore. */

static int append_game_map_object(GamePreviewAssets *assets,
                                  int background_index,
                                  int world_x,
                                  int world_y,
                                  int sprite,
                                  int object_id,
                                  int owner,
                                  int variant_base);

/* R10.1d: map sprite-index queries moved to
 * platform/sdl1/render/game_map_render.c. */

static int build_map_preview_objects_from_data(GamePreviewAssets *assets,
                                               int background_index,
                                               const MapData *map)
{
    size_t i;
    if (assets == NULL || map == NULL || background_index < 0 ||
        background_index >= GAME_BACKGROUND_COUNT || map->route_count <= 1) return 0;
    for (i = 1; i < map->land_count; i++) {
        const MapLandRecord *record = &map->lands[i];
        if (record->level > 0) {
            int sprite = record->type == 0
                ? game_map_view_sprite_for_level(background_index, record->level)
                : game_map_overlay_sprite(background_index);
            if (sprite > 0 && sprite < GAME_MAX_MAP_OBJECT_SPRITES) {
                append_game_map_object(assets, background_index, record->x, record->y,
                                       sprite, 0x7d0 + (int)i,
                                       record->owner_raw, record->variant);
            }
        } else if (record->owner_raw != 0) {
            int object_index = assets->map.map_object_count[background_index];
            if (append_game_map_object(assets, background_index, record->x, record->y,
                                       GAME_MAP_OBJECT_SPRITE_COMMON_25,
                                       0x7d0 + (int)i, 0xff, 0)) {
                assets->map.map_object_variant_owner[background_index][object_index] = record->owner_raw;
            }
        }
    }
    for (i = 1; i < map->facility_count; i++) {
        const MapFacilityRecord *record = &map->facilities[i];
        if (record->level > 0) {
            int sprite = game_map_facility_sprite(background_index, record->type, record->level);
            if (sprite > 0 && sprite < GAME_MAX_MAP_OBJECT_SPRITES) {
                append_game_map_object(assets, background_index, record->x, record->y,
                                       sprite, 0xfa0 + (int)i,
                                       record->owner_raw, record->variant);
            }
        } else if (record->owner_raw != 0) {
            int object_index = assets->map.map_object_count[background_index];
            if (append_game_map_object(assets, background_index, record->x, record->y,
                                       GAME_MAP_OBJECT_SPRITE_COMMON_25,
                                       0xfa0 + (int)i, 0xff, 0)) {
                assets->map.map_object_variant_owner[background_index][object_index] = record->owner_raw;
            }
        }
    }
    for (i = 1; i < map->auxiliary_count; i++) {
        const MapAuxiliaryRecord *record = &map->auxiliary[i];
        if (record->sprite_resource > 0 && record->sprite_resource < GAME_MAX_MAP_OBJECT_SPRITES) {
            append_game_map_object(assets, background_index, record->x, record->y,
                                   record->sprite_resource, 0x1770 + (int)i,
                                   record->owner_raw, record->variant);
        }
    }
    for (i = 1; i < map->special_count; i++) {
        const MapSpecialRecord *record = &map->specials[i];
        if (record->sprite_resource > 0 && record->sprite_resource < GAME_MAX_MAP_OBJECT_SPRITES) {
            append_game_map_object(assets, background_index, record->x, record->y,
                                   record->sprite_resource, 0x1f40 + (int)i,
                                   0xff, record->variant);
        }
    }
    return 1;
}


static int append_game_map_object(GamePreviewAssets *assets,
                                  int background_index,
                                  int world_x,
                                  int world_y,
                                  int sprite,
                                  int object_id,
                                  int owner,
                                  int variant_base)
{
    int object_index;

    if (assets == NULL ||
        background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        (sprite != GAME_MAP_OBJECT_SPRITE_COMMON_25 &&
         (sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES)) ||
        assets->map.map_object_count[background_index] >= GAME_MAX_MAP_OBJECTS) {
        return 0;
    }
    object_index = assets->map.map_object_count[background_index]++;
    assets->map.map_object_x[background_index][object_index] = world_x;
    assets->map.map_object_y[background_index][object_index] = world_y;
    assets->map.map_object_sprite[background_index][object_index] = sprite;
    assets->map.map_object_chunk[background_index][object_index] = 0;
    assets->map.map_object_id[background_index][object_index] = object_id;
    assets->map.map_object_owner[background_index][object_index] = owner & 0xff;
    assets->map.map_object_variant_base[background_index][object_index] = variant_base & 7;
    assets->map.map_object_variant_owner[background_index][object_index] = 0;
    return 1;
}

static void load_game_map_object_sprite(Rich4Mkf *map_mkf,
                                        GamePreviewAssets *assets,
                                        int sprite)
{
    unsigned char *data = NULL;
    size_t size = 0;

    if (map_mkf == NULL || assets == NULL ||
        sprite == GAME_MAP_OBJECT_SPRITE_COMMON_25 ||
        sprite <= 0 || sprite >= GAME_MAX_MAP_OBJECT_SPRITES ||
        assets->map.has_map_object_sprite[sprite]) {
        return;
    }
    if (game_preview_borrow_resource(assets, map_mkf,
                                (size_t)(sprite + 38),
                                &data,
                                &size,
                                NULL) == RICH4_MKF_OK)
        assets->map.map_object_resource_present[sprite] = data != NULL;
    if (data != NULL &&
        rich4_spr_smp_probe(data, size, &assets->map.map_object_spr[sprite]) &&
        assets->map.map_object_spr[sprite].num_chunks > 0) {
        assets->map.map_object_data[sprite] = data;
        assets->map.map_object_size[sprite] = size;
        assets->map.map_object_resource[sprite] = game_preview_handle_for_borrowed_view(assets, data);
        assets->map.has_map_object_sprite[sprite] = 1;
    } else {
        game_preview_release_borrowed_view(data);
    }
}

static void register_game_map_object_sprite_alias(GamePreviewAssets *assets,
                                                  int sprite,
                                                  unsigned char *data,
                                                  size_t size)
{
    Rich4SprSmpInfo info;
    if (assets == NULL || sprite <= 0 ||
        sprite >= GAME_MAX_MAP_OBJECT_SPRITES) {
        return;
    }
    if (data != NULL)
        assets->map.map_object_resource_present[sprite] = 1;
    if (data == NULL || assets->map.has_map_object_sprite[sprite] ||
        !rich4_spr_smp_probe(data, size, &info) || info.num_chunks == 0)
        return;
    assets->map.map_object_data[sprite] = data;
    assets->map.map_object_size[sprite] = size;
    assets->map.map_object_resource[sprite] = game_preview_handle_for_borrowed_view(assets, data);
    assets->map.map_object_spr[sprite] = info;
    assets->map.has_map_object_sprite[sprite] = 1;
}

int game_load_game_map_routes(ResourceStore *store,
                                const SdlOptions *options,
                                GamePreviewAssets *assets)
{
    static const char *map_names[] = { "Map.mkf", "MAP.MKF", "map.mkf" };
    static const char *mapdat_names[] = { "Mapdat.mkf", "MAPDAT.MKF", "mapdat.mkf" };
    char path[512];
    char mapdat_path[512];
    Rich4Mkf *map_mkf;
    Rich4Mkf *mapdat_mkf;
    int loaded = 0;
    int common_loaded = 0;
    int view_loaded = 0;
    int overlay_loaded = 0;
    int stage_tile_loaded = 0;
    int i;

    if (options == NULL || assets == NULL) {
        return 0;
    }

    map_mkf = game_open_named_mkf_from_data_dir(store,
                                           options->data_dir,
                                           map_names,
                                           sizeof(map_names) / sizeof(map_names[0]),
                                           path,
                                           sizeof(path));
    if (map_mkf == NULL) {
        fprintf(stderr, "Map.mkf not found; game preview route data unavailable\n");
        return 0;
    }
    assets->map_archive = map_mkf;
    mapdat_mkf = game_open_named_mkf_from_data_dir(store,
                                              options->data_dir,
                                              mapdat_names,
                                              sizeof(mapdat_names) / sizeof(mapdat_names[0]),
                                              mapdat_path,
                                              sizeof(mapdat_path));
    if (mapdat_mkf != NULL) {
        printf("Mapdat.mkf found; map-data tables will prefer %s\n", mapdat_path);
    }

    {
        int common_index;
        for (common_index = 0;
             common_index < GAME_MAP_COMMON_RESOURCE_COUNT;
             common_index++) {
            ResourceCatalogEntry catalog;
            if (rich4_resource_catalog_map_common(common_index, &catalog) &&
                game_load_raw_mkf_resource(assets,
                                      map_mkf,
                                      catalog.resource_index,
                                      &assets->map.map_common_data[common_index],
                                      &assets->map.map_common_size[common_index])) {
                assets->map.has_map_common[common_index] = 1;
                assets->map.map_common_resource[common_index] =
                    game_preview_handle_for_borrowed_view(assets, assets->map.map_common_data[common_index]);
                if (rich4_spr_smp_probe(assets->map.map_common_data[common_index],
                                            assets->map.map_common_size[common_index],
                                            &assets->map.map_common_spr[common_index]) &&
                    assets->map.map_common_spr[common_index].num_chunks > 0) {
                    assets->map.has_map_common_graph[common_index] = 1;
                }
                common_loaded++;
            }
        }
    }

    for (i = 0; i < GAME_BACKGROUND_COUNT; i++) {
        int ground_resource_index = i * 2;
        int overview_resource_index = i + 16;
        ResourceCatalogEntry overlay_resource;
        int j;
        unsigned char *data = NULL;
        size_t size = 0;
        Rich4MkfError err;
        uint16_t ground_width = 0;
        uint16_t ground_height = 0;

        if (game_preview_borrow_resource(assets, map_mkf,
                                    (size_t)ground_resource_index,
                                    &assets->map.map_ground_data[i],
                                    &assets->map.map_ground_size[i],
                                    NULL) == RICH4_MKF_OK &&
            assets->map.map_ground_size[i] >= 0x2a90 + 1024u &&
            memcmp(assets->map.map_ground_data[i], "GND", 3) == 0 &&
            rich4_byte_reader_le16(assets->map.map_ground_data[i],
                                   assets->map.map_ground_size[i], 4,
                                   &ground_width) &&
            rich4_byte_reader_le16(assets->map.map_ground_data[i],
                                   assets->map.map_ground_size[i], 6,
                                   &ground_height) &&
            ground_width == 72 && ground_height == 72) {
            assets->map.has_map_ground[i] = 1;
            assets->map.map_ground_resource[i] =
                game_preview_handle_for_borrowed_view(assets, assets->map.map_ground_data[i]);
        } else {
            game_preview_release_borrowed_view(assets->map.map_ground_data[i]);
            assets->map.map_ground_data[i] = NULL;
            assets->map.map_ground_size[i] = 0;
        }

        {
            MapDataSelection selection = rich4_map_data_select(i / GAME_MAPS_PER_STAGE,
                                                               i % GAME_MAPS_PER_STAGE,
                                                               mapdat_mkf != NULL);
            Rich4Mkf *selected_mkf = selection.archive == MAP_DATA_ARCHIVE_MAPDAT
                ? mapdat_mkf
                : map_mkf;
            err = game_preview_borrow_resource(assets, selected_mkf,
                                          selection.resource_index,
                                          &data,
                                          &size,
                                          NULL);
            if (err == RICH4_MKF_OK &&
                rich4_map_data_parse(data,
                                     size,
                                     selection.archive,
                                     selection.resource_index,
                                     &assets->map.maps[i]) == MAP_DATA_OK &&
                ((assets->load_scope != 0 &&
                  (assets->load_scope != 2 || i != assets->active_background)) ||
                 build_map_preview_objects_from_data(assets, i, &assets->map.maps[i]))) {
                int object_index;
                loaded++;
                for (object_index = 0;
                     object_index < assets->map.map_object_count[i];
                     object_index++) {
                    load_game_map_object_sprite(map_mkf,
                                                assets,
                                                assets->map.map_object_sprite[i][object_index]);
                }
                for (object_index = 16;
                     (assets->load_scope == 0 ||
                      (assets->load_scope == 2 && i == assets->active_background)) && object_index <= 18;
                     object_index++)
                    load_game_map_object_sprite(
                        map_mkf, assets, object_index);
            } else {
                rich4_map_data_free(&assets->map.maps[i]);
            }
            game_preview_release_borrowed_view(data);
        }

        if ((assets->load_scope != 0 && assets->load_scope != 2) ||
            (assets->load_scope == 2 && i != assets->active_background)) continue;
        if (game_preview_borrow_resource(assets, map_mkf,
                                    (size_t)overview_resource_index,
                                    &assets->map.map_overview_data[i],
                                    &assets->map.map_overview_size[i],
                                    NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->map.map_overview_data[i],
                                    assets->map.map_overview_size[i],
                                    &assets->map.map_overview_spr[i]) &&
            assets->map.map_overview_spr[i].num_chunks >= 1) {
            assets->map.has_map_overview[i] = 1;
            assets->map.map_overview_resource[i] =
                game_preview_handle_for_borrowed_view(assets, assets->map.map_overview_data[i]);
        } else {
            game_preview_release_borrowed_view(assets->map.map_overview_data[i]);
            assets->map.map_overview_data[i] = NULL;
            assets->map.map_overview_size[i] = 0;
        }

        if (rich4_resource_catalog_map_overlay(i, &overlay_resource) &&
            game_load_raw_mkf_resource(assets,
                                  map_mkf,
                                  overlay_resource.resource_index,
                                  &assets->map.map_overlay_data[i],
                                  &assets->map.map_overlay_size[i])) {
            assets->map.has_map_overlay[i] = 1;
            register_game_map_object_sprite_alias(
                assets, game_map_overlay_sprite(i),
                assets->map.map_overlay_data[i], assets->map.map_overlay_size[i]);
            overlay_loaded++;
        }

        for (j = 0; j < GAME_MAP_VIEW_RESOURCE_COUNT; j++) {
            ResourceCatalogEntry view_resource;
            if (rich4_resource_catalog_map_view(i, j, &view_resource) &&
                game_load_raw_mkf_resource(assets,
                                      map_mkf,
                                      view_resource.resource_index,
                                      &assets->map.map_view_data[i][j],
                                      &assets->map.map_view_size[i][j])) {
                assets->map.has_map_view[i][j] = 1;
                register_game_map_object_sprite_alias(
                    assets, game_map_view_sprite_for_level(i, j + 1),
                    assets->map.map_view_data[i][j],
                    assets->map.map_view_size[i][j]);
                view_loaded++;
            }
        }

        for (j = 0; j < GAME_MAP_STAGE_TILE_RESOURCE_COUNT; j++) {
            int stage = i / GAME_MAPS_PER_STAGE;
            int map = i % GAME_MAPS_PER_STAGE;
            ResourceCatalogEntry tile_resource;
            if (rich4_resource_catalog_map_stage_tile(stage, map, j, &tile_resource) &&
                game_load_raw_mkf_resource(assets,
                                      map_mkf,
                                      tile_resource.resource_index,
                                      &assets->map.map_stage_tile_data[i][j],
                                      &assets->map.map_stage_tile_size[i][j])) {
                assets->map.has_map_stage_tile[i][j] = 1;
                register_game_map_object_sprite_alias(
                    assets, game_map_stage_tile_sprite(i, j),
                    assets->map.map_stage_tile_data[i][j],
                    assets->map.map_stage_tile_size[i][j]);
                stage_tile_loaded++;
            }
        }
    }

    for (i = 0; i < 12; i++) {
        ResourceCatalogEntry character_resource;
        if (rich4_resource_catalog_map_character(i, &character_resource) &&
            game_preview_borrow_resource(assets, map_mkf,
                                    character_resource.resource_index,
                                    &assets->character.map_character_data[i],
                                    &assets->character.map_character_size[i],
                                    NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->character.map_character_data[i],
                                    assets->character.map_character_size[i],
                                    &assets->character.map_character_spr[i]) &&
            assets->character.map_character_spr[i].num_chunks > 5) {
            assets->character.has_map_character[i] = 1;
            assets->character.map_character_resource[i] =
                game_preview_handle_for_borrowed_view(assets, assets->character.map_character_data[i]);
        } else {
            game_preview_release_borrowed_view(assets->character.map_character_data[i]);
            assets->character.map_character_data[i] = NULL;
            assets->character.map_character_size[i] = 0;
        }
    }

    if (loaded > 0) {
        int overview_loaded = 0;
        int ground_loaded = 0;
        int character_loaded = 0;
        int object_sprite_loaded = 0;
        int object_loaded = 0;
        for (i = 0; i < GAME_BACKGROUND_COUNT; i++) {
            if (assets->map.has_map_overview[i]) {
                overview_loaded++;
            }
            if (assets->map.has_map_ground[i]) {
                ground_loaded++;
            }
            object_loaded += assets->map.map_object_count[i];
        }
        for (i = 0; i < 12; i++) {
            if (assets->character.has_map_character[i]) {
                character_loaded++;
            }
        }
        for (i = 0; i < GAME_MAX_MAP_OBJECT_SPRITES; i++) {
            if (assets->map.has_map_object_sprite[i]) {
                object_sprite_loaded++;
            }
        }
        printf("loaded %d map route(s), %d ground map(s), %d overview map(s), %d map object(s), %d object sprite(s), and %d map character set(s) from %s\n",
               loaded,
               ground_loaded,
               overview_loaded,
               object_loaded,
               object_sprite_loaded,
               character_loaded,
               path);
        printf("loaded original map lifetime resources: %d common, %d view, %d overlay, %d stage-tile resource(s)\n",
               common_loaded,
               view_loaded,
               overlay_loaded,
               stage_tile_loaded);
    } else {
        fprintf(stderr, "Map.mkf route resources not recognized; game preview route data unavailable\n");
    }

    return loaded;
}
