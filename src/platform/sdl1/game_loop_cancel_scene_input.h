#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_SCENE_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_CANCEL_SCENE_INPUT_H

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "app/scene.h"
#include "platform/sdl1/runtime_options_types.h"

/* R10.1d: scene-level virtual-cancel transitions moved out of
 * runtime_main.c. Modal cancellation remains in the earlier cancel modules;
 * this seam owns only scene return/exit state and its cleanup timing. */
typedef struct GameCancelSceneContext {
    Rich4SceneId app_state;
    GameSession *session;
    NewGameSettings *settings;
    Rich4OptionsPanelState *options_panel;
    Rich4SceneMachine *scene_machine;
    Rich4SceneId *app_state_out;
    Rich4Config *config;
    const char *music_dir;
    Rich4Audio *audio;
    int *running;
} GameCancelSceneContext;

/* Returns non-zero when the current scene consumed virtual cancel. */
int game_handle_cancel_scene_input(GameCancelSceneContext *context);

#endif
