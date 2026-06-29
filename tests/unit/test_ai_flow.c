#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/ai_flow.h"
#include "core/stock.h"

typedef struct SinkProbe {
    Rich4AiCommandSink sink;
    int calls;
    int defer;
    GameSession *session;
    int sales;
    int reject_sale;
    int count_changes;
    GameCommand first;
    GameCommand last;
} SinkProbe;

static int accept_command(Rich4AiCommandSink *sink,
                          const GameCommand *command, uint32_t now)
{
    SinkProbe *probe = (SinkProbe *)sink;
    (void)now;
    if (probe->calls == 0) probe->first = *command;
    probe->calls++;
    probe->last = *command;
    if (probe->session && command->type == GAME_COMMAND_SELL_STOCK) {
        GameEventQueue events;
        probe->sales++;
        if (probe->reject_sale) return 0;
        rich4_game_event_queue_reset(&events);
        assert(rich4_stock_apply_command(&probe->session->state, command,
            &events) == GAME_STOCK_TRADE_APPLIED);
    }
    if (probe->session && command->type == GAME_COMMAND_SELECT_DICE_COUNT) {
        probe->count_changes++;
        probe->session->state.players[command->player].ndices = command->value;
    }
    return probe->defer ? RICH4_AI_COMMAND_DEFERRED : 1;
}

static void repeated_distress_sales(void)
{
    GameSession session;
    SinkProbe probe;
    uint8_t on_map[GAME_MAX_PLAYERS] = {0};
    GameDomainRef none = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    int reject, i;
    for (reject = 0; reject < 2; reject++) {
        memset(&session, 0, sizeof(session));
        memset(&probe, 0, sizeof(probe));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.price_index = 1;
        session.state.calendar_start_year = 2026;
        session.state.calendar_start_month = 8;
        session.state.calendar_start_day = 11;
        session.state.players[0].is_ai = 1;
        session.state.players[0].play_state = 2;
        session.state.players[0].cash = 950;
        session.state.players[0].loan = 1000;
        session.state.players[0].loan_due_date = 5;
        session.ai_selection.card_target_raw = 0x8001;
        for (i = 0; i < 2; i++) {
            session.state.stocks[i].f16 = session.state.stocks[i].f20 = 10.0f;
            session.state.player_stock_amount[0][i] = 10;
            session.state.player_stock_average_cost[0][i] = 10.0f;
        }
        probe.sink.submit = accept_command;
        probe.session = &session;
        probe.reject_sale = reject;
        rich4_ai_run_preactions(&session, none, on_map, NULL, 0, 0,
            &probe.sink);
        assert(probe.sales == (reject ? 1 : 2));
        assert(probe.last.type == GAME_COMMAND_ROLL_DICE);
        assert(!session.ai_selection.stock_distress);
        assert(session.ai_selection.card_target_raw == 0x8001);
        assert(session.state.players[0].cash + session.state.players[0].bank ==
            (reject ? 950 : 1150));
    }
}

static void pre_roll_count_command(void)
{
    GameSession session;
    SinkProbe probe;
    GameCommand command;
    uint8_t on_map[GAME_MAX_PLAYERS] = {0};
    GameDomainRef none = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    memset(&session, 0, sizeof(session));
    memset(&probe, 0, sizeof(probe));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].is_ai = 1;
    session.state.players[0].play_state = 2;
    session.state.players[0].traffic_method = 2;
    session.state.players[0].ndices = 3;
    session.state.players[0].attached_timed_bomb_slot = 0;
    session.state.tool_placed_objects[0].kind = 0x12;
    session.state.tool_placed_objects[0].attached_player = 1;
    session.state.tool_placed_objects[0].countdown = 14;
    probe.sink.submit = accept_command;
    probe.session = &session;
    rich4_ai_run_preactions(&session, none, on_map, NULL, 0, 0, &probe.sink);
    assert(probe.count_changes == 1 && session.state.players[0].ndices == 1);
    assert(probe.last.type == GAME_COMMAND_ROLL_DICE);
    /* Deferred tool completion uses the same app-owned preparation entry. */
    session.state.players[0].ndices = 3;
    assert(rich4_ai_prepare_move(&session, 0, &command));
    assert(command.type == GAME_COMMAND_SELECT_DICE_COUNT && command.value == 1);
}

static void shared_stock_preview_and_card_stream(void)
{
    GameSession session;
    GameEventQueue events;
    GameAiDecision decision;
    GameCommand count_command;
    int16_t x[7] = {0, 10, 20, 30, 40, 50, 60}, y[7] = {0};
    uint16_t next[7][4] = {{0}};
    Rich4Rng global_before, global_after;
    int i;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.players[0].is_ai = 1;
    session.state.players[0].play_state = 2;
    session.state.players[1].play_state = 1;
    session.state.players[0].node = 1;
    session.state.players[0].traffic_method = 2;
    session.state.players[0].ndices = 3;
    session.state.players[0].attached_timed_bomb_slot = -1;
    session.state.ai_rng_state = 1;
    session.state.stock_last_update_day = -1;
    for (i = 0; i < GAME_STOCK_COUNT; i++) session.state.stocks[i].f6 = 1;
    for (i = 1; i < 6; i++) next[i][0] = (uint16_t)(i + 1);
    assert(rich4_tool_set_map_geometry(&session.state, 7, x, y, next,
        0, NULL, NULL, 0, NULL, NULL));
    session.state.land_count = 3;
    for (i = 0; i < 3; i++) {
        session.state.land_owner[i] = 1;
        session.state.route_object_raw[i + 2] = (uint16_t)(0x7d1 + i);
    }
    rich4_rng_global_export(&global_before);
    rich4_game_event_queue_reset(&events);
    /* Suspended stocks still consume the single market-shock draw. */
    assert(rich4_stock_day_update(&session.state, &events) == GAME_STOCK_DAY_UPDATED);
    assert(session.state.ai_rng_state == UINT32_C(1103527590));
    /* Car preview consumes the next shared draw, 5758, selecting two dice. */
    assert(rich4_ai_prepare_move(&session, 0, &count_command));
    assert(count_command.value == 2 &&
        session.state.ai_rng_state == UINT32_C(2524885223));
    assert(rich4_ai_select_command(&session.state, 0, GAME_AI_PHASE_CARD_OR_TOOL,
        session.state.ai_rng_state, &decision));
    assert(decision.rng_after == UINT32_C(662824084));
    rich4_rng_global_export(&global_after);
    assert(global_before.state == global_after.state);
}

int main(void)
{
    GameSession session;
    GameDomainRef current_property;
    uint8_t player_on_map[GAME_MAX_PLAYERS];
    SinkProbe probe;

    memset(&session, 0, sizeof(session));
    memset(player_on_map, 0, sizeof(player_on_map));
    memset(&probe, 0, sizeof(probe));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].is_ai = 1;
    session.state.players[0].play_state = 2;
    session.state.players[0].ai_action_flags = 0;
    probe.sink.submit = accept_command;

    rich4_ai_run_preactions(&session, current_property, player_on_map,
                            NULL, 0, 123, &probe.sink);
    assert(probe.calls == 2);
    assert(probe.first.type == GAME_COMMAND_SETTLE_AI_SPECIAL_FINANCE);
    assert(probe.last.type == GAME_COMMAND_ROLL_DICE);
    probe.calls = 0;
    probe.defer = 1;
    rich4_ai_run_preactions(&session, current_property, player_on_map,
                            NULL, 0, 123, &probe.sink);
    assert(probe.calls == 1);
    assert(probe.last.type != GAME_COMMAND_ROLL_DICE);
    repeated_distress_sales();
    pre_roll_count_command();
    shared_stock_preview_and_card_stream();
    puts("app ai flow: ok");
    return 0;
}
