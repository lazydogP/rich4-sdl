#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/card.h"
#include "core/character_template.h"
#include "core/facility.h"
#include "core/game_dispatch.h"
#include "core/game_event_payload.h"
#include "core/ledger.h"
#include "core/new_game_init.h"
#include "core/property.h"
#include "core/rng.h"
#include "core/route_event_executor.h"
#include "core/route_movement.h"
#include "core/special_event.h"
#include "core/stock.h"
#include "core/tool.h"
#include "core/turn_rotation.h"
#include "core/victory.h"

typedef struct Replay {
    char text[32768];
    size_t used;
} Replay;

static int append_text(Replay *replay, const char *format, ...)
{
    va_list args;
    int written;

    if (replay == NULL || format == NULL || replay->used >= sizeof(replay->text))
        return 0;
    va_start(args, format);
    written = vsnprintf(replay->text + replay->used,
                        sizeof(replay->text) - replay->used,
                        format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(replay->text) - replay->used)
        return 0;
    replay->used += (size_t)written;
    return 1;
}

static uint32_t state_digest(const GameState *state)
{
    const unsigned char *bytes = (const unsigned char *)state;
    uint32_t digest = UINT32_C(2166136261);
    size_t i;

    for (i = 0; i < sizeof(*state); i++) {
        digest ^= bytes[i];
        digest *= UINT32_C(16777619);
    }
    return digest;
}

static int append_events(Replay *replay, GameEventQueue *events)
{
    GameEvent event;

    while (rich4_game_event_queue_pop(events, &event)) {
        GameDomainRef subject = rich4_game_event_subject_ref(&event);
        GameDomainRef target = rich4_game_event_target_ref(&event);
        if (!append_text(replay, "event=%d,p=%d,v=%d,v2=%d,f=%u,s=%d:%d,t=%d:%d,r=%d\n",
                         event.type, event.player,
                         rich4_game_event_value(&event),
                         rich4_game_event_value2(&event),
                         rich4_game_event_flags(&event), subject.kind,
                         subject.id, target.kind, target.id,
                         event.reject_reason))
            return 0;
    }
    return 1;
}

static int checkpoint(Replay *replay, const char *name,
                      const GameState *state)
{
    return append_text(replay,
                       "state=%s,h=%08x,day=%d,current=%d,round=%d,over=%d,winner=%d\n",
                       name, (unsigned int)state_digest(state),
                       state->elapsed_days, state->current_player,
                       state->full_round_count, state->game_over,
                       state->winner_player);
}

static RouteEvent stop_event(int player, int node, int raw_object,
                             int property_branch)
{
    RouteEvent event;
    memset(&event, 0, sizeof(event));
    event.phase = ROUTE_EVENT_PHASE_STOP;
    event.node = node;
    event.object_ref = rich4_map_object_ref((unsigned short)raw_object);
    event.snapshot.player = player;
    event.snapshot.property_branch = property_branch;
    event.snapshot.property_owner = -1;
    return event;
}

static void route_begin(GameState *state, GameFlowState *flow,
                        const RouteEvent *event, int actor,
                        unsigned int token, Replay *replay,
                        const char *name)
{
    GameEventQueue events;

    rich4_route_event_executor_reset(&flow->route_events);
    assert(rich4_route_event_queue_push(&flow->route_events.queue, event));
    assert(rich4_route_event_executor_begin(
        &flow->route_events, actor, token));
    flow->turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    flow->turn_flow_active = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(
        &flow->route_events, state, &events) !=
           ROUTE_EVENT_EXECUTION_INVALID);
    assert(append_text(replay, "route=%s,actor=%d,token=%u\n",
                       name, actor, token));
    assert(append_events(replay, &events));
}

static void route_finish(GameState *state, GameFlowState *flow,
                         Replay *replay)
{
    GameEventQueue events;

    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(
        &flow->route_events, state, &events) ==
           ROUTE_EVENT_EXECUTION_FINISHED);
    assert(append_events(replay, &events));
    flow->turn_flow_active = 0;
    flow->turn_state = GAME_TURN_STATE_IDLE;
}

static void route_purchase(GameState *state, GameFlowState *flow,
                           Replay *replay, const RouteEvent *event,
                           int actor, unsigned int token,
                           GameCommandType type, int value,
                           GameDomainRef subject, const char *name)
{
    GameCommand command;
    GameEventQueue events;

    route_begin(state, flow, event, actor, token, replay, name);
    command = rich4_game_domain_command(
        type, state->players[actor].is_ai
            ? GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN,
        actor, subject, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), value);
    command.turn_token = token;
    rich4_game_event_queue_reset(&events);
    assert(rich4_game_dispatch_command(state, flow, &command, &events));
    assert(append_events(replay, &events));
    route_finish(state, flow, replay);
}

static void route_charge(GameState *state, GameFlowState *flow,
                         Replay *replay, const RouteEvent *event,
                         int actor, unsigned int token,
                         const char *name)
{
    GameEventQueue events;

    route_begin(state, flow, event, actor, token, replay, name);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_resume_charge(
        &flow->route_events, state, &events) !=
           ROUTE_EVENT_EXECUTION_INVALID);
    assert(append_events(replay, &events));
    route_finish(state, flow, replay);
}

static void dispatch_command(GameState *state, GameFlowState *flow,
                             Replay *replay, const GameCommand *command,
                             const char *name)
{
    GameEventQueue events;

    flow->turn_flow_active = 0;
    flow->turn_state = GAME_TURN_STATE_IDLE;
    flow->steps_remaining = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_game_dispatch_command(state, flow, command, &events));
    assert(append_text(replay, "command=%s\n", name));
    assert(append_events(replay, &events));
}

static void move_id_to_front(uint8_t *order, int count, int id)
{
    int i;

    for (i = 0; i < count; i++) {
        if (order[i] == (uint8_t)id) {
            uint8_t first = order[0];
            order[0] = order[i];
            order[i] = first;
            return;
        }
    }
    assert(0);
}

static void initialize_state(GameState *state)
{
    GameCharacterTemplate characters[3];
    GameStockInfo stocks[GAME_STOCK_COUNT];
    NewGameInitConfig config;
    NewGameInitReport report;
    Rich4Rng rng;
    int i;

    memset(characters, 0, sizeof(characters));
    memset(stocks, 0, sizeof(stocks));
    memset(&config, 0, sizeof(config));
    characters[0] = *rich4_character_template(0);
    characters[1] = *rich4_character_template(1);
    characters[2] = *rich4_character_template(2);
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        stocks[i].f4 = i == 0 ? 1 : 0;
        stocks[i].f8 = 1000;
        stocks[i].f10 = 500;
        stocks[i].f12 = 100.0f;
        stocks[i].f16 = 100.0f;
        stocks[i].f20 = 100.0f;
        stocks[i].f24 = 1.0f;
    }
    config.player_extra = 1;
    config.initial_fund = 20000;
    config.stage = 0;
    config.map = 0;
    config.day_limit = 0;
    config.win_target = 0;
    config.calendar_start_day = 5;
    config.calendar_start_month = 1;
    config.calendar_start_year = 2026;
    config.selected_characters[0] = 0;
    config.selected_characters[1] = 1;
    /* Keep the third participant AI-controlled, but use the lower-fund
     * template so the scripted human can legitimately become the final
     * asset leader after buying and upgrading property. */
    config.selected_characters[2] = 1;
    config.selected_ai[0] = 0;
    config.selected_ai[1] = 1;
    config.selected_ai[2] = 1;
    config.character_templates = characters;
    config.character_template_count = 3;
    config.map_stocks = stocks;
    config.map_stock_count = GAME_STOCK_COUNT;
    rich4_rng_state_seed(&rng, 311U);
    config.special_event_rng = &rng;
    assert(rich4_new_game_init(state, &config, &report));

    state->land_count = 2;
    state->land_owner[0] = -1;
    state->land_owner[1] = -1;
    state->land_group[0] = state->land_group[1] = 0;
    state->land_price[0] = 100;
    state->land_price[1] = 150;
    state->land_price_per_level[0] = 50;
    state->land_price_per_level[1] = 50;
    state->land_toll_by_level[0][0] = 25;
    state->land_toll_by_level[0][1] = 75;
    state->land_toll_by_level[1][0] = 30;
    state->land_toll_by_level[1][1] = 80;
    state->facility_count = 1;
    state->facility_owner[0] = -1;
    state->facility_price[0] = 200;
    state->facility_price_per_level[0] = 100;
    state->facility_type[0] = 0;
    state->facility_level[0] = 0;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        state->stocks[i].f6 = 0;
        state->stocks[i].f7 = 0;
        state->stocks[i].f16 = 100.0f;
        state->stocks[i].f20 = 100.0f;
        state->stocks[i].f28 = 0.0f;
    }
    state->stock_market_closed = 0;
    state->stock_last_update_day = -1;
    state->current_player = 0;
    state->turn_rotation_index = -1;
    state->players[0].node = 1;
    state->players[1].node = 2;
    state->players[2].node = 3;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        state->players[i].linked_player = -1;
}

static void configure_tool_geometry(GameState *state)
{
    static const uint8_t placeable[8] = {0, 1, 1, 1, 1, 1, 1, 1};
    static const int16_t x[8] = {0, 0, 50, 100, 150, 200, 250, 300};
    static const int16_t y[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    static const int16_t land_x[2] = {100, 200};
    static const int16_t land_y[2] = {0, 0};
    static const int16_t facility_x[1] = {150};
    static const int16_t facility_y[1] = {0};
    uint16_t next[8][4];
    int i;

    memset(next, 0, sizeof(next));
    for (i = 1; i < 8; i++) {
        if (i > 1) next[i][0] = (uint16_t)(i - 1);
        if (i < 7) next[i][1] = (uint16_t)(i + 1);
    }
    assert(rich4_tool_set_route_targets(
        state, 8, placeable, sizeof(placeable)));
    assert(rich4_tool_set_map_geometry(
        state, 8, x, y, next, state->land_count, land_x, land_y,
        state->facility_count, facility_x, facility_y));
}

static void configure_replay_route(MapData *map, MapRouteNode *nodes)
{
    memset(map, 0, sizeof(*map));
    memset(nodes, 0, sizeof(MapRouteNode) * 7);
    map->routes = nodes;
    map->route_count = 7;
    nodes[1].next[0] = 2;
    nodes[1].next[1] = 3;
    nodes[2].next[0] = 1;
    nodes[2].next[1] = 4;
    nodes[3].next[0] = 1;
    nodes[3].next[1] = 5;
    nodes[4].next[0] = 2;
    nodes[4].next[1] = 6;
    nodes[5].next[0] = 3;
    nodes[5].next[1] = 6;
    nodes[6].next[0] = 4;
    nodes[6].next[1] = 5;
}

static void replay_route_movement(GameState *state, const MapData *map,
                                  Replay *replay)
{
    GameEventQueue events;
    RouteMoveState move;
    RouteMoveResult result;
    int player = 0;

    rich4_game_event_queue_reset(&events);
    result = rich4_route_move_begin_events(
        &move, map, player, state->players[player].node,
        state->players[player].previous_node, 2, &events);
    assert(result == ROUTE_MOVE_READY);
    assert(append_text(replay, "movement=begin,player=0,steps=2\n"));
    assert(append_events(replay, &events));

    while (move.steps_remaining > 0) {
        int previous_node = move.node;
        int options[4];
        int option_count;

        rich4_game_event_queue_reset(&events);
        result = rich4_route_move_commit_events(&move, map, player, &events);
        assert(result != ROUTE_MOVE_INVALID);
        assert(append_events(replay, &events));

        /* The application movement flow commits the visual step to GameState
         * before asking the core movement state machine for the next edge. */
        state->players[player].previous_node = previous_node;
        state->players[player].node = move.node;
        if (move.steps_remaining > 0) {
            state->players[player].next_node = move.next_node;
        } else {
            option_count = rich4_route_collect_non_return(
                map, move.node, previous_node, options);
            state->players[player].next_node = option_count > 0 ? options[0] : 0;
        }
    }
    assert(append_text(replay, "movement=end,player=0,node=%d,previous=%d\n",
                       state->players[player].node,
                       state->players[player].previous_node));
}

static int generate(Replay *replay, GameState *final_state)
{
    GameState state;
    GameFlowState flow;
    GameEventQueue events;
    GameCommand command;
    RouteEvent event;
    TurnAdvanceResult turn;
    VictoryResult victory;
    MapData route_map;
    MapRouteNode route_nodes[7];
    int payment;
    int target_assets;

    memset(replay, 0, sizeof(*replay));
    memset(&flow, 0, sizeof(flow));
    rich4_rng_seed(311U);
    initialize_state(&state);
    configure_tool_geometry(&state);
    configure_replay_route(&route_map, route_nodes);
    assert(checkpoint(replay, "new-game", &state));

    replay_route_movement(&state, &route_map, replay);
    assert(checkpoint(replay, "after-movement", &state));

    state.current_player = 0;
    event = stop_event(0, 4, 0x7d1, GAME_PROPERTY_STOP_BRANCH_UNOWNED);
    route_purchase(&state, &flow, replay, &event, 0, 101,
                   GAME_COMMAND_PURCHASE_PROPERTY, 0,
                   rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
                   "human-buy-land");
    assert(state.land_owner[0] == 0);
    assert(checkpoint(replay, "after-buy-land", &state));

    command = rich4_game_domain_command(
        GAME_COMMAND_UPGRADE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    dispatch_command(&state, &flow, replay, &command, "human-upgrade-land");
    assert(state.land_level[0] == 1);
    assert(checkpoint(replay, "after-upgrade-land", &state));

    state.current_player = 1;
    event = stop_event(1, 5, 0x7d1, GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER);
    route_charge(&state, &flow, replay, &event, 1, 102, "human-charge-land");
    assert(state.players[1].cash < 8000);
    assert(checkpoint(replay, "after-charge", &state));

    state.current_player = 0;
    event = stop_event(0, 6, 0xfa1, GAME_PROPERTY_STOP_BRANCH_UNOWNED);
    route_purchase(&state, &flow, replay, &event, 0, 103,
                   GAME_COMMAND_PURCHASE_PROPERTY, 0,
                   rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0),
                   "human-buy-facility");
    event = stop_event(0, 6, 0xfa1, GAME_PROPERTY_STOP_BRANCH_OWNED_SELF);
    route_purchase(&state, &flow, replay, &event, 0, 104,
                   GAME_COMMAND_UPGRADE_PROPERTY, 3,
                   rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0),
                   "human-build-facility");
    assert(state.facility_owner[0] == 0 && state.facility_level[0] == 1 &&
           state.facility_type[0] == 3);
    assert(checkpoint(replay, "after-facility", &state));

    state.current_player = 2;
    event = stop_event(2, 7, 0x7d2, GAME_PROPERTY_STOP_BRANCH_UNOWNED);
    route_begin(&state, &flow, &event, 2, 105, replay, "ai-buy-land");
    route_finish(&state, &flow, replay);
    assert(state.land_owner[1] == 2);
    assert(checkpoint(replay, "after-ai-land", &state));
    rich4_game_event_queue_reset(&events);
    assert(rich4_ledger_spend_cash(&state, 2, 100, &events));
    assert(append_text(replay, "ai-maintenance=100\n"));
    assert(append_events(replay, &events));
    assert(checkpoint(replay, "after-ai-maintenance", &state));

    state.current_player = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_inventory_add_from_pool(&state, 0, 13, &events));
    assert(append_text(replay, "inventory=card-14\n"));
    assert(append_events(replay, &events));
    command = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 13),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), 0);
    dispatch_command(&state, &flow, replay, &command, "use-stop-card");
    assert(state.players[1].days_stopping == 1);
    assert(checkpoint(replay, "after-card", &state));

    command = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 1),
        rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 2), 0);
    dispatch_command(&state, &flow, replay, &command, "use-roadblock");
    assert(state.tool_placed_objects[0].kind == 0x10 &&
           state.tool_placed_objects[0].node == 2);
    assert(checkpoint(replay, "after-tool", &state));

    state.current_player = 0;
    state.stocks[0].f16 = state.stocks[0].f20 = 100.0f;
    command = rich4_game_domain_command(
        GAME_COMMAND_BUY_STOCK, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 2);
    dispatch_command(&state, &flow, replay, &command, "buy-stock");
    command.type = GAME_COMMAND_SELL_STOCK;
    command.value = 1;
    dispatch_command(&state, &flow, replay, &command, "sell-stock");
    assert(state.player_stock_amount[0][0] == 1);
    assert(checkpoint(replay, "after-stock", &state));

    move_id_to_front(state.news_event_order, GAME_NEWS_EVENT_COUNT, 24);
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_event_begin_news(&state, 0, &events) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(append_text(replay, "special=news\n"));
    assert(append_events(replay, &events));
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_event_commit(
               &state, GAME_SPECIAL_DEFENSE_NONE, &events) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(append_events(replay, &events));
    assert(checkpoint(replay, "after-news", &state));

    state.elapsed_days = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_stock_day_update(&state, &events) == GAME_STOCK_DAY_UPDATED);
    assert(append_text(replay, "stock-day=1\n"));
    assert(append_events(replay, &events));
    assert(checkpoint(replay, "after-stock-day", &state));

    state.players[1].cash = 0;
    state.players[1].bank = 0;
    rich4_game_event_queue_reset(&events);
    payment = rich4_ledger_pay(
        &state, 1, rich4_ledger_system(), 1, 0, &events);
    assert(payment == GAME_PAYMENT_BANKRUPTCY);
    assert(append_text(replay, "bankruptcy=player-1\n"));
    assert(append_events(replay, &events));
    assert((state.players[1].play_state & 0xff) == 0 && !state.game_over);
    assert(checkpoint(replay, "after-bankruptcy", &state));

    state.current_player = 0;
    state.turn_rotation_index = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&state, &turn, &events));
    assert((turn.events & TURN_EVENT_FULL_ROUND) != 0 &&
           (turn.events & TURN_EVENT_DAY_ADVANCED) != 0);
    assert(append_text(replay, "turn=full-round\n"));
    assert(append_events(replay, &events));
    assert(checkpoint(replay, "after-day-end", &state));

    target_assets = rich4_player_total_assets(&state, 0, NULL);
    assert(target_assets > 0);
    state.win_target = target_assets;
    rich4_game_event_queue_reset(&events);
    assert(rich4_victory_finalize_events(&state, &victory, &events));
    assert(victory.event == VICTORY_EVENT_CANDIDATE &&
           state.game_over && state.winner_player == 0);
    assert(append_text(replay, "victory=player-0\n"));
    assert(append_events(replay, &events));
    assert(checkpoint(replay, "final", &state));
    if (final_state != NULL) *final_state = state;
    return 1;
}

int main(void)
{
    Replay first;
    Replay second;
    GameState first_state;
    GameState second_state;

    assert(generate(&first, &first_state));
    assert(generate(&second, &second_state));
    if (strcmp(first.text, second.text) != 0 ||
        memcmp(&first_state, &second_state, sizeof(first_state)) != 0) {
        fprintf(stderr, "full-game replay mismatch\nfirst:\n%ssecond:\n%s",
                first.text, second.text);
        return 1;
    }
    puts(first.text);
    puts("headless full-game replay without save/load: ok");
    return 0;
}
