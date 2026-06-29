/* R10.1d: modal/panel state defaults moved verbatim out of runtime_main.c. */

#include <string.h>

#include "platform/sdl1/runtime_state_bootstrap.h"
#include "app/ai_settings_flow.h"
#include "app/card_panel_flow.h"
#include "app/options_flow.h"
#include "app/tool_panel_flow.h"

void game_init_runtime_panel_states(
    Rich4OptionsPanelState *options_panel,
    Rich4AiSettingsState *ai_settings_panel, HelpPanelState *help_panel,
    SmallMapPanelState *small_map_panel, CardPanelState *card_panel,
    ToolPanelState *tool_panel, StockPanelState *stock_panel)
{
    rich4_options_panel_reset(options_panel);
    rich4_ai_settings_reset(ai_settings_panel);
    memset(help_panel, 0, sizeof(*help_panel));
    help_panel->pressed_control = -1;
    memset(small_map_panel, 0, sizeof(*small_map_panel));
    rich4_card_panel_reset(card_panel);
    rich4_tool_panel_reset(tool_panel);
    memset(stock_panel, 0, sizeof(*stock_panel));
}
