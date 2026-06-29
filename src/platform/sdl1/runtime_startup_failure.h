#ifndef RICH4_PLATFORM_SDL1_RUNTIME_STARTUP_FAILURE_H
#define RICH4_PLATFORM_SDL1_RUNTIME_STARTUP_FAILURE_H

#include <SDL/SDL.h>

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "resource/resource_store.h"

/* R10.1d: partial-startup cleanup after initial-scene failure moved out of
 * runtime_main.c. This is intentionally separate from normal shutdown. */
typedef struct RuntimeStartupFailureContext {
    Rich4Audio *audio;
    SDL_Joystick *joystick;
    SDL_Surface *screen;
    GamePreviewAssets *game_preview_assets;
    CursorAssets *cursor_assets;
    MenuAssets *menu_assets;
    ResourceStore *resource_store;
} RuntimeStartupFailureContext;

void game_abort_runtime_startup(RuntimeStartupFailureContext *context);

#endif
