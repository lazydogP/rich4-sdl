#ifndef RICH4_PLATFORM_SDL1_RUNTIME_STATE_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_STATE_BOOTSTRAP_H

#include "app/ai_settings_state.h"
#include "app/card_panel_state.h"
#include "app/options_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: modal/panel state defaults moved out of runtime_main.c. */
void game_init_runtime_panel_states(
    Rich4OptionsPanelState *options_panel,
    Rich4AiSettingsState *ai_settings_panel,
    HelpPanelState *help_panel, SmallMapPanelState *small_map_panel,
    CardPanelState *card_panel, ToolPanelState *tool_panel,
    StockPanelState *stock_panel);

#endif
