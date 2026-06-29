#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_DOWN_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_DOWN_H

#include "app/ai_settings_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: modal left-button-down dispatch moved out of runtime_main.c. The
 * event loop still owns pointer normalization and the outer input gates; this
 * context only borrows the modal state needed by the original dispatch. */
typedef struct GameMouseButtonDownContext {
    Rich4SceneId app_state;
    const GamePreviewAssets *assets;
    GameSession *session;
    NewGameSettings *settings;
    VirtualCursor *cursor;
    StockPanelState *stock_panel;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    Rich4AiSettingsState *ai_settings_panel;
    ToolPanelState *tool_panel;
    Rich4Audio *audio;
} GameMouseButtonDownContext;

/* Returns non-zero when a modal branch consumed the left-button-down event. */
int game_handle_mouse_button_down_modal(
    GameMouseButtonDownContext *context);

void game_handle_mouse_button_down_right_cancel(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session,
    const SmallMapPanelState *small_map_panel,
    const HelpPanelState *help_panel,
    const Rich4AiSettingsState *ai_settings_panel, VirtualCursor *cursor,
    int button);

#endif
