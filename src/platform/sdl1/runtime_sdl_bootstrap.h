#ifndef RICH4_PLATFORM_SDL1_RUNTIME_SDL_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_SDL_BOOTSTRAP_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "resource/resource_store.h"

/* R10.1d: SDL subsystem initialization and its pre-audio failure cleanup
 * moved out of runtime_main.c. */
typedef struct RuntimeSdlBootstrapContext {
    GamePreviewAssets *game_preview_assets;
    CursorAssets *cursor_assets;
    MenuAssets *menu_assets;
    ResourceStore *resource_store;
} RuntimeSdlBootstrapContext;

int game_init_runtime_sdl(RuntimeSdlBootstrapContext *context);

#endif
