/* R10.1d: non-preview scene frame presentation moved verbatim out of
 * runtime_main.c. Scene transitions remain in the event loop. */

#include "platform/sdl1/game_loop_scene_presentation.h"

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/game_preview_frame.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/render/scene_options_render.h"

void game_draw_scene_fallback_menu(const GameSceneFallbackMenuContext *menu)
{
    if (menu == NULL || menu->screen == NULL || menu->menu_assets == NULL)
        return;
    if (!menu->menu_assets->available || !draw_resource_menu(
            menu->screen, menu->menu_assets, menu->cursor))
        SDL_BlitSurface(menu->background, NULL, menu->screen, NULL);
    if (menu->assets != NULL && menu->session != NULL)
        game_draw_save_load_panel(menu->screen, menu->assets, menu->session);
}

void game_draw_scene_fallback_options_menu(
    const GameSceneFallbackOptionsMenuContext *options)
{
    if (options == NULL || options->screen == NULL || options->assets == NULL ||
        options->menu_assets == NULL || options->options_panel == NULL)
        return;
    if (!options->menu_assets->available || !draw_resource_menu(
            options->screen, options->menu_assets, NULL))
        SDL_BlitSurface(options->background, NULL, options->screen, NULL);
    draw_options_panel(options->screen, options->assets,
                       options->options_panel);
    if (options->options_panel->child == 5) {
        options->options_panel->child = 0;
        game_open_help_panel(options->help_panel, options->help_assets,
                             options->screen);
    }
    game_draw_help_panel(options->screen, options->help_assets, options->help_panel);
}

void game_draw_scene_fallback_options_game(
    const GameSceneFallbackOptionsGameContext *options)
{
    GameSceneFallbackPreviewContext *preview;
    if (options == NULL || options->screen == NULL ||
        options->preview == NULL || options->options_panel == NULL)
        return;
    preview = options->preview;
    if (preview->assets == NULL || preview->config == NULL) return;
    draw_game_preview_adapter(
        options->screen, preview->assets, preview->settings, preview->session,
        preview->render_cache, preview->config->view, preview->config->calendar_view);
    draw_options_panel(options->screen, preview->assets,
                       options->options_panel);
}
