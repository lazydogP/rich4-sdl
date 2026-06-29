#ifndef RICH4_APP_SCENE_INPUT_FLOW_H
#define RICH4_APP_SCENE_INPUT_FLOW_H

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "app/scene.h"

/* App-owned scene-level cancel semantics.  The platform adapter supplies
 * audio/music side effects after this function returns. */
typedef enum Rich4AppSceneCancelAction {
    RICH4_APP_SCENE_CANCEL_NONE = 0,
    RICH4_APP_SCENE_CANCEL_STAY,
    RICH4_APP_SCENE_CANCEL_SWITCH,
    RICH4_APP_SCENE_CANCEL_QUIT
} Rich4AppSceneCancelAction;

typedef struct Rich4AppSceneCancelContext {
    Rich4SceneId app_state;
    GameSession *session;
    NewGameSettings *settings;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    Rich4SceneId *app_state_out;
} Rich4AppSceneCancelContext;

Rich4AppSceneCancelAction rich4_app_scene_cancel(
    Rich4AppSceneCancelContext *context);

#endif
