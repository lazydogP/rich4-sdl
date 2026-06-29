#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/stock_interaction_flow.h"

int main(void)
{
    GameSession session;
    Rich4AppStockInteractionResult result;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].bank = 1000;
    session.state.stocks[0].f10 = 50;
    session.state.stocks[0].f16 = 10.0f;
    session.state.stocks[0].f20 = 10.0f;
    session.state.stocks[1].f10 = 50;
    session.state.stocks[1].f16 = 20.0f;
    session.state.stocks[1].f20 = 20.0f;

    assert(rich4_app_stock_quantity_control_at(281, 185) ==
           RICH4_STOCK_QUANTITY_CONTROL_SLIDER);
    assert(rich4_app_stock_quantity_control_at(280, 208) ==
           RICH4_STOCK_QUANTITY_CONTROL_MAX);
    assert(rich4_app_stock_quantity_control_at(336, 208) ==
           RICH4_STOCK_QUANTITY_CONTROL_ENTER);
    assert(rich4_app_stock_quantity_control_at(280, 240) == 4);
    assert(rich4_app_stock_quantity_control_at(320, 240) == 5);
    assert(rich4_app_stock_quantity_control_at(360, 240) == 6);
    assert(rich4_app_stock_quantity_control_at(264, 183) ==
           RICH4_STOCK_QUANTITY_CONTROL_NONE);
    assert(rich4_app_stock_quantity_bar_width(0, 50) == 0);
    assert(rich4_app_stock_quantity_bar_width(25, 50) == 53);
    assert(rich4_app_stock_quantity_bar_width(50, 50) == 107);
    assert(rich4_app_stock_quantity_bar_width(75, 50) == 107);
    assert(rich4_app_stock_quantity_from_bar(50, 266) == 0);
    assert(rich4_app_stock_quantity_from_bar(50, 269) == 0);
    assert(rich4_app_stock_quantity_from_bar(50, 270) == 1);
    assert(rich4_app_stock_quantity_from_bar(50, 373) == 50);
    assert(rich4_app_stock_quantity_from_bar(50, 374) == -1);

    rich4_app_stock_interaction_open(&session);
    assert(rich4_app_stock_interaction_is_active(&session));
    assert(session.presentation.stock_interaction.page ==
           RICH4_STOCK_PANEL_QUOTES);
    assert(rich4_app_stock_interaction_hit(150, 20) == RICH4_STOCK_HIT_BUY);
    assert(rich4_app_stock_interaction_hit(50, 128) ==
           RICH4_STOCK_HIT_ROW_BASE + 1);

    rich4_app_stock_interaction_mouse_down(&session, 50, 128);
    rich4_app_stock_interaction_mouse_release(
        &session, RICH4_APP_POINTER_BUTTON_LEFT, 50, 128, &result);
    assert(session.presentation.stock_interaction.selected_stock == 1);
    assert(session.presentation.stock_interaction.page ==
           RICH4_STOCK_PANEL_QUOTES);
    assert(result.audio_cue == RICH4_APP_STOCK_AUDIO_SELECT);

    rich4_app_stock_interaction_mouse_down(&session, 50, 128);
    rich4_app_stock_interaction_mouse_release(
        &session, RICH4_APP_POINTER_BUTTON_LEFT, 50, 128, &result);
    assert(session.presentation.stock_interaction.page ==
           RICH4_STOCK_PANEL_DETAIL);

    rich4_app_stock_interaction_mouse_release(
        &session, RICH4_APP_POINTER_BUTTON_RIGHT, 50, 128, &result);
    assert(session.presentation.stock_interaction.page ==
           RICH4_STOCK_PANEL_QUOTES);

    rich4_app_stock_interaction_mouse_down(&session, 150, 20);
    rich4_app_stock_interaction_mouse_release(
        &session, RICH4_APP_POINTER_BUTTON_LEFT, 150, 20, &result);
    assert(session.presentation.stock_interaction.quantity_active);
    assert(session.presentation.stock_interaction.maximum == 50);
    assert(rich4_app_stock_interaction_cursor_chunk_index(&session, 41) ==
           27);

    rich4_app_stock_interaction_mouse_down(&session, 281, 185);
    rich4_app_stock_interaction_mouse_motion(
        &session, NULL, 281, 185, &result);
    assert(session.presentation.stock_interaction.quantity == 6);
    assert(result.audio_cue == RICH4_APP_STOCK_AUDIO_SLIDER);
    rich4_app_stock_interaction_mouse_release(
        &session, RICH4_APP_POINTER_BUTTON_LEFT, 281, 185, &result);
    assert(session.presentation.stock_interaction.quantity == 6);
    rich4_app_stock_interaction_mouse_down(&session, 280, 240);
    rich4_app_stock_interaction_mouse_release(
        &session, RICH4_APP_POINTER_BUTTON_LEFT, 280, 240, &result);
    assert(session.presentation.stock_interaction.quantity == 0);

    rich4_app_stock_interaction_key_down(
        &session, RICH4_APP_KEY_DIGIT_2, &result);
    rich4_app_stock_interaction_key_down(
        &session, RICH4_APP_KEY_RETURN, &result);
    assert(result.command_ready);
    assert(result.command.type == GAME_COMMAND_BUY_STOCK);
    assert(result.command.subject.kind == GAME_DOMAIN_STOCK);
    assert(result.command.subject.id == 1);
    assert(result.command.value == 2);
    assert(rich4_app_stock_interaction_command_applied(&session, 1) ==
           RICH4_APP_STOCK_AUDIO_OK);
    assert(!session.presentation.stock_interaction.quantity_active);

    rich4_app_stock_interaction_cancel(&session);
    assert(!rich4_app_stock_interaction_is_active(&session));
    puts("app stock interaction flow: ok");
    return 0;
}
