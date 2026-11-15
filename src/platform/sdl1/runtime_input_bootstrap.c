/* R10.1d: initial cursor visibility and joystick discovery moved verbatim
 * out of runtime_main.c. */

#include <stdio.h>

#include "platform/sdl1/runtime_input_bootstrap.h"

void game_init_runtime_input_devices(SDL_Joystick **joystick)
{
    /* media_init.c calls ShowCursor(FALSE) after cursor initialization even
     * when the resource cursor later falls back to a debug shape. */
    SDL_ShowCursor(SDL_DISABLE);
#if !defined(RICH4_PLATFORM_PORTMASTER)
    if (SDL_NumJoysticks() > 0) {
        *joystick = SDL_JoystickOpen(0);
        if (*joystick != NULL) {
            printf("opened joystick: %s\n", SDL_JoystickName(0));
        }
    }
#else
    /* Avoid duplicate actions from raw joystick events and gptokeyb. */
    *joystick = NULL;
#endif
}
