#ifndef RICH4_PLATFORM_SDL1_RUNTIME_INPUT_CONFIG_H
#define RICH4_PLATFORM_SDL1_RUNTIME_INPUT_CONFIG_H

#include "platform/sdl1/input/input_config_sdl.h"

/* R10.1d: input-config defaults/file loading moved out of runtime_main.c. */
void game_load_runtime_input_config(Rich4InputConfig *input, const char *path);

#endif
