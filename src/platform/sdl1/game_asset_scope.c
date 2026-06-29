#include <stdio.h>
#include <string.h>
#include "platform/sdl1/game_asset_api.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_trace.h"

/* Original init_new_game (rich4/csrc/game_init.c) loads the selected JUMP
 * background and frees its setup resources on exit. 0x40B93B selects a
 * Data model from 0x80 + character*21; 0x407E3B selects the current map's
 * graphics. Scope these same resource families, without changing indices. */

int game_load_game_preview_assets(ResourceStore *store, const SdlOptions *options,
                                  GamePreviewAssets *assets)
{
    return game_load_scoped_preview_assets(store, options, assets, 0, 0, 0xfffu);
}

int game_prepare_asset_scope(GamePreviewAssets *assets, int scope,
                             int background, unsigned int characters)
{
    ResourceStore *store;
    const char *data_dir;
    SdlOptions options;
    unsigned int started, phase_started;
    if (assets == NULL || assets->load_scope == 0) return 0;
    if (assets->load_scope == scope &&
        (scope != 2 || (assets->active_background == background &&
                       assets->active_characters == characters))) return 0;
    store = assets->resource_store;
    data_dir = assets->data_dir;
    started = runtime_profile_now();
    runtime_profile_mark("assets.begin");
    /* Free all scene-owned views before releasing their leases. Menu/cursor
     * and help have independent permanent borrows and remain valid. */
    game_free_preview_assets(assets);
    runtime_profile_stage("assets.release", started);
    memset(&options, 0, sizeof(options));
    options.data_dir = data_dir;
    phase_started = runtime_profile_now();
    game_load_scoped_preview_assets(store, &options, assets, scope, background, characters);
    runtime_profile_stage("assets.data_jump_panel", phase_started);
    phase_started = runtime_profile_now();
    game_load_game_map_routes(store, &options, assets);
    runtime_profile_stage("assets.map", phase_started);
    phase_started = runtime_profile_now();
    game_load_new_game_character_assets(assets->data_archive, assets);
    runtime_profile_stage("assets.characters", phase_started);
    runtime_profile_stage("assets.total", started);
    runtime_profile_mark("assets.ready");
    printf("asset scope: phase=%d map=%d characters=%03x cache_bytes=%lu resources=%lu\n",
           scope, background, characters,
           (unsigned long)rich4_resource_store_cached_bytes(store),
           (unsigned long)rich4_resource_store_cached_count(store));
    if (scope == 2 && (background < 0 || background >= GAME_BACKGROUND_COUNT ||
                      assets->map.maps[background].routes == NULL)) return -1;
    return 1;
}

int game_sync_runtime_asset_scope(GamePreviewAssets *assets, Rich4SceneId scene,
    const GameSession *session, const NewGameSettings *settings)
{
    unsigned int characters = 0;
    int player;
    int active = session != NULL && session->state.active;
    /* 0x404E44 phase 2 still paints JUMP setup sprites. WinMain calls
     * 0x401543 (Data[601], present) after 0x406DE7 returns, then loads
     * the map. INTRO here names that setup exit phase. */
    int scope = (scene == RICH4_SCENE_NEW_GAME || scene == RICH4_SCENE_INTRO) ? 3 :
        scene == RICH4_SCENE_OPTIONS ? (active ? 2 : 4) :
        scene == RICH4_SCENE_GAME ? 2 : 1;
    int background = settings != NULL ? settings->stage * GAME_MAPS_PER_STAGE + settings->map : 0;
    if (scope == 2 && active) {
        for (player = 0; player < session->state.player_count; player++) {
            int character = session->state.players[player].character;
            if (character >= 0 && character < 12) characters |= 1u << character;
        }
    } else if (scope == 2 && settings != NULL) {
        for (player = 0; player < settings->selected_count && player < GAME_MAX_PLAYERS; player++) {
            int character = settings->selected_characters[player];
            if (character >= 0 && character < 12) characters |= 1u << character;
        }
    }
    return game_prepare_asset_scope(assets, scope, background, characters);
}
