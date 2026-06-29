#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_INPUT_FRAME_FINALIZE_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_INPUT_FRAME_FINALIZE_H

#include <SDL/SDL.h>

#include "app/ai_settings_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/scene.h"
#include "platform/sdl1/input/input_config_sdl.h"
#include "platform/sdl1/game_loop_cancel_input.h"
#include "platform/sdl1/game_loop_confirm_input.h"
#include "platform/sdl1/game_loop_input_hover.h"

/* R10.1d: post-event input-frame finalization moved out of
 * runtime_main.c. The helper keeps the original finish/update/hover/cancel/
 * confirm order while borrowing the event-loop-owned state. */
typedef struct GameInputFinalizeCursorContext {
    Rich4SceneId *app_state;
    NewGameSettings *settings;
    Rich4AiSettingsState *ai_settings_panel;
    GameSession *session;
    VirtualCursor *cursor;
    const Rich4InputConfig *input;
    SDL_Joystick *joystick;
    int *host_pointer_sync_needed;
} GameInputFinalizeCursorContext;

typedef struct GameInputFrameFinalizeContext {
    GameSession *session;
    GameInputFinalizeCursorContext *cursor;
    GameInputHoverContext *hover;
    GameCancelInputContext *cancel;
    GameConfirmInputContext *confirm;
} GameInputFrameFinalizeContext;

void game_finalize_input_frame(GameInputFrameFinalizeContext *context);

#endif
