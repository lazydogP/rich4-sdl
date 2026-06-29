/* R10.1d: early virtual-confirm modal branches moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_confirm_modals.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "app/god_money_flow.h"
#include "app/monthly_finance_flow.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/game_loop_turns.h"

int game_handle_confirm_modals(GameConfirmModalsContext *context)
{
    if (context->app_state == RICH4_SCENE_GAME && context->session->presentation.property.insurance_active) {
        if (!context->cursor->confirm_pointer_pressed)
            (void)game_handle_insurance_charge_input(context->session, context->assets, SDL_BUTTON_LEFT,
                -1, -1, rich4_sdl1_ticks());
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_monthly_finance_release(context->session)) {
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_presentation_god_owns_frame(&context->session->presentation)) {
        rich4_god_money_stop(context->session);
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED) {
        (void)game_save_load_panel_confirm(
            context->session, context->assets, context->settings,
            context->audio);
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_sale_interaction_is_active(context->session)) {
        game_sale_panel_mouse_release(
            context->session, context->assets, context->settings,
            context->audio, SDL_BUTTON_LEFT,
            context->cursor->x, context->cursor->y);
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_stock_interaction_is_active(context->session)) {
        game_stock_panel_mouse_release(
            context->stock_panel, context->session, context->assets,
            context->settings, context->audio, SDL_BUTTON_LEFT,
            context->cursor->x, context->cursor->y);
        context->cursor->confirm_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->small_map_panel->active) {
        context->cursor->confirm_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.special_event.active) {
        if (!context->cursor->confirm_pointer_pressed) {
            (void)game_fortune_choice_press(context->session,context->assets,
                SDL_BUTTON_LEFT,context->cursor->x,context->cursor->y);
            (void)game_handle_fortune_choice_input(context->session,context->assets,
                SDL_BUTTON_LEFT,-1,-1,rich4_sdl1_ticks());
        }
        context->cursor->confirm_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.facility_roulette.active) {
        game_stop_facility_roulette(
            context->session, context->audio, rich4_sdl1_ticks());
        context->cursor->confirm_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.ending.active) {
        (void)game_save_load_panel_open_after_ending(
            context->session, context->assets);
        context->cursor->confirm_pressed = 0;
        return 1;
    }
    return 0;
}
