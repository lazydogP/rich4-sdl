#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_KEY_DOWN_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_KEY_DOWN_INPUT_H

#include "app/ai_settings_state.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/app_command.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: complete SDL_KEYDOWN dispatch moved out of runtime_main.c. The
 * helper returns non-zero only when the early modal gate consumed the key,
 * matching the caller's original event-loop continue. */
typedef struct GameKeyDownInputContext {
    Rich4SceneId app_state;
    GameSession *session;
    StockPanelState *stock_panel;
    SmallMapPanelState *small_map_panel;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    Rich4Config *config;
    Rich4Audio *audio;
    VirtualCursor *cursor;
    Rich4AiSettingsState *ai_settings_panel;
    Rich4AppKey key;
    int key_modifiers;
    Rich4SetupTransitionInput *setup_transition_input;
    Rich4AppKey confirm_key;
    Rich4AppKey cancel_key;
} GameKeyDownInputContext;

int game_handle_key_down_input(GameKeyDownInputContext *context);

#endif
