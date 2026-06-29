#ifndef RICH4_PLATFORM_SDL1_RUNTIME_INPUT_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_INPUT_BOOTSTRAP_H

#include <SDL/SDL.h>

/* R10.1d: initial cursor/joystick device setup moved out of runtime_main.c.
 * The main loop retains ownership of the opened joystick handle. */
void game_init_runtime_input_devices(SDL_Joystick **joystick);

#endif
