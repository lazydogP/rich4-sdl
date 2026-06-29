/* R10.1d: joystick button-down dispatch moved verbatim out of
 * runtime_main.c. Modal gates remain ahead of virtual-cursor mapping. */

#include "platform/sdl1/game_loop_joy_button_down_input.h"

#include "platform/sdl1/game_loop_joy_button_down_modals.h"
#include "platform/sdl1/runtime_cursor.h"

int game_handle_joy_button_down_input(
    GameJoyButtonDownInputContext *context)
{
    GameJoyButtonDownModalsContext modals = {
        context->app_state,
        context->session,
        context->small_map_panel,
        context->help_panel,
        context->cursor,
        context->setup_transition_input
    };

    if (game_handle_joy_button_down_modals(&modals))
        return 1;

    rich4_app_input_apply_joy(
        context->app_state, context->settings, context->session,
        context->ai_settings_panel != NULL &&
            context->ai_settings_panel->active,
        context->cursor, context->button,
        context->confirm_button, context->cancel_button);
    return 0;
}
