#ifndef RICH4_APP_SCENE_FRAME_FLOW_H
#define RICH4_APP_SCENE_FRAME_FLOW_H

#include <stdint.h>

#include "app/scene.h"

typedef void (*Rich4AppSceneFrameCallback)(void *context);

typedef struct Rich4AppSceneFrameServices {
    void *game_context;
    void *intro_context;
    void *new_game_context;
    void *fallback_menu_context;
    void *fallback_options_menu_context;
    void *fallback_options_game_context;
    Rich4AppSceneFrameCallback game;
    Rich4AppSceneFrameCallback intro;
    Rich4AppSceneFrameCallback new_game;
    Rich4AppSceneFrameCallback fallback_menu;
    Rich4AppSceneFrameCallback fallback_options_menu;
    Rich4AppSceneFrameCallback fallback_options_game;
} Rich4AppSceneFrameServices;

int rich4_app_scene_frame_update(
    Rich4SceneMachine *machine, const Rich4SceneId *fallback_scene,
    uint32_t now_ms, const Rich4AppSceneFrameServices *services,
    Rich4SceneId *out_scene);

#endif
