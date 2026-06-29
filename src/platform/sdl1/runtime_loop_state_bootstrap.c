/* R10.1d: scene and frame-event initialization moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/runtime_loop_state_bootstrap.h"

void game_init_runtime_loop_state(Rich4SceneMachine *scene_machine,
                                  Rich4AudioEventQueue *audio_events)
{
    rich4_scene_machine_init(scene_machine, RICH4_SCENE_MENU);
    rich4_audio_event_queue_init(audio_events);
}
