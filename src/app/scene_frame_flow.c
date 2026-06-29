/* R10.1e: application-owned scene selection and frame callback dispatch. */

#include "app/scene_frame_flow.h"

#include <stddef.h>

static void invoke_scene_callback(Rich4AppSceneFrameCallback callback,
                                  void *context)
{
    if (callback != NULL) callback(context);
}

int rich4_app_scene_frame_update(
    Rich4SceneMachine *machine, const Rich4SceneId *fallback_scene,
    uint32_t now_ms, const Rich4AppSceneFrameServices *services,
    Rich4SceneId *out_scene)
{
    Rich4SceneId scene = RICH4_SCENE_NONE;
    if (services == NULL || out_scene == NULL) return 0;
    if (machine != NULL) {
        rich4_scene_machine_update(machine, now_ms);
        scene = machine->current;
    } else if (fallback_scene != NULL) {
        scene = *fallback_scene;
    }
    *out_scene = scene;
    switch (scene) {
    case RICH4_SCENE_GAME:
        invoke_scene_callback(services->game, services->game_context);
        break;
    case RICH4_SCENE_INTRO:
        invoke_scene_callback(services->intro, services->intro_context);
        break;
    case RICH4_SCENE_NEW_GAME:
        invoke_scene_callback(services->new_game, services->new_game_context);
        break;
    case RICH4_SCENE_MENU:
        invoke_scene_callback(
            services->fallback_menu, services->fallback_menu_context);
        break;
    case RICH4_SCENE_OPTIONS:
        if (machine != NULL &&
            machine->scenes[RICH4_SCENE_OPTIONS].return_scene == RICH4_SCENE_GAME)
            invoke_scene_callback(
                services->fallback_options_game,
                services->fallback_options_game_context);
        else
            invoke_scene_callback(
                services->fallback_options_menu,
                services->fallback_options_menu_context);
        break;
    default:
        invoke_scene_callback(
            services->fallback_menu, services->fallback_menu_context);
        break;
    }
    return 1;
}
