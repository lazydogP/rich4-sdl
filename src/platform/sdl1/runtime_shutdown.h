#ifndef RICH4_PLATFORM_SDL1_RUNTIME_SHUTDOWN_H
#define RICH4_PLATFORM_SDL1_RUNTIME_SHUTDOWN_H

#include <SDL/SDL.h>

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/render/game_render_cache.h"
#include "resource/resource_store.h"

/* Normal-exit cleanup receives three narrow borrowed groups and releases each
 * resource in the original order; no group owns the surrounding runtime. */
typedef struct RuntimeShutdownUiContext {
    HelpPanelState *help_panel;
    SmallMapPanelState *small_map_panel;
    StockPanelState *stock_panel;
    GameRenderCache *render_cache;
} RuntimeShutdownUiContext;

typedef struct RuntimeShutdownAssetContext {
    SDL_Surface *cursor_free_frame;
    SDL_Surface *background;
    MenuAssets *menu_assets;
    GamePreviewAssets *game_preview_assets;
    CursorAssets *cursor_assets;
} RuntimeShutdownAssetContext;

typedef struct RuntimeShutdownServiceContext {
    SDL_Joystick *joystick;
    Rich4Audio *audio;
    SDL_Surface *screen;
    ResourceStore *resource_store;
} RuntimeShutdownServiceContext;

typedef struct RuntimeShutdownContext {
    RuntimeShutdownUiContext *ui;
    RuntimeShutdownAssetContext *assets;
    RuntimeShutdownServiceContext *services;
} RuntimeShutdownContext;

void game_shutdown_runtime(RuntimeShutdownContext *context);

#endif
