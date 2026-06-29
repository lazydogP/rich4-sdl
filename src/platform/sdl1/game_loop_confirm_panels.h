#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_PANELS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CONFIRM_PANELS_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/app_asset_types.h"

/* R10.1d: card/tool/AI confirmation panel dispatch moved out of
 * runtime_main.c. */
int game_handle_confirm_panels(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    CardPanelState *card_panel, ToolPanelState *tool_panel,
    Rich4AiSettingsState *ai_settings_panel, VirtualCursor *cursor,
    Rich4Audio *audio);

#endif
