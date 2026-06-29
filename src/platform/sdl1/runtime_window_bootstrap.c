/* R10.1d: SDL window creation and its failure cleanup moved verbatim out of
 * runtime_main.c. */

#include <stdio.h>

#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_window_bootstrap.h"
#include "view/view_constants.h"

int game_open_runtime_window(RuntimeWindowBootstrapContext *context)
{
    *context->screen = rich4_sdl1_create_window(
        SCREEN_W, SCREEN_H, 16, "rich4-sdl M3 preview");
    if (*context->screen == NULL) {
        fprintf(stderr, "error: SDL_SetVideoMode failed: %s\n",
                SDL_GetError());
        rich4_audio_shutdown(context->audio);
        if (context->joystick != NULL)
            SDL_JoystickClose(context->joystick);
        SDL_Quit();
        game_free_preview_assets(context->game_preview_assets);
        game_free_cursor_assets(context->cursor_assets);
        free_menu_assets(context->menu_assets);
        rich4_resource_store_destroy(context->resource_store);
        return 0;
    }
    return 1;
}
