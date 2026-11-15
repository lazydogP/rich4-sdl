/* R10.1d: SDL subsystem initialization and failure cleanup moved verbatim
 * out of runtime_main.c. */

#include <stdio.h>

#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_sdl_bootstrap.h"

int game_init_runtime_sdl(RuntimeSdlBootstrapContext *context)
{
    Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO;
#if !defined(RICH4_PLATFORM_PORTMASTER)
    /* PortMaster's gptokeyb owns the pad and supplies keyboard/mouse events. */
    flags |= SDL_INIT_JOYSTICK;
#endif
    if (SDL_Init(flags) != 0) {
        fprintf(stderr, "error: SDL_Init failed: %s\n", SDL_GetError());
        game_free_preview_assets(context->game_preview_assets);
        game_free_cursor_assets(context->cursor_assets);
        free_menu_assets(context->menu_assets);
        rich4_resource_store_destroy(context->resource_store);
        return 0;
    }
    return 1;
}
