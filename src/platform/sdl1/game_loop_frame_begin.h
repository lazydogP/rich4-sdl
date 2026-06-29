#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_FRAME_BEGIN_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_FRAME_BEGIN_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/card_panel_state.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* Frame preparation is a sequencing seam, not a data model. Keep its
 * arguments explicit so a one-use tree of pointer-only contexts cannot grow
 * another hidden ownership boundary. */
void game_begin_runtime_frame(
    VirtualCursor *cursor, Rich4SceneId app_state,
    const Rich4OptionsPanelState *options_panel,
    const Rich4Config *rich4_config, Rich4Audio *audio,
    CardPanelState *card_panel, GameSession *game_session,
    const GamePreviewAssets *game_preview_assets,
    NewGameSettings *new_game_settings, int mouse_left_down);

#endif
