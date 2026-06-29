/* Test scene ownership against the actual archive bytes. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif
#include <SDL/SDL.h>
#ifdef _WIN32
#undef main
#endif
#include "platform/sdl1/game_asset_api.h"
#include "app/new_game_setup_flow.h"
#include "app/magic_house_flow.h"
#include "app/point_minigame_flow.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_input_hover.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/game_point_minigame_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_menu_input.h"

static unsigned int demand_reads, demand_hits;
static void demand_observer(void *ctx, int phase, const char *archive,
                            size_t index, size_t bytes, Rich4MkfError error)
{
    (void)ctx; (void)archive; (void)index; (void)bytes; (void)error;
    if (phase == 1) demand_reads++;
    if (phase == 0) demand_hits++;
}

/* Real deferred assets: exercising input/update/draw must not turn an AI
 * map notice into a read of the human modal's resources. */
static void check_automatic_event_assets(GamePreviewAssets *assets)
{
    static GameSession session;
    GameInputHoverContext hover;
    NewGameSettings settings;
    VirtualCursor cursor;
    CardPanelState card_panel;
    ToolPanelState tool_panel;
    int menu_hover = -1, character_hover = -1, top_hover = -1;
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
                                               0xf800, 0x07e0, 0x001f, 0);
    int mode, action;
    unsigned int before;
    assert(screen != NULL);
    assert(game_asset_ready(assets, &assets->ui.has_common_dialog));
    memset(&hover, 0, sizeof(hover));
    memset(&cursor, 0, sizeof(cursor));
    memset(&card_panel, 0, sizeof(card_panel));
    memset(&tool_panel, 0, sizeof(tool_panel));
    rich4_new_game_settings_init(&settings, 0);
    hover.app_state = RICH4_SCENE_GAME;
    hover.assets = assets; hover.settings = &settings; hover.session = &session;
    hover.cursor = &cursor; hover.card_panel = &card_panel; hover.tool_panel = &tool_panel;
    hover.last_menu_hover = &menu_hover;
    hover.last_new_game_character_hover = &character_hover;
    hover.last_top_button_hover = &top_hover;
    before = demand_reads;
    for (mode = 0; mode < 2; mode++) {
        Rich4GameEventPresentationSnapshot snapshot;
        GameEvent request = rich4_game_event_magic_house_auto_request(0, 0, 1u, 6);
        memset(&session, 0, sizeof(session));
        session.state.active = 1; session.state.player_count = 1;
        session.state.players[0].play_state = mode ? 5 : 2;
        session.state.players[0].is_ai = !mode;
        session.state.ai_rng_state = 123;
        game_begin_magic_house(&session, &request, NULL, rich4_sdl1_ticks(), 1);
        assert(game_update_magic_house(&session, NULL, assets, NULL, 0));
        assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_AI_NOTICE);
        game_update_input_hovers(&hover);
        snapshot = rich4_app_build_game_event_presentation_snapshot(&session.state, &session.presentation);
        draw_magic_house(screen, assets, &snapshot);
        session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_TARGET;
        session.presentation.magic_house.target_player = 0;
        session.presentation.magic_house.target_until = rich4_sdl1_ticks() + 1500;
        assert(game_update_magic_house(&session, NULL, assets, NULL, 0));
        game_update_input_hovers(&hover);
        draw_magic_house(screen, assets, &snapshot);
        assert(session.state.ai_rng_state == 123);
        assert(demand_reads == before);
        assert(!assets->ui.panel_data[18] && !assets->ui.panel_data[19] && !assets->ui.panel_data[20]);
    }
    for (action = 6; action <= 8; action++) {
        GamePointMinigameRenderView view;
        memset(&session, 0, sizeof(session));
        session.state.active = 1; session.state.player_count = 1;
        session.state.ai_rng_state = 456;
        session.presentation.point_minigame.active = 1;
        session.presentation.point_minigame.action = action;
        session.presentation.point_minigame.automatic = RICH4_POINT_MINIGAME_AUTOMATIC_RESOLVED_FIRST_SPEECH;
        session.presentation.point_minigame.phase = RICH4_POINT_MINIGAME_RESULT;
        session.presentation.point_minigame.score = 59;
        session.presentation.point_minigame.next_tick = rich4_sdl1_ticks();
        session.presentation.point_minigame.finish_until = rich4_sdl1_ticks() + 2000;
        assert(game_update_point_minigame(&session, assets, NULL, 0, 320));
        assert(game_point_minigame_click(&session, assets, NULL, 320, 240, 0));
        assert(build_point_minigame_render_view(&session, &view));
        draw_point_minigame(screen, assets, &view);
        assert(session.presentation.point_minigame.score == 59);
        assert(session.state.ai_rng_state == 456);
        assert(demand_reads == before);
        assert(!assets->ui.panel_data[78] && !assets->ui.panel_data[81]);
    }
    /* Human input and finale still demand the same mask and FLC. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1; session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.presentation.magic_house.active = 1;
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_SELECT;
    session.presentation.magic_house.hover = -1;
    session.presentation.magic_house.animation_tick = rich4_sdl1_ticks() + 100;
    game_update_input_hovers(&hover);
    assert(assets->ui.panel_data[19] != NULL);
    assert(game_update_magic_house(&session, NULL, assets, NULL, 0));
    assert(assets->ui.panel_data[20] != NULL);
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_SELECT);
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_FINALE;
    assert(game_update_magic_house(&session, NULL, assets, NULL, 0));
    assert(session.presentation.magic_house.phase == RICH4_MAGIC_HOUSE_FINALE);
    SDL_FreeSurface(screen);
    puts("QA_AI_EVENT_ASSETS_PASS magic_ai_and_trustee=2 point_results=3 unused_reads=0 human_mask_and_finale=1");
}

int main(int argc, char **argv)
{
    ResourceStore *store = rich4_resource_store_create();
    SdlOptions options;
    GamePreviewAssets assets;
    size_t menu_bytes;
    size_t model_bytes[12] = {0};
    unsigned int four_characters = 0;
    int cycle, character, part, choice;
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IOLBF, 0);
    memset(&options, 0, sizeof(options));
    options.data_dir = "assets/original";
    memset(&assets, 0, sizeof(assets));
    game_load_scoped_preview_assets(store, &options, &assets, 1, 0, 0);
    game_load_game_map_routes(store, &options, &assets);
    game_load_new_game_character_assets(assets.data_archive, &assets);
    menu_bytes = rich4_resource_store_cached_bytes(store);
    assert(!assets.menu.has_options_ui && assets.ui.has_save_load_panel);
    assert(assets.character.map_player_data[0][0] == NULL);
    assert(assets.map.maps[7].routes != NULL);
    printf("QA_SCOPE_MENU cache_bytes=%lu\n", (unsigned long)menu_bytes);
    for (character = 0; character < 12; character++) {
        for (part = 0; part < GAME_CHARACTER_MODEL_RESOURCES; part++) {
            Rich4MkfResourceInfo info;
            assert(rich4_mkf_get_info(assets.data_archive,
                (size_t)(128 + character * 21 + part), &info) == RICH4_MKF_OK);
            model_bytes[character] += info.uncompressed_size;
        }
    }
    for (choice = 0; choice < GAME_MAX_PLAYERS; choice++) {
        int largest = -1;
        for (character = 0; character < 12; character++)
            if (!(four_characters & (1u << character)) &&
                (largest < 0 || model_bytes[character] > model_bytes[largest])) largest = character;
        four_characters |= 1u << largest;
        printf("QA_SCOPE_FOUR character=%d model_bytes=%lu\n", largest,
               (unsigned long)model_bytes[largest]);
    }
    {
        static GameSession session;
        MenuAssets menu = {0};
        Rich4OptionsPanelState panel = {0};
        Rich4Config config = {0};
        Rich4SdlConfig sdl_config = {0};
        Rich4SceneMachine machine = {0};
        Rich4SceneId scene = RICH4_SCENE_MENU;
        NewGameSettings settings = {0};
        VirtualCursor cursor = {0};
        Rich4SprSmpChunk chunk;
        int running = 1;
        rich4_scene_machine_init(&machine, RICH4_SCENE_MENU);
        cursor.x = 60; cursor.y = 150;
        assert(game_handle_menu_input(&menu, &assets, &panel, &config,
            &sdl_config, &session, &settings, &machine, &scene, &cursor,
            "", NULL, &running));
        assert(scene == RICH4_SCENE_OPTIONS && assets.load_scope == 4);
        assert(rich4_spr_smp_get_chunk(assets.menu.options_data,
            assets.menu.options_size, &assets.menu.options_spr, 0, &chunk) == RICH4_SPR_SMP_OK);
        assert(panel.width == chunk.width && panel.height == chunk.height);
        assert(panel.left == 320 - chunk.width / 2);
        assert(panel.top == 240 - chunk.height / 2);
    }
    assert(assets.menu.has_options_ui);
    assert(game_prepare_asset_scope(&assets, 3, 0, 0) == 1);
    assert(assets.menu.has_setup_ui && assets.character.has_walk[11][2]);
    {
        NewGameSettings settings;
        size_t setup_bytes = rich4_resource_store_cached_bytes(store);
        unsigned char *setup_data = assets.menu.setup_data;
        int frame;
        rich4_new_game_settings_init(&settings, 0);
        settings.selected_count = 2;
        settings.selected_characters[0] = 0;
        settings.selected_characters[1] = 1;
        rich4_new_game_start_transition(&settings);
        for (frame = 0; frame <= 33; frame++) {
            rich4_new_game_update_transition(&settings, 100 + frame * 50);
            assert(game_sync_runtime_asset_scope(&assets, RICH4_SCENE_INTRO,
                                                 NULL, &settings) == 0);
            assert(assets.load_scope == 3 && assets.menu.setup_data == setup_data);
            assert(rich4_resource_store_cached_bytes(store) == setup_bytes);
            assert(assets.character.map_player_data[0][0] == NULL);
        }
        puts("QA_SCOPE_EXIT setup_retained=1 game_loads=0");
    }
    assert(game_prepare_asset_scope(&assets, 1, 0, 0) == 1);
    assert(rich4_resource_store_cached_bytes(store) == menu_bytes);
    for (cycle = 0; cycle < 3; cycle++) {
        assert(game_prepare_asset_scope(&assets, 2, 0, 3u) == 1);
        assert(assets.character.map_player_data[0][0] == NULL);
        assert(game_asset_ready(&assets, &assets.character.has_map_player[0][0]));
        assert(game_asset_ready(&assets, &assets.character.has_map_player[1][0]));
        assert(assets.character.map_player_data[11][0] == NULL);
        assert(assets.map.has_map_ground[0] && !assets.map.has_map_ground[1]);
        assert(game_prepare_asset_scope(&assets, 2, 1, (1u << 3) | (1u << 5)) == 1);
        assert(assets.character.map_player_data[0][0] == NULL);
        assert(game_asset_ready(&assets, &assets.character.has_map_player[3][0]));
        assert(assets.map.has_map_ground[1] && !assets.map.has_map_ground[0]);
        assert(game_prepare_asset_scope(&assets, 2, 7, four_characters) == 1);
        for (character = 0; character < 12; character++)
            if (four_characters & (1u << character))
                assert(game_asset_ready(&assets, &assets.character.has_map_player[character][20]));
        assert(assets.map.has_map_ground[7] && !assets.map.has_map_ground[1]);
        assert(game_prepare_asset_scope(&assets, 1, 0, 0) == 1);
        assert(rich4_resource_store_cached_bytes(store) == menu_bytes);
        printf("QA_SCOPE_RELEASE cycle=%d cache_bytes=%lu\n", cycle,
               (unsigned long)rich4_resource_store_cached_bytes(store));
    }
    /* Every panel can be revisited after its previous bytes were released.
     * Active metadata users keep the same bytes through the first draw. */
    assert(game_prepare_asset_scope(&assets, 2, 0, 3u) == 1);
    rich4_resource_store_set_debug_observer(store, demand_observer, NULL);
    check_automatic_event_assets(&assets);
    game_lazy_assets_begin_frame(&assets);
    game_lazy_assets_begin_frame(&assets);
    game_lazy_assets_clear_recent(&assets);
    {
        size_t resident = rich4_resource_store_cached_bytes(store);
        int panel;
        rich4_resource_store_set_debug_observer(store, demand_observer, NULL);
        assert(resident > 0);
        for (panel = 0; panel < GAME_PANEL_RESOURCE_COUNT; panel++) {
            int round;
            for (round = 0; round < 2; round++) {
                unsigned int before_reads = demand_reads, before_hits = demand_hits;
                assert(assets.ui.panel_data[panel] == NULL);
                assert(game_asset_require_data(&assets, &assets.ui.panel_data[panel]));
                assert(assets.ui.panel_size[panel] != 0);
                if (round == 1 && assets.ui.panel_size[panel] <= GAME_LAZY_RECENT_BYTES) {
                    assert(demand_reads == before_reads);
                    assert(demand_hits == before_hits + 1);
                }
                game_lazy_assets_begin_frame(&assets);
                game_lazy_assets_begin_frame(&assets);
                assert(assets.ui.panel_data[panel] == NULL);
                assert(rich4_resource_store_cached_bytes(store) <= resident + GAME_LAZY_RECENT_BYTES);
            }
        }
        assert(game_asset_graph_frames(&assets, &assets.character.first_landing_data[0],0) > 0);
        assert(assets.character.first_landing_data[0] != NULL);
        game_lazy_assets_begin_frame(&assets);
        game_lazy_assets_begin_frame(&assets);
        assert(assets.character.first_landing_data[0] == NULL);
        assert(rich4_resource_store_cached_bytes(store) <= resident + GAME_LAZY_RECENT_BYTES);
        printf("QA_DEMAND_PASS panels=113 visits=226 bounded_warm_cache=1 timing_lease_reused=1 released_when_idle=1 resident_bytes=%lu\n",
               (unsigned long)resident);
    }
#if defined(RICH4_PLATFORM_MIYOO)
    game_lazy_assets_clear_recent(&assets);
    game_lazy_assets_prepare_intro(&assets);
    {
        unsigned int before = demand_reads;
        static const int panels[] = {63,78,79,80};
        size_t i;
        for (i=0;i<sizeof(panels)/sizeof(panels[0]);i++)
            assert(game_asset_require_data(&assets,&assets.ui.panel_data[panels[i]]));
        assert(game_asset_require_data(&assets,&assets.tool.tool_effect_data[4]));
        assert(demand_reads == before);
        puts("QA_EVENT_WARMUP_PASS first_use_reads=0 panel=63,78,79,80 data=553");
    }
#endif
    printf("QA_DEMAND_READS reads=%u hits=%u\n", demand_reads, demand_hits);
    game_free_preview_assets(&assets);
    assert(rich4_resource_store_cached_bytes(store) == 0);
    assert(rich4_resource_store_cached_count(store) == 0);
    rich4_resource_store_destroy(store);
#ifndef _WIN32
    {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        printf("QA_SCOPE_MEMORY peak_rss_kb=%ld\n", usage.ru_maxrss);
    }
#endif
    puts("QA_SCOPE_PASS cycles=3 old_characters_released=1 old_maps_released=1 final_cache_bytes=0");
    return 0;
}
