#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/ledger.h"
#include "core/turn_rotation.h"

static void setup_ring_map(MapData *map, MapRouteNode routes[13])
{
    int i;
    memset(map, 0, sizeof(*map));
    memset(routes, 0, sizeof(MapRouteNode) * 13);
    map->routes = routes;
    map->route_count = 13;
    for (i = 1; i <= 12; i++) {
        routes[i].next[0] = (uint16_t)(i == 1 ? 12 : i - 1);
        routes[i].next[1] = (uint16_t)(i == 12 ? 1 : i + 1);
    }
}

static void test_special_auto_action(void)
{
    GameState first;
    GameState second;
    MapData map;
    MapRouteNode routes[13];
    TurnSpecialAutoActionResult a;
    TurnSpecialAutoActionResult b;
    uint32_t rng_before;

    setup_ring_map(&map, routes);
    memset(&first, 0, sizeof(first));
    first.special_player_f10[0] = 0;
    first.special_player_node[0] = 1;
    first.special_player_previous_node[0] = 12;
    first.ai_rng_state = 7;
    second = first;
    assert(rich4_turn_run_special_auto_action(&first, &map, 0, &a));
    assert(rich4_turn_run_special_auto_action(&second, &map, 0, &b));
    assert(a.kind == TURN_SPECIAL_AUTO_MOVED &&
           a.requested_steps >= 2 && a.requested_steps <= 10 &&
           a.completed_steps == a.requested_steps &&
           a.start_node == 1 && a.end_node != 1 &&
           a.path_count == a.completed_steps + 1 &&
           a.path_nodes[0] == a.start_node &&
           a.path_nodes[a.path_count - 1] == a.end_node);
    assert(memcmp(&a, &b, sizeof(a)) == 0 &&
           first.special_player_node[0] == second.special_player_node[0] &&
           first.special_player_previous_node[0] ==
               second.special_player_previous_node[0] &&
           first.ai_rng_state == second.ai_rng_state);

    memset(&first, 0, sizeof(first));
    first.special_player_node[0] = 1;
    first.special_player_previous_node[0] = 12;
    first.special_player_status[0][3] = 3;
    assert(rich4_turn_run_special_auto_action(&first, &map, 0, &a));
    assert(a.kind == TURN_SPECIAL_AUTO_MOVED &&
           a.requested_steps == 1 && a.completed_steps == 1 &&
           first.special_player_node[0] == 2);

    first.special_player_node[0] = 1;
    first.special_player_previous_node[0] = 12;
    first.special_player_status[0][0] = 1;
    first.special_player_status[0][3] = 0;
    first.ai_rng_state = 99;
    rng_before = first.ai_rng_state;
    assert(rich4_turn_run_special_auto_action(&first, &map, 0, &a));
    assert(a.kind == TURN_SPECIAL_AUTO_BLOCKED &&
           a.requested_steps == 0 && a.completed_steps == 0 &&
           first.special_player_node[0] == 1 &&
           first.ai_rng_state == rng_before);

    /* ref498df5 (sleepwalking) does not block 0x40DD1F.  It only suppresses
     * the special economic handler later in 0x41B42D. */
    first.special_player_status[0][0] = 0;
    first.special_player_status[0][1] = 1;
    first.special_player_status[0][3] = 1;
    assert(rich4_turn_run_special_auto_action(&first, &map, 0, &a));
    assert(a.kind == TURN_SPECIAL_AUTO_MOVED &&
           a.requested_steps == 1 && a.completed_steps == 1);

    memset(&first, 0, sizeof(first));
    first.special_player_node[0] = 1;
    first.special_player_previous_node[0] = 12;
    first.special_player_status[0][3] = 1;
    first.special_player_route_reversed[0] = 1;
    assert(rich4_turn_run_special_auto_action(&first, &map, 0, &a));
    assert(a.requested_steps == 1 && a.completed_steps == 1 &&
           first.special_player_node[0] == 12 &&
           first.special_player_previous_node[0] == 1 &&
           first.special_player_route_reversed[0] == 0);
}

static void test_monthly_bank_settlement(void)
{
    GameState state;
    TurnAdvanceResult result;
    GameEventQueue events;
    GameEvent event;
    int found = 0;
    int i;
    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.players[0].play_state = 1;
    state.players[0].bank = 1000;
    state.calendar_start_year = 2000;
    state.calendar_start_month = 1;
    state.calendar_start_day = 31;
    state.turn_rotation_index = 0;
    for (i = 0; i < GAME_MAX_PLAYERS; i++) state.special_player_f10[i] = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &result, &events));
    assert(state.elapsed_days == 1 && state.players[0].bank == 1100);
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type != GAME_EVENT_BANK_SETTLEMENT_APPLIED) continue;
        assert(event.player == 0 && rich4_game_event_subject(&event) == 25 &&
               rich4_game_event_value(&event) == 100 &&
               rich4_game_event_value2(&event) == 1100 &&
               rich4_game_event_flags(&event) == 0);
        found++;
    }
    assert(found == 1);
}

static void setup_loan_day(GameState *state, int player_count)
{
    int i;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->player_count = player_count;
    for (i = 0; i < player_count; i++) state->players[i].play_state = 1;
    for (i = 0; i < GAME_MAX_PLAYERS; i++) state->special_player_f10[i] = 1;
    state->turn_rotation_index = player_count - 1;
}

static void test_loan_due_reminder_and_forced_payment(void)
{
    GameState state;
    TurnAdvanceResult result;
    GameEventQueue events;
    GameEvent event;
    int reminders = 0;
    int forced = 0;

    setup_loan_day(&state, 1);
    state.players[0].loan = 1000;
    state.players[0].loan_due_date = 4;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &result, &events));
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED) {
        assert(event.player == 0 && rich4_game_event_subject(&event) == 1000 &&
               rich4_game_event_value(&event) == 3 &&
               rich4_game_event_flags(&event) == 0);
            reminders++;
        }
    assert(reminders == 1 && state.players[0].loan == 1000);

    setup_loan_day(&state, 1);
    state.players[0].cash = 500;
    state.players[0].bank = 700;
    state.players[0].loan = 1000;
    state.players[0].loan_due_date = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &result, &events));
    assert(state.players[0].bank == 0 && state.players[0].cash == 200 &&
           state.players[0].loan == 0 &&
           state.players[0].loan_due_date == 0);
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED) {
        assert(rich4_game_event_value(&event) == 0 &&
               rich4_game_event_subject(&event) == 1000 &&
               rich4_game_event_value2(&event) == GAME_PAYMENT_COMMITTED &&
               rich4_game_event_flags(&event) == 1);
            forced++;
        }
    assert(forced == 1);
}

static void test_loan_due_bankruptcy_and_closed_day_shift(void)
{
    GameState state;
    TurnAdvanceResult result;
    GameEventQueue events;
    GameEvent event;
    int eliminated = 0;

    setup_loan_day(&state, 2);
    state.players[0].cash = 200;
    state.players[0].bank = 100;
    state.players[0].loan = 1000;
    state.players[0].loan_due_date = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &result, &events));
    assert((state.players[0].play_state & 0xff) == 0 &&
           state.players[0].loan_due_date == 0);
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_PLAYER_ELIMINATED && event.player == 0)
            eliminated++;
    assert(eliminated == 1);

    setup_loan_day(&state, 1);
    state.calendar_start_year = 2000;
    state.calendar_start_month = 1;
    state.calendar_start_day = 1;
    state.players[0].cash = 1000;
    state.players[0].loan = 1000;
    state.players[0].loan_due_date = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &result, &events));
    assert(state.players[0].loan == 1000 &&
           state.players[0].loan_due_date == 2);

    /* The original shifts the stored date before computing reminders, not
     * only after the closed deadline has already been reached. */
    setup_loan_day(&state, 1);
    state.turn_rotation_index = -1;
    state.calendar_start_year = 2000;
    state.calendar_start_month = 1;
    state.calendar_start_day = 1;
    state.players[0].cash = 1000;
    state.players[0].loan = 1000;
    state.players[0].loan_due_date = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &result, &events));
    assert(state.elapsed_days == 0 && state.players[0].loan == 1000 &&
           state.players[0].loan_due_date == 2);
}

static void check_ai_cancel(void)
{
    GameState state;
    TurnAdvanceResult result;
    int i;
    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 3;
    state.current_player = state.turn_rotation_index = 1;
    for (i = 0; i < 3; ++i) {
        state.players[i].play_state = 5;
        state.players[i].is_ai = 1;
    }
    assert(rich4_turn_request_ai_cancel(&state));
    assert(state.ai_cancel_pending && state.players[0].play_state == 5);
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 2 && !state.ai_cancel_pending);
    assert(state.players[0].play_state == 1 && !state.players[0].is_ai);
    assert(state.players[1].play_state == 5 && state.players[2].play_state == 5);
    assert(!rich4_turn_request_ai_cancel(&state));
    state.players[0].days_sleeping = 3;
    assert(!rich4_turn_request_ai_cancel(&state));
    state.players[0].days_in_hospital = 2;
    assert(rich4_turn_request_ai_cancel(&state));
    state.players[2].play_state |= 0x10;
    assert(rich4_turn_advance_once(&state, &result));
    assert(state.ai_cancel_pending); /* special state returns before 0x419008 */
    state.players[0].play_state = 2;
    state.players[0].is_ai = 1;
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.kind == TURN_SELECTION_SPECIAL && !state.ai_cancel_pending);
    assert(state.players[0].play_state == 2 && state.players[0].is_ai);
    assert(state.players[2].play_state == 5);
}

static void test_daily_shared_rng_clock(void)
{
    GameState s = {0};
    TurnAdvanceResult result;
    GameEventQueue events;
    int i;
    s.active = 1;
    s.player_count = 2;
    s.players[0].play_state = s.players[1].play_state = 1;
    s.turn_rotation_index = 1;
    s.calendar_start_year = 2026;
    s.calendar_start_month = 1;
    s.calendar_start_day = 5;
    s.ai_rng_state = 99;
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) s.special_player_f10[i] = 1;
    for (i = 0; i < GAME_STOCK_COUNT; i++) s.stocks[i].f6 = 255;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events_at_tick(&s, &result, &events, 0));
    assert(s.elapsed_days == 1 && s.ai_rng_state == 12345);
    /* A second day in the same month seeds again before its shock draw. */
    s.turn_rotation_index = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events_at_tick(&s, &result, &events, 1));
    assert(s.elapsed_days == 2 && s.ai_rng_state == UINT32_C(1103527590));
    s.day_limit = 3;
    s.players[0].cash = 100;
    s.turn_rotation_index = 1;
    s.ai_rng_state = 99;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events_at_tick(&s, &result, &events, 2));
    assert(s.elapsed_days == 3 && s.ai_rng_state == 99);
}

static void test_property_status_day_expiry(void)
{
    GameState s = {0}, before;
    TurnAdvanceResult result;
    GameEventQueue events;
    int i, day;
    s.active = 1;
    s.player_count = 2;
    s.players[0].play_state = s.players[1].play_state = 1;
    s.players[0].cash = s.players[1].cash = 100;
    s.calendar_start_year = 2000;
    s.calendar_start_month = 1;
    s.calendar_start_day = 3;
    s.land_count = s.facility_count = 2;
    s.land_price_status[0] = s.facility_price_status[0] = 0x50;
    s.land_price_status[1] = s.facility_price_status[1] = 0x51;
    s.land_closed_status[1] = s.facility_closed_status[1] = 5;
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) s.special_player_f10[i] = 1;
    s.turn_rotation_index = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&s, &result, &events));
    assert(s.elapsed_days == 0 && s.land_price_status[0] == 0x50);
    /* Failed event append must not consume either effect's day. */
    s.turn_rotation_index = 1;
    before = s;
    while (events.count < GAME_EVENT_QUEUE_CAPACITY) {
        GameEvent event = events.events[0];
        assert(rich4_game_event_queue_push(&events, &event));
    }
    assert(!rich4_turn_advance_once_events(&s, &result, &events));
    assert(memcmp(&s, &before, sizeof(s)) == 0);
    for (day = 1; day <= 5; day++) {
        s.turn_rotation_index = 1;
        rich4_game_event_queue_reset(&events);
        assert(rich4_turn_advance_once_events(&s, &result, &events));
        assert(s.elapsed_days == day);
        assert(s.land_price_status[0] == (5 - day) * 16);
        assert(s.facility_price_status[0] == s.land_price_status[0]);
        assert(s.land_price_status[1] == (day < 5 ? (5 - day) * 16 + 1 : 0));
        assert(s.facility_price_status[1] == s.land_price_status[1]);
        assert(s.land_closed_status[1] == 5 - day &&
               s.facility_closed_status[1] == 5 - day);
    }
}

int main(void)
{
    GameState state;
    TurnAdvanceResult result;
    GameEventQueue events;
    GameEvent event;
    int i;
    check_ai_cancel();
    test_daily_shared_rng_clock();
    test_property_status_day_expiry();
    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 3;
    for (i = 0; i < 3; i++) state.players[i].play_state = 1;
    state.players[1].is_ai = 1;
    for (i = 0; i < 4; i++) state.special_player_f10[i] = 1;
    state.turn_rotation_index = 0;
    state.players[1].play_state = 0;
    state.players[1].node = 0;
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 2);
    assert(result.skipped_normal_mask == (1u << 1));
    state.turn_rotation_index = 0;
    state.players[1].node = 9;
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 2 && result.kind == TURN_SELECTION_NORMAL);
    assert(result.skipped_normal_mask == (1u << 1));
    state.turn_rotation_index = 2;
    for (i = 0; i < 4; i++) state.special_player_f10[i] = 0;
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 4 && result.kind == TURN_SELECTION_SPECIAL);
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 5 && result.kind == TURN_SELECTION_SPECIAL);
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 6 && result.kind == TURN_SELECTION_SPECIAL);
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 7 && result.kind == TURN_SELECTION_SPECIAL);
    state.day_limit = 1;
    state.players[0].cash = 100;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &result, &events));
    assert(result.selected_index == 0 && result.kind == TURN_SELECTION_NORMAL);
    assert((result.events & TURN_EVENT_FULL_ROUND) != 0);
    assert((result.events & TURN_EVENT_DAY_ADVANCED) != 0);
    assert(state.full_round_count == 1 && state.elapsed_days == 1 &&
           state.day_limit_reached == 1);
    assert((result.events & TURN_EVENT_MARK_DIRTY) != 0);
    assert(events.count == 4);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_FULL_ROUND_COMPLETED &&
           rich4_game_event_value(&event) == 1);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_DAY_ADVANCED &&
           rich4_game_event_value(&event) == 1 &&
           rich4_game_event_value2(&event) == 1);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_STATUS_TICK_REQUESTED && event.player == 0);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED && event.player == 0);
    state.players[0].play_state = 0x31;
    assert(rich4_turn_advance_once(&state, &result));
    assert(result.selected_index == 0 && result.kind == TURN_SELECTION_NORMAL);
    assert(state.players[0].play_state == 1);
    assert(state.players[0].is_ai == 0 && state.players[1].is_ai == 1);
    assert((result.events & TURN_EVENT_NORMAL_STATE_CLEARED) != 0);
    assert((result.events & TURN_EVENT_MARK_DIRTY) != 0);
    test_special_auto_action();
    test_monthly_bank_settlement();
    test_loan_due_reminder_and_forced_payment();
    test_loan_due_bankruptcy_and_closed_day_shift();
    puts("turn rotation 0x418EBD: ok");
    return 0;
}
