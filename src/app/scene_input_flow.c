#include "app/scene_input_flow.h"

#include "app/game_session.h"

/* App-owned scene cancel flow. It mutates only session/settings/options and
 * scene-machine state; SDL audio, music and process-loop state stay outside.
 * Evidence/test: A5/A6 and tests/unit/test_scene.c. */
Rich4AppSceneCancelAction rich4_app_scene_cancel(
    Rich4AppSceneCancelContext *context)
{
    if (context == NULL || context->app_state_out == NULL)
        return RICH4_APP_SCENE_CANCEL_NONE;
    if (context->app_state == RICH4_SCENE_GAME) {
        /* Do not discard an active match by navigating back to setup. */
        return RICH4_APP_SCENE_CANCEL_NONE;
    }
    if (context->app_state == RICH4_SCENE_INTRO) {
        rich4_new_game_cancel_transition(context->settings);
        if (context->settings != NULL)
            rich4_new_game_settings_from_session(
                context->settings, context->session);
        rich4_app_reset_session(context->session);
        rich4_scene_machine_request_transition(
            context->scene_machine, context->app_state_out,
            RICH4_SCENE_NEW_GAME);
        return RICH4_APP_SCENE_CANCEL_SWITCH;
    }
    if (context->app_state == RICH4_SCENE_NEW_GAME) {
        if (context->settings != NULL &&
            context->settings->dropdown_row >= 0) {
            rich4_new_game_close_dropdown(context->settings);
            return RICH4_APP_SCENE_CANCEL_STAY;
        }
        rich4_scene_machine_request_transition(
            context->scene_machine, context->app_state_out,
            RICH4_SCENE_MENU);
        return RICH4_APP_SCENE_CANCEL_SWITCH;
    }
    if (context->app_state == RICH4_SCENE_OPTIONS) {
        if (context->options_panel != NULL) {
            if (context->options_panel->child) {
                rich4_options_child_cancel(context->options_panel);
                return RICH4_APP_SCENE_CANCEL_STAY;
            }
            rich4_options_panel_restore(context->options_panel);
            context->options_panel->active = 0;
            rich4_scene_machine_request_transition(
                context->scene_machine, context->app_state_out,
                context->options_panel->return_app_state);
        }
        return RICH4_APP_SCENE_CANCEL_SWITCH;
    }
    return RICH4_APP_SCENE_CANCEL_QUIT;
}
