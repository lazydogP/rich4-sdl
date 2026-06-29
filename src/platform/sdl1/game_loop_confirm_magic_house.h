#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_MAGIC_HOUSE_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_MAGIC_HOUSE_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: magic-house confirm modal dispatch moved out of runtime_main.c. */
typedef struct GameConfirmMagicHouseContext {
    Rich4SceneId app_state;
    GameSession *session;
    const GamePreviewAssets *assets;
    VirtualCursor *cursor;
    Rich4Audio *audio;
} GameConfirmMagicHouseContext;

/* Returns non-zero when magic house owns confirm. */
int game_handle_confirm_magic_house(
    GameConfirmMagicHouseContext *context);

#endif
