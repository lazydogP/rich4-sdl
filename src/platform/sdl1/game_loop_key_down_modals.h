#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_KEY_DOWN_MODALS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_KEY_DOWN_MODALS_H

#include "app/app_command.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: early modal/key ownership gates from SDL_KEYDOWN moved out of
 * runtime_main.c. The company/bank/game command key branches remain in the
 * event loop for their own later seams. */
typedef struct GameKeyDownModalsContext {
    Rich4SceneId app_state;
    GameSession *session;
    StockPanelState *stock_panel;
    SmallMapPanelState *small_map_panel;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    const Rich4Config *config;
    Rich4Audio *audio;
    VirtualCursor *cursor;
    Rich4AppKey key;
    int key_modifiers;
    Rich4SetupTransitionInput *setup_transition_input;
} GameKeyDownModalsContext;

/* Returns non-zero when a modal owns the key-down event. */
int game_handle_key_down_modals(GameKeyDownModalsContext *context);

#endif
