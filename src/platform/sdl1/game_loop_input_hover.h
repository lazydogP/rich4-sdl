#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_INPUT_HOVER_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_INPUT_HOVER_H

#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: post-event virtual-cursor hover/selection synchronization moved
 * out of runtime_main.c. The event loop still owns the cursor and panel
 * instances; this context only makes those existing references explicit. */
typedef struct GameInputHoverContext {
    Rich4SceneId app_state;
    int view_mode;
    const Rich4SdlConfig *sdl_config;
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    GameSession *session;
    VirtualCursor *cursor;
    const MenuAssets *menu_assets;
    Rich4OptionsPanelState *options_panel;
    HelpPanelState *help_panel;
    SmallMapPanelState *small_map_panel;
    Rich4AiSettingsState *ai_settings_panel;
    CardPanelState *card_panel;
    ToolPanelState *tool_panel;
    StockPanelState *stock_panel;
    Rich4Audio *audio;
    int *last_menu_hover;
    int *last_new_game_character_hover;
    int *last_top_button_hover;
} GameInputHoverContext;

void game_update_input_hovers(GameInputHoverContext *context);

#endif
