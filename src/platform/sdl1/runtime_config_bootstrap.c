/* R10.1d: runtime config-file reads moved verbatim out of runtime_main.c. */

#include "app/app_constants.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_config_bootstrap.h"

void game_load_runtime_configs(Rich4Config *rich4_config,
                               Rich4SdlConfig *rich4_sdl_config)
{
    rich4_config_read(rich4_config, "RICH4.CFG");
    rich4_sdl_config_read(rich4_sdl_config, RICH4_SDL_CONFIG_PATH);
}
