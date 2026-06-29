#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/game_event_payload.h"
#include "core/stock.h"
#include "core/victory.h"

static GameCommand trade(GameCommandType type, int player, int stock, int quantity)
{
    return rich4_game_domain_command(
        type, player == 0 ? GAME_COMMAND_SOURCE_HUMAN : GAME_COMMAND_SOURCE_AI,
        player, rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), quantity);
}

static void setup(GameState *state)
{
    int i;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->player_count = 2;
    state->players[0].play_state = 1;
    state->players[1].play_state = 2;
    state->players[1].is_ai = 1;
    state->players[0].bank = 1000;
    state->players[1].bank = 1000;
    memset(state->stock_company_auxiliary, 0xff,
           sizeof(state->stock_company_auxiliary));
    memset(state->stock_company_owner, 0xff,
           sizeof(state->stock_company_owner));
    memset(state->stock_company_ranking, 0xff,
           sizeof(state->stock_company_ranking));
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        state->stocks[i].f8 = 1000;
        state->stocks[i].f10 = 500;
        state->stocks[i].f12 = 100.0f;
        state->stocks[i].f16 = 100.0f;
        state->stocks[i].f20 = 100.0f;
        state->stocks[i].f24 = 1.0f;
    }
    state->stocks[0].f16 = state->stocks[0].f20 = 10.75f;
    state->stocks[0].f4 = 1;
    state->stock_company_auxiliary[0] = 0;
}

static void test_trade(void)
{
    GameState state, before;
    GameEventQueue events;
    GameCommand command;
    setup(&state);
    rich4_game_event_queue_reset(&events);
    command = trade(GAME_COMMAND_BUY_STOCK, 0, 0, 10);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_APPLIED);
    assert(state.players[0].bank == 893);
    assert(state.player_stock_amount[0][0] == 10);
    assert(state.player_stock_average_cost[0][0] > 10.69f &&
           state.player_stock_average_cost[0][0] < 10.71f);
    assert(state.stocks[0].f8 == 990 && state.stocks[0].f10 == 490);
    assert(state.stock_company_owner[0] == 0 && events.count == 3);

    rich4_game_event_queue_reset(&events);
    command = trade(GAME_COMMAND_BUY_STOCK, 1, 0, 20);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_APPLIED);
    assert(state.stock_company_owner[0] == 1);
    rich4_game_event_queue_reset(&events);
    command = trade(GAME_COMMAND_SELL_STOCK, 1, 0, 20);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_APPLIED);
    assert(state.player_stock_amount[1][0] == 0 &&
           state.player_stock_average_cost[1][0] == 0.0f &&
           state.stock_company_owner[0] == 0);

    before = state;
    command = trade(GAME_COMMAND_BUY_STOCK, 0, 0, 0);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    command = trade(GAME_COMMAND_BUY_STOCK, 0, 0, 491);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_INSUFFICIENT_VOLUME);
    command = trade(GAME_COMMAND_BUY_STOCK, 0, 0, 100);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_INSUFFICIENT_MONEY);
    command = trade(GAME_COMMAND_SELL_STOCK, 0, 0, 11);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_INSUFFICIENT_HOLDING);
    state.stock_market_closed = 1;
    command = trade(GAME_COMMAND_BUY_STOCK, 0, 0, 1);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_CLOSED);
    state.stock_market_closed = 0;
    state.stocks[0].f6 = 1;
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_SUSPENDED);

    state.stocks[0].f6 = 0;
    rich4_game_event_queue_reset(&events);
    {
        GameEvent event = rich4_game_event_text_requested_from_fields(
            (Rich4GameEventFields){
                0, 0, 0, 0, 0,
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
            });
        while (events.count < GAME_EVENT_QUEUE_CAPACITY - 2)
            assert(rich4_game_event_queue_push(&events, &event));
    }
    before = state;
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_EVENT_CAPACITY);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
}

static void test_daily_and_snapshot(void)
{
    GameState a, b, restored;
    GameEventQueue ea, eb;
    GameStockSnapshot snapshot;
    Rich4Rng ra, rb;
    int i;
    setup(&a);
    setup(&b);
    a.stocks[1].f6 = b.stocks[1].f6 = 1;
    a.stocks[2].f7 = b.stocks[2].f7 = 0x10;
    a.stocks[3].f7 = b.stocks[3].f7 = 1;
    a.stocks[4].f8 = b.stocks[4].f8 = 10000;
    rich4_rng_state_seed(&ra, 12345);
    rich4_rng_state_seed(&rb, 12345);
    rich4_stock_state_init(&a, &ra);
    rich4_stock_state_init(&b, &rb);
    assert(memcmp(&a.stocks, &b.stocks, sizeof(a.stocks)) == 0);
    assert(a.ai_rng_state == b.ai_rng_state);
    assert(a.stocks[4].f10 >= 1000 && a.stocks[4].f10 <= 2999);
    a.stock_history_cursor = b.stock_history_cursor = 143;
    a.elapsed_days = b.elapsed_days = 1;
    rich4_game_event_queue_reset(&ea);
    rich4_game_event_queue_reset(&eb);
    assert(rich4_stock_day_update(&a, &ea) == GAME_STOCK_DAY_UPDATED);
    assert(rich4_stock_day_update(&b, &eb) == GAME_STOCK_DAY_UPDATED);
    assert(a.stock_history_cursor == 0 &&
           memcmp(a.stocks, b.stocks, sizeof(a.stocks)) == 0 &&
           a.ai_rng_state == b.ai_rng_state);
    assert(a.stocks[1].f20 == 100.0f && a.stocks[1].f28 == 0.0f);
    assert(a.stocks[2].f20 == 110.0f && a.stocks[2].f28 == 10.0f);
    assert(a.stocks[3].f20 == 90.0f && a.stocks[3].f28 == -10.0f);
    for (i = 0; i < GAME_STOCK_COUNT; i++)
        assert(a.stocks[i].f20 >= 1.0f && a.stocks[i].f20 <= 9999.0f &&
               a.stock_history[i][143] == a.stocks[i].f20);
    assert(rich4_stock_day_update(&a, &ea) ==
           GAME_STOCK_DAY_ALREADY_UPDATED);

    assert(rich4_stock_snapshot_export(&a, &snapshot));
    setup(&restored);
    restored.elapsed_days = a.elapsed_days;
    assert(rich4_stock_snapshot_import(&restored, &snapshot));
    a.elapsed_days++;
    restored.elapsed_days++;
    rich4_game_event_queue_reset(&ea);
    rich4_game_event_queue_reset(&eb);
    assert(rich4_stock_day_update(&a, &ea) == GAME_STOCK_DAY_UPDATED);
    assert(rich4_stock_day_update(&restored, &eb) == GAME_STOCK_DAY_UPDATED);
    assert(memcmp(a.stocks, restored.stocks, sizeof(a.stocks)) == 0 &&
           a.ai_rng_state == restored.ai_rng_state &&
           a.stock_history_cursor == restored.stock_history_cursor);

    b.stock_market_closed = 1;
    b.elapsed_days++;
    {
        uint32_t rng = b.ai_rng_state;
        int cursor = b.stock_history_cursor;
        assert(rich4_stock_day_update(&b, &eb) == GAME_STOCK_DAY_CLOSED);
        assert(b.ai_rng_state == rng && b.stock_history_cursor == cursor);
    }
}

static void test_original_new_game_history_initialization(void)
{
    GameState state;
    Rich4Rng rng;
    int i;

    setup(&state);
    rich4_rng_state_seed(&rng, 12345);
    rich4_stock_state_init(&state, &rng);
    assert(state.stock_history_cursor == 1);
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        int j;
        assert(state.stock_history[i][0] == state.stocks[i].f20);
        for (j = 1; j < GAME_STOCK_HISTORY_COUNT; j++)
            assert(state.stock_history[i][j] == 0.0f);
    }

    /* 0x4291D6 returns before writing history when the startup calendar is
     * closed, so the cursor and all history slots remain empty. */
    setup(&state);
    state.calendar_start_year = 2026;
    state.calendar_start_month = 8;
    state.calendar_start_day = 9; /* Sunday */
    rich4_rng_state_seed(&rng, 12345);
    rich4_stock_state_init(&state, &rng);
    assert(state.stock_history_cursor == 0);
    for (i = 0; i < GAME_STOCK_COUNT; i++)
        assert(state.stock_history[i][0] == 0.0f);
}

static void test_limit_rules(void)
{
    GameState state;
    GameEventQueue events;
    GameCommand command;
    setup(&state);
    rich4_game_event_queue_reset(&events);

    state.stocks[0].f16 = 10.75f;
    state.stocks[0].f20 = rich4_stock_price_after_trend(10.75f, 10.0f);
    assert(rich4_stock_price_state(state.stocks[0].f16,
                                   state.stocks[0].f20) ==
           GAME_STOCK_PRICE_LIMIT_UP);
    command = trade(GAME_COMMAND_BUY_STOCK, 0, 0, 1);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_LIMIT_UP);
    state.player_stock_amount[0][0] = 2;
    command = trade(GAME_COMMAND_SELL_STOCK, 0, 0, 1);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_APPLIED);

    rich4_game_event_queue_reset(&events);
    state.stocks[0].f16 = 10.75f;
    state.stocks[0].f20 = rich4_stock_price_after_trend(10.75f, -10.0f);
    assert(rich4_stock_price_state(state.stocks[0].f16,
                                   state.stocks[0].f20) ==
           GAME_STOCK_PRICE_LIMIT_DOWN);
    command = trade(GAME_COMMAND_SELL_STOCK, 0, 0, 1);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_LIMIT_DOWN);
    command = trade(GAME_COMMAND_BUY_STOCK, 0, 0, 1);
    assert(rich4_stock_apply_command(&state, &command, &events) ==
           GAME_STOCK_TRADE_APPLIED);

    assert(rich4_stock_price_state(10.75f, 11.0f) ==
           GAME_STOCK_PRICE_RISE);
    assert(rich4_stock_price_state(10.75f, 10.5f) ==
           GAME_STOCK_PRICE_FALL);
    assert(rich4_stock_price_state(10.75f, 10.75f) ==
           GAME_STOCK_PRICE_FLAT);
}

static void test_status_tick(void)
{
    GameState state;
    setup(&state);
    state.stock_market_closed = 2;
    state.stocks[0].f6 = 2;
    state.stocks[0].f7 = 0x23;
    rich4_stock_tick_status(&state);
    assert(state.stock_market_closed == 1);
    assert(state.stocks[0].f6 == 1 && state.stocks[0].f7 == 0x12);
    rich4_stock_tick_status(&state);
    assert(state.stock_market_closed == 0x80);
    assert(state.stocks[0].f6 == 0 && state.stocks[0].f7 == 0x01);
    rich4_stock_tick_status(&state);
    assert(state.stock_market_closed == 0);
    assert(state.stocks[0].f7 == 0);
}

static void test_calendar_closure(void)
{
    GameState state;
    setup(&state);
    /* 2026-08-09 was Sunday; crossing into September increments the
     * original monthly-profit divisor once. */
    state.calendar_start_year = 2026;
    state.calendar_start_month = 8;
    state.calendar_start_day = 9;
    state.elapsed_days = 0;
    assert(rich4_stock_market_is_closed(&state));
    state.elapsed_days = 1;
    assert(!rich4_stock_market_is_closed(&state));
    assert(rich4_stock_elapsed_months(&state) == 0);
    state.elapsed_days = 23;
    assert(rich4_stock_elapsed_months(&state) == 1);

    /* sub_4523D5 also uses the map-specific 0x47FF4A calendar table. */
    state.calendar_start_year = 1998;
    state.calendar_start_month = 1;
    state.calendar_start_day = 1;
    state.elapsed_days = 0;
    state.stage = 0;
    state.map = 0;
    assert(rich4_stock_market_is_closed(&state)); /* fixed New Year */
    state.calendar_start_month = 1;
    state.calendar_start_day = 28;
    assert(rich4_stock_market_is_closed(&state)); /* lunar 1/1 */
    assert(rich4_stock_calendar_event_index(&state) == 17);

    /* sub_4521F0 returns visible calendar artwork even when the matching
     * record does not close the market. */
    state.calendar_start_month = 2;
    state.calendar_start_day = 14;
    assert(!rich4_stock_market_is_closed(&state));
    assert(rich4_stock_calendar_event_index(&state) == 1);
    state.calendar_start_month = 10;
    state.calendar_start_day = 31;
    assert(rich4_stock_calendar_event_index(&state) == 13);

    /* dword_47639C contains two low-word matches in leap-lunar years;
     * Rich4 deliberately accepts both dates. */
    state.calendar_start_month = 5;
    state.calendar_start_day = 30;
    assert(rich4_stock_market_is_closed(&state));
    assert(rich4_stock_calendar_event_index(&state) == 21);
    state.calendar_start_month = 6;
    state.calendar_start_day = 28;
    assert(rich4_stock_market_is_closed(&state));
    assert(rich4_stock_calendar_event_index(&state) == 21);
    state.calendar_start_month = 9;
    state.calendar_start_day = 5;
    assert(!rich4_stock_market_is_closed(&state));
    assert(rich4_stock_calendar_event_index(&state) == 22);
    state.calendar_start_month = 10;
    state.calendar_start_day = 5;
    assert(rich4_stock_market_is_closed(&state));
    assert(rich4_stock_calendar_event_index(&state) == 23);
    state.calendar_start_day = 11;
    assert(rich4_stock_calendar_event_index(&state) == -1);

    state.calendar_start_year = 2026;
    state.calendar_start_month = 2;
    state.calendar_start_day = 16;
    state.map = 3;
    assert(rich4_stock_market_is_closed(&state)); /* third Monday */
    state.map = 0;
    assert(!rich4_stock_market_is_closed(&state));

    /* When the month starts after the requested weekday, sub_4521F0 wraps
     * into the following week instead of replacing the weekday with 7. */
    state.calendar_start_month = 1;
    state.calendar_start_day = 19;
    state.map = 3;
    assert(rich4_stock_calendar_event_index(&state) == 2);
}

static void test_forced_quote(void)
{
    GameState state;
    float up, down;
    setup(&state);
    state.stock_history_cursor = 0;
    state.stocks[0].f7 = 0x20;
    up = rich4_stock_price_after_trend(state.stocks[0].f16, 10.0f);
    assert(rich4_stock_apply_forced_quote(&state, 0));
    assert(state.stocks[0].f20 == up && state.stocks[0].f28 == 10.0f);
    assert(state.stock_history[0][GAME_STOCK_HISTORY_COUNT - 1] == up);
    state.stocks[0].f7 = 0x02;
    down = rich4_stock_price_after_trend(state.stocks[0].f16, -10.0f);
    assert(rich4_stock_apply_forced_quote(&state, 0));
    assert(state.stocks[0].f20 == down && state.stocks[0].f28 == -10.0f);
}

static void test_price_and_assets(void)
{
    GameState state;
    float up = rich4_stock_price_after_trend(100.0f, 3.37f);
    float down = rich4_stock_price_after_trend(100.0f, -3.37f);
    assert(up > 102.99f && up < 103.01f);
    assert(down > 96.99f && down < 97.01f);
    assert(rich4_stock_price_after_trend(1.0f, -10.0f) == 1.0f);
    assert(rich4_stock_price_after_trend(9999.0f, 10.0f) == 9999.0f);
    /* 0x428EC5: rises discard the remainder DOWN, falls add it back UP.
     * The tick is applied to the change, anchored on yesterday's close. */
    up = rich4_stock_price_after_trend(10.75f, 10.0f);
    down = rich4_stock_price_after_trend(10.75f, -10.0f);
    assert(up > 11.79999f && up < 11.80001f);
    assert(down > 9.69999f && down < 9.70001f);
    up = rich4_stock_price_after_trend(10.77f, 10.0f);
    down = rich4_stock_price_after_trend(10.77f, -10.0f);
    assert(up > 11.81999f && up < 11.82001f);
    assert(down > 9.71999f && down < 9.72001f);
    assert(rich4_stock_price_after_trend(10.75f, 0.2f) == 10.75f);
    assert(rich4_stock_price_after_trend(10.75f, -0.2f) == 10.75f);
    setup(&state);
    state.player_stock_amount[0][0] = 3;
    assert(rich4_player_stock_assets(&state, 0) == 32);
}

int main(void)
{
    test_trade();
    test_daily_and_snapshot();
    test_original_new_game_history_initialization();
    test_limit_rules();
    test_status_tick();
    test_calendar_closure();
    test_forced_quote();
    test_price_and_assets();
    puts("stock 0x428D01/0x428D2A/0x428E23/0x4291D6: ok");
    return 0;
}
