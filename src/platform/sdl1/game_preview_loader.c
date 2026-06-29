#include <stdio.h>
#include <string.h>

#include "app/new_game_setup_flow.h"
#include "data/resource_catalog.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/runtime_options_types.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"
#include "platform/sdl1/runtime_trace.h"
#include "platform/sdl1/game_lazy_assets.h"

/* Data/Jump/Panel preview bytes are borrowed from the ResourceStore. */

int game_load_scoped_preview_assets(ResourceStore *store,
                                    const SdlOptions *options,
                                    GamePreviewAssets *assets, int scope,
                                    int background, unsigned int characters)
{
    static const char *data_names[] = { "Data.mkf", "DATA.MKF", "data.mkf" };
    static const char *jump_names[] = { "Jump.mkf", "JUMP.MKF", "jump.mkf" };
    static const char *panel_names[] = { "Panel.mkf", "PANEL.MKF", "panel.mkf" };
    char path[512];
    Rich4Mkf *data_mkf;
    Rich4Mkf *jump_mkf;
    Rich4Mkf *panel_mkf;
    size_t i;
    int loaded = 0;
    unsigned int phase_started = runtime_profile_now();

    memset(assets, 0, sizeof(*assets));
    assets->resource_store = store;
    assets->data_dir = options->data_dir;
    assets->load_scope = scope;
    assets->active_background = background;
    assets->active_characters = characters;
    if (!game_lazy_assets_init(assets)) return 0;
    data_mkf = game_open_named_mkf_from_data_dir(store,
                                            options->data_dir,
                                            data_names,
                                            sizeof(data_names) / sizeof(data_names[0]),
                                            path,
                                            sizeof(path));
    assets->data_archive = data_mkf;
    if (data_mkf != NULL) {
        {
            /* sub_433F24 selects Data.MKF[base[map] + event] and decodes it
             * directly into the bank card's 200x200 RGB565 calendar area. */
            static const int calendar_base[GAME_BACKGROUND_COUNT] = {
                4, 28, 47, 67, 87, 108, 95, 118
            };
            int map_index;
            int event_index;
            for (map_index = 0; map_index < GAME_BACKGROUND_COUNT;
                 map_index++) {
                if (scope == 2 && map_index != background) continue;
                for (event_index = 0;
                     event_index < GAME_BANK_CALENDAR_EVENT_COUNT;
                     event_index++) {
                    int resource = calendar_base[map_index] + event_index;
                    if (game_preview_borrow_resource(
                            assets, data_mkf, (size_t)resource,
                            &assets->ui.bank_calendar_data[map_index][event_index],
                            &assets->ui.bank_calendar_size[map_index][event_index],
                            NULL) == RICH4_MKF_OK &&
                        assets->ui.bank_calendar_size[map_index][event_index] ==
                            (size_t)(200 * 200 * 2)) {
                        assets->ui.has_bank_calendar[map_index][event_index] = 1;
                        loaded = 1;
                    } else {
                        game_preview_release_borrowed_view(
                            assets->ui.bank_calendar_data[map_index][event_index]);
                        assets->ui.bank_calendar_data[map_index][event_index] = NULL;
                        assets->ui.bank_calendar_size[map_index][event_index] = 0;
                    }
                }
            }
        }
        for (i = 0; i < GAME_CARD_COUNT; i++) {
            const size_t resource_index = (size_t)(571 + i);
            if (game_preview_borrow_resource(assets, data_mkf, resource_index,
                                        &assets->ui.card_effect_data[i],
                                        &assets->ui.card_effect_size[i],
                                        NULL) == RICH4_MKF_OK &&
                assets->ui.card_effect_size[i] >=
                    (size_t)ORIGINAL_CARD_ART_WIDTH *
                    (size_t)ORIGINAL_CARD_ART_HEIGHT * 2u)
                assets->ui.has_card_effect[i] = 1;
        }
        if (game_preview_borrow_resource(assets, data_mkf,
                                    3,
                                    &assets->menu.options_data,
                                    &assets->menu.options_size,
                                    NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->menu.options_data,
                                    assets->menu.options_size,
                                    &assets->menu.options_spr) &&
            assets->menu.options_spr.num_chunks >= 16) {
            assets->menu.has_options_ui = 1;
            printf("loaded original options UI from %s resource 3\n", path);
        } else {
            game_preview_release_borrowed_view(assets->menu.options_data);
            assets->menu.options_data = NULL;
            assets->menu.options_size = 0;
            if (scope != 1)
                if (!game_lazy_asset_deferred(assets, &assets->menu.options_data))
                    printf("Data.mkf resource 3 options UI not available; using options fallback\n");
        }
        {
            ResourceCatalogEntry confirm_entry;
            if (rich4_resource_catalog_data_confirm(&confirm_entry) &&
                game_preview_borrow_resource(assets, data_mkf,
                                        confirm_entry.resource_index,
                                        &assets->ui.confirm_dialog_data,
                                        &assets->ui.confirm_dialog_size,
                                        NULL) == RICH4_MKF_OK &&
                rich4_spr_smp_probe(assets->ui.confirm_dialog_data,
                                    assets->ui.confirm_dialog_size,
                                    &assets->ui.confirm_dialog_spr) &&
                assets->ui.confirm_dialog_spr.num_chunks >= 3) {
                assets->ui.has_confirm_dialog = 1;
            } else {
                game_preview_release_borrowed_view(assets->ui.confirm_dialog_data);
                assets->ui.confirm_dialog_data = NULL;
                assets->ui.confirm_dialog_size = 0;
            }
        }
        {
            ResourceCatalogEntry dialog_entry;
            if (rich4_resource_catalog_data_common_dialog(&dialog_entry) &&
                game_preview_borrow_resource(assets, data_mkf,
                                        dialog_entry.resource_index,
                                        &assets->ui.common_dialog_data,
                                        &assets->ui.common_dialog_size,
                                        NULL) == RICH4_MKF_OK &&
                rich4_spr_smp_probe(assets->ui.common_dialog_data,
                                    assets->ui.common_dialog_size,
                                    &assets->ui.common_dialog_spr) &&
                assets->ui.common_dialog_spr.num_chunks > 6) {
                assets->ui.has_common_dialog = 1;
            } else {
                game_preview_release_borrowed_view(assets->ui.common_dialog_data);
                assets->ui.common_dialog_data = NULL;
                assets->ui.common_dialog_size = 0;
            }
        }
        /* Original save/list windows at 0x403D74 and 0x404165 read
         * Data.mkf[520] and Data.mkf[2] before calling 0x456418/0x4563F5. */
        if (game_preview_borrow_resource(
                assets, data_mkf, 520,
                &assets->ui.save_load_panel_data,
                &assets->ui.save_load_panel_size, NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->ui.save_load_panel_data,
                assets->ui.save_load_panel_size,
                &assets->ui.save_load_panel_spr) &&
            assets->ui.save_load_panel_spr.num_chunks > 10) {
            assets->ui.has_save_load_panel = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(
                assets->ui.save_load_panel_data);
            assets->ui.save_load_panel_data = NULL;
            assets->ui.save_load_panel_size = 0;
        }
        if (game_preview_borrow_resource(
                assets, data_mkf, 2,
                &assets->ui.save_load_icons_data,
                &assets->ui.save_load_icons_size, NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->ui.save_load_icons_data,
                assets->ui.save_load_icons_size,
                &assets->ui.save_load_icons_spr) &&
            assets->ui.save_load_icons_spr.num_chunks > 0) {
            assets->ui.has_save_load_icons = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(
                assets->ui.save_load_icons_data);
            assets->ui.save_load_icons_data = NULL;
            assets->ui.save_load_icons_size = 0;
        }
        {
            ResourceCatalogEntry icon_entry;
            if (rich4_resource_catalog_data_speech_icons(&icon_entry) &&
                game_preview_borrow_resource(assets, data_mkf, icon_entry.resource_index,
                                        &assets->ui.speech_icon_data,
                                        &assets->ui.speech_icon_size,
                                        NULL) == RICH4_MKF_OK &&
                rich4_spr_smp_probe(assets->ui.speech_icon_data,
                                    assets->ui.speech_icon_size,
                                    &assets->ui.speech_icon_spr)) {
                assets->ui.has_speech_icons = 1;
            } else {
                game_preview_release_borrowed_view(assets->ui.speech_icon_data);
                assets->ui.speech_icon_data = NULL;
                assets->ui.speech_icon_size = 0;
            }
        }
        {
            /* 0x440E1A, not a Panel.MKF resource. */
            if (game_preview_borrow_resource(assets, data_mkf, 518,
                    &assets->ui.player_choice_data, &assets->ui.player_choice_size,
                    NULL) == RICH4_MKF_OK && rich4_spr_smp_probe(
                    assets->ui.player_choice_data, assets->ui.player_choice_size,
                    &assets->ui.player_choice_spr)) assets->ui.has_player_choice = 1;
        }
        {
            ResourceCatalogEntry max_level_entry;
            if (rich4_resource_catalog_data_property_max_level(&max_level_entry) &&
                game_preview_borrow_resource(assets, data_mkf, max_level_entry.resource_index,
                                        &assets->tool.property_max_level_data,
                                        &assets->tool.property_max_level_size,
                                        NULL) == RICH4_MKF_OK &&
                game_graph_frame_count_data(assets->tool.property_max_level_data,
                                            assets->tool.property_max_level_size) > 0) {
                assets->tool.has_property_max_level_graph = 1;
                printf("loaded maximum-level property FLC from Data.mkf resource 523\n");
                loaded = 1;
            } else {
                game_preview_release_borrowed_view(assets->tool.property_max_level_data);
                assets->tool.property_max_level_data = NULL;
                assets->tool.property_max_level_size = 0;
                if (!game_lazy_asset_deferred(assets, &assets->tool.property_max_level_data))
                    printf("Data.mkf resource 523 maximum-level property FLC not available\n");
            }
        }
        {
            static const int tool_effect_resource[
                GAME_TOOL_EFFECT_ASSET_COUNT] = {
                    524, 525, 528, 530, 553, 538, 533, 558, 532, 552
            };
            int effect_index;
            for (effect_index = 0;
                 effect_index < GAME_TOOL_EFFECT_ASSET_COUNT;
                 effect_index++) {
                int resource = tool_effect_resource[effect_index];
                assets->tool.tool_effect_resource[effect_index] = resource;
                if (game_preview_borrow_resource(assets,
                        data_mkf, (size_t)resource,
                        &assets->tool.tool_effect_data[effect_index],
                        &assets->tool.tool_effect_size[effect_index],
                        NULL) == RICH4_MKF_OK &&
                    game_graph_frame_count_data(
                        assets->tool.tool_effect_data[effect_index],
                        assets->tool.tool_effect_size[effect_index]) > 0) {
                    assets->tool.has_tool_effect[effect_index] = 1;
                    loaded = 1;
                } else {
                    game_preview_release_borrowed_view(assets->tool.tool_effect_data[effect_index]);
                    assets->tool.tool_effect_data[effect_index] = NULL;
                    assets->tool.tool_effect_size[effect_index] = 0;
                }
            }
        }
        if (game_preview_borrow_resource(assets, data_mkf, 517,
                                    &assets->ui.worker_choice_data,
                                    &assets->ui.worker_choice_size,
                                    NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->ui.worker_choice_data,
                                assets->ui.worker_choice_size,
                                &assets->ui.worker_choice_spr) &&
            assets->ui.worker_choice_spr.num_chunks >= 18) {
            assets->ui.has_worker_choice_ui = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(assets->ui.worker_choice_data);
            assets->ui.worker_choice_data = NULL;
            assets->ui.worker_choice_size = 0;
        }
        if (game_preview_borrow_resource(assets, data_mkf, 557,
                                    &assets->ui.monster_card_data,
                                    &assets->ui.monster_card_size,
                                    NULL) == RICH4_MKF_OK &&
            game_graph_frame_count_data(assets->ui.monster_card_data,
                                        assets->ui.monster_card_size) > 0) {
            assets->ui.has_monster_card_animation = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(assets->ui.monster_card_data);
            assets->ui.monster_card_data = NULL;
            assets->ui.monster_card_size = 0;
        }
        /* 0x443B0F plays Data.MKF[529] with packed flags 0x260001
         * after a successful demolish-card property action. */
        if (game_preview_borrow_resource(assets, data_mkf, 529,
                                    &assets->ui.demolish_card_data,
                                    &assets->ui.demolish_card_size,
                                    NULL) == RICH4_MKF_OK &&
            game_graph_frame_count_data(assets->ui.demolish_card_data,
                                        assets->ui.demolish_card_size) > 0) {
            assets->ui.has_demolish_card_animation = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(assets->ui.demolish_card_data);
            assets->ui.demolish_card_data = NULL;
            assets->ui.demolish_card_size = 0;
        }
        for (int character = 0; character < 12; ++character)
            for (int kind = 0; kind < 2; ++kind)
                (void)game_preview_borrow_resource(assets, data_mkf,
                    (size_t)(416 + 2 * character + kind),
                    &assets->ui.monthly_award_data[character][kind],
                    &assets->ui.monthly_award_size[character][kind], NULL);
        /* 0x40CD87 loads Data.MKF[555] through the shared graph presenter
         * before the fixed two-second bankruptcy hold. */
        if (game_preview_borrow_resource(assets, data_mkf, 555,
                                    &assets->ui.bankruptcy_data,
                                    &assets->ui.bankruptcy_size,
                                    NULL) == RICH4_MKF_OK &&
            game_graph_frame_count_data(assets->ui.bankruptcy_data,
                                        assets->ui.bankruptcy_size) > 0) {
            assets->ui.has_bankruptcy_animation = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(assets->ui.bankruptcy_data);
            assets->ui.bankruptcy_data = NULL;
            assets->ui.bankruptcy_size = 0;
        }
        /* 0x407842's lpBuffer == 0 confirmation path plays Data.MKF[556]
         * through the same FLC presenter before returning code 4. */
        if (game_preview_borrow_resource(assets, data_mkf, 556,
                                    &assets->ui.ending_success_data,
                                    &assets->ui.ending_success_size,
                                    NULL) == RICH4_MKF_OK &&
            game_graph_frame_count_data(assets->ui.ending_success_data,
                                        assets->ui.ending_success_size) > 0) {
            assets->ui.has_ending_success_animation = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(
                assets->ui.ending_success_data);
            assets->ui.ending_success_data = NULL;
            assets->ui.ending_success_size = 0;
        }
        {
            static const int special_reward_resource[2] = { 536, 537 };
            int reward_index;
            for (reward_index = 0; reward_index < 2; reward_index++) {
                int resource = special_reward_resource[reward_index];
                if (game_preview_borrow_resource(assets,
                        data_mkf, (size_t)resource,
                        &assets->ui.special_reward_data[reward_index],
                        &assets->ui.special_reward_size[reward_index],
                        NULL) == RICH4_MKF_OK &&
                    game_graph_frame_count_data(
                        assets->ui.special_reward_data[reward_index],
                        assets->ui.special_reward_size[reward_index]) > 0) {
                    assets->ui.has_special_reward[reward_index] = 1;
                    loaded = 1;
                } else {
                    game_preview_release_borrowed_view(assets->ui.special_reward_data[reward_index]);
                    assets->ui.special_reward_data[reward_index] = NULL;
                    assets->ui.special_reward_size[reward_index] = 0;
                }
            }
        }
        {
            /* God callbacks 0x40EC14..0x40F2EB use these Data.MKF FLCs.
             * Types 11/13/14 are not god-acquisition callbacks; type 11's
             * separate dog encounter uses Data.MKF[532]/[552]. */
            static const int god_effect_resource[GAME_GOD_TYPE_COUNT + 1] = {
                0, 540, 541, 542, 543, 544, 545, 546, 547, 548,
                549, 0, 550, 0, 0, 551, 0, 0, 0
            };
            int god_type;
            for (god_type = 1; god_type <= GAME_GOD_TYPE_COUNT;
                 god_type++) {
                int resource = god_effect_resource[god_type];
                assets->ui.god_effect_resource[god_type] = resource;
                if (resource != 0 &&
                    game_preview_borrow_resource(
                        assets, data_mkf, (size_t)resource,
                        &assets->ui.god_effect_data[god_type],
                        &assets->ui.god_effect_size[god_type], NULL) ==
                        RICH4_MKF_OK &&
                    game_graph_frame_count_data(
                        assets->ui.god_effect_data[god_type],
                        assets->ui.god_effect_size[god_type]) > 0) {
                    assets->ui.has_god_effect[god_type] = 1;
                    loaded = 1;
                } else if (resource != 0) {
                    game_preview_release_borrowed_view(
                        assets->ui.god_effect_data[god_type]);
                    assets->ui.god_effect_data[god_type] = NULL;
                    assets->ui.god_effect_size[god_type] = 0;
                }
            }
        }
        {
            /* Data[441..525] is the complete contiguous 36-news plus
             * 49-fortune presentation range used by 0x44B6DF/0x44DB81. */
            int event_index;
            for (event_index = 0;
                 event_index < GAME_SPECIAL_EVENT_ASSET_COUNT;
                 event_index++) {
                int resource = 441 + event_index;
                unsigned char *data;
                size_t size;
                int raw_image = 0;
                assets->ui.special_event_resource[event_index] = resource;
                if (game_preview_borrow_resource(assets,
                        data_mkf, (size_t)resource,
                        &assets->ui.special_event_data[event_index],
                        &assets->ui.special_event_size[event_index],
                        NULL) != RICH4_MKF_OK)
                    continue;
                data = assets->ui.special_event_data[event_index];
                size = assets->ui.special_event_size[event_index];
                if (size == (size_t)(388 * 251 * 2)) {
                    raw_image = 1;
                } else if (size >= 4) {
                    uint16_t width16 = 0;
                    uint16_t height16 = 0;
                    if (rich4_byte_reader_le16(data, size, 0, &width16) &&
                        rich4_byte_reader_le16(data, size, 2, &height16)) {
                        int width = (int)width16;
                        int height = (int)height16;
                        raw_image = width > 0 && height > 0 &&
                            size >= 4u + (size_t)width *
                                          (size_t)height * 2u;
                    }
                }
                if (raw_image ||
                    game_graph_frame_count_data(data, size) > 0) {
                    assets->ui.has_special_event[event_index] = 1;
                    loaded = 1;
                } else {
                    game_preview_release_borrowed_view(assets->ui.special_event_data[event_index]);
                    assets->ui.special_event_data[event_index] = NULL;
                    assets->ui.special_event_size[event_index] = 0;
                }
            }
        }
        {
            /* News phase-1 callbacks 0x44913D/0x4492A0/0x44A453/
             * 0x44AB2C/0x44AC99 load these Data.mkf FLC resources through
             * handle 0x48A0E4 and play them over the main map at (0,40). */
            static const int consequence_resource[
                GAME_NEWS_CONSEQUENCE_COUNT] = {
                    531, 539, 527, 534, 535
                };
            int consequence;
            for (consequence = 0;
                 consequence < GAME_NEWS_CONSEQUENCE_COUNT;
                 consequence++) {
                int resource = consequence_resource[consequence];
                assets->ui.news_consequence_resource[consequence] = resource;
                if (game_preview_borrow_resource(assets,
                        data_mkf, (size_t)resource,
                        &assets->ui.news_consequence_data[consequence],
                        &assets->ui.news_consequence_size[consequence],
                        NULL) == RICH4_MKF_OK &&
                    game_graph_frame_count_data(
                        assets->ui.news_consequence_data[consequence],
                        assets->ui.news_consequence_size[consequence]) > 0) {
                    assets->ui.has_news_consequence[consequence] = 1;
                    loaded = 1;
                } else {
                    game_preview_release_borrowed_view(assets->ui.news_consequence_data[consequence]);
                    assets->ui.news_consequence_data[consequence] = NULL;
                    assets->ui.news_consequence_size[consequence] = 0;
                }
            }
        }
        if (game_preview_borrow_resource(assets,
                data_mkf, 526,
                &assets->ui.point_minigame_effect_data,
                &assets->ui.point_minigame_effect_size,
                NULL) == RICH4_MKF_OK &&
            game_graph_frame_count_data(
                assets->ui.point_minigame_effect_data,
                assets->ui.point_minigame_effect_size) > 0) {
            assets->ui.has_point_minigame_effect = 1;
            loaded = 1;
        } else {
            game_preview_release_borrowed_view(assets->ui.point_minigame_effect_data);
            assets->ui.point_minigame_effect_data = NULL;
            assets->ui.point_minigame_effect_size = 0;
        }
    } else {
        printf("Data.mkf not found; options UI will use fallback\n");
    }

    runtime_profile_stage("loader.data", phase_started);
    phase_started = runtime_profile_now();
    jump_mkf = game_open_named_mkf_from_data_dir(store,
                                            options->data_dir,
                                            jump_names,
                                            sizeof(jump_names) / sizeof(jump_names[0]),
                                            path,
                                            sizeof(path));
    assets->jump_archive = jump_mkf;
    if (jump_mkf == NULL) {
        printf("Jump.mkf not found; game preview will use a placeholder background\n");
        return 0;
    }

    for (i = 0; i < GAME_BACKGROUND_COUNT; i++) {
        Rich4MkfError err = game_preview_borrow_resource(assets, jump_mkf,
                                                    i,
                                                    &assets->intro.jump_data[i],
                                                    &assets->intro.jump_size[i],
                                                    NULL);
        if (err == RICH4_MKF_OK &&
            assets->intro.jump_size[i] >= (size_t)(SCREEN_W * SCREEN_H * 2)) {
            assets->intro.has_jump_background[i] = 1;
            loaded++;
        } else {
            game_preview_release_borrowed_view(assets->intro.jump_data[i]);
            assets->intro.jump_data[i] = NULL;
            assets->intro.jump_size[i] = 0;
        }
    }

    if (loaded == 0) {
        printf("Jump.mkf does not expose 640x480 RGB565 resources 0-7; using placeholder\n");
    } else {
        printf("loaded %d game preview background(s) from %s resources 0-7\n", loaded, path);
    }

    {
        size_t jump_count = rich4_mkf_count(jump_mkf);
        int intro_base = GAME_ORIGINAL_JUMP_INTRO_BASE;
        int phase;
        int character;

        assets->intro.jump_intro_resource_base = intro_base;
        if ((size_t)(intro_base + 25) < jump_count &&
            game_preview_borrow_resource(assets, jump_mkf,
                                    (size_t)intro_base,
                                    &assets->intro.jump_intro_scene_data,
                                    &assets->intro.jump_intro_scene_size,
                                    NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->intro.jump_intro_scene_data,
                                    assets->intro.jump_intro_scene_size,
                                    &assets->intro.jump_intro_scene_spr) &&
            assets->intro.jump_intro_scene_spr.num_chunks >= 15) {
            assets->intro.has_jump_intro_scene = 1;
            printf("loaded original jump intro scene from %s resource %d\n",
                   path,
                   intro_base);
        } else {
            game_preview_release_borrowed_view(assets->intro.jump_intro_scene_data);
            assets->intro.jump_intro_scene_data = NULL;
            assets->intro.jump_intro_scene_size = 0;
        }

        if ((size_t)(intro_base + 1) < jump_count &&
            game_preview_borrow_resource(assets, jump_mkf,
                                    (size_t)(intro_base + 1),
                                    &assets->intro.jump_intro_window_data,
                                    &assets->intro.jump_intro_window_size,
                                    NULL) == RICH4_MKF_OK &&
            game_graph_frame_count_data(assets->intro.jump_intro_window_data,
                                        assets->intro.jump_intro_window_size) > 0) {
            assets->intro.has_jump_intro_window_graph = 1;
        } else {
            game_preview_release_borrowed_view(assets->intro.jump_intro_window_data);
            assets->intro.jump_intro_window_data = NULL;
            assets->intro.jump_intro_window_size = 0;
        }

        for (phase = 0; phase < 2; phase++) {
            for (character = 0; character < GAME_JUMP_CHARACTER_COUNT; character++) {
                int resource_index = intro_base + 2 + phase * GAME_JUMP_CHARACTER_COUNT + character;
                if ((size_t)resource_index < jump_count &&
                    game_preview_borrow_resource(assets, jump_mkf,
                                            (size_t)resource_index,
                                            &assets->character.jump_intro_actor_data[phase][character],
                                            &assets->character.jump_intro_actor_size[phase][character],
                                            NULL) == RICH4_MKF_OK &&
                    game_graph_frame_count_data(assets->character.jump_intro_actor_data[phase][character],
                                                assets->character.jump_intro_actor_size[phase][character]) > 0) {
                    assets->character.has_jump_intro_actor_graph[phase][character] = 1;
                } else {
                    game_preview_release_borrowed_view(assets->character.jump_intro_actor_data[phase][character]);
                    assets->character.jump_intro_actor_data[phase][character] = NULL;
                    assets->character.jump_intro_actor_size[phase][character] = 0;
                }
            }
        }
    }

    if (game_preview_borrow_resource(assets, jump_mkf,
                                8,
                                &assets->menu.setup_data,
                                &assets->menu.setup_size,
                                NULL) == RICH4_MKF_OK &&
        rich4_spr_smp_probe(assets->menu.setup_data, assets->menu.setup_size, &assets->menu.setup_spr) &&
        assets->menu.setup_spr.num_chunks >= 22) {
        assets->menu.has_setup_ui = 1;
        printf("loaded original new game UI from %s resource 8\n", path);
    } else {
        game_preview_release_borrowed_view(assets->menu.setup_data);
        assets->menu.setup_data = NULL;
        assets->menu.setup_size = 0;
        if (scope == 0 || scope == 3)
            printf("Jump.mkf resource 8 UI chunks not available; using setup fallback\n");
    }

    {
        int character;
        int traffic;
        int anim_loaded = 0;
        for (character = 0; character < 12; character++) {
            for (traffic = 0; traffic < 3; traffic++) {
                size_t resource_index = (size_t)
                    rich4_setup_character_resource(character, traffic);
                if (game_preview_borrow_resource(assets, jump_mkf,
                                            resource_index,
                                            &assets->character.walk_data[character][traffic],
                                            &assets->character.walk_size[character][traffic],
                                            NULL) == RICH4_MKF_OK &&
                    rich4_spr_smp_probe(assets->character.walk_data[character][traffic],
                                            assets->character.walk_size[character][traffic],
                                            &assets->character.walk_spr[character][traffic]) &&
                    assets->character.walk_spr[character][traffic].num_chunks > 0) {
                    assets->character.has_walk[character][traffic] = 1;
                    anim_loaded++;
                } else {
                    game_preview_release_borrowed_view(assets->character.walk_data[character][traffic]);
                    assets->character.walk_data[character][traffic] = NULL;
                    assets->character.walk_size[character][traffic] = 0;
                }
            }
        }
        printf("loaded %d new game character animation resource(s) from %s resources 9-44\n",
               anim_loaded,
               path);
    }


    runtime_profile_stage("loader.jump", phase_started);
    phase_started = runtime_profile_now();
    panel_mkf = game_open_named_mkf_from_data_dir(store,
                                             options->data_dir,
                                             panel_names,
                                             sizeof(panel_names) / sizeof(panel_names[0]),
                                             path,
                                             sizeof(path));
    assets->panel_archive = panel_mkf;
    if (panel_mkf != NULL) {
        static const int panel_resources[] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
            18, 19, 20, 21, 22, 23, 24, 25,
            26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
            38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
            50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
            62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 75, 76,
            78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
            73, 74, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
            101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
            111, 112
        };
        ResourceCatalogEntry ai_settings_entry;
        int panel_loaded = 0;
        size_t panel_index;
        for (panel_index = 0;
             panel_index < sizeof(panel_resources) / sizeof(panel_resources[0]);
             panel_index++) {
            int resource_index = panel_resources[panel_index];
            if (game_preview_borrow_resource(assets, panel_mkf,
                                        (size_t)resource_index,
                                        &assets->ui.panel_data[resource_index],
                                        &assets->ui.panel_size[resource_index],
                                        NULL) == RICH4_MKF_OK &&
                rich4_spr_smp_probe(assets->ui.panel_data[resource_index],
                                        assets->ui.panel_size[resource_index],
                                        &assets->ui.panel_spr[resource_index]) &&
                assets->ui.panel_spr[resource_index].num_chunks > 0) {
                assets->ui.has_panel[resource_index] = 1;
                panel_loaded++;
            } else if (((resource_index >= 4 && resource_index <= 6) ||
                        resource_index == 14 || resource_index == 16 ||
                        resource_index == 17 || resource_index == 20 ||
                        resource_index == 78) &&
                       assets->ui.panel_data[resource_index] != NULL &&
                       game_graph_frame_count_data(
                           assets->ui.panel_data[resource_index],
                           assets->ui.panel_size[resource_index]) > 0) {
                assets->ui.has_panel_graph[resource_index] = 1;
                panel_loaded++;
            } else if ((resource_index == 19 || resource_index == 22 ||
                        resource_index == 81 || resource_index == 92) &&
                       assets->ui.panel_data[resource_index] != NULL &&
                       assets->ui.panel_size[resource_index] >=
                           (resource_index == 22
                                ? (size_t)(128 * 192)
                            : resource_index == 19 || resource_index == 81
                                ? (size_t)(SCREEN_W * SCREEN_H)
                                : (size_t)(SCREEN_W * SCREEN_H * 2))) {
                assets->ui.has_panel_raw[resource_index] = 1;
                panel_loaded++;
            } else if (resource_index == 8 &&
                       assets->ui.panel_data[resource_index] != NULL &&
                       assets->ui.panel_size[resource_index] >=
                           (size_t)(GAME_DICE_STATUS_W * GAME_DICE_STATUS_H)) {
                assets->ui.has_panel_mask[resource_index] = 1;
                panel_loaded++;
            } else {
                game_preview_release_borrowed_view(assets->ui.panel_data[resource_index]);
                assets->ui.panel_data[resource_index] = NULL;
                assets->ui.panel_size[resource_index] = 0;
            }
        }
        /* 0x41E345 reads Panel.mkf[77].  Its 18 SMP chunks are the panel,
         * row states, markers, arrows, and all twelve character portraits. */
        if (rich4_resource_catalog_panel_ai_settings(&ai_settings_entry) &&
            game_preview_borrow_resource(assets, panel_mkf,
                                    ai_settings_entry.resource_index,
                                    &assets->menu.ai_settings_data,
                                    &assets->menu.ai_settings_size,
                                    NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->menu.ai_settings_data,
                                assets->menu.ai_settings_size,
                                &assets->menu.ai_settings_spr) &&
            memcmp(assets->menu.ai_settings_spr.signature, "SMP", 4) == 0 &&
            assets->menu.ai_settings_spr.num_chunks >= 18) {
            assets->menu.has_ai_settings_ui = 1;
            panel_loaded++;
        } else {
            game_preview_release_borrowed_view(assets->menu.ai_settings_data);
            assets->menu.ai_settings_data = NULL;
            assets->menu.ai_settings_size = 0;
        }
        /* sub_4470F8 reads the dedicated six-face selector from
         * Panel.mkf[72]. */
        if (game_preview_borrow_resource(assets, panel_mkf, 72,
                                    &assets->ui.remote_dice_data,
                                    &assets->ui.remote_dice_size,
                                    NULL) == RICH4_MKF_OK &&
            rich4_spr_smp_probe(assets->ui.remote_dice_data,
                                assets->ui.remote_dice_size,
                                &assets->ui.remote_dice_spr) &&
            assets->ui.remote_dice_spr.num_chunks >= 7) {
            assets->ui.has_remote_dice_ui = 1;
            panel_loaded++;
        } else {
            game_preview_release_borrowed_view(assets->ui.remote_dice_data);
            assets->ui.remote_dice_data = NULL;
            assets->ui.remote_dice_size = 0;
        }
        printf("loaded %d game panel resource(s) from %s\n", panel_loaded, path);
    } else {
        printf("Panel.mkf not found; game HUD will use debug panels\n");
    }

    runtime_profile_stage("loader.panel", phase_started);
    return loaded > 0 || assets->menu.has_setup_ui;
}
