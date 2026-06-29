/* R10.1d: complete physical mouse-button-down dispatch moved verbatim out
 * of runtime_main.c. Pointer normalization still happens before this call. */

#include <SDL/SDL.h>

#include "platform/sdl1/game_loop_mouse_button_down.h"
#include "platform/sdl1/game_loop_mouse_button_down_input.h"

#include "app/map_input_flow.h"
#include "app/options_flow.h"
#include "app/magic_house_flow.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_preview_toolbar_input.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/render/scene_new_game_setup_render.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/runtime_render_api.h"
#include "view/view_constants.h"

int game_handle_mouse_button_down_input(
    GameMouseButtonDownInputContext *context)
{
    if (context->app_state && *context->app_state == RICH4_SCENE_GAME &&
        game_fortune_choice_press(context->session,context->assets,
            context->button,context->cursor->x,context->cursor->y)) {
        if (context->mouse_left_down && context->button == SDL_BUTTON_LEFT)
            *context->mouse_left_down = 1;
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state && *context->app_state == RICH4_SCENE_GAME &&
        game_company_issue_pointer(context->session, context->assets, context->audio,
            0, context->button, context->cursor->x, context->cursor->y)) {
        if (context->mouse_left_down && context->button == SDL_BUTTON_LEFT)
            *context->mouse_left_down = 1;
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state != NULL && *context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.property.insurance_active) return 1;
    if (context->app_state != NULL && *context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.special_place_notice.queue.count > 0) return 1;
    if (context->app_state != NULL && *context->app_state == RICH4_SCENE_GAME &&
        ((context->session->presentation.periodic_finance.active &&
          !context->session->presentation.bankruptcy.active &&
          !context->session->presentation.bankruptcy.awaiting_auction &&
          !context->session->presentation.auction.active) ||
         context->session->presentation.bank_settlement.active)) return 1;
    if (context->app_state != NULL && *context->app_state == RICH4_SCENE_OPTIONS &&
        context->button == SDL_BUTTON_RIGHT &&
        rich4_options_panel_has_pending_action(context->options_panel)) {
        /* 0x45367E declines only on right release. */
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state != NULL && *context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.magic_house.active &&
        context->session->presentation.magic_house.phase != RICH4_MAGIC_HOUSE_TARGET &&
        context->session->presentation.magic_house.condition == RICH4_MAGIC_HOUSE_SURRENDER) {
        if (context->button == SDL_BUTTON_LEFT) {
            Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(context->session);
            int target = rich4_magic_house_surrender_target_at(context->session,
                context->cursor->x, context->cursor->y);
            rich4_magic_house_surrender_choose(context->session, target, rich4_sdl1_ticks(), events);
            game_session_audio_flush(context->session, context->audio);
        }
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state != NULL && *context->app_state == RICH4_SCENE_GAME &&
        context->session != NULL &&
        context->session->presentation.ai_tool.phase != GAME_AI_TOOL_NONE) {
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state != NULL && *context->app_state == RICH4_SCENE_GAME &&
        context->session != NULL &&
        (rich4_presentation_god_owns_frame(&context->session->presentation) ||
         (context->session->presentation.special_reward.active &&
          (context->session->presentation.special_reward.action == 14 ||
           context->session->presentation.special_reward.action == 15)))) {
        /* Blocking god/chest callbacks own the click until release. */
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->button == SDL_BUTTON_LEFT && context->session != NULL)
        rich4_app_input_set_top_button_pressed(context->session, -1);
    if (context->button == SDL_BUTTON_LEFT && context->app_state != NULL &&
        *context->app_state == RICH4_SCENE_GAME)
        rich4_app_input_request_map_info(context->session);
    if (context->button == SDL_BUTTON_LEFT) {
        int cursor_ui_active = rich4_app_input_ui_active(
            *context->app_state, context->settings, context->session,
            context->ai_settings_panel != NULL &&
                context->ai_settings_panel->active);
        if (!cursor_ui_active) {
            context->cursor->confirm_pressed = 0;
            context->cursor->confirm_pointer_pressed = 0;
            return 1;
        }
        if (*context->app_state == RICH4_SCENE_GAME &&
            context->session->presentation.speech.active) {
            /* player_say owns input until button-up. */
            context->cursor->confirm_pressed = 0;
            context->cursor->confirm_pointer_pressed = 0;
            return 1;
        }
        if (*context->app_state == RICH4_SCENE_GAME &&
            context->small_map_panel->active) {
            /* sub_40A801 has no WM_LBUTTONDOWN branch. */
            *context->mouse_left_down = 1;
            context->cursor->confirm_pressed = 0;
            context->cursor->confirm_pointer_pressed = 0;
            return 1;
        }
        *context->mouse_left_down = 1;
        context->cursor->confirm_pressed = 1;
        context->cursor->confirm_pointer_pressed = 1;
        {
            GameMouseButtonDownContext modal = {
                *context->app_state,
                context->assets,
                context->session,
                context->settings,
                context->cursor,
                context->stock_panel,
                context->help_panel,
                context->help_assets,
                context->ai_settings_panel,
                context->tool_panel,
                context->audio
            };
            if (game_handle_mouse_button_down_modal(&modal))
                return 1;
        }
        if (*context->app_state == RICH4_SCENE_OPTIONS) {
            if (context->options_panel->child) {
                Rich4OptionsPanelState *p = context->options_panel;
                p->child_pressed = rich4_options_child_hit(p,
                    context->cursor->x, context->cursor->y);
                if (p->child_pressed == 5) rich4_audio_play_cancel(context->audio);
                else if (p->child_pressed == 6) rich4_audio_play_target_confirm(context->audio);
                else if (p->child_pressed >= 0) rich4_audio_play_ok(context->audio);
                if (p->child == 3 && p->child_pressed >= 101)
                    rich4_options_child_apply(p, context->config, p->child_pressed);
                context->cursor->confirm_pressed = 0;
                context->cursor->confirm_pointer_pressed = 0;
                return 1;
            }
            int control = rich4_options_panel_hit(
                context->options_panel, context->cursor->x,
                context->cursor->y);
            if (rich4_options_panel_has_pending_action(
                    context->options_panel) ||
                (control >= 3 && control <= 5)) {
                if (!rich4_options_panel_has_pending_action(
                        context->options_panel)) {
                    rich4_options_panel_set_pressed(
                        context->options_panel, control);
                    /* fcn_004105F4: Effect.mkf[1] on action down. */
                    rich4_audio_play_ok(context->audio);
                }
                /* The action is committed by the matching button-up, not
                 * by the generic per-frame confirm pass. */
                context->cursor->confirm_pressed = 0;
                context->cursor->confirm_pointer_pressed = 0;
                return 1;
            }
            rich4_options_panel_set_hover(context->options_panel, control);
        } else if (*context->app_state == RICH4_SCENE_GAME &&
                   !context->session->presentation.intro.active &&
                   !context->session->presentation.route_interaction.active &&
                   !context->session->presentation.confinement_visit.active &&
                   !context->session->presentation.lottery.active &&
                   !context->session->presentation.point_minigame.active &&
                   !context->session->presentation.shop_visit.active &&
                   !context->session->presentation.facility_roulette.active &&
                   !context->session->presentation.company_issue.active &&
                   !context->session->presentation.magic_house.active &&
                   !context->session->presentation.auction.active &&
                   !context->session->presentation.special_event.active &&
                   context->session->presentation.tool_animation_count == 0 &&
                   !context->session->presentation.tool_placement_animation.active &&
                   !context->session->state.tool_robot.active &&
                   context->card_panel->phase == CARD_PANEL_CLOSED &&
                   context->tool_panel->phase == TOOL_PANEL_CLOSED) {
            int status_action = handle_game_dice_status_mouse_down(
                context->session, context->assets, context->cursor->x,
                context->cursor->y, rich4_sdl1_ticks());
            if (status_action) {
                if (status_action == 1)
                    rich4_audio_play_select(context->audio);
                else if (status_action == 2)
                    rich4_audio_play_ok(context->audio);
                context->cursor->confirm_pressed = 0;
            } else {
                GamePreviewToolbarMouseDownContext toolbar = {
                    context->config,
                    context->sdl_config,
                    context->assets,
                    context->session,
                    context->options_panel,
                    context->scene_machine,
                    context->app_state,
                    context->help_panel,
                    context->small_map_panel,
                    context->ai_settings_panel,
                    context->card_panel,
                    context->tool_panel,
                    context->stock_panel,
                    context->cursor,
                    context->audio
                };
                if (!game_handle_preview_toolbar_mouse_down(&toolbar) &&
                    !game_handle_preview_minimap_rotate_mouse_down(&toolbar) &&
                    rich4_app_begin_minimap_drag(
                        context->session, context->cursor->x,
                        context->cursor->y, context->config->view)) {
                    context->cursor->confirm_pressed = 0;
                }
            }
        } else if (*context->app_state == RICH4_SCENE_NEW_GAME) {
            rich4_new_game_apply_pointer_down(
                context->settings, context->cursor->x, context->cursor->y);
            /* Physical setup clicks commit on button-up, one event at a time. */
            context->cursor->confirm_pressed = 0;
            context->cursor->confirm_pointer_pressed = 0;
        }
    } else if (context->button == SDL_BUTTON_RIGHT) {
        /* 0x41DDA9 closes the AI draft on WM_RBUTTONUP only. */
        if (*context->app_state == RICH4_SCENE_GAME &&
            context->ai_settings_panel->active) return 1;
        game_handle_mouse_button_down_right_cancel(
            *context->app_state, context->settings, context->session,
            context->small_map_panel,
            context->help_panel, context->ai_settings_panel,
            context->cursor, context->button);
    }
    return 0;
}
