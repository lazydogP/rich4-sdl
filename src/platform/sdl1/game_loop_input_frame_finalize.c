/* R10.1d: post-event input-frame finalization moved verbatim out of
 * runtime_main.c. The original helper order remains explicit below. */

#include "platform/sdl1/game_loop_cancel_input.h"
#include "platform/sdl1/game_loop_confirm_input.h"
#include "platform/sdl1/game_loop_input_frame_finalize.h"
#include "platform/sdl1/game_loop_input_hover.h"
#include "app/input_flow.h"
#include "app/intro_flow.h"
#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/runtime.h"

void game_finalize_input_frame(GameInputFrameFinalizeContext *context)
{
    GameInputFinalizeCursorContext *cursor;
    if (context == NULL) return;
    cursor = context->cursor;
    if (context->session != NULL)
        rich4_intro_finish_if_requested_at_tick(context->session, rich4_sdl1_ticks());
    if (cursor != NULL && cursor->app_state != NULL &&
        cursor->host_pointer_sync_needed != NULL &&
        rich4_app_input_ui_active(
            *cursor->app_state, cursor->settings, cursor->session,
            cursor->ai_settings_panel != NULL &&
                cursor->ai_settings_panel->active))
        *cursor->host_pointer_sync_needed = update_cursor(
            cursor->cursor, cursor->input, cursor->joystick);
    if (context->hover != NULL)
        game_update_input_hovers(context->hover);
    if (context->cancel != NULL)
        game_handle_cancel_input(context->cancel);
    if (context->confirm != NULL)
        game_handle_confirm_input(context->confirm);
}
