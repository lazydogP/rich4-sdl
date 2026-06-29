#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_UP_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_UP_INPUT_H

#include <SDL/SDL.h>

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/options_flow.h"
#include "app/scene.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "app/tool_panel_state.h"

/* R10.1d: complete SDL_MOUSEBUTTONUP dispatch moved out of runtime_main.c.
 * The caller still performs pointer normalization and owns the event-loop
 * continue; this seam returns non-zero only for the same early modal exits. */
typedef struct GameMouseButtonUpInputContext {
    Rich4SceneId app_state;
    GameSession *session;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    StockPanelState *stock_panel;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    SmallMapPanelState *small_map_panel;
    ToolPanelState *tool_panel;
    VirtualCursor *cursor;
    Rich4Audio *audio;
    SDL_Surface *capture_source;
    int button;
    int *mouse_left_down;
    Rich4SetupTransitionInput *setup_transition_input;
    int *shop_music_restore_pending;
    const Rich4Config *config;
    const Rich4SdlConfig *sdl_config;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    Rich4SceneId *scene_state;
    const char *music_dir;
} GameMouseButtonUpInputContext;

int game_handle_mouse_button_up_input(
    GameMouseButtonUpInputContext *context);

#endif
