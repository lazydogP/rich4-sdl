/* R10.1f: setup frame lifecycle and scene transition belong to app. */

#include "app/new_game_scene_flow.h"

Rich4AppNewGameSetupInputResult rich4_app_new_game_setup_pointer_up(
    NewGameSettings *settings, int cursor_x, int cursor_y,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state)
{
    Rich4AppNewGameSetupInputResult result = {
        RICH4_NEW_GAME_SETUP_NO_ACTION, 0, 0
    };
    if (settings == NULL || app_state == NULL ||
        *app_state != RICH4_SCENE_NEW_GAME)
        return result;
    result.action = rich4_new_game_apply_pointer_up(
        settings, cursor_x, cursor_y);
    result.selected_count = settings->selected_count;
    result.map = settings->map;
    if (result.action == RICH4_NEW_GAME_SETUP_EXIT)
        rich4_scene_machine_request_transition(
            scene_machine, app_state, RICH4_SCENE_MENU);
    return result;
}

int rich4_app_new_game_setup_frame_update(
    NewGameSettings *settings, int cursor_x, int cursor_y, uint32_t now,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    const Rich4AppNewGameSetupServices *services)
{
    Rich4NewGameSetupFrameResult frame;
    if (settings == NULL || app_state == NULL || services == NULL ||
        *app_state != RICH4_SCENE_NEW_GAME)
        return 0;
    if (!rich4_new_game_update_setup_frame(
            settings, cursor_x, cursor_y, now, &frame))
        return 0;
    if ((frame.animation_flags & 1) && services->play_ok != NULL)
        services->play_ok(services->audio_context);
    if (frame.transition_complete) {
        if (services->play_transition != NULL)
            services->play_transition(services->audio_context);
        rich4_new_game_start_transition(settings);
        rich4_scene_machine_request_transition(
            scene_machine, app_state, RICH4_SCENE_INTRO);
        if (services->draw != NULL) services->draw(services->draw_context, 1);
    } else if (services->draw != NULL) {
        services->draw(services->draw_context, 0);
    }
    return 1;
}

int rich4_app_new_game_transition_frame_update(
    NewGameSettings *settings, uint32_t now,
    Rich4SetupTransitionInput input, Rich4SceneMachine *scene_machine,
    Rich4SceneId *app_state,
    const Rich4AppNewGameTransitionServices *services)
{
    int animation_complete;
    if (settings == NULL || app_state == NULL || services == NULL ||
        *app_state != RICH4_SCENE_INTRO)
        return 0;
    animation_complete = rich4_new_game_update_transition(settings, now);
    if (!rich4_setup_transition_should_complete(animation_complete, input)) {
        if (services->draw != NULL) services->draw(services->draw_context);
        return 1;
    }
    rich4_new_game_cancel_transition(settings);
    if (services->complete != NULL)
        services->complete(services->complete_context);
    rich4_scene_machine_request_transition(
        scene_machine, app_state, RICH4_SCENE_GAME);
    if (services->scene_ready != NULL)
        services->scene_ready(services->scene_ready_context);
    return 1;
}
