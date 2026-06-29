#ifndef RICH4_APP_SCENE_H
#define RICH4_APP_SCENE_H

#include <stdint.h>

typedef enum Rich4SceneId {
    RICH4_SCENE_MENU = 0,
    RICH4_SCENE_OPTIONS,
    RICH4_SCENE_NEW_GAME,
    RICH4_SCENE_INTRO,
    RICH4_SCENE_GAME,
    RICH4_SCENE_COUNT,
    RICH4_SCENE_NONE = -1
} Rich4SceneId;

typedef enum Rich4SceneCommandType {
    RICH4_SCENE_COMMAND_NONE = 0,
    RICH4_SCENE_COMMAND_NEW_GAME,
    RICH4_SCENE_COMMAND_OPEN_OPTIONS,
    RICH4_SCENE_COMMAND_START_GAME,
    RICH4_SCENE_COMMAND_INTRO_FINISHED,
    RICH4_SCENE_COMMAND_BACK,
    /* The original ending dialog returns transition code 1 after its
     * ten-second idle timer; the game scene then returns to the outer menu. */
    RICH4_SCENE_COMMAND_ENDING_TIMEOUT,
    RICH4_SCENE_COMMAND_QUIT
} Rich4SceneCommandType;

typedef struct Rich4SceneCommand {
    Rich4SceneCommandType type;
    Rich4SceneId return_scene;
} Rich4SceneCommand;

typedef enum Rich4SceneResultType {
    RICH4_SCENE_RESULT_STAY = 0,
    RICH4_SCENE_RESULT_SWITCH,
    RICH4_SCENE_RESULT_QUIT
} Rich4SceneResultType;

typedef struct Rich4SceneResult {
    Rich4SceneResultType type;
    Rich4SceneId next_scene;
} Rich4SceneResult;

typedef struct Rich4Scene Rich4Scene;

typedef struct Rich4SceneOps {
    void (*enter)(Rich4Scene *scene, Rich4SceneId from);
    Rich4SceneResult (*handle_command)(Rich4Scene *scene,
                                       const Rich4SceneCommand *command);
    Rich4SceneResult (*update)(Rich4Scene *scene, uint32_t now_ms);
    void (*leave)(Rich4Scene *scene, Rich4SceneId to);
} Rich4SceneOps;

struct Rich4Scene {
    Rich4SceneId id;
    const Rich4SceneOps *ops;
    Rich4SceneId return_scene;
};

typedef struct Rich4SceneMachine {
    Rich4Scene scenes[RICH4_SCENE_COUNT];
    Rich4SceneId current;
    int running;
} Rich4SceneMachine;

Rich4SceneResult rich4_scene_stay(void);
Rich4SceneResult rich4_scene_switch(Rich4SceneId next_scene);
Rich4SceneResult rich4_scene_quit(void);
void rich4_scene_machine_init(Rich4SceneMachine *machine, Rich4SceneId initial);
Rich4SceneResult rich4_scene_machine_transition(Rich4SceneMachine *machine,
                                                Rich4SceneId next_scene);
void rich4_scene_machine_request_transition(Rich4SceneMachine *machine,
                                             Rich4SceneId *scene,
                                             Rich4SceneId next_scene);
Rich4SceneResult rich4_scene_machine_handle(Rich4SceneMachine *machine,
                                            const Rich4SceneCommand *command);
Rich4SceneResult rich4_scene_machine_update(Rich4SceneMachine *machine,
                                            uint32_t now_ms);

#endif
