#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/ai_settings_flow.h"

int main(void)
{
    GameSession session;
    Rich4AiSettingsState panel;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.players[0].is_ai = 0;
    session.state.players[0].play_state = 1;
    session.state.players[0].ai_action_flags = 4;
    session.state.players[0].ai_personality = -1;
    session.state.players[0].ai_cash_ratio = 140;
    session.state.players[0].ai_stock_ratio = 30;
    session.state.players[1].is_ai = 1;
    session.state.players[1].play_state = 5;
    session.state.players[1].ai_action_flags = 3;
    session.state.players[1].ai_personality = 2;
    session.state.players[1].ai_cash_ratio = 20;
    session.state.players[1].ai_stock_ratio = 40;

    rich4_ai_settings_open(&panel, &session);
    assert(panel.active == 1);
    assert(panel.player_count == 2);
    assert(panel.action_flags[0] == 0);
    assert(panel.personality[0] == 0);
    assert(panel.cash_ratio[0] == 100);
    assert(panel.stock_ratio[1] == 40);
    assert(rich4_ai_settings_hit(120, 80) == 100);
    assert(rich4_ai_settings_hit(320, 340) == 11);
    assert(rich4_ai_settings_hit(250, 250) == -1);

    assert(rich4_ai_settings_apply(&panel, &session, 100, 0) == 0);
    assert(panel.selected_row == 0);
    assert(panel.hosted[0] == 1);
    assert(rich4_ai_settings_apply(&panel, &session, 6, 0) == 0);
    assert(panel.cash_ratio[0] == 100);
    assert(rich4_ai_settings_apply(&panel, &session, 12, 350) == 0);
    assert(panel.stock_ratio[0] == 50);
    assert(rich4_ai_settings_apply(&panel, &session, 9, 0) == 1);
    assert(panel.active == 0);
    assert(session.state.players[0].is_ai == 1);
    assert(session.state.players[0].play_state == 5);
    assert(session.state.players[0].ai_cash_ratio == 100);
    assert(session.state.players[0].ai_stock_ratio == 50);

    panel.active = 1;
    rich4_ai_settings_cancel(&panel);
    assert(panel.active == 0);

    /* Mixed identities compact the list without changing player indices. */
    session.state.player_count = 4;
    session.state.current_player = 2;
    session.state.players[0].play_state = 2;
    session.state.players[0].is_ai = 1;
    session.state.players[1].play_state = 0;
    session.state.players[2].play_state = 1;
    session.state.players[2].days_in_hospital = 3;
    session.state.players[3].play_state = 5;
    session.state.players[3].is_ai = 1;
    rich4_ai_settings_open(&panel, &session);
    assert(panel.player_count == 2 && panel.selected_row == 0);
    assert(panel.player_index[0] == 2 && panel.player_index[1] == 3);
    assert(!panel.hosted[0] && panel.hosted[1]);
    rich4_ai_settings_apply(&panel, &session, 101, 0);
    assert(panel.selected_row == 1 && panel.hosted[1]);
    rich4_ai_settings_apply(&panel, &session, 101, 0);
    assert(!panel.hosted[1]);
    assert(session.state.players[3].play_state == 5);
    rich4_ai_settings_apply(&panel, &session, 10, 0);
    assert(session.state.players[3].play_state == 5);
    rich4_ai_settings_open(&panel, &session);
    rich4_ai_settings_apply(&panel, &session, 101, 0);
    rich4_ai_settings_apply(&panel, &session, 101, 0);
    rich4_ai_settings_apply(&panel, &session, 9, 0);
    assert(session.state.players[3].play_state == 1);
    assert(!session.state.players[3].is_ai);
    assert(session.state.players[0].play_state == 2 &&
           session.state.players[0].is_ai);
    session.state.current_player = 3;
    rich4_ai_settings_open(&panel, &session);
    assert(panel.selected_row == 1);
    rich4_ai_settings_apply(&panel, &session, 101, 0);
    rich4_ai_settings_cancel(&panel);
    assert(session.state.players[3].play_state == 1);
    session.state.players[2].play_state = 0;
    session.state.players[3].play_state = 2;
    rich4_ai_settings_open(&panel, &session);
    assert(panel.player_count == 0);
    assert(rich4_ai_settings_apply(&panel, &session, 10, 0) == -1);

    printf("app AI-settings flow: ok\n");
    return 0;
}
