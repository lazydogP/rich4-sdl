#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_SHOP_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_SHOP_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"

/* R10.1d: shop confirmation/click dispatch moved out of runtime_main.c. */
int game_handle_confirm_shop(
    Rich4SceneId app_state, GameSession *session,
    VirtualCursor *cursor, Rich4Audio *audio);

#endif
