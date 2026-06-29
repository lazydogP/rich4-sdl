#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_MENU_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_MENU_INPUT_H

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/options_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/scene.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/runtime_options_types.h"

/* R10.1d: menu click dispatch, including the asset-less fallback, moved out
 * of runtime_main.c. */
int game_handle_menu_input(
    const MenuAssets *menu_assets, GamePreviewAssets *preview_assets,
    Rich4OptionsPanelState *options_panel, Rich4Config *config,
    Rich4SdlConfig *sdl_config, GameSession *session,
    NewGameSettings *settings, Rich4SceneMachine *scene_machine,
    Rich4SceneId *app_state, const VirtualCursor *cursor,
    const char *music_dir, Rich4Audio *audio, int *running);

#endif
