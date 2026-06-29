/* R10.1d: virtual-confirm priority dispatch moved verbatim out of
 * runtime_main.c. Each handler retains its original state mutation and the
 * outer loop still decides when this pass runs. */

#include "platform/sdl1/game_loop_confirm_input.h"

#include "app/auction_flow.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/game_loop_confirm_confinement.h"
#include "platform/sdl1/game_loop_confirm_magic_house.h"
#include "platform/sdl1/game_loop_confirm_modals.h"
#include "platform/sdl1/game_loop_confirm_panels.h"
#include "platform/sdl1/game_loop_confirm_point_minigame.h"
#include "platform/sdl1/game_loop_confirm_route_interaction.h"
#include "platform/sdl1/game_loop_confirm_shop.h"
#include "platform/sdl1/game_loop_menu_input.h"
#include "platform/sdl1/game_loop_new_game_setup_input.h"
#include "platform/sdl1/game_loop_options_input.h"

void game_handle_confirm_input(GameConfirmInputContext *context)
{
    GameConfirmModalsContext confirm_modals;

    if (context == NULL || context->app_state == NULL ||
        context->session == NULL || context->cursor == NULL ||
        !context->cursor->confirm_pressed)
        return;

    confirm_modals.app_state = *context->app_state;
    confirm_modals.session = context->session;
    confirm_modals.stock_panel = context->stock_panel;
    confirm_modals.small_map_panel = context->small_map_panel;
    confirm_modals.assets = context->assets;
    confirm_modals.settings = context->settings;
    confirm_modals.cursor = context->cursor;
    confirm_modals.audio = context->audio;
    if (game_handle_confirm_modals(&confirm_modals)) {
        /* Early modal consumed confirm. */
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.auction.active) {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(context->session);
        int action = rich4_auction_action_at(
            context->cursor->x, context->cursor->y);
        if (context->cursor->confirm_pointer_pressed) {
            /* Physical WM_LBUTTONDOWN only depresses the control.  The
             * original posts action 0x407 from WM_LBUTTONUP. */
            rich4_auction_flow_set_pressed(context->session, action);
        } else if (action >= 0) {
            (void)rich4_auction_flow_submit(
                context->session, (GameAuctionAction)action,
                rich4_sdl1_ticks(), audio_events);
        }
        game_session_audio_flush(context->session, context->audio);
        context->cursor->confirm_pressed = 0;
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.company_issue.active) {
        GameConfirmCompanyIssueContext company_issue = {
            *context->app_state,
            context->session,
            context->assets,
            context->cursor,
            context->audio
        };
        game_handle_confirm_company_issue(&company_issue);
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.magic_house.active) {
        GameConfirmMagicHouseContext magic_house = {
            *context->app_state,
            context->session,
            context->assets,
            context->cursor,
            context->audio
        };
        game_handle_confirm_magic_house(&magic_house);
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.point_minigame.active) {
        game_handle_confirm_point_minigame(
            *context->app_state, context->session, context->assets,
            context->cursor, context->audio);
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.shop_visit.active) {
        game_handle_confirm_shop(
            *context->app_state, context->session,
            context->cursor, context->audio);
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.lottery.active) {
        /* Lottery selection is handled only by the physical
         * left-button-down branch above. */
        context->cursor->confirm_pressed = 0;
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.confinement_visit.active) {
        game_handle_confirm_confinement(
            *context->app_state, context->session, context->assets,
            context->cursor, context->audio,
            context->host_pointer_sync_needed);
    } else if (game_handle_confirm_panels(
                   *context->app_state, context->session, context->assets,
                   context->settings, context->card_panel,
                   context->tool_panel, context->ai_settings_panel,
                   context->cursor, context->audio)) {
        /* Card, tool or AI panel consumed confirm. */
    } else if (*context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.route_interaction.active) {
        game_handle_confirm_route_interaction(
            *context->app_state, context->session, context->assets,
            context->settings, context->cursor, context->audio,
            context->mouse_left_down);
    } else if (*context->app_state == RICH4_SCENE_MENU) {
        game_handle_menu_input(
            context->menu_assets, context->assets, context->options_panel,
            context->config, context->sdl_config, context->session,
            context->settings, context->scene_machine, context->app_state,
            context->cursor, context->music_dir, context->audio,
            context->running);
    } else if (*context->app_state == RICH4_SCENE_OPTIONS) {
        game_handle_options_input(
            context->options_panel, context->config, context->sdl_config,
            context->scene_machine, context->app_state, context->session,
            context->assets, context->settings, context->cursor,
            context->music_dir, context->audio, 0);
    } else if (*context->app_state == RICH4_SCENE_NEW_GAME) {
        game_handle_new_game_setup_input(
            context->settings, context->cursor, context->scene_machine,
            context->app_state, context->config, context->music_dir,
            context->audio);
    }
}
