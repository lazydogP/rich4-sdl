#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_KEY_UP_MODALS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_KEY_UP_MODALS_H

#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"

/* R10.1d: key-up interruption hooks moved out of runtime_main.c. Intro
 * interruption is recorded by the app intro flow and consumed at finalize. */
typedef struct GameKeyUpModalsContext {
    Rich4SceneId app_state;
    GameSession *session;
    Rich4Audio *audio;
} GameKeyUpModalsContext;

void game_handle_key_up_modals(GameKeyUpModalsContext *context);

/* R10.1d: physical mouse-button-up interruption hooks share the same modal
 * semantics as key-up; the caller still owns the frame-local intro flag. */
void game_handle_mouse_button_up_modals(
    Rich4SceneId app_state, GameSession *session, Rich4Audio *audio,
    int button);

#endif
