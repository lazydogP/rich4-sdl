#include "app/input_flow.h"

void rich4_app_input_set_top_button_hover(GameSession *session, int hover)
{
    if (session != NULL) session->presentation.top_button_hover = hover;
}

void rich4_app_input_set_top_button_pressed(GameSession *session, int button)
{
    if (session != NULL) session->presentation.top_button_pressed = button;
}

int rich4_app_input_take_top_button_pressed(GameSession *session)
{
    int button;
    if (session == NULL) return -1;
    button = session->presentation.top_button_pressed;
    session->presentation.top_button_pressed = -1;
    return button;
}

void rich4_app_input_clear_map_info(GameSession *session)
{
    if (session == NULL) return;
    session->presentation.map_info.active = 0;
    session->presentation.map_info.press_pending = 0;
}

void rich4_app_input_request_map_info(GameSession *session)
{
    if (session != NULL) session->presentation.map_info.press_pending = 1;
}

int rich4_app_input_take_map_info_request(GameSession *session)
{
    if (session == NULL || !session->presentation.map_info.press_pending)
        return 0;
    session->presentation.map_info.press_pending = 0;
    return 1;
}

void rich4_app_input_set_map_info(GameSession *session,
                                  int object_id,
                                  int cursor_x,
                                  int cursor_y)
{
    if (session == NULL) return;
    session->presentation.map_info.active = 1;
    session->presentation.map_info.press_pending = 0;
    session->presentation.map_info.object_id = object_id;
    session->presentation.map_info.cursor_x = cursor_x;
    session->presentation.map_info.cursor_y = cursor_y;
}

#include "app/new_game_setup_flow.h"

int rich4_app_input_ui_active(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active)
{
    /* 0x440CAC hides the pointer and consumes release events in its own wait. */
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        session->presentation.special_place_notice.queue.count > 0) return 0;
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        (session->presentation.engineering_attack.active ||
         session->presentation.engineering_attack.pending_return)) return 0;
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        session->presentation.ai_tool.phase != GAME_AI_TOOL_NONE) return 0;
    /* 0x418C55 enables the pointer only for 0x40C912's human-input result;
     * prison/hospital reactions and the 1500 ms notice return zero. */
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        session->presentation.confinement_status.active) return 0;
    if (app_state == RICH4_SCENE_INTRO) return 0;
    if (app_state == RICH4_SCENE_NEW_GAME && settings != NULL &&
        (settings->auto_fill_active || settings->transition_wait_active))
        return 0;
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        ((session->presentation.periodic_finance.active &&
          !session->presentation.bankruptcy.active &&
          !session->presentation.bankruptcy.awaiting_auction &&
          !session->presentation.auction.active) ||
         session->presentation.bank_settlement.active ||
         session->presentation.bank_loan_due.active))
        return 0;
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED)
        return 1;
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        session->presentation.ending.active)
        return 1;
    /* 0x43B093 enables the pointer for human auction bids; 0x43B0E7
     * hides it for AI. The suspended route/intro must not mask this modal. */
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        session->presentation.auction.active) {
        int player = session->presentation.auction.bidding.current_player;
        return session->presentation.auction.stage ==
                   RICH4_AUCTION_PRESENT_BIDDING &&
               player >= 0 && player < session->state.player_count &&
               !session->state.players[player].is_ai &&
               session->presentation.auction.bidding.current_price <=
                   session->state.players[player].cash;
    }
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        (session->presentation.intro.active ||
         (session->flow.turn_flow_active &&
          !session->presentation.route_interaction.active &&
          !session->presentation.confinement_visit.active &&
          !session->presentation.lottery.active &&
          !session->presentation.point_minigame.active &&
          !session->presentation.shop_visit.active &&
          !session->presentation.facility_roulette.active &&
          !session->presentation.company_issue.active &&
          !session->presentation.magic_house.active &&
          !ai_settings_active)))
        return 0;
    return 1;
}

int rich4_app_input_display_visible(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active)
{
    int visible = rich4_app_input_ui_active(
        app_state, settings, session, ai_settings_active);
    if (app_state == RICH4_SCENE_GAME && session != NULL) {
        const GamePresentationState *p = &session->presentation;
        /* 0x440706 -> 0x43F23E keeps the pointer hidden while accepting
         * button/key releases. Settlement can yield to a nested auction. */
        if (rich4_presentation_god_owns_frame(p) && p->god_acquisition.money.active)
            visible = 0;
        else if (!p->bankruptcy.active && !p->bankruptcy.awaiting_auction &&
                 !p->auction.active) {
            /* 0x44090E -> 0x43F7C6 never enables the software pointer.
             * 0x432A18 enables it for magic selection; 0x432FD4 hides it
             * on submission. Mask 13 repeats the condition without hiding
             * it (0x433004). 0x431CAA can open a facility picker. */
            if (p->facility_roulette.active ||
                (p->magic_house.active &&
                 p->magic_house.phase != RICH4_MAGIC_HOUSE_SELECT &&
                 p->magic_house.phase != RICH4_MAGIC_HOUSE_FACILITY_CHOICE &&
                 !(p->magic_house.phase == RICH4_MAGIC_HOUSE_CONDITION &&
                   p->magic_house.return_to_selection)))
                visible = 0;
        }
    }
    if (app_state == RICH4_SCENE_GAME && session != NULL &&
        ((session->presentation.point_minigame.active &&
          (session->presentation.point_minigame.action == 8 ||
           session->presentation.point_minigame.phase ==
               RICH4_POINT_MINIGAME_INTRO)) ||
         (session->presentation.shop_visit.active &&
          session->presentation.shop_visit.phase != RICH4_SHOP_ACTIVE)))
        visible = 0;
    return visible;
}

int rich4_app_input_apply_pointer(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active, VirtualCursor *cursor,
    const Rich4AppCommand *command)
{
    if (cursor == NULL || !rich4_app_input_ui_active(
                              app_state, settings, session, ai_settings_active) ||
        !rich4_app_command_is_valid(command) ||
        (command->type != RICH4_APP_COMMAND_POINTER_MOVE &&
         command->type != RICH4_APP_COMMAND_POINTER_DOWN &&
         command->type != RICH4_APP_COMMAND_POINTER_UP))
        return 0;
    cursor->x = command->x;
    cursor->y = command->y;
    return 1;
}

int rich4_app_input_apply_key(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active, VirtualCursor *cursor,
    Rich4AppKey key, Rich4AppKey confirm_key, Rich4AppKey cancel_key)
{
    if (cursor == NULL || !rich4_app_input_ui_active(
                              app_state, settings, session, ai_settings_active))
        return 0;
    if (key == confirm_key) {
        cursor->confirm_pressed = 1;
        return 1;
    }
    if (key == cancel_key) {
        cursor->cancel_pressed = 1;
        return 1;
    }
    return 0;
}

int rich4_app_input_apply_joy(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active, VirtualCursor *cursor,
    int button, int confirm_button, int cancel_button)
{
    if (cursor == NULL || !rich4_app_input_ui_active(
                              app_state, settings, session, ai_settings_active))
        return 0;
    if (button == confirm_button) {
        cursor->confirm_pressed = 1;
        return 1;
    }
    if (button == cancel_button) {
        cursor->cancel_pressed = 1;
        return 1;
    }
    return 0;
}
