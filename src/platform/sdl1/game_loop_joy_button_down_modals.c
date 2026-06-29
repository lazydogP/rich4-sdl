/* R10.1d: joystick modal ownership gates moved verbatim out of
 * runtime_main.c. Confirm/cancel mapping remains in the caller. */

#include "platform/sdl1/game_loop_joy_button_down_modals.h"

int game_handle_joy_button_down_modals(
    GameJoyButtonDownModalsContext *context)
{
    if (context->small_map_panel->active) {
        /* The original callback has no joystick messages. */
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->help_panel->active) {
        /* The original help callback has no joystick messages. */
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_INTRO) {
        *context->setup_transition_input =
            RICH4_SETUP_TRANSITION_INPUT_CONTROLLER_BUTTON_DOWN;
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->session->presentation.intro.active) {
        /* No joystick equivalent exists in the original flags. */
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.lottery.active) {
        /* The original modal callback has no joystick messages. */
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    return 0;
}
