#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_OPTIONS_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_OPTIONS_INPUT_H

#include "app/config_types.h"
#include "app/input_flow.h"
#include "app/options_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/scene.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: options-scene confirmation/input dispatch moved out of
 * runtime_main.c. */
int game_handle_options_input(
    Rich4OptionsPanelState *panel, Rich4Config *config,
    Rich4SdlConfig *sdl_config, Rich4SceneMachine *scene_machine,
    Rich4SceneId *app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, const char *music_dir,
    Rich4Audio *audio, int physical_pointer_up);

/* Synchronizes the options panel's ordinary control hover or its Data[440]
 * confirmation-half hover after pointer motion. */
void game_update_options_hover(
    Rich4OptionsPanelState *panel, const GamePreviewAssets *assets,
    int x, int y, Rich4Audio *audio);

#endif
