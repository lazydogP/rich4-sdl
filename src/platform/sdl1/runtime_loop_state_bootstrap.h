#ifndef RICH4_PLATFORM_SDL1_RUNTIME_LOOP_STATE_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_LOOP_STATE_BOOTSTRAP_H

#include "app/audio_event.h"
#include "app/scene.h"

/* R10.1d: scene and frame-event state initialization moved out of
 * runtime_main.c. */
void game_init_runtime_loop_state(Rich4SceneMachine *scene_machine,
                                  Rich4AudioEventQueue *audio_events);

#endif
