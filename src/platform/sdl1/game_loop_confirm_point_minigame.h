#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_POINT_MINIGAME_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_POINT_MINIGAME_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* Returns non-zero when the point minigame owns confirm. */
int game_handle_confirm_point_minigame(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, VirtualCursor *cursor,
    Rich4Audio *audio);

#endif
