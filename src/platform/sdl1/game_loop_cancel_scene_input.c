/* R10.1d: scene-level virtual-cancel transitions moved verbatim out of
 * runtime_main.c. */

#include "app/scene_input_flow.h"
#include "platform/sdl1/game_loop_cancel_scene_input.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/runtime_bootstrap.h"

int game_handle_cancel_scene_input(GameCancelSceneContext *context)
{
    Rich4AppSceneCancelContext app_context;
    Rich4AppSceneCancelAction action;
    Rich4SceneId previous_scene;
    int restore_menu_music = 0;
    int restore_options_music = 0;
    if (context == NULL || context->app_state_out == NULL ||
        context->running == NULL)
        return 0;
    previous_scene = context->app_state;
    if (previous_scene == RICH4_SCENE_OPTIONS &&
        context->options_panel != NULL) {
        restore_options_music = context->options_panel->return_app_state ==
            RICH4_SCENE_MENU && context->options_panel->saved.music != 0;
    }
    app_context.app_state = context->app_state;
    app_context.session = context->session;
    app_context.settings = context->settings;
    app_context.options_panel = context->options_panel;
    app_context.scene_machine = context->scene_machine;
    app_context.app_state_out = context->app_state_out;
    action = rich4_app_scene_cancel(&app_context);
    if (action == RICH4_APP_SCENE_CANCEL_NONE)
        return 0;
    if (action == RICH4_APP_SCENE_CANCEL_QUIT) {
        *context->running = 0;
        return 1;
    }
    restore_menu_music = previous_scene == RICH4_SCENE_NEW_GAME &&
        context->config != NULL && context->config->music != 0 &&
        action == RICH4_APP_SCENE_CANCEL_SWITCH;
    if (restore_menu_music)
        start_menu_music(context->audio, context->music_dir);
    else if (restore_options_music)
        start_menu_music(context->audio, context->music_dir);
    /* Cancelling a game options panel returns to the already-running map
     * music.  Do not stop it here: the Options Cancel button follows the
     * same rule, and stopping it makes the main-map playlist advance on the
     * first frame after a right-click return. */
    return 1;
}
