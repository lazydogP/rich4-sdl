#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_VISITS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_VISITS_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/scene.h"

/* R10.1d: shop/lottery/confinement cancel branches moved out of
 * runtime_main.c. */
typedef struct GameCancelVisitsContext {
    Rich4SceneId app_state;
    GameSession *session;
    VirtualCursor *cursor;
    Rich4Audio *audio;
    int *shop_music_restore_pending;
} GameCancelVisitsContext;

/* Returns non-zero when a visit modal consumed cancel. */
int game_handle_cancel_visits(GameCancelVisitsContext *context);

#endif
