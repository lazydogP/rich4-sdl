/* R10.1d: complete SDL_MOUSEBUTTONUP dispatch moved verbatim out of
 * runtime_main.c. Existing panel, modal and visit handlers retain ownership
 * of their individual state transitions. */

#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/runtime.h"

#include "platform/sdl1/game_loop_key_up_modals.h"
#include "platform/sdl1/game_loop_mouse_button_up_game.h"
#include "platform/sdl1/game_loop_mouse_button_up_panels.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_new_game_setup_input.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_preview_toolbar_input.h"
#include "platform/sdl1/game_loop_options_input.h"
#include "app/map_input_flow.h"
#include "app/ai_settings_flow.h"
#include "app/magic_house_flow.h"
#include "app/monthly_finance_flow.h"
#include "platform/sdl1/game_loop_audio_events.h"

int game_handle_mouse_button_up_input(
    GameMouseButtonUpInputContext *context)
{
    GameMouseButtonUpPanelsContext button_up_panels;
    GamePreviewSmallMapButtonUpContext small_map_up;

    if (context == NULL || context->session == NULL ||
        context->cursor == NULL)
        return 0;
    if (context->app_state == RICH4_SCENE_GAME &&
        game_handle_fortune_choice_input(context->session,context->assets,
            context->button,context->cursor->x,context->cursor->y,rich4_sdl1_ticks())) {
        if (context->mouse_left_down) *context->mouse_left_down = 0;
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        game_company_issue_pointer(context->session, context->assets, context->audio,
            2, context->button, context->cursor->x, context->cursor->y)) {
        if (context->mouse_left_down) *context->mouse_left_down = 0;
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.special_place_notice.queue.count > 0) {
        game_handle_mouse_button_up_modals(context->app_state, context->session,
            context->audio, context->button);
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        if (context->mouse_left_down != NULL) *context->mouse_left_down = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        (context->button == SDL_BUTTON_LEFT || context->button == SDL_BUTTON_RIGHT) &&
        rich4_monthly_finance_release(context->session)) {
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        if (context->mouse_left_down != NULL) *context->mouse_left_down = 0;
        return 1;
    }
    if (context->button == SDL_BUTTON_LEFT &&
        rich4_app_release_minimap_rotate(context->session)) {
        if (context->mouse_left_down != NULL) *context->mouse_left_down = 0;
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.ai_settings.active) {
        Rich4AiSettingsState *panel = &context->session->presentation.ai_settings;
        if (context->button == SDL_BUTTON_RIGHT)
            rich4_ai_settings_cancel(panel);
        else if (context->button == SDL_BUTTON_LEFT) {
            if (panel->pressed_control >= 0 && panel->pressed_control <= 10)
                rich4_ai_settings_apply(panel, context->session,
                    panel->pressed_control, context->cursor->x);
            panel->pressed_control = -1;
            if (context->mouse_left_down) *context->mouse_left_down = 0;
        }
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_OPTIONS &&
        context->button == SDL_BUTTON_RIGHT &&
        rich4_options_panel_has_pending_action(context->options_panel)) {
        rich4_audio_play_cancel(context->audio);
        rich4_options_panel_cancel_action(context->options_panel);
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.magic_house.active &&
        context->session->presentation.magic_house.phase != RICH4_MAGIC_HOUSE_TARGET &&
        context->session->presentation.magic_house.condition == RICH4_MAGIC_HOUSE_SURRENDER) {
        if (context->button == SDL_BUTTON_RIGHT) {
            Rich4AudioEventQueue *events = rich4_app_session_audio_borrow(context->session);
            rich4_magic_house_surrender_cancel(context->session, events);
            game_session_audio_flush(context->session, context->audio);
        }
        if (context->mouse_left_down) *context->mouse_left_down = 0;
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        (rich4_presentation_god_owns_frame(&context->session->presentation) ||
         (context->session->presentation.special_reward.active &&
          (context->session->presentation.special_reward.action == 14 ||
           context->session->presentation.special_reward.action == 15)) ||
         context->session->presentation.ai_tool.phase != GAME_AI_TOOL_NONE)) {
        game_handle_mouse_button_up_modals(context->app_state,
            context->session, context->audio, context->button);
        if (context->button == SDL_BUTTON_LEFT &&
            context->mouse_left_down != NULL)
            *context->mouse_left_down = 0;
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }

    if (context->app_state == RICH4_SCENE_GAME &&
        (context->button == SDL_BUTTON_LEFT || context->button == SDL_BUTTON_RIGHT) &&
        game_handle_insurance_charge_input(context->session, context->assets,
            context->button, context->cursor->x, context->cursor->y, rich4_sdl1_ticks())) {
        if (context->mouse_left_down) *context->mouse_left_down = 0;
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->button == SDL_BUTTON_LEFT)
        rich4_app_input_clear_map_info(context->session);

    /* A minimap drag owns one physical left click.  Release it before any
     * modal-specific button-up handler can consume that click. */
    if (context->button == SDL_BUTTON_LEFT)
        rich4_app_end_minimap_drag(context->session);

    button_up_panels.app_state = context->app_state;
    button_up_panels.session = context->session;
    button_up_panels.assets = context->assets;
    button_up_panels.settings = context->settings;
    button_up_panels.stock_panel = context->stock_panel;
    button_up_panels.help_panel = context->help_panel;
    button_up_panels.help_assets = context->help_assets;
    button_up_panels.audio = context->audio;
    button_up_panels.cursor = context->cursor;
    button_up_panels.capture_source = context->capture_source;
    button_up_panels.button = context->button;
    button_up_panels.cursor_x = context->cursor->x;
    button_up_panels.cursor_y = context->cursor->y;
    button_up_panels.mouse_left_down = context->mouse_left_down;
    if (game_handle_mouse_button_up_panels(&button_up_panels))
        return 1;

    small_map_up.panel = context->small_map_panel;
    small_map_up.session = context->session;
    small_map_up.assets = context->assets;
    small_map_up.settings = context->settings;
    small_map_up.capture_source = context->capture_source;
    small_map_up.cursor = context->cursor;
    small_map_up.button = context->button;
    small_map_up.mouse_left_down = context->mouse_left_down;
    if (game_handle_preview_small_map_button_up(&small_map_up))
        return 1;

    if (context->help_panel->active &&
        (context->button == SDL_BUTTON_LEFT ||
         context->button == SDL_BUTTON_RIGHT)) {
        if (context->button == SDL_BUTTON_LEFT) {
            *context->mouse_left_down = 0;
            game_help_panel_mouse_up(
                context->help_panel, context->help_assets,
                context->cursor->x, context->cursor->y);
        } else {
            /* 0x44E546: right-button release uses Effect.mkf[4]. */
            rich4_audio_play_cancel(context->audio);
            game_close_help_panel(context->help_panel);
        }
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }

    game_handle_mouse_button_up_modals(
        context->app_state, context->session, context->audio,
        context->button);
    if (game_tool_panel_finish_inventory_release(
            context->app_state, context->session, context->assets,
            context->settings, context->tool_panel, context->cursor,
            context->audio, context->button)) {
        if (context->button == SDL_BUTTON_LEFT &&
            context->mouse_left_down != NULL)
            *context->mouse_left_down = 0;
        /* The tool panel owns this button-up, just like toolsProc.  The
         * pending left-button action is dispatched by the outer event loop
         * only after this callback returns. */
        return 1;
    }
    if (context->button == SDL_BUTTON_LEFT &&
        context->app_state == RICH4_SCENE_OPTIONS &&
        game_handle_options_input(
            context->options_panel, (Rich4Config *)context->config,
            (Rich4SdlConfig *)context->sdl_config, context->scene_machine,
            context->scene_state, context->session, context->assets,
            context->settings, context->cursor, context->music_dir,
            context->audio, 1)) {
        if (context->mouse_left_down != NULL)
            *context->mouse_left_down = 0;
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_INTRO &&
        (context->button == SDL_BUTTON_LEFT ||
         context->button == SDL_BUTTON_RIGHT)) {
        *context->setup_transition_input =
            RICH4_SETUP_TRANSITION_INPUT_MOUSE_BUTTON_UP;
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
    }
    if (context->button == SDL_BUTTON_RIGHT && context->app_state == RICH4_SCENE_GAME &&
        context->config != NULL && rich4_app_restore_minimap_follow(
            context->session, context->cursor->x, context->cursor->y, context->config->view)) {
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    {
        GameMouseButtonUpGameContext button_up_game = {
            context->app_state,
            context->session,
            context->assets,
            context->settings,
            context->cursor,
            context->audio,
            context->button,
            context->mouse_left_down,
            context->shop_music_restore_pending
        };
        game_handle_mouse_button_up_game(&button_up_game);
    }
    if (context->button == SDL_BUTTON_LEFT &&
        context->app_state == RICH4_SCENE_NEW_GAME) {
        if (rich4_app_input_ui_active(
                context->app_state, context->settings, context->session, 0))
            game_handle_new_game_setup_input(
                context->settings, context->cursor, context->scene_machine,
                context->scene_state, context->config, context->music_dir,
                context->audio);
        context->cursor->confirm_pressed = 0;
        context->cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (context->button == SDL_BUTTON_LEFT &&
        context->app_state == RICH4_SCENE_GAME &&
        context->scene_state != NULL) {
        GamePreviewToolbarMouseUpContext toolbar = {
            context->config,
            context->sdl_config,
            context->session,
            context->assets,
            context->settings,
            context->options_panel,
            context->scene_machine,
            context->scene_state,
            context->help_panel,
            context->help_assets,
            context->small_map_panel,
            context->capture_source,
            context->cursor,
            context->button
        };
        if (game_handle_preview_toolbar_mouse_up(&toolbar))
            return 1;
    }
    return 0;
}
