/* R10.1d: in-game SDL_MOUSEBUTTONUP cleanup and visit dispatch moved out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_confirm_route_interaction.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_mouse_button_up_game.h"
#include "platform/sdl1/game_loop_input.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "app/bank_interaction_flow.h"
#include "app/auction_flow.h"
#include "app/confinement_flow.h"
#include "app/dice_flow.h"
#include "app/map_input_flow.h"

void game_handle_mouse_button_up_game(
    GameMouseButtonUpGameContext *context)
{
    int released_bank_control;
    int released_shop_control;
    if (context == NULL || context->session == NULL ||
        context->cursor == NULL || context->settings == NULL)
        return;
    if (context->button == SDL_BUTTON_LEFT) {
        int release_player_info =
            context->app_state == RICH4_SCENE_GAME &&
            context->session->presentation.player_info.active;
        released_bank_control = rich4_app_bank_interaction_pressed_control(
            context->session);
        released_shop_control =
            context->session->presentation.shop_visit.pressed;
        if (context->mouse_left_down != NULL)
            *context->mouse_left_down = 0;
        context->settings->pressed_control = -1;
        rich4_app_end_minimap_drag(context->session);
        rich4_dice_release_status_drag(context->session);
        rich4_app_bank_interaction_set_pressed_control(context->session, -1);
        if (context->app_state == RICH4_SCENE_GAME &&
            context->session->presentation.auction.active) {
            int action = context->session->presentation.auction.pressed_action;
            rich4_auction_flow_set_pressed(context->session, -1);
            if (action >= 0) {
                Rich4AudioEventQueue *audio_events =
                    rich4_app_session_audio_borrow(context->session);
                (void)rich4_auction_flow_submit(
                    context->session, (GameAuctionAction)action,
                    rich4_sdl1_ticks(), audio_events);
                game_session_audio_flush(context->session, context->audio);
            }
            context->cursor->confirm_pressed = 0;
            context->cursor->confirm_pointer_pressed = 0;
            return;
        }
        if (release_player_info) {
            rich4_app_player_info_release(context->session);
            /* The original player-info window procedure owns this release;
             * never forward the same click to a covered route/shop modal. */
            return;
        }
        game_handle_route_interaction_mouse_button_up(
            context->app_state, context->session, context->assets,
            context->settings, context->cursor, context->audio,
            context->button);
        if (context->app_state == RICH4_SCENE_GAME &&
            context->session->presentation.shop_visit.active &&
            released_shop_control != RICH4_SHOP_HIT_NONE)
            game_shop_mouse_button_up(
                context->session, context->audio, released_shop_control,
                context->cursor->x, context->cursor->y, context->cursor);
        if (context->app_state == RICH4_SCENE_GAME &&
            released_bank_control >= 0)
            game_bank_interaction_mouse_button_up(
                context->session, context->assets, context->settings,
                released_bank_control, context->cursor->x,
                context->cursor);
    } else if (context->button == SDL_BUTTON_RIGHT &&
               context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.player_info.active) {
        /* 0x424409 closes the modal only on WM_RBUTTONUP. */
        rich4_app_player_info_close(context->session);
        context->cursor->cancel_pressed = 0;
    } else if (context->button == SDL_BUTTON_RIGHT &&
               context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.route_interaction.active &&
               (rich4_app_bank_interaction_is_transfer(context->session) ||
                rich4_app_bank_interaction_is_loan_page(context->session))) {
        GameCommand command;
        /* Bank window procedure sub_435062/sub_436EF8 owns physical
         * WM_RBUTTONUP.  Dispatch it here instead of waiting for the
         * virtual-cancel flag, which the ordinary game scene suppresses. */
        if (rich4_app_bank_interaction_right_button_up(
                context->session, rich4_sdl1_ticks(), &command))
            game_session_apply_command(
                context->session, context->assets, context->settings,
                &command, rich4_sdl1_ticks());
        context->cursor->cancel_pressed = 0;
    } else if (context->button == SDL_BUTTON_RIGHT &&
               context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.confinement_visit.active) {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(context->session);
        /* 0x43CAAB (prison) and 0x43DA27 (hospital) both own WM_RBUTTONUP.
         * The app cancel flow preserves the hospital phase-specific behavior. */
        (void)rich4_confinement_handle_cancel(
            context->session, rich4_sdl1_ticks(), audio_events);
        game_session_audio_flush(context->session, context->audio);
        context->cursor->cancel_pressed = 0;
    } else if (context->button == SDL_BUTTON_RIGHT &&
               context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.shop_visit.active) {
        game_shop_right_button_up(
            context->session, context->audio,
            context->shop_music_restore_pending);
        context->cursor->cancel_pressed = 0;
    } else if (context->button == SDL_BUTTON_RIGHT &&
               context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.lottery.active) {
        game_lottery_right_button_up(
            context->session, context->audio, rich4_sdl1_ticks(),
            context->cursor);
    }
}
