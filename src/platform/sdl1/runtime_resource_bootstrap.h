#ifndef RICH4_PLATFORM_SDL1_RUNTIME_RESOURCE_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_RESOURCE_BOOTSTRAP_H

#include <stddef.h>

#include "resource/mkf.h"
#include "platform/sdl1/runtime_options_types.h"
#include "resource/resource_store.h"

/* R10.1d: primary resource-store/MKF acquisition moved out of
 * runtime_main.c. */
int game_open_runtime_resources(
    ResourceStore **resource_store, Rich4Mkf **mkf,
    const SdlOptions *options, char *mkf_path, size_t mkf_path_size);

#endif
