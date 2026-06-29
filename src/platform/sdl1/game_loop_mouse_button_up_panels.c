/* R10.1d: stock/help panel button-up consumption moved verbatim out of
 * runtime_main.c. The caller retains small-map ordering and later session
 * state cleanup. */

#include <stdio.h>

#include "platform/sdl1/game_loop_mouse_button_up_panels.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/game_hud_render.h"

int game_handle_mouse_button_up_panels(
    GameMouseButtonUpPanelsContext *context)
{
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED &&
        (context->button == SDL_BUTTON_LEFT ||
         context->button == SDL_BUTTON_RIGHT)) {
        if (context->button == SDL_BUTTON_LEFT)
            *context->mouse_left_down = 0;
        (void)game_save_load_panel_mouse_release(
            context->session, context->assets, context->settings,
            context->audio, context->button,
            context->cursor_x, context->cursor_y);
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_sale_interaction_is_active(context->session) &&
        (context->button == SDL_BUTTON_LEFT ||
         context->button == SDL_BUTTON_RIGHT)) {
        if (context->button == SDL_BUTTON_LEFT)
            *context->mouse_left_down = 0;
        game_sale_panel_mouse_release(
            context->session, context->assets, context->settings,
            context->audio, context->button,
            context->cursor_x, context->cursor_y);
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_stock_interaction_is_active(context->session) &&
        (context->button == SDL_BUTTON_LEFT ||
         context->button == SDL_BUTTON_RIGHT)) {
        if (context->button == SDL_BUTTON_LEFT)
            *context->mouse_left_down = 0;
        game_stock_panel_mouse_release(
            context->stock_panel, context->session, context->assets,
            context->settings, context->audio, context->button,
            context->cursor_x, context->cursor_y);
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->button == SDL_BUTTON_LEFT &&
        context->help_panel->button_pressed &&
        !context->help_panel->active) {
        *context->mouse_left_down = 0;
        context->help_panel->button_pressed = 0;
        rich4_app_input_set_top_button_hover(context->session, -1);
        draw_game_top_panel_normal(
            context->capture_source, context->assets);
        if (!game_open_help_panel(
                context->help_panel, context->help_assets,
                context->capture_source))
            fprintf(stderr,
                    "warning: cannot open original Help.mkf panel\n");
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    return 0;
}
