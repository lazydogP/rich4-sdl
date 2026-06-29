#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/scene_input_flow.h"

static Rich4AppSceneCancelContext context_for(
    Rich4SceneId scene, GameSession *session, NewGameSettings *settings,
    Rich4OptionsPanelState *options, Rich4SceneMachine *machine,
    Rich4SceneId *app_state)
{
    Rich4AppSceneCancelContext context = {
        scene, session, settings, options, machine, app_state
    };
    return context;
}

int main(void)
{
    GameSession session;
    NewGameSettings settings;
    Rich4OptionsPanelState options;
    Rich4SceneMachine machine;
    Rich4SceneId app_state;
    Rich4AppSceneCancelContext context;

    memset(&session, 0, sizeof(session));
    memset(&settings, 0, sizeof(settings));
    memset(&options, 0, sizeof(options));
    rich4_scene_machine_init(&machine, RICH4_SCENE_GAME);
    app_state = RICH4_SCENE_GAME;
    session.state.active = 1;
    settings.transition_active = 1;
    context = context_for(
        RICH4_SCENE_GAME, &session, &settings, &options,
        &machine, &app_state);
    assert(rich4_app_scene_cancel(&context) ==
           RICH4_APP_SCENE_CANCEL_NONE);
    assert(app_state == RICH4_SCENE_GAME);
    assert(session.state.active);
    assert(settings.transition_active);

    rich4_scene_machine_init(&machine, RICH4_SCENE_INTRO);
    app_state = RICH4_SCENE_INTRO;
    settings.transition_active = 1;
    context = context_for(
        RICH4_SCENE_INTRO, &session, &settings, &options,
        &machine, &app_state);
    assert(rich4_app_scene_cancel(&context) ==
           RICH4_APP_SCENE_CANCEL_SWITCH);
    assert(app_state == RICH4_SCENE_NEW_GAME);
    assert(!settings.transition_active);

    rich4_scene_machine_init(&machine, RICH4_SCENE_NEW_GAME);
    app_state = RICH4_SCENE_NEW_GAME;
    settings.dropdown_row = 2;
    context = context_for(
        RICH4_SCENE_NEW_GAME, &session, &settings, &options,
        &machine, &app_state);
    assert(rich4_app_scene_cancel(&context) ==
           RICH4_APP_SCENE_CANCEL_STAY);
    assert(settings.dropdown_row < 0);
    assert(app_state == RICH4_SCENE_NEW_GAME);

    rich4_scene_machine_init(&machine, RICH4_SCENE_OPTIONS);
    machine.scenes[RICH4_SCENE_OPTIONS].return_scene = RICH4_SCENE_MENU;
    app_state = RICH4_SCENE_OPTIONS;
    options.active = 1;
    options.return_app_state = RICH4_SCENE_MENU;
    options.saved.game_speed = 2;
    options.draft.game_speed = 0;
    context = context_for(
        RICH4_SCENE_OPTIONS, &session, &settings, &options,
        &machine, &app_state);
    assert(rich4_app_scene_cancel(&context) ==
           RICH4_APP_SCENE_CANCEL_SWITCH);
    assert(!options.active && options.draft.game_speed == 2);
    assert(app_state == RICH4_SCENE_MENU);

    puts("app scene input flow: ok");
    return 0;
}
