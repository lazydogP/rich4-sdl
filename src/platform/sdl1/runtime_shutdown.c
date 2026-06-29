/* R10.1d: normal-exit resource and subsystem cleanup moved verbatim out of
 * runtime_main.c. Error-path cleanup remains local until its ownership
 * differences are isolated. */

#include "platform/sdl1/game_loop_preview_toolbar_input.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/runtime_shutdown.h"
#include "platform/sdl1/text/runtime_text.h"

void game_shutdown_runtime(RuntimeShutdownContext *context)
{
    game_close_help_panel(context->ui->help_panel);
    game_close_small_map_panel(context->ui->small_map_panel);
    game_destroy_stock_panel(context->ui->stock_panel);
    if (context->assets->cursor_free_frame != NULL)
        SDL_FreeSurface(context->assets->cursor_free_frame);
    SDL_FreeSurface(context->assets->background);
    reset_game_dice_cache(context->ui->render_cache);
    reset_game_property_flash_cache(context->ui->render_cache);
    SDL_ShowCursor(SDL_ENABLE);
    game_free_preview_assets(context->assets->game_preview_assets);
    game_free_cursor_assets(context->assets->cursor_assets);
    free_menu_assets(context->assets->menu_assets);
    if (context->services->joystick != NULL)
        SDL_JoystickClose(context->services->joystick);
    rich4_audio_shutdown(context->services->audio);
    rich4_sdl_text_shutdown();
    rich4_sdl1_destroy_window(context->services->screen);
    SDL_Quit();
    rich4_resource_store_destroy(context->services->resource_store);
}
