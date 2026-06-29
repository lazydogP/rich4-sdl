#include <stdio.h>

#include "core/game_constants.h"
#include "data/resource_catalog.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/spr_smp.h"
#include "platform/sdl1/game_lazy_assets.h"

/* Resource bytes are borrowed from the ResourceStore.  The loader names the
 * borrow operation explicitly so ownership cannot be changed by a macro. */

/* Map actors are drawn from eight directional chunks.  The renderer derives
 * frames_per_direction as num_chunks / 8, so accepting a partial final
 * direction would silently select the wrong resource for some headings. */
static int game_probe_directional_model(const unsigned char *data,
                                        size_t size,
                                        Rich4SprSmpInfo *out_info)
{
    Rich4SprSmpInfo info;

    if (!rich4_spr_smp_probe(data, size, &info) ||
        info.num_chunks < 8 || (info.num_chunks & 7u) != 0)
        return 0;
    if (out_info != NULL) *out_info = info;
    return 1;
}

/* Data.mkf[396..415] is not one homogeneous directional family.  The first
 * eighteen entries are the eight directional map objects; the last two are
 * the six-frame countdown object and the single-frame special marker.  The
 * renderer has separate geometry for those two entries, so accepting an
 * arbitrary non-empty SPR/SMP here would hide a broken resource mapping until
 * the first frame is drawn. */
static int game_probe_dynamic_model(const unsigned char *data,
                                    size_t size,
                                    int status_index,
                                    Rich4SprSmpInfo *out_info)
{
    static const unsigned int expected_chunks[GAME_DATA_MAP_STATUS_RESOURCE_COUNT] = {
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 6, 1
    };
    Rich4SprSmpInfo info;

    if (status_index < 0 ||
        status_index >= GAME_DATA_MAP_STATUS_RESOURCE_COUNT ||
        !rich4_spr_smp_probe(data, size, &info) ||
        info.num_chunks != expected_chunks[status_index])
        return 0;
    if (out_info != NULL) *out_info = info;
    return 1;
}

static int load_character_portraits(Rich4Mkf *data_mkf, GamePreviewAssets *assets)
{
    int loaded = 0;
    if (game_preview_borrow_resource(assets, data_mkf,
                                2,
                                &assets->character.character_data,
                                &assets->character.character_size,
                                NULL) == RICH4_MKF_OK &&
        rich4_spr_smp_probe(assets->character.character_data,
                                assets->character.character_size,
                                &assets->character.character_spr) &&
        assets->character.character_spr.num_chunks >= 12) {
        assets->character.has_character_ui = 1;
        printf("loaded original character select portraits from Data.mkf resource 2\n");
        loaded = 1;
    } else {
        game_preview_release_borrowed_view(assets->character.character_data);
        assets->character.character_data = NULL;
        assets->character.character_size = 0;
        printf("Data.mkf resource 2 character portraits not available; using character fallback\n");
    }

    return loaded;
}

int game_load_new_game_character_assets(Rich4Mkf *data_mkf,
                                        GamePreviewAssets *assets)
{
    int loaded = 0;

    if (data_mkf == NULL || assets == NULL) {
        return 0;
    }
    if (assets->load_scope != 0 && assets->load_scope != 2)
        return load_character_portraits(data_mkf, assets);

    if (game_preview_borrow_resource(assets, data_mkf,
                                0x205,
                                &assets->map.map_control_data,
                                &assets->map.map_control_size,
                                NULL) == RICH4_MKF_OK &&
        rich4_spr_smp_probe(assets->map.map_control_data,
                                assets->map.map_control_size,
                                &assets->map.map_control_spr) &&
        assets->map.map_control_spr.num_chunks > 21) {
        assets->map.has_map_control = 1;
        loaded = 1;
        printf("loaded game map control sprites from Data.mkf resource 0x205\n");
    } else {
        game_preview_release_borrowed_view(assets->map.map_control_data);
        assets->map.map_control_data = NULL;
        assets->map.map_control_size = 0;
        printf("Data.mkf resource 0x205 map control sprites not available; using debug map controls\n");
    }

    if (game_load_raw_mkf_resource(assets,
                                   data_mkf,
                                   0x207,
                                   &assets->map.map_control_extra_data,
                                   &assets->map.map_control_extra_size)) {
        assets->map.has_map_control_extra = 1;
        loaded = 1;
        printf("loaded original map auxiliary resource from Data.mkf resource 0x207\n");
    } else {
        printf("Data.mkf resource 0x207 map auxiliary resource not available\n");
    }

    {
        int status_index;
        int status_loaded = 0;
        for (status_index = 0;
             status_index < GAME_DATA_MAP_STATUS_RESOURCE_COUNT;
             status_index++) {
            ResourceCatalogEntry resource;
            int resource_present = 0;
            int decoded = 0;
            if (rich4_resource_catalog_data_dynamic_object(
                    status_index + 1, &resource) &&
                game_load_raw_mkf_resource(
                    assets, data_mkf, resource.resource_index,
                    &assets->map.data_map_status_data[status_index],
                    &assets->map.data_map_status_size[status_index])) {
                resource_present =
                    assets->map.data_map_status_data[status_index] != NULL;
                if (resource_present)
                    decoded = game_probe_dynamic_model(
                        assets->map.data_map_status_data[status_index],
                        assets->map.data_map_status_size[status_index],
                        status_index,
                        &assets->map.data_map_status_spr[status_index]);
            }
            assets->map.data_map_status_resource_present[status_index] =
                resource_present;
            if (decoded) {
                assets->map.has_data_map_status[status_index] = 1;
                status_loaded++;
            } else {
                game_preview_release_borrowed_view(assets->map.data_map_status_data[status_index]);
                assets->map.data_map_status_data[status_index] = NULL;
                assets->map.data_map_status_size[status_index] = 0;
            }
        }
        if (status_loaded > 0) {
            printf("loaded %d original map status resource(s) from Data.mkf resources 396-415\n",
                   status_loaded);
            loaded = 1;
        } else {
            printf("Data.mkf map status resources 396-415 not available\n");
        }
    }

    if (load_character_portraits(data_mkf, assets)) loaded = 1;

    {
        int character;
        int landing_loaded = 0;
        for (character = 0; character < GAME_JUMP_CHARACTER_COUNT; character++) {
            size_t resource_index = (size_t)(0x22f + character);
            if (game_preview_borrow_resource(assets, data_mkf,
                                        resource_index,
                                        &assets->character.first_landing_data[character],
                                        &assets->character.first_landing_size[character],
                                        NULL) == RICH4_MKF_OK &&
                game_graph_frame_count_data(assets->character.first_landing_data[character],
                                            assets->character.first_landing_size[character]) > 0) {
                assets->character.has_first_landing_graph[character] = 1;
                landing_loaded++;
            } else {
                game_preview_release_borrowed_view(assets->character.first_landing_data[character]);
                assets->character.first_landing_data[character] = NULL;
                assets->character.first_landing_size[character] = 0;
            }
        }
        if (landing_loaded > 0) {
            printf("loaded %d first-turn landing FLC resource(s) from Data.mkf resources 559-570\n",
                   landing_loaded);
            loaded = 1;
        } else {
            if (assets->load_scope != 2)
                printf("Data.mkf first-turn landing resources 559-570 not available; using map-player fallback\n");
        }
    }

    {
        int character;
        int part;
        int model_loaded = 0;
        for (character = 0; character < 12; character++) {
            for (part = 0; part < GAME_CHARACTER_MODEL_RESOURCES; part++) {
                ResourceCatalogEntry resource;
                int resource_present = 0;
                assets->character.map_player_resource_present[character][part] = 0;
                if (rich4_resource_catalog_data_player_model(
                        character, part, &resource) &&
                    game_preview_borrow_resource(
                        assets, data_mkf, resource.resource_index,
                        &assets->character.map_player_data[character][part],
                        &assets->character.map_player_size[character][part],
                        NULL) == RICH4_MKF_OK) {
                    resource_present =
                        assets->character.map_player_data[character][part] != NULL;
                }
                assets->character.map_player_resource_present[character][part] =
                    resource_present;
                if (resource_present && game_probe_directional_model(
                        assets->character.map_player_data[character][part],
                        assets->character.map_player_size[character][part],
                        &assets->character.map_player_spr[character][part])) {
                    assets->character.has_map_player[character][part] = 1;
                    model_loaded++;
                } else {
                    game_preview_release_borrowed_view(assets->character.map_player_data[character][part]);
                    assets->character.map_player_data[character][part] = NULL;
                    assets->character.map_player_size[character][part] = 0;
                }
            }
        }
        if (model_loaded > 0) {
            printf("loaded %d original map player model resource(s) from Data.mkf resources 128-379\n",
                   model_loaded);
            loaded = 1;
        } else {
            if (assets->load_scope != 2)
                printf("Data.mkf map player model resources 128-379 not available; using map head fallback\n");
        }
    }
    {
        int actor;
        int part;
        int model_loaded = 0;
        for (actor = 0; actor < GAME_SPECIAL_ACTOR_MODEL_COUNT; actor++) {
            for (part = 0; part < GAME_SPECIAL_ACTOR_MODEL_RESOURCES; part++) {
                ResourceCatalogEntry resource;
                int resource_present = 0;
                int decoded = 0;
                if (rich4_resource_catalog_data_special_actor(
                        actor, part, &resource) &&
                    game_preview_borrow_resource(
                        assets, data_mkf, resource.resource_index,
                        &assets->character.map_special_actor_data[actor][part],
                        &assets->character.map_special_actor_size[actor][part],
                        NULL) == RICH4_MKF_OK) {
                    resource_present = assets->character
                        .map_special_actor_data[actor][part] != NULL;
                    if (resource_present)
                        decoded = game_probe_directional_model(
                            assets->character
                                .map_special_actor_data[actor][part],
                            assets->character
                                .map_special_actor_size[actor][part],
                            &assets->character
                                .map_special_actor_spr[actor][part]);
                }
                assets->character
                    .map_special_actor_resource_present[actor][part] =
                    resource_present;
                if (decoded) {
                    assets->character.has_map_special_actor[actor][part] = 1;
                    model_loaded++;
                } else {
                    game_preview_release_borrowed_view(assets->character.map_special_actor_data[actor][part]);
                    assets->character.map_special_actor_data[actor][part] = NULL;
                    assets->character.map_special_actor_size[actor][part] = 0;
                }
            }
        }
        if (model_loaded > 0) {
            printf("loaded %d original special actor model resource(s) from Data.mkf resources 380-395\n",
                   model_loaded);
            loaded = 1;
        } else {
            if (assets->load_scope != 2)
                printf("Data.mkf special actor model resources 380-395 not available\n");
        }
    }
    {
        int part, robot_loaded = 0;
        for (part = 0; part < 2; part++) {
            size_t resource_index = (size_t)(521 + part);
            unsigned char *robot_data = NULL;
            size_t robot_size = 0;
            if (game_preview_borrow_resource(assets,
                    data_mkf, resource_index,
                    &robot_data, &robot_size, NULL) ==
                    RICH4_MKF_OK &&
                rich4_spr_smp_probe(
                    robot_data, robot_size,
                    &assets->character.tool_robot_spr[part]) &&
                assets->character.tool_robot_spr[part].num_chunks > 0) {
                assets->character.tool_robot[part].data = robot_data;
                assets->character.tool_robot[part].size = robot_size;
                assets->character.has_tool_robot[part] = 1;
                assets->character.tool_robot[part].handle =
                    game_preview_handle_for_borrowed_view(
                        assets, assets->character.tool_robot[part].data);
                assets->character.tool_robot[part].ownership = RICH4_ASSET_BORROWED;
                robot_loaded++;
            } else {
                game_preview_release_borrowed_view(robot_data);
                assets->character.tool_robot[part].data = NULL;
                assets->character.tool_robot[part].size = 0;
                assets->character.tool_robot[part].ownership = RICH4_ASSET_BORROWED;
            }
        }
        if (robot_loaded > 0) {
            printf("loaded %d machine-doll actor resource(s) from Data.mkf resources 521-522\n",
                   robot_loaded);
            loaded = 1;
        }
    }
    game_lazy_assets_finish(assets);
    return loaded;
}
