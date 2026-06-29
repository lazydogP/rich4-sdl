#include "app/config_types.h"
#include "app/game_session.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/new_game_init.h"
#include "core/character_template.h"
#include "core/rng.h"
#include "debug/trace.h"
#include "app/game_session.h"
#include "app/game_session_runtime_init.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/render/game_render_cache.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_session_internal.h"
void init_game_session(GameSession *session,
                       const GamePreviewAssets *assets,
                       const NewGameSettings *settings,
                       const Rich4Config *runtime_config,
                       GameRenderCache *render_cache,
                       int route_count)
{
    int i;
    int map_index;
    NewGameInitConfig config;
    NewGameInitReport init_report;
    GameCharacterTemplate templates[12];
    Rich4Rng special_event_rng;
    const MapData *map = NULL;
    if (session == NULL) {
        return;
    }
    trace_game_start("init_game_session: begin");
    rich4_trace_emit(RICH4_TRACE_INTRO_BEGIN, 0,
                     settings != NULL ? settings->map : 0, 0);
    if (session->state.active && render_cache != NULL) {
        reset_game_dice_cache(render_cache);
    reset_game_property_flash_cache(render_cache);
    }
    rich4_app_reset_session(session);
    memset(&config, 0, sizeof(config));
    config.player_extra = settings != NULL ? settings->player_extra : 0;
    config.initial_fund = initial_money_from_settings(settings);
    config.traffic_method = settings != NULL ? settings->traffic_index : 0;
    config.stage = settings != NULL ? settings->stage : 0;
    config.map = settings != NULL ? settings->map : 0;
    config.day_limit = game_day_limit_from_settings(settings);
    config.win_target = game_win_target_from_settings(settings, config.initial_fund);
    config.land_period_index = settings != NULL ? settings->land_index : 0;
    config.calendar_start_day = runtime_config != NULL
        ? runtime_config->day : 0;
    config.calendar_start_month = runtime_config != NULL
        ? runtime_config->month : 0;
    config.calendar_start_year = runtime_config != NULL
        ? runtime_config->year : 0;
    for (i = 0; i < 12; i++) templates[i] = *rich4_character_template(i);
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        config.selected_characters[i] = settings != NULL ? settings->selected_characters[i] : i;
        config.selected_ai[i] = settings != NULL ? settings->selected_ai[i] : 0;
        if (config.selected_characters[i] >= 0 && config.selected_characters[i] < 12) {
            config.character_commit_before[config.selected_characters[i]] = 4;
        }
    }
    map_index = config.stage * GAME_MAPS_PER_STAGE + config.map;
    if (map_index < 0 || map_index >= GAME_BACKGROUND_COUNT) map_index = 0;
    if (assets != NULL && settings != NULL)
        map = &assets->map.maps[map_index];
    config.character_templates = templates;
    config.character_template_count = 12;
    config.map_stocks = &game_stock_table[map_index * GAME_STOCK_COUNT];
    config.map_stock_count = GAME_STOCK_COUNT;
    config.defer_stock_initialization = 1;
    rich4_rng_global_export(&special_event_rng);
    config.special_event_rng = &special_event_rng;
    if (!rich4_new_game_init(&session->state, &config, &init_report)) return;
    rich4_rng_global_import(&special_event_rng);
    session->debug.original_selected_player_count = init_report.selected_count;
    session->debug.original_num_players = init_report.num_players;
    for (i = 0; i < 12; i++) {
        session->debug.setup_character_normalized[i] = init_report.character_commit_after[i];
    }
    if (render_cache != NULL) {
        render_cache->dice_graph_surface = NULL;
        render_cache->dice_source_surface = NULL;
        render_cache->dice_rendered_frame = -1;
        render_cache->dice_surface_resource = -1;
        render_cache->dice_surface_x = 0;
        render_cache->dice_surface_y = 0;
    }
    rich4_app_initialize_session_runtime(
        session, map, route_count, NULL, NULL);
    game_session_prepare_intro_avi(session, settings);
    trace_game_start("init_game_session: end");
}
