#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_MODALS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_MODALS_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: early virtual-confirm modal branches moved out of runtime_main.c. */
typedef struct GameConfirmModalsContext {
    Rich4SceneId app_state;
    GameSession *session;
    StockPanelState *stock_panel;
    SmallMapPanelState *small_map_panel;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    VirtualCursor *cursor;
    Rich4Audio *audio;
} GameConfirmModalsContext;

/* Returns non-zero when one of the early modal branches consumed confirm. */
int game_handle_confirm_modals(GameConfirmModalsContext *context);

#endif
