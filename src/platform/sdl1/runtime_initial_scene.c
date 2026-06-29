/* R10.1d: initial runtime state and first-frame presentation moved verbatim
 * out of runtime_main.c. */

#include <stdio.h>
#include "app/new_game_setup_flow.h"
#include "app/card_panel_flow.h"
#include "app/game_session.h"
#include "app/game_session_runtime_init.h"
#include "app/options_flow.h"
#include "app/tool_panel_flow.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_initial_scene.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_asset_api.h"
#include "view/view_constants.h"

int game_prepare_initial_scene(RuntimeInitialSceneContext *context)
{
    SDL_Surface *screen;
    SDL_Surface **background;
    SDL_Surface **cursor_free_frame;
    DrawSelection *selection;
    VirtualCursor *cursor;
    NewGameSettings *new_game_settings;
    GameSession *game_session;
    const SdlOptions *options;
    const Rich4Config *rich4_config;
    Rich4Audio *audio;
    Rich4Mkf *mkf;
    MenuAssets *menu_assets;
    CursorAssets *cursor_assets;

    if (context == NULL) return 0;
    screen = context->screen;
    background = context->background;
    cursor_free_frame = context->cursor_free_frame;
    selection = context->selection;
    cursor = context->cursor;
    new_game_settings = context->new_game_settings;
    game_session = context->game_session;
    options = context->options;
    rich4_config = context->rich4_config;
    audio = context->audio;
    mkf = context->mkf;
    menu_assets = context->menu_assets;
    cursor_assets = context->cursor_assets;

    cursor->x = SCREEN_W / 2;
    cursor->y = SCREEN_H / 2;
    cursor->confirm_pressed = 0;
    cursor->confirm_pointer_pressed = 0;
    cursor->cancel_pressed = 0;
    rich4_new_game_settings_init(new_game_settings, 0);
    rich4_app_reset_session(game_session);
    rich4_card_panel_reset(&game_session->presentation.card_panel);
    rich4_options_panel_reset(&game_session->presentation.options);
    rich4_tool_panel_reset(&game_session->presentation.tool_panel);

    /* main.c invokes literal START.AVI once here, before the first menu
     * frame. It is deliberately not tied to the new-game Start button. */
    if (!play_start_avi_once(options->data_dir))
        printf("Start.avi unavailable or AVI playback backend rejected it\n");
    if (rich4_config->music != 0)
        start_menu_music(audio, options->music_dir);

    fill_background(screen);
    selection->has_resource_index = options->has_resource_index;
    selection->resource_index = options->resource_index;
    selection->chunk_index = options->has_chunk_index
        ? options->chunk_index : 0;
    if (menu_assets->available) {
        if (!draw_resource_menu(screen, menu_assets, cursor)) {
            fprintf(stderr,
                    "warning: cannot draw original menu assets; showing clean frame\n");
            fill_background(screen);
        }
    } else if (!draw_spr_smp(screen, mkf, selection)) {
        fprintf(stderr,
                "warning: no drawable SPR/SMP chunk found; showing test gradient\n");
    }

    *background = SDL_CreateRGBSurface(
        SDL_SWSURFACE, SCREEN_W, SCREEN_H,
        screen->format->BitsPerPixel, screen->format->Rmask,
        screen->format->Gmask, screen->format->Bmask,
        screen->format->Amask);
    if (*background == NULL) {
        fprintf(stderr, "error: cannot create background surface: %s\n",
                SDL_GetError());
        return 0;
    }
    SDL_BlitSurface(screen, NULL, *background, NULL);
    *cursor_free_frame = create_game_work_surface(screen);
    if (*cursor_free_frame != NULL)
        SDL_BlitSurface(screen, NULL, *cursor_free_frame, NULL);
    (void)game_draw_resource_cursor(screen, cursor_assets, cursor);
    rich4_sdl1_present(screen);
    return 1;
}
