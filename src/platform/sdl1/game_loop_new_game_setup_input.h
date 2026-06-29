#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_NEW_GAME_SETUP_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_NEW_GAME_SETUP_INPUT_H

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/scene.h"
#include "app/new_game_setup_flow.h"
#include "resource/mkf.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/render/game_render_cache.h"

/* R10.1d: new-game setup click dispatch moved out of runtime_main.c. */
int game_handle_new_game_setup_input(
    NewGameSettings *settings, const VirtualCursor *cursor,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    const Rich4Config *config, const char *music_dir, Rich4Audio *audio);

void game_update_new_game_setup_scene(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    NewGameSettings *settings, VirtualCursor *cursor,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    Uint32 now, Rich4Audio *audio);

void game_update_new_game_transition_scene(
    SDL_Surface *screen, Rich4Mkf *mkf,
    GamePreviewAssets *assets, NewGameSettings *settings,
    GameSession *session, const Rich4Config *config,
    const Rich4SdlConfig *sdl_config, const char *music_dir,
    VirtualCursor *cursor,
    GameRenderCache *render_cache,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    Rich4Audio *audio, int *main_map_music_track,
    int *main_map_music_active, int *game_preview_trace_count,
    Rich4SetupTransitionInput setup_transition_input, Uint32 now);

#endif
