/* R10.1d: virtual-cancel priority dispatch moved verbatim out of
 * runtime_main.c. The outer loop still owns the flag lifetime and invokes
 * this seam after hover synchronization. */

#include "platform/sdl1/game_loop_cancel_input.h"

#include "platform/sdl1/game_loop_cancel_modals.h"
#include "platform/sdl1/game_loop_cancel_panels.h"
#include "platform/sdl1/game_loop_cancel_scene_input.h"
#include "platform/sdl1/game_loop_cancel_visits.h"
#include "platform/sdl1/game_loop_confirm_route_interaction.h"

void game_handle_cancel_input(GameCancelInputContext *context)
{
    GameCancelModalsContext cancel_modals;

    if (context == NULL || context->app_state == NULL ||
        context->session == NULL || context->cursor == NULL ||
        !context->cursor->cancel_pressed)
        return;

    /* While the in-game options action dialog is open, Escape/right-click
     * declines that dialog; it must not close the underlying options panel. */
    if (*context->app_state == RICH4_SCENE_OPTIONS &&
        context->options_panel != NULL &&
        rich4_options_panel_has_pending_action(context->options_panel)) {
        rich4_audio_play_cancel(context->audio);
        rich4_options_panel_cancel_action(context->options_panel);
        context->cursor->cancel_pressed = 0;
        return;
    }

    /* sub_43BDE5 has no cancel path: right-click/Escape is swallowed while
     * the auction owns the fullscreen modal. */
    if (*context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.auction.active) {
        context->cursor->cancel_pressed = 0;
        return;
    }

    cancel_modals.app_state = *context->app_state;
    cancel_modals.session = context->session;
    cancel_modals.stock_panel = context->stock_panel;
    cancel_modals.cursor = context->cursor;
    cancel_modals.audio = context->audio;
    if (game_handle_cancel_modals(&cancel_modals)) {
        return;
    }
    if (*context->app_state == RICH4_SCENE_GAME &&
        context->small_map_panel->active) {
        /* Only an actual right-button release closes sub_40A801. */
        context->cursor->cancel_pressed = 0;
        return;
    }
    {
        GameCancelVisitsContext cancel_visits = {
            *context->app_state,
            context->session,
            context->cursor,
            context->audio,
            context->shop_music_restore_pending
        };
        GameCancelPanelsContext cancel_panels = {
            *context->app_state,
            context->session,
            context->card_panel,
            context->tool_panel,
            context->ai_settings_panel,
            context->cursor,
            context->audio
        };
        if (game_handle_cancel_visits(&cancel_visits)) {
            /* Visit modal consumed cancel. */
        } else if (game_handle_cancel_panels(&cancel_panels)) {
            /* Panel modal consumed cancel. */
        } else if (game_handle_cancel_route_interaction(
                       *context->app_state, context->session,
                       context->assets, context->settings,
                       context->cursor, context->audio)) {
            /* Route interaction consumed cancel. */
        } else if (*context->app_state == RICH4_SCENE_GAME) {
            /* The game scene has no global back action. */
            context->cursor->cancel_pressed = 0;
        } else {
            GameCancelSceneContext cancel_scene = {
                *context->app_state,
                context->session,
                context->settings,
                context->options_panel,
                context->scene_machine,
                context->app_state,
                context->config,
                context->music_dir,
                context->audio,
                context->running
            };
            game_handle_cancel_scene_input(&cancel_scene);
        }
    }
}
