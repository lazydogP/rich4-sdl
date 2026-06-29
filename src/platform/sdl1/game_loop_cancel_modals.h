#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_MODALS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_MODALS_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/scene.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: early virtual-cancel modal branches moved out of runtime_main.c.
 * The caller deliberately continues into the existing confirm pass after
 * this handler, preserving the frame-level ordering. */
typedef struct GameCancelModalsContext {
    Rich4SceneId app_state;
    GameSession *session;
    StockPanelState *stock_panel;
    VirtualCursor *cursor;
    Rich4Audio *audio;
} GameCancelModalsContext;

/* Returns non-zero when one of the early modal branches consumed cancel. */
int game_handle_cancel_modals(GameCancelModalsContext *context);

#endif
