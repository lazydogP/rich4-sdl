/* R10.1d: early virtual-cancel modal branches moved verbatim out of
 * runtime_main.c. */

#include "platform/sdl1/game_loop_cancel_modals.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "app/god_money_flow.h"
#include "app/monthly_finance_flow.h"
#include "app/magic_house_flow.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/game_loop_turns.h"

int game_handle_cancel_modals(GameCancelModalsContext *context)
{
    if (context->app_state == RICH4_SCENE_GAME &&
        game_handle_insurance_charge_input(context->session, NULL, SDL_BUTTON_RIGHT,
            -1, -1, rich4_sdl1_ticks())) {
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_monthly_finance_release(context->session)) {
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_presentation_god_owns_frame(&context->session->presentation)) {
        rich4_god_money_stop(context->session);
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if ((context->app_state == RICH4_SCENE_GAME ||
         context->app_state == RICH4_SCENE_MENU) &&
        context->session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED) {
        game_save_load_panel_cancel(context->session, context->audio);
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_sale_interaction_is_active(context->session)) {
        rich4_app_sale_interaction_cancel(context->session);
        rich4_audio_play_cancel(context->audio);
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_stock_interaction_is_active(context->session)) {
        game_stock_panel_cancel(context->stock_panel, context->session);
        rich4_audio_play_cancel(context->audio);
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.speech.active) {
        /* player_say consumes the release as a skip; it must not also unwind
         * the pending tool handler. */
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.special_event.active) {
        (void)game_handle_fortune_choice_input(context->session,NULL,
            SDL_BUTTON_RIGHT,-1,-1,rich4_sdl1_ticks());
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.facility_roulette.active) {
        /* 0x43F7C6 has no cancel branch after the wheel opens. */
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.company_issue.active) {
        rich4_audio_play_cancel(context->audio);
        game_finish_company_issue(context->session, 0);
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.magic_house.active) {
        if (context->session->presentation.magic_house.condition != RICH4_MAGIC_HOUSE_SURRENDER)
            game_magic_house_skip_intro(
                context->session, context->audio, rich4_sdl1_ticks());
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.point_minigame.active) {
        /* 0x414858/0x414BBC/0x414FCD have no cancel branch. */
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    return 0;
}
