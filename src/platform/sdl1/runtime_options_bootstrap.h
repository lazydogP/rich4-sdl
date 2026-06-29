#ifndef RICH4_PLATFORM_SDL1_RUNTIME_OPTIONS_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_OPTIONS_BOOTSTRAP_H

#include "platform/sdl1/runtime_options_types.h"

/* R10.1d: CLI parsing and default-path resolution moved out of
 * runtime_main.c. */
int game_parse_runtime_options(int argc, char **argv, SdlOptions *options);

#endif
