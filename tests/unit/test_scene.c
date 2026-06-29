#include <assert.h>
#include <stdio.h>

#include "app/scene.h"
#include "app/scene_frame_flow.h"

typedef struct SceneFrameProbe {
    int game;
    int intro;
    int new_game;
    int fallback;
} SceneFrameProbe;

static void mark_game(void *opaque) { ((SceneFrameProbe *)opaque)->game++; }
static void mark_intro(void *opaque) { ((SceneFrameProbe *)opaque)->intro++; }
static void mark_new_game(void *opaque)
{
    ((SceneFrameProbe *)opaque)->new_game++;
}
static void mark_fallback(void *opaque)
{
    ((SceneFrameProbe *)opaque)->fallback++;
}

static void send(Rich4SceneMachine *machine, Rich4SceneCommandType type)
{
    Rich4SceneCommand command = { type, RICH4_SCENE_NONE };
    rich4_scene_machine_handle(machine, &command);
}

int main(void)
{
    Rich4SceneMachine machine;
    Rich4SceneId scene;
    Rich4SceneResult result;
    Rich4AppSceneFrameServices services;
    SceneFrameProbe probe = { 0, 0, 0, 0 };
    rich4_scene_machine_init(&machine, RICH4_SCENE_MENU);
    assert(machine.current == RICH4_SCENE_MENU);
    send(&machine, RICH4_SCENE_COMMAND_NEW_GAME);
    assert(machine.current == RICH4_SCENE_NEW_GAME);
    send(&machine, RICH4_SCENE_COMMAND_START_GAME);
    assert(machine.current == RICH4_SCENE_INTRO);
    send(&machine, RICH4_SCENE_COMMAND_INTRO_FINISHED);
    assert(machine.current == RICH4_SCENE_GAME);
    send(&machine, RICH4_SCENE_COMMAND_OPEN_OPTIONS);
    assert(machine.current == RICH4_SCENE_OPTIONS);
    send(&machine, RICH4_SCENE_COMMAND_BACK);
    assert(machine.current == RICH4_SCENE_GAME);
    assert(machine.current == RICH4_SCENE_GAME);
    assert(rich4_scene_machine_update(&machine, 123).type ==
           RICH4_SCENE_RESULT_STAY);
    result = rich4_scene_machine_transition(&machine, RICH4_SCENE_NONE);
    assert(result.type == RICH4_SCENE_RESULT_STAY);
    assert(machine.current == RICH4_SCENE_GAME);
    send(&machine, RICH4_SCENE_COMMAND_BACK);
    assert(machine.current == RICH4_SCENE_GAME);
    assert(rich4_scene_machine_transition(
               &machine, RICH4_SCENE_NEW_GAME).type ==
           RICH4_SCENE_RESULT_SWITCH);
    assert(machine.current == RICH4_SCENE_NEW_GAME);
    send(&machine, RICH4_SCENE_COMMAND_BACK);
    assert(machine.current == RICH4_SCENE_MENU);
    send(&machine, RICH4_SCENE_COMMAND_QUIT);
    assert(!machine.running);
    assert(rich4_scene_machine_handle(&machine, NULL).type ==
           RICH4_SCENE_RESULT_QUIT);
    rich4_scene_machine_init(&machine, RICH4_SCENE_MENU);
    assert(rich4_scene_machine_transition(&machine, RICH4_SCENE_NEW_GAME).type ==
           RICH4_SCENE_RESULT_SWITCH);
    assert(machine.current == RICH4_SCENE_NEW_GAME);
    assert(rich4_scene_machine_transition(&machine, RICH4_SCENE_INTRO).type ==
           RICH4_SCENE_RESULT_SWITCH);
    assert(machine.current == RICH4_SCENE_INTRO);
    services.game_context = &probe;
    services.intro_context = &probe;
    services.new_game_context = &probe;
    services.fallback_menu_context = &probe;
    services.fallback_options_menu_context = &probe;
    services.fallback_options_game_context = &probe;
    services.game = mark_game;
    services.intro = mark_intro;
    services.new_game = mark_new_game;
    services.fallback_menu = mark_fallback;
    services.fallback_options_menu = mark_fallback;
    services.fallback_options_game = mark_fallback;
    rich4_scene_machine_init(&machine, RICH4_SCENE_MENU);
    assert(rich4_app_scene_frame_update(
               &machine, &machine.current, 200, &services, &scene));
    assert(scene == RICH4_SCENE_MENU && probe.fallback == 1);
    send(&machine, RICH4_SCENE_COMMAND_NEW_GAME);
    assert(rich4_app_scene_frame_update(
               &machine, &machine.current, 201, &services, &scene));
    assert(scene == RICH4_SCENE_NEW_GAME && probe.new_game == 1);
    send(&machine, RICH4_SCENE_COMMAND_START_GAME);
    assert(rich4_app_scene_frame_update(
               &machine, &machine.current, 202, &services, &scene));
    assert(scene == RICH4_SCENE_INTRO && probe.intro == 1);
    send(&machine, RICH4_SCENE_COMMAND_INTRO_FINISHED);
    assert(rich4_app_scene_frame_update(
               &machine, &machine.current, 203, &services, &scene));
    assert(scene == RICH4_SCENE_GAME && probe.game == 1);
    assert(rich4_app_scene_frame_update(
               NULL, &(Rich4SceneId){ RICH4_SCENE_OPTIONS }, 204,
               &services, &scene));
    assert(scene == RICH4_SCENE_OPTIONS && probe.fallback == 2);
    puts("scene lifecycle and transitions: ok");
    return 0;
}
