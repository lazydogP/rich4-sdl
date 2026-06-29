/* R10.1d: initial-scene partial-startup cleanup moved verbatim out of
 * runtime_main.c. Normal-exit cleanup remains in runtime_shutdown.c. */

#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_startup_failure.h"

void game_abort_runtime_startup(RuntimeStartupFailureContext *context)
{
    rich4_audio_shutdown(context->audio);
    if (context->joystick != NULL)
        SDL_JoystickClose(context->joystick);
    rich4_sdl1_destroy_window(context->screen);
    SDL_Quit();
    game_free_preview_assets(context->game_preview_assets);
    game_free_cursor_assets(context->cursor_assets);
    free_menu_assets(context->menu_assets);
    rich4_resource_store_destroy(context->resource_store);
}
