/* R10.1d: modal left-button-down dispatch moved verbatim out of
 * runtime_main.c. The caller retains the physical pointer gates and the
 * non-modal toolbar/new-game branches. */

#include <SDL/SDL.h>

#include "app/ai_settings_flow.h"
#include "app/bank_interaction_flow.h"
#include "app/map_input_flow.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "app/shop_flow.h"
#include "app/route_flow.h"
#include "platform/sdl1/game_loop_mouse_button_down.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_input.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/game_route_api.h"

int game_handle_mouse_button_down_modal(
    GameMouseButtonDownContext *context)
{
    Rich4SceneId app_state = context->app_state;
    GameSession *session = context->session;
    VirtualCursor *cursor = context->cursor;
    StockPanelState *stock_panel = context->stock_panel;
    HelpPanelState *help_panel = context->help_panel;
    Rich4AiSettingsState *ai_settings_panel = context->ai_settings_panel;
    ToolPanelState *tool_panel = context->tool_panel;
    Rich4Audio *audio = context->audio;

    if (app_state == RICH4_SCENE_GAME &&
        session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED) {
        game_save_load_panel_mouse_down(
            session, context->assets, context->settings, audio,
            cursor->x, cursor->y);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
        rich4_app_stock_interaction_is_active(session)) {
        /* 0x42AD6C applies sub_451B9E to top controls on WM_LBUTTONDOWN;
         * dispatch still waits for button-up. */
        game_stock_panel_mouse_down(
            stock_panel, session, context->assets,
            cursor->x, cursor->y);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               rich4_app_sale_interaction_is_active(session)) {
        game_sale_panel_mouse_down(
            session, context->assets, cursor->x, cursor->y);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (help_panel->active) {
        game_help_panel_mouse_down(
            help_panel, context->help_assets, audio, cursor->x, cursor->y);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               session->presentation.lottery.active) {
        /* sub_42F7FC owns WM_LBUTTONDOWN for the lifetime of the modal
         * callback. Resolve it here so it cannot also reach the underlying
         * minimap or toolbar. */
        game_lottery_left_button_down(
            session, audio, cursor->x, cursor->y, rich4_sdl1_ticks());
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               session->presentation.point_minigame.active) {
        game_point_minigame_click(
            session, context->assets, audio, cursor->x, cursor->y,
            rich4_sdl1_ticks());
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               session->presentation.shop_visit.active) {
        Uint32 shop_now = rich4_sdl1_ticks();
        if (!game_shop_skip_message(session, audio, shop_now)) {
            Rich4AudioEventQueue *events =
                rich4_app_session_audio_borrow(session);
            rich4_shop_pointer_down(
                session, cursor->x, cursor->y, shop_now, events);
            game_session_audio_flush(session, audio);
        }
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               ai_settings_panel->active) {
        int control = rich4_ai_settings_hit(cursor->x, cursor->y);
        rich4_ai_settings_set_hover(ai_settings_panel, control);
        ai_settings_panel->pressed_control = control;
        /* 0x41DE95 changes rows on press; other buttons commit on release. */
        if (control >= 100 || control == 11 || control == 12)
            rich4_ai_settings_apply(
                ai_settings_panel, session, control, cursor->x);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               session->presentation.player_info.active) {
        rich4_app_player_info_press(session, cursor->x, cursor->y);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
        session->presentation.intro.active) {
        cursor->confirm_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               session->presentation.route_interaction.active &&
               (session->presentation.route_interaction.accept.type ==
                    GAME_COMMAND_BANK_WITHDRAW ||
                session->presentation.route_interaction.accept.type ==
                    GAME_COMMAND_BANK_APPLY_LOAN)) {
        int control = game_bank_interaction_hit(
            session, context->assets, cursor->x, cursor->y);
        if (rich4_app_bank_interaction_control_enabled(session, control)) {
            GameCommand ignored;
            rich4_app_bank_interaction_set_pressed_control(session, control);
            if (session->presentation.route_interaction.accept.type ==
                GAME_COMMAND_BANK_WITHDRAW)
                game_bank_play_press_sound(audio, control);
            else if (control == 16)
                rich4_audio_play_bank_slider(audio);
            else
                rich4_audio_play_bank_key(audio);
            /* 0x436EF8 changes selector and slider state on button-down.
             * Keypad/exit/confirm actions wait for button-up while their
             * pressed chunk remains visible. */
            if ((session->presentation.route_interaction.accept.type ==
                     GAME_COMMAND_BANK_WITHDRAW &&
                 (control == 0 || control == 1 || control == 3)) ||
                (session->presentation.route_interaction.accept.type ==
                     GAME_COMMAND_BANK_APPLY_LOAN &&
                 rich4_app_bank_interaction_is_amount_entry(session) &&
                 control == 16))
                game_bank_panel_apply_control(
                    session, control, cursor->x, &ignored);
        }
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               (rich4_app_route_interaction_uses_confirm_dialog(session) ||
                rich4_app_route_interaction_is_facility_choice(session))) {
        /* sub_45367E leaves YES/NO selection pending until WM_LBUTTONUP.
         * Do not start purchase speech while this click's release is still
         * outstanding. */
        if (rich4_app_route_interaction_is_facility_choice(session)) {
            int mandatory =
                rich4_app_route_interaction_is_mandatory_facility_build(session);
            int choice = rich4_app_route_interaction_is_facility_research(session)
                ? rich4_app_route_facility_research_choice_at(
                    cursor->x, cursor->y)
                : rich4_app_route_facility_choice_at(cursor->x, cursor->y);
            int choice_count = mandatory ? 5 :
                session->state.facility_level[
                    session->presentation.route_interaction.accept.subject.id];
            rich4_app_route_set_pressed(
                session, choice >= 0 && choice < choice_count ? choice : -1);
            /* Facility choice commits on button-up, like 0x4402D7's
             * WM_LBUTTONUP branch; suppress the per-frame virtual confirm. */
            cursor->confirm_pressed = 0;
        }
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    } else if (app_state == RICH4_SCENE_GAME &&
               tool_panel->phase == TOOL_PANEL_INVENTORY) {
        /* Original toolsProc separates the pressed visual and sound
         * (WM_LBUTTONDOWN) from tool dispatch (WM_LBUTTONUP). */
        game_tool_panel_press_inventory(
            session, tool_panel, cursor->x, cursor->y, audio);
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    return 0;
}

void game_handle_mouse_button_down_right_cancel(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session,
    const SmallMapPanelState *small_map_panel,
    const HelpPanelState *help_panel,
    const Rich4AiSettingsState *ai_settings_panel, VirtualCursor *cursor,
    int button)
{
    if (button != SDL_BUTTON_RIGHT || cursor == NULL)
        return;
    if ((app_state == RICH4_SCENE_GAME && session != NULL &&
         rich4_app_stock_interaction_is_active(session)) ||
        (app_state == RICH4_SCENE_GAME && session != NULL &&
         rich4_app_sale_interaction_is_active(session)) ||
        (small_map_panel != NULL && small_map_panel->active) ||
        (help_panel != NULL && help_panel->active)) {
        /* These callbacks own right-button-up, not button-down. */
        cursor->cancel_pressed = 0;
    } else if (app_state == RICH4_SCENE_GAME && session != NULL &&
               session->presentation.shop_visit.active) {
        /* 0x42D37F exits on WM_RBUTTONUP; leave the flag untouched. */
    } else if (app_state == RICH4_SCENE_GAME && session != NULL &&
               session->presentation.lottery.active) {
        /* sub_42F7FC handles only WM_RBUTTONUP. */
        cursor->cancel_pressed = 0;
    } else if (app_state == RICH4_SCENE_GAME && session != NULL &&
               session->presentation.player_info.active) {
        /* sub_424409 owns the release; retain no cancel action on down. */
        cursor->cancel_pressed = 0;
    } else if (app_state == RICH4_SCENE_GAME && session != NULL &&
               session->presentation.confinement_visit.active) {
        /* 0x43CAAB and 0x43DA27 both defer right-button handling to
         * WM_RBUTTONUP; do not turn the button-down into a virtual cancel. */
        cursor->cancel_pressed = 0;
    } else if (app_state == RICH4_SCENE_MENU) {
        /* The plain menu ignores right click; its load dialog owns cancel. */
        cursor->cancel_pressed = session != NULL &&
            session->presentation.save_load.mode ==
                GAME_SAVE_LOAD_PANEL_LOAD;
    } else if (app_state == RICH4_SCENE_GAME && session != NULL &&
               session->presentation.save_load.mode !=
                   GAME_SAVE_LOAD_PANEL_CLOSED) {
        /* This modal owns the right-button release, which cancels it. */
        cursor->cancel_pressed = 0;
    } else if (app_state == RICH4_SCENE_GAME && session != NULL &&
               (session->presentation.card_panel.phase != CARD_PANEL_CLOSED ||
                session->presentation.tool_panel.phase != TOOL_PANEL_CLOSED ||
                (ai_settings_panel != NULL && ai_settings_panel->active))) {
        /* Card/tool/AI panels are virtual-cancel modals.  Keep the ordinary
         * game scene from treating a right click as a scene-level action, but
         * let these panels receive the cancel on the following frame. */
        cursor->cancel_pressed = 1;
    } else if (app_state == RICH4_SCENE_GAME) {
        cursor->cancel_pressed = 0;
    } else if (rich4_app_input_ui_active(
                   app_state, settings, session,
                   ai_settings_panel != NULL && ai_settings_panel->active)) {
        cursor->cancel_pressed = 1;
    }
}
