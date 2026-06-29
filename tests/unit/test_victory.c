#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/victory.h"
#include "core/game_event_payload.h"

int main(void)
{
    GameState state;
    PlayerAssetBreakdown assets;
    VictoryResult result;
    GameEventQueue events;
    GameEvent event;
    memset(&state, 0, sizeof(state));
    state.player_count = 2;
    state.players[0].play_state = 1;
    state.players[1].play_state = 2;
    state.players[0].cash = 100;
    state.players[0].bank = 200;
    state.players[0].loan = 50;
    state.players[0].special_finance = 999999;
    state.player_stock_amount[0][0] = 3;
    state.stocks[0].f20 = 10.5f;
    state.land_count = 2;
    state.land_owner[0] = 0;
    state.land_value[0] = 400;
    state.facility_count = 2;
    state.facility_owner[1] = 0;
    state.facility_value[1] = 500;
    assert(rich4_player_cash_bank_assets(&state, 0) == 250);
    assert(rich4_player_stock_assets(&state, 0) == 31);
    assert(rich4_player_land_assets(&state, 0) == 400);
    assert(rich4_player_facility_assets(&state, 0) == 500);
    assert(rich4_player_total_assets(&state, 0, &assets) == 1181);
    assert(assets.cash_and_bank == 300 && assets.loan_deduction == 50);
    state.players[1].cash = 1000;
    state.win_target = 1100;
    rich4_game_event_queue_reset(&events);
    assert(rich4_victory_evaluate_events(&state, &result, &events));
    assert(result.event == VICTORY_EVENT_CANDIDATE && result.player == 0);
    assert(result.reason == GAME_WIN_CANDIDATE_TARGET_ASSETS);
    assert(result.special_finance_excluded == 1);
    assert(state.win_candidate_player == 0 && state.win_candidate_assets == 1181);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_VICTORY_CANDIDATE && event.player == 0 &&
           rich4_game_event_subject(&event) == GAME_WIN_CANDIDATE_TARGET_ASSETS &&
           rich4_game_event_value(&event) == 1181);

    state.game_over = 0;
    state.winner_player = -1;
    state.winner_reason = GAME_WIN_CANDIDATE_NONE;
    rich4_game_event_queue_reset(&events);
    assert(rich4_victory_finalize_events(&state, &result, &events));
    assert(state.game_over && state.winner_player == 0 &&
           state.winner_reason == GAME_WIN_CANDIDATE_TARGET_ASSETS);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_VICTORY_CANDIDATE);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_GAME_ENDED && event.player == 0 &&
           rich4_game_event_game_ended_reason(&event) ==
               GAME_WIN_CANDIDATE_TARGET_ASSETS &&
           rich4_game_event_game_ended_assets(&event) == 1181);
    assert(events.count == 0);

    {
        GameState before;
        GameEvent filler;
        state.game_over = 0;
        before = state;
        rich4_game_event_queue_reset(&events);
        filler = rich4_game_event_dice_roll_requested_from_fields(
            (Rich4GameEventFields){
                0, 0, 0, 0, 0,
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
            });
        while (events.count < GAME_EVENT_QUEUE_CAPACITY - 1)
            assert(rich4_game_event_queue_push(&events, &filler));
        assert(!rich4_victory_finalize_events(&state, &result, &events));
        assert(memcmp(&state, &before, sizeof(state)) == 0);
        /* The failed two-event append must not consume the existing queue. */
        assert(events.count == GAME_EVENT_QUEUE_CAPACITY - 1);
    }

    state.players[0].play_state = 0;
    state.day_limit = 3;
    state.elapsed_days = 3;
    state.game_over = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_victory_evaluate_events(&state, &result, &events));
    assert(result.event == VICTORY_EVENT_CANDIDATE && result.player == 1);
    assert(result.reason == GAME_WIN_CANDIDATE_DAY_LIMIT);
    state.day_limit = 0;
    state.win_target = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_victory_evaluate_events(&state, &result, &events));
    assert(result.event == VICTORY_EVENT_NONE && state.win_candidate_player == -1);
    assert(events.count == 0);
    puts("victory 0x41D89E: ok");
    return 0;
}
