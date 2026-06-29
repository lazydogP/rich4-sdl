#ifndef RICH4_PLATFORM_SDL1_RUNTIME_WINDOW_BOOTSTRAP_H
#define RICH4_PLATFORM_SDL1_RUNTIME_WINDOW_BOOTSTRAP_H

#include <SDL/SDL.h>

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "resource/resource_store.h"

/* R10.1d: SDL window creation and its partially-initialized failure cleanup
 * moved out of runtime_main.c. Success leaves the window owned by the caller. */
typedef struct RuntimeWindowBootstrapContext {
    SDL_Surface **screen;
    Rich4Audio *audio;
    SDL_Joystick *joystick;
    GamePreviewAssets *game_preview_assets;
    CursorAssets *cursor_assets;
    MenuAssets *menu_assets;
    ResourceStore *resource_store;
} RuntimeWindowBootstrapContext;

int game_open_runtime_window(RuntimeWindowBootstrapContext *context);

#endif
