#include "app/scene.h"

#include <string.h>

Rich4SceneResult rich4_scene_stay(void)
{
    Rich4SceneResult result = { RICH4_SCENE_RESULT_STAY, RICH4_SCENE_NONE };
    return result;
}

Rich4SceneResult rich4_scene_switch(Rich4SceneId next_scene)
{
    Rich4SceneResult result = { RICH4_SCENE_RESULT_SWITCH, next_scene };
    return result;
}

Rich4SceneResult rich4_scene_quit(void)
{
    Rich4SceneResult result = { RICH4_SCENE_RESULT_QUIT, RICH4_SCENE_NONE };
    return result;
}

static void scene_enter(Rich4Scene *scene, Rich4SceneId from)
{
    (void)scene;
    (void)from;
}

static void scene_leave(Rich4Scene *scene, Rich4SceneId to)
{
    (void)scene;
    (void)to;
}

static Rich4SceneResult scene_update(Rich4Scene *scene, uint32_t now_ms)
{
    (void)scene;
    (void)now_ms;
    return rich4_scene_stay();
}

static Rich4SceneResult menu_command(Rich4Scene *scene,
                                     const Rich4SceneCommand *command)
{
    (void)scene;
    if (command == NULL) return rich4_scene_stay();
    if (command->type == RICH4_SCENE_COMMAND_NEW_GAME)
        return rich4_scene_switch(RICH4_SCENE_NEW_GAME);
    if (command->type == RICH4_SCENE_COMMAND_OPEN_OPTIONS)
        return rich4_scene_switch(RICH4_SCENE_OPTIONS);
    if (command->type == RICH4_SCENE_COMMAND_BACK ||
        command->type == RICH4_SCENE_COMMAND_QUIT) return rich4_scene_quit();
    return rich4_scene_stay();
}

static Rich4SceneResult new_game_command(Rich4Scene *scene,
                                         const Rich4SceneCommand *command)
{
    (void)scene;
    if (command == NULL) return rich4_scene_stay();
    if (command->type == RICH4_SCENE_COMMAND_START_GAME)
        return rich4_scene_switch(RICH4_SCENE_INTRO);
    if (command->type == RICH4_SCENE_COMMAND_OPEN_OPTIONS)
        return rich4_scene_switch(RICH4_SCENE_OPTIONS);
    if (command->type == RICH4_SCENE_COMMAND_BACK)
        return rich4_scene_switch(RICH4_SCENE_MENU);
    return rich4_scene_stay();
}

static Rich4SceneResult intro_command(Rich4Scene *scene,
                                      const Rich4SceneCommand *command)
{
    (void)scene;
    if (command == NULL) return rich4_scene_stay();
    if (command->type == RICH4_SCENE_COMMAND_INTRO_FINISHED)
        return rich4_scene_switch(RICH4_SCENE_GAME);
    if (command->type == RICH4_SCENE_COMMAND_BACK)
        return rich4_scene_switch(RICH4_SCENE_NEW_GAME);
    return rich4_scene_stay();
}

static Rich4SceneResult game_command(Rich4Scene *scene,
                                     const Rich4SceneCommand *command)
{
    (void)scene;
    if (command == NULL) return rich4_scene_stay();
    if (command->type == RICH4_SCENE_COMMAND_ENDING_TIMEOUT)
        return rich4_scene_switch(RICH4_SCENE_MENU);
    if (command->type == RICH4_SCENE_COMMAND_OPEN_OPTIONS)
        return rich4_scene_switch(RICH4_SCENE_OPTIONS);
    /* A running game has no scene-level back navigation.  Cancel input is
     * reserved for its active modal/panel handlers. */
    if (command->type == RICH4_SCENE_COMMAND_BACK)
        return rich4_scene_stay();
    return rich4_scene_stay();
}

static Rich4SceneResult options_command(Rich4Scene *scene,
                                        const Rich4SceneCommand *command)
{
    if (command == NULL) return rich4_scene_stay();
    if (command->type == RICH4_SCENE_COMMAND_BACK)
        return rich4_scene_switch(scene->return_scene);
    return rich4_scene_stay();
}

static const Rich4SceneOps menu_ops = {
    scene_enter, menu_command, scene_update, scene_leave
};
static const Rich4SceneOps new_game_ops = {
    scene_enter, new_game_command, scene_update, scene_leave
};
static const Rich4SceneOps intro_ops = {
    scene_enter, intro_command, scene_update, scene_leave
};
static const Rich4SceneOps game_ops = {
    scene_enter, game_command, scene_update, scene_leave
};
static const Rich4SceneOps options_ops = {
    scene_enter, options_command, scene_update, scene_leave
};

static Rich4SceneResult apply_result(Rich4SceneMachine *machine,
                                     Rich4SceneResult result)
{
    Rich4Scene *current;
    Rich4Scene *next;
    if (machine == NULL || !machine->running) return rich4_scene_quit();
    if (result.type == RICH4_SCENE_RESULT_QUIT) {
        machine->running = 0;
        return result;
    }
    if (result.type != RICH4_SCENE_RESULT_SWITCH) return result;
    if (result.next_scene < 0 || result.next_scene >= RICH4_SCENE_COUNT)
        return rich4_scene_stay();
    current = &machine->scenes[machine->current];
    next = &machine->scenes[result.next_scene];
    if (result.next_scene == RICH4_SCENE_OPTIONS)
        next->return_scene = machine->current;
    current->ops->leave(current, result.next_scene);
    machine->current = result.next_scene;
    next->ops->enter(next, current->id);
    return result;
}

void rich4_scene_machine_init(Rich4SceneMachine *machine, Rich4SceneId initial)
{
    static const Rich4SceneOps *ops[RICH4_SCENE_COUNT] = {
        &menu_ops, &options_ops, &new_game_ops, &intro_ops, &game_ops
    };
    int i;
    if (machine == NULL) return;
    memset(machine, 0, sizeof(*machine));
    for (i = 0; i < RICH4_SCENE_COUNT; i++) {
        machine->scenes[i].id = (Rich4SceneId)i;
        machine->scenes[i].ops = ops[i];
        machine->scenes[i].return_scene = RICH4_SCENE_MENU;
    }
    machine->current = initial >= 0 && initial < RICH4_SCENE_COUNT ?
                       initial : RICH4_SCENE_MENU;
    machine->running = 1;
    machine->scenes[machine->current].ops->enter(
        &machine->scenes[machine->current], RICH4_SCENE_NONE);
}

Rich4SceneResult rich4_scene_machine_transition(Rich4SceneMachine *machine,
                                                Rich4SceneId next_scene)
{
    return apply_result(machine, rich4_scene_switch(next_scene));
}

void rich4_scene_machine_request_transition(Rich4SceneMachine *machine,
                                             Rich4SceneId *scene,
                                             Rich4SceneId next_scene)
{
    Rich4SceneResult result;
    if (machine == NULL) return;
    result = rich4_scene_machine_transition(machine, next_scene);
    if (result.type == RICH4_SCENE_RESULT_SWITCH)
        if (scene != NULL) *scene = machine->current;
}

Rich4SceneResult rich4_scene_machine_handle(Rich4SceneMachine *machine,
                                            const Rich4SceneCommand *command)
{
    Rich4Scene *scene;
    if (machine == NULL || !machine->running) return rich4_scene_quit();
    scene = &machine->scenes[machine->current];
    return apply_result(machine, scene->ops->handle_command(scene, command));
}

Rich4SceneResult rich4_scene_machine_update(Rich4SceneMachine *machine,
                                            uint32_t now_ms)
{
    Rich4Scene *scene;
    if (machine == NULL || !machine->running) return rich4_scene_quit();
    scene = &machine->scenes[machine->current];
    return apply_result(machine, scene->ops->update(scene, now_ms));
}
