#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_EVENT_POLL_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_EVENT_POLL_H

#include <SDL/SDL.h>

#include "platform/sdl1/game_loop_joy_button_down_input.h"
#include "platform/sdl1/game_loop_key_down_input.h"
#include "platform/sdl1/game_loop_key_up_modals.h"
#include "platform/sdl1/game_loop_mouse_button_down_input.h"
#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/game_loop_pointer_motion.h"

/* SDL remains the event source; typed per-event contexts retain ownership of
 * each feature's borrowed state. The poller itself does not aggregate it. */
typedef struct GameEventPollContext {
    int *running;
    Rich4SceneId *app_state;
    SDL_Surface *screen;
    int *mouse_left_down;
    GamePointerMotionContext *pointer_motion;
    GameMouseButtonDownInputContext *mouse_button_down;
    GameMouseButtonUpInputContext *mouse_button_up;
    GameKeyDownInputContext *key_down;
    GameKeyUpModalsContext *key_up;
    GameJoyButtonDownInputContext *joy_button_down;
} GameEventPollContext;

void game_poll_runtime_events(GameEventPollContext *context);

#endif
