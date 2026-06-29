#ifndef RICH4_APP_NEW_GAME_SCENE_FLOW_H
#define RICH4_APP_NEW_GAME_SCENE_FLOW_H

#include <stdint.h>

#include "app/new_game_setup_flow.h"
#include "app/scene.h"

typedef void (*Rich4AppSetupAudioCallback)(void *context);
typedef void (*Rich4AppSetupDrawCallback)(void *context, int transition);
typedef void (*Rich4AppSetupTransitionCallback)(void *context);

typedef struct Rich4AppNewGameSetupServices {
    void *audio_context;
    void *draw_context;
    Rich4AppSetupAudioCallback play_ok;
    Rich4AppSetupAudioCallback play_transition;
    Rich4AppSetupDrawCallback draw;
} Rich4AppNewGameSetupServices;

int rich4_app_new_game_setup_frame_update(
    NewGameSettings *settings, int cursor_x, int cursor_y, uint32_t now,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    const Rich4AppNewGameSetupServices *services);

typedef struct Rich4AppNewGameSetupInputResult {
    Rich4NewGameSetupAction action;
    int selected_count;
    int map;
} Rich4AppNewGameSetupInputResult;

/* Apply a normalized setup pointer-up and request scene changes at the app
 * boundary; SDL consumes only the typed action and its trace facts. */
Rich4AppNewGameSetupInputResult rich4_app_new_game_setup_pointer_up(
    NewGameSettings *settings, int cursor_x, int cursor_y,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state);

typedef struct Rich4AppNewGameTransitionServices {
    void *complete_context;
    void *draw_context;
    void *scene_ready_context;
    Rich4AppSetupTransitionCallback complete;
    Rich4AppSetupTransitionCallback draw;
    Rich4AppSetupTransitionCallback scene_ready;
} Rich4AppNewGameTransitionServices;

int rich4_app_new_game_transition_frame_update(
    NewGameSettings *settings, uint32_t now,
    Rich4SetupTransitionInput input, Rich4SceneMachine *scene_machine,
    Rich4SceneId *app_state,
    const Rich4AppNewGameTransitionServices *services);

#endif
