/* R10.1d: card/tool/AI confirmation panel dispatch moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_confirm_panels.h"
#include "app/ai_settings_flow.h"
#include "app/tool_panel_flow.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_panels.h"

int game_handle_confirm_panels(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    CardPanelState *card_panel, ToolPanelState *tool_panel,
    Rich4AiSettingsState *ai_settings_panel, VirtualCursor *cursor,
    Rich4Audio *audio)
{
    if (app_state != RICH4_SCENE_GAME)
        return 0;
    if (card_panel->phase != CARD_PANEL_CLOSED) {
        game_card_panel_confirm(
            session, assets, settings, card_panel,
            cursor->x, cursor->y, rich4_sdl1_ticks(), audio);
        cursor->confirm_pressed = 0;
        return 1;
    } else if (tool_panel->phase != TOOL_PANEL_CLOSED) {
        if (rich4_tool_panel_allows_edge_scroll(tool_panel) &&
            tool_panel->edge_scroll.direction != 0 &&
            cursor->confirm_pointer_pressed) {
            cursor->confirm_pressed = 0;
            return 1;
        }
        game_tool_panel_confirm(
            session, assets, settings, tool_panel,
            cursor->x, cursor->y, rich4_sdl1_ticks(), audio);
        cursor->confirm_pressed = 0;
        return 1;
    } else if (ai_settings_panel->active) {
        rich4_ai_settings_apply(
            ai_settings_panel, session, 9, cursor->x);
        cursor->confirm_pressed = 0;
        return 1;
    }
    return 0;
}
