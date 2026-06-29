#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_UPDATE_ADAPTER_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_UPDATE_ADAPTER_H

#include <SDL/SDL.h>

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"

void rich4_sdl_update_game_session_adapter(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now, int cursor_x);

#endif
