#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_CONFINEMENT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_CONFINEMENT_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: confinement/hospital confirmation dispatch moved out of
 * runtime_main.c. */
int game_handle_confirm_confinement(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, VirtualCursor *cursor,
    Rich4Audio *audio, int *host_pointer_sync_needed);

#endif
