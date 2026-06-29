/* R10.1d: input-config defaults/file loading moved verbatim out of
 * runtime_main.c. */

#include <stdio.h>

#include "platform/sdl1/runtime_input_config.h"

void game_load_runtime_input_config(Rich4InputConfig *input, const char *path)
{
    rich4_input_set_defaults(input);
    if (rich4_input_load_file(input, path)) {
        printf("loaded input config: %s\n", path);
    } else {
        printf("input config not found; using defaults: %s\n", path);
    }
}
