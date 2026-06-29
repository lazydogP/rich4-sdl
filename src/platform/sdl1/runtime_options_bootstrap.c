/* R10.1d: CLI/path startup handling moved verbatim out of runtime_main.c. */

#include <stdio.h>

#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_options_bootstrap.h"

int game_parse_runtime_options(int argc, char **argv, SdlOptions *options)
{
    if (!rich4_sdl_parse_options(argc, argv, options)) {
        rich4_sdl_print_usage(stderr);
        return 0;
    }
    rich4_sdl_resolve_default_paths(options);
    return 1;
}
