#ifndef RICH4_PLATFORM_SDL1_RUNTIME_CONFIG_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_CONFIG_BOOTSTRAP_H

#include "app/config_types.h"
#include "platform/sdl1/app_config_types.h"

/* R10.1d: runtime config-file reads moved out of runtime_main.c. */
void game_load_runtime_configs(Rich4Config *rich4_config,
                               Rich4SdlConfig *rich4_sdl_config);

#endif
