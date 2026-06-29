/* R10.1d: card/tool/AI panel cancel branches moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_cancel_panels.h"
#include "app/ai_settings_flow.h"
#include "app/card_panel_flow.h"
#include "app/tool_panel_flow.h"
#include "app/audio_event.h"
#include "platform/sdl1/game_loop_audio_events.h"

int game_handle_cancel_panels(GameCancelPanelsContext *context)
{
    if (context->app_state == RICH4_SCENE_GAME &&
        context->card_panel->phase != CARD_PANEL_CLOSED) {
        rich4_card_panel_cancel(context->card_panel);
        rich4_audio_play_cancel(context->audio);
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->tool_panel->phase != TOOL_PANEL_CLOSED) {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(context->session);
        rich4_tool_panel_cancel(
            context->session, context->tool_panel, audio_events);
        game_session_audio_flush(context->session, context->audio);
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->ai_settings_panel->active) {
        rich4_ai_settings_cancel(context->ai_settings_panel);
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    return 0;
}
