#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/ai_flow.h"
#include "app/app_command.h"
#include "app/auction_flow.h"
#include "app/bankruptcy_flow.h"
#include "app/command_event_flow.h"
#include "app/dice_flow.h"
#include "app/first_turn_flow.h"
#include "app/game_command_flow.h"
#include "app/game_session_map_init.h"
#include "app/game_session_runtime_init.h"
#include "app/game_speech_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/insurance_flow.h"
#include "app/hotel_flow.h"
#include "app/engineering_attack_flow.h"
#include "app/magic_house_flow.h"
#include "app/monthly_finance_flow.h"
#include "app/property_flow.h"
#include "app/route_facts.h"
#include "app/route_event_flow.h"
#include "app/route_flow.h"
#include "app/roulette_flow.h"
#include "app/special_presentation_flow.h"
#include "app/special_place_notice_flow.h"
#include "app/tool_animation_flow.h"
#include "app/tool_followup_flow.h"
#include "app/turn_status_flow.h"
#include "app/turn_start_flow.h"
#include "app/turn_movement_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/card.h"
#include "core/character_template.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/new_game_init.h"
#include "core/facility.h"
#include "core/property.h"
#include "core/rng.h"
#include "core/route_event.h"
#include "core/route_event_executor.h"
#include "core/route_movement.h"
#include "core/turn_flow.h"
#include "core/victory.h"
#include "data/map_data.h"
#include "resource/mkf.h"
#include "view/game_view_model.h"

#ifndef REAL_MAP_REPLAY_SEED
#define REAL_MAP_REPLAY_SEED 29U
#endif

typedef struct RealMapFactSummary {
    int phase;
    int node;
    int raw_object_id;
    int event_kind;
    int route_action;
    int property_branch;
    int property_owner;
    int property_value;
    int dice_total;
} RealMapFactSummary;

typedef struct RealMapDynamicSummary {
    int type;
    int node_direction;
    int node;
    int countdown;
    int attached_player;
    int flight_active;
    int flight_direction;
    float world_x;
    float world_y;
    float velocity_x;
    float velocity_y;
} RealMapDynamicSummary;

typedef struct RealMapReplaySummary {
    int moved_steps;
    int final_node;
    int previous_node;
    int next_node;
    int route_fact_count;
    int dynamic_count;
    int route_event_applied;
    int route_action_applied;
    int points_after_route_event;
    int special_request_resource;
    int special_request_hold_ms;
    int special_reward_card;
    int special_reward_amount;
    int special_reward_inventory_count;
    int special_reward_pool_remaining;
    int route_event_result;
    int long_turn_count;
    int long_stop_count;
    int long_route_fact_count;
    int long_full_round_count;
    int long_elapsed_days;
    int long_special_place_count;
    int long_game_over;
    int long_winner_player;
    int long_winner_reason;
    uint32_t long_state_digest;
    int long_nodes[12];
    int long_players[12];
    int natural_ai_turn_count;
    int natural_ai_stop_count;
    int natural_ai_route_fact_count;
    int natural_ai_full_round_count;
    int natural_ai_elapsed_days;
    int natural_ai_game_over;
    int natural_ai_winner_player;
    int natural_ai_winner_reason;
    uint32_t natural_ai_state_digest;
    int property_land_index;
    int property_land_owner;
    int property_land_level;
    int property_facility_index;
    int property_facility_owner;
    int property_facility_level;
    int owned_land_index;
    int owned_land_owner;
    int owned_land_actor_cash_before;
    int owned_land_actor_cash_after;
    int owned_land_owner_bank_before;
    int owned_land_owner_bank_after;
    int owned_land_route_result;
    int owned_facility_index;
    int owned_facility_owner;
    int owned_facility_type;
    int owned_facility_roulette_outcome;
    int owned_facility_actor_cash_before;
    int owned_facility_actor_cash_after;
    int owned_facility_owner_bank_before;
    int owned_facility_owner_bank_after;
    int owned_facility_route_result;
    int route_action_smoke_count;
    unsigned int route_action_smoke_mask;
    uint32_t route_action_smoke_digest;
    int asset_turn_count;
    int asset_land_visit_mask;
    int asset_facility_visit_mask;
    int asset_land_index;
    int asset_land_owner;
    int asset_facility_index;
    int asset_facility_owner;
    int asset_land_charge_count;
    int asset_facility_charge_count;
    int asset_game_over;
    int asset_winner_player;
    int asset_winner_reason;
    uint32_t asset_state_digest;
    int target_turn_count;
    int target_land_visit_mask;
    int target_land_index;
    int target_land_owner;
    int target_land_level;
    int target_land_charge_count;
    int target_initial_assets;
    int target_win_target;
    int target_game_over;
    int target_winner_player;
    int target_winner_reason;
    uint32_t target_state_digest;
    int target_dynamic_initial_count;
    int target_dynamic_attached_count;
    RealMapFactSummary facts[GAME_MAX_TURN_ROUTE_EVENTS];
    RealMapDynamicSummary dynamic_objects[GAME_MAP_DYNAMIC_OBJECT_COUNT];
} RealMapReplaySummary;

static int load_map(const char *path, MapData *out_map)
{
    Rich4Mkf *archive = NULL;
    MapDataSelection selection;
    unsigned char *data = NULL;
    size_t size = 0;
    Rich4MkfError read_error;
    MapDataError parse_error;

    if (rich4_mkf_open(path, &archive) != RICH4_MKF_OK) return 0;
    selection = rich4_map_data_select(0, 0, 0);
    read_error = rich4_mkf_read_resource(
        archive, selection.resource_index, &data, &size, NULL);
    parse_error = read_error == RICH4_MKF_OK
        ? rich4_map_data_parse(data, size, selection.archive,
                               selection.resource_index, out_map)
        : MAP_DATA_ERR_RANGE;
    free(data);
    rich4_mkf_close(archive);
    return parse_error == MAP_DATA_OK;
}

static int first_next_node(const MapData *map, int node)
{
    int edge;

    if (map == NULL || map->routes == NULL || node <= 0 ||
        (size_t)node >= map->route_count)
        return 0;
    for (edge = 0; edge < 4; edge++) {
        int next = map->routes[node].next[edge];
        if (next > 0 && (size_t)next < map->route_count) return next;
    }
    return 0;
}

/* Match the app route-update boundary: 0x4198C3 returns 0x80 for an
 * empty road, without starting an executor. Do not step an absent queue. */
static RouteEventExecutionResult step_real_map_route_events(
    GameSession *session, uint32_t now, int animation_enabled,
    GameEventQueue *events, Rich4AudioEventQueue *audio)
{
    if (!session->flow.route_events.state.execution_started) {
        assert(session->flow.route_facts.count == 0);
        assert(session->flow.route_events.queue.count == 0);
        rich4_game_event_queue_reset(events);
        return ROUTE_EVENT_EXECUTION_FINISHED;
    }
    return rich4_app_route_event_step(
        session, now, animation_enabled, events, audio);
}

static void initialize_session(GameSession *session, const MapData *map)
{
    GameCharacterTemplate characters[3];
    GameStockInfo stocks[GAME_STOCK_COUNT];
    NewGameInitConfig config;
    NewGameInitReport report;
    Rich4Rng special_rng;
    int start_nodes[3] = {1, 2, 3};
    int next_nodes[3];
    int i;

    memset(session, 0, sizeof(*session));
    memset(characters, 0, sizeof(characters));
    memset(stocks, 0, sizeof(stocks));
    memset(&config, 0, sizeof(config));
    for (i = 0; i < 3; i++)
        characters[i] = *rich4_character_template(i);
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        stocks[i].f4 = i == 0 ? 1 : 0;
        stocks[i].f8 = 1000;
        stocks[i].f10 = 500;
        stocks[i].f12 = 100.0f;
        stocks[i].f16 = 100.0f;
        stocks[i].f20 = 100.0f;
        stocks[i].f24 = 1.0f;
    }
    for (i = 0; i < 3; i++) {
        next_nodes[i] = first_next_node(map, start_nodes[i]);
        assert(next_nodes[i] > 0);
    }
    config.player_extra = 1;
    config.initial_fund = 20000;
    config.stage = 0;
    config.map = 0;
    config.day_limit = 30;
    config.calendar_start_day = 5;
    config.calendar_start_month = 1;
    config.calendar_start_year = 2026;
    config.selected_characters[0] = 0;
    config.selected_characters[1] = 1;
    config.selected_characters[2] = 2;
    config.selected_ai[1] = 1;
    config.selected_ai[2] = 1;
    config.character_templates = characters;
    config.character_template_count = 3;
    config.map_stocks = stocks;
    config.map_stock_count = GAME_STOCK_COUNT;
    config.defer_stock_initialization = 1;
    rich4_rng_state_seed(&special_rng, 311U);
    config.special_event_rng = &special_rng;

    assert(rich4_new_game_init(&session->state, &config, &report));
    rich4_game_session_load_map_state(session, map);
    rich4_rng_seed(311U);
    rich4_app_initialize_session_runtime(
        session, map, (int)map->route_count, start_nodes, next_nodes);
    assert(session->state.tool_route_node_count == (int)map->route_count);
    assert(session->state.land_count == (int)map->land_count - 1);
    assert(session->state.facility_count == (int)map->facility_count - 1);
}

static void execute_real_map_special_fact(
    GameSession *session, const RouteEvent *fact,
    RealMapReplaySummary *summary, int *out_applied)
{
    GameEventQueue presentation_events;
    Rich4AudioEventQueue audio_events;
    GameEvent event;
    Rich4AppRoutePresentation presentation;
    RouteEventExecutionResult result;

    if (out_applied != NULL) *out_applied = 0;
    if (session == NULL || fact == NULL ||
        fact->phase != ROUTE_EVENT_PHASE_STOP ||
        fact->snapshot.route_action < 10 ||
        fact->snapshot.route_action > 13)
        return;

    rich4_app_route_prepare_dispatch(session, session->state.current_player);
    result = step_real_map_route_events(
        session, 100, 0, &presentation_events, &audio_events);
    assert(result == ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(rich4_game_event_queue_pop(&presentation_events, &event));
    assert(event.type == GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED);
    assert(rich4_app_route_event_presentation(&event, &presentation));
    assert(presentation.kind == RICH4_ROUTE_PRESENTATION_SPECIAL_PLACE);
    assert(rich4_game_event_special_place_action(&event) ==
           fact->snapshot.route_action);
    assert(rich4_game_event_special_place_resource(&event) ==
           (fact->snapshot.route_action == 13 ? 536 : 537));
    assert(rich4_game_event_special_place_hold_ms(&event) ==
           (fact->snapshot.route_action == 13 ? 0 : 1000));
    if (summary != NULL) {
        summary->special_request_resource =
            rich4_game_event_special_place_resource(&event);
        summary->special_request_hold_ms =
            rich4_game_event_special_place_hold_ms(&event);
    }
    assert(!rich4_game_event_queue_pop(&presentation_events, NULL));

    {
        GameEventQueue events;
        rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_resume_special_place(
        &session->flow.route_events, &session->state, &events);
    assert(result == ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_INVENTORY_CHANGED);
    if (fact->snapshot.route_action == 13) {
        GameDomainRef card = rich4_game_event_inventory_subject(&event);
        GameDomainRef target = rich4_game_event_target_ref(&event);
        assert(card.kind == GAME_DOMAIN_CARD && card.id >= 0 &&
               card.id < GAME_CARD_COUNT);
        assert(target.kind == GAME_DOMAIN_PLAYER && target.id == 0);
        assert(rich4_game_event_value(&event) == 1);
        assert(rich4_game_event_value2(&event) > 0);
        assert(rich4_card_inventory_has(&session->state, 0, card.id));
        assert(session->state.card_amount[card.id] >= 0);
        if (summary != NULL) {
            summary->special_reward_card = card.id;
            summary->special_reward_amount = rich4_game_event_value(&event);
            summary->special_reward_inventory_count =
                rich4_game_event_value2(&event);
            summary->special_reward_pool_remaining =
                session->state.card_amount[card.id];
        }
    } else if (summary != NULL) {
        GameDomainRef subject = rich4_game_event_inventory_subject(&event);
        assert(subject.kind == GAME_DOMAIN_NONE);
        summary->special_reward_card = -1;
        summary->special_reward_amount = rich4_game_event_value(&event);
        summary->special_reward_inventory_count =
            rich4_game_event_value2(&event);
        summary->special_reward_pool_remaining = -1;
    }
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED);
    assert(rich4_game_event_route_event_result(&event) ==
           ROUTE_EVENT_APPLIED);
    if (summary != NULL)
        summary->route_event_result =
            rich4_game_event_route_event_result(&event);
    assert(!rich4_game_event_queue_pop(&events, NULL));
    assert(session->flow.route_events.queue.count == 0);
    }
    if (out_applied != NULL) *out_applied = 1;
}

static int find_special_stop_target(const MapData *map, int *out_previous,
                                    int *out_target)
{
    int target;

    if (out_previous != NULL) *out_previous = 0;
    if (out_target != NULL) *out_target = 0;
    if (map == NULL || map->routes == NULL || map->route_count <= 1)
        return 0;
    for (target = 1; (size_t)target < map->route_count; target++) {
        int action = rich4_app_route_action_for_node(map, target);
        int previous;
        int edge;
        if (action < 10 || action > 13) continue;
        for (previous = 1; (size_t)previous < map->route_count; previous++) {
            for (edge = 0; edge < 4; edge++) {
                if (map->routes[previous].next[edge] == target) {
                    if (out_previous != NULL) *out_previous = previous;
                    if (out_target != NULL) *out_target = target;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void run_real_map_special_stop(GameSession *session,
                                       const MapData *map,
                                       RealMapReplaySummary *summary)
{
    Rich4AppTurnMovementResult result;
    RouteMoveResult move_result;
    const RouteEvent *fact;
    int previous;
    int target;
    int calls;
    uint32_t now = 100;

    if (session == NULL || map == NULL || summary == NULL ||
        !find_special_stop_target(map, &previous, &target))
        return;
    rich4_app_route_reset_facts(session);
    session->state.current_player = 0;
    session->state.players[0].previous_node = 0;
    session->state.players[0].node = previous;
    session->state.players[0].next_node = target;
    session->presentation.actors[0].move_active = 0;
    session->presentation.movement_tick = 0;
    move_result = rich4_route_move_begin_with_next(
        &session->flow.route_move, map, previous, 0, target, 1);
    assert(move_result == ROUTE_MOVE_READY);
    session->flow.turn_state = GAME_TURN_STATE_MOVE;
    session->flow.steps_remaining = 1;
    for (calls = 0; calls < 2000 &&
         session->flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
        assert(rich4_app_update_turn_movement(
            session, map, 20, now, &result));
        now += 20;
    }
    assert(calls < 2000);
    assert(session->flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    fact = rich4_route_event_collection_last_phase(
        &session->flow.route_facts, ROUTE_EVENT_PHASE_STOP);
    assert(fact != NULL && fact->node == target &&
           fact->snapshot.route_action >= 10 &&
           fact->snapshot.route_action <= 13);
    summary->route_action_applied = fact->snapshot.route_action;
    execute_real_map_special_fact(
        session, fact, summary, &summary->route_event_applied);
    summary->points_after_route_event = session->state.players[0].points;
}

static uint32_t game_state_digest(const GameState *state)
{
    const unsigned char *bytes = (const unsigned char *)state;
    uint32_t digest = UINT32_C(2166136261);
    size_t i;

    if (state == NULL) return 0;
    for (i = 0; i < sizeof(*state); i++) {
        digest ^= bytes[i];
        digest *= UINT32_C(16777619);
    }
    return digest;
}

static int real_map_safe_next(const MapData *map, int node, int previous)
{
    int options[4];
    int count;
    int i;

    count = rich4_route_collect_non_return(map, node, previous, options);
    for (i = 0; i < count; i++) {
        int object_type;
        rich4_app_route_object_at(map, options[i], NULL, &object_type, NULL);
        /* A raw second-special object with action 0 is the confirmed
         * original no-op tail in the route executor. */
        if ((object_type == GAME_ROUTE_OBJECT_NONE ||
             object_type == GAME_ROUTE_OBJECT_NODE ||
             object_type == GAME_ROUTE_OBJECT_SECOND_SPECIAL) &&
            (rich4_app_route_action_for_node(map, options[i]) == 0 ||
             (rich4_app_route_action_for_node(map, options[i]) >= 10 &&
              rich4_app_route_action_for_node(map, options[i]) <= 13)))
            return options[i];
    }
    return 0;
}

static int real_map_edge_open(const MapData *map, int from, int to)
{
    int edge;

    if (map == NULL || map->routes == NULL || from <= 0 || to <= 0 ||
        (size_t)from >= map->route_count || (size_t)to >= map->route_count)
        return 0;
    for (edge = 0; edge < 4; edge++) {
        if (map->routes[from].next[edge] == to &&
            (map->routes[from].flags & (UINT32_C(0x40000000) >> edge)) == 0)
            return 1;
    }
    return 0;
}

static int real_map_path_next(const MapData *map, int previous, int start,
                              int target)
{
    static int first_next[GAME_MAX_MAP_ROUTE_NODES *
                          GAME_MAX_MAP_ROUTE_NODES];
    static unsigned char visited[GAME_MAX_MAP_ROUTE_NODES *
                                 GAME_MAX_MAP_ROUTE_NODES];
    static int queue[GAME_MAX_MAP_ROUTE_NODES * GAME_MAX_MAP_ROUTE_NODES];
    int head = 0;
    int tail = 0;
    int state;
    int i;

    if (map == NULL || map->routes == NULL || start <= 0 || target <= 0 ||
        previous < 0 ||
        (size_t)start >= map->route_count ||
        (size_t)target >= map->route_count ||
        previous >= GAME_MAX_MAP_ROUTE_NODES)
        return 0;
    if (start == target) return target;
    memset(visited, 0, sizeof(visited));
    state = previous * GAME_MAX_MAP_ROUTE_NODES + start;
    visited[state] = 1;
    first_next[state] = 0;
    queue[tail++] = state;
    while (head < tail) {
        int current = queue[head++];
        int current_previous = current / GAME_MAX_MAP_ROUTE_NODES;
        int current_node = current % GAME_MAX_MAP_ROUTE_NODES;
        int options[4];
        int option_count = rich4_route_collect_non_return(
            map, current_node, current_previous, options);
        if (current_node == target)
            return first_next[current];
        for (i = 0; i < option_count; i++) {
            int next = options[i];
            int next_state;
            if (!real_map_edge_open(map, current_node, next)) continue;
            next_state = current_node * GAME_MAX_MAP_ROUTE_NODES + next;
            if (visited[next_state]) continue;
            visited[next_state] = 1;
            first_next[next_state] = first_next[current] == 0
                ? next : first_next[current];
            queue[tail++] = next_state;
        }
    }
    return 0;
}

static int find_real_map_asset_target(const MapData *map, int object_type,
                                      int *out_target, int *out_index)
{
    int target;

    if (out_target != NULL) *out_target = 0;
    if (out_index != NULL) *out_index = -1;
    if (map == NULL || map->routes == NULL ||
        (object_type != GAME_ROUTE_OBJECT_LAND &&
         object_type != GAME_ROUTE_OBJECT_FACILITY))
        return 0;
    for (target = 1; (size_t)target < map->route_count; target++) {
        int node_object_type;
        int object_index;
        if (rich4_app_route_action_for_node(map, target) != 0) continue;
        rich4_app_route_object_at(
            map, target, NULL, &node_object_type, &object_index);
        /* gmap0 stores multiple route nodes for the same facility.  Use the
         * first facility record so the target remains a stable real asset
         * while all actors share the same route destination. */
        if (node_object_type != object_type ||
            (object_type == GAME_ROUTE_OBJECT_FACILITY && object_index != 0) ||
            real_map_path_next(map, 0, 1, target) == 0 ||
            real_map_path_next(map, 0, 2, target) == 0 ||
            real_map_path_next(map, 0, 3, target) == 0)
            continue;
        if (out_target != NULL) *out_target = target;
        if (out_index != NULL) *out_index = object_index;
        return 1;
    }
    return 0;
}

static int find_real_map_property_target(const MapData *map, int object_type,
                                         int *out_previous, int *out_target,
                                         int *out_index)
{
    int target;

    if (out_previous != NULL) *out_previous = 0;
    if (out_target != NULL) *out_target = 0;
    if (out_index != NULL) *out_index = -1;
    if (map == NULL || map->routes == NULL || map->route_count <= 1)
        return 0;
    for (target = 1; (size_t)target < map->route_count; target++) {
        int node_object_type;
        int object_index;
        int previous;
        int edge;

        rich4_app_route_object_at(
            map, target, NULL, &node_object_type, &object_index);
        if (node_object_type != object_type ||
            rich4_app_route_action_for_node(map, target) != 0)
            continue;
        for (previous = 1; (size_t)previous < map->route_count; previous++) {
            for (edge = 0; edge < 4; edge++) {
                if (map->routes[previous].next[edge] == target) {
                    if (out_previous != NULL) *out_previous = previous;
                    if (out_target != NULL) *out_target = target;
                    if (out_index != NULL) *out_index = object_index;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void drain_real_map_property_presentation(
    GameSession *session, uint32_t *io_now, int *out_roulette_outcome)
{
    int guard;

    assert(session != NULL && io_now != NULL);
    for (guard = 0; guard < 256; guard++) {
        Rich4AudioEventQueue audio_events;
        int active = session->presentation.property.phase !=
                GAME_PROPERTY_PRESENT_NONE ||
            session->presentation.property.max_level_pending ||
            session->presentation.property.max_level_active ||
            session->presentation.speech.active ||
            session->presentation.facility_roulette.active ||
            session->presentation.insurance.count > 0 ||
            session->flow.property_turn_end_ticks > 0;
        if (!active) return;
        *io_now += 2000;
        if (session->presentation.speech.active) {
            rich4_speech_request_skip(session);
            (void)rich4_speech_finish_update(session, *io_now, 0);
        }
        if (session->presentation.property.phase !=
                GAME_PROPERTY_PRESENT_NONE)
            (void)rich4_property_update_charge_presentation(session, *io_now);
        if (session->presentation.property.max_level_pending &&
            !session->presentation.speech.active)
            (void)rich4_property_start_max_level_animation(
                session, 1, *io_now);
        if (session->presentation.property.max_level_active)
            (void)rich4_property_update_max_level_animation(
                session, 1, 20, *io_now);
        if (session->presentation.facility_roulette.active) {
            if (out_roulette_outcome != NULL &&
                session->presentation.facility_roulette.phase ==
                    RICH4_ROULETTE_LANDED &&
                (int32_t)(*io_now -
                    session->presentation.facility_roulette.finish_until) >= 0)
                *out_roulette_outcome =
                    session->presentation.facility_roulette.outcome;
            rich4_audio_event_queue_reset(&audio_events);
            (void)rich4_roulette_update(session, *io_now, &audio_events);
        }
        if (session->flow.property_turn_end_ticks > 0)
            (void)rich4_turn_flow_update_property_end_delay(
                &session->flow, *io_now, 20);
        if (session->presentation.insurance.count > 0)
            (void)rich4_insurance_flow_update(session, *io_now);
    }
    assert(session->presentation.property.phase ==
           GAME_PROPERTY_PRESENT_NONE);
    assert(!session->presentation.speech.active);
    assert(!session->presentation.facility_roulette.active);
    assert(session->flow.property_turn_end_ticks == 0);
}

static void drain_real_map_special_presentation(
    GameSession *session, uint32_t *io_now);

static void drain_real_map_finance_presentation(
    GameSession *session, const MapData *map, uint32_t *io_now)
{
    int guard;

    /* 0x41CF67 returns from the finance windows before 0x41C84F ticks
     * the selected actor's status. Match game_update_monthly_finance and
     * the SDL update adapter's resume boundary, with animation/audio off
     * for this headless replay. Keep the app's real accounting and timers. */
    for (guard = 0; guard < 4096; guard++) {
        GameEventQueue events;
        if (!session->presentation.periodic_finance.active &&
            !session->presentation.bank_settlement.active &&
            session->pending_turn_events.count == 0)
            return;
        *io_now += 100;
        if (session->presentation.auction.active)
            drain_real_map_special_presentation(session, io_now);
        (void)rich4_monthly_finance_take_speech(session, 0);
        (void)rich4_monthly_finance_take_effect(session);
        if (session->presentation.bank_settlement.active) {
            (void)rich4_monthly_finance_update(
                session, *io_now, 0, 0, 0, 71);
        } else if (session->presentation.periodic_finance.active &&
                   !session->presentation.bankruptcy.active &&
                   !session->presentation.bankruptcy.awaiting_auction &&
                   !session->presentation.auction.active) {
            rich4_game_event_queue_reset(&events);
            (void)rich4_periodic_finance_update(
                session, *io_now, 0, 0, 0, 71, &events);
            rich4_bankruptcy_capture_events(session, &events, *io_now);
        }
        if (!session->presentation.periodic_finance.active &&
            !session->presentation.bank_settlement.active)
            (void)rich4_app_resume_turn_events(session, map, *io_now);
    }
    assert(guard < 4096);
}

static int complete_real_map_route_turn(
    GameSession *session, const MapData *map, uint32_t *now,
    Rich4AppTurnRotationResult *rotation)
{
    Rich4AppRouteEventServices services = {0};
    int guard;
    drain_real_map_finance_presentation(session, map, now);
    /* Complete 0x40D5A5's one movement segment before the EA5 rotation.
     * The fixture supplies its own presentation clock rather than SDL. */
    for (guard = 0; guard < 256 &&
         (session->presentation.hotel_entry.pending ||
          session->presentation.hotel_entry.active); guard++) {
        (void)rich4_hotel_update_entry(session, map, *now, 20);
        *now += 20;
    }
    assert(guard < 256);
    if (!rich4_app_complete_route_event_turn(session, map, *now, rotation))
        return 0;
    drain_real_map_finance_presentation(session, map, now);
    /* 0x418F59/0x418F78 may return the same actor through god and
     * engineering presentations before the next idle turn. */
    for (guard = 0; guard < 256 &&
         (session->presentation.engineering_attack.pending_return ||
          session->presentation.engineering_attack.active ||
          session->presentation.confinement_exit.active); guard++) {
        GameEvent event;
        drain_real_map_property_presentation(session, now, NULL);
        /* Returning god construction can enqueue Data[523/526]. Supply the
         * same one-frame stand-in used for nested fortune admissions. */
        while (rich4_game_event_queue_pop(&session->deferred_events, &event)) {
            if (event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED)
                assert(rich4_tool_animation_enqueue(
                    session, &event, 1, 1, 0, 0, 0, *now));
            else {
                /* Ledger/scene facts are already committed; app-owned
                 * presentations consume their own events here. */
                assert(event.type != GAME_EVENT_COMMAND_REJECTED);
                (void)rich4_app_route_event_apply(
                    session, &event, NULL, *now, 0, NULL);
            }
        }
        drain_real_map_special_presentation(session, now);
        if (session->presentation.confinement_exit.active)
            (void)rich4_app_update_confinement_exit(session, *now, 20);
        else if (session->presentation.engineering_attack.active)
            (void)rich4_engineering_attack_update(
                session, 20, 1, 20, *now, NULL);
        else
            assert(rich4_app_route_event_update(
                session, map, *now, 0, &services));
        *now += 20;
    }
    assert(guard < 256);
    return 1;
}

static void run_real_map_property_stop(const MapData *map, int object_type,
                                       int *out_index, int *out_owner,
                                       int *out_level)
{
    GameSession session;
    Rich4AppTurnMovementResult movement;
    RouteMoveResult move_result;
    RouteEventExecutionResult execution_result =
        ROUTE_EVENT_EXECUTION_INVALID;
    GameEventQueue presentation_events;
    Rich4AudioEventQueue audio_events;
    Rich4AppTurnRotationResult rotation;
    uint32_t now = 100;
    int previous;
    int target;
    int object_index;
    int calls;
    int actor;

    assert(map != NULL);
    assert(object_type == GAME_ROUTE_OBJECT_LAND ||
           object_type == GAME_ROUTE_OBJECT_FACILITY);
    assert(find_real_map_property_target(
        map, object_type, &previous, &target, &object_index));
    initialize_session(&session, map);
    for (actor = 0; actor < session.state.player_count; actor++)
        session.state.players[actor].is_ai = 1;
    session.state.current_player = 0;
    session.state.players[0].previous_node = 0;
    session.state.players[0].node = previous;
    session.state.players[0].next_node = target;
    session.presentation.actors[0].move_active = 0;
    session.presentation.movement_tick = 0;
    move_result = rich4_route_move_begin_with_next(
        &session.flow.route_move, map, previous, 0, target, 1);
    assert(move_result == ROUTE_MOVE_READY);
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.steps_remaining = 1;
    for (calls = 0; calls < 2000 &&
         session.flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
        assert(rich4_app_update_turn_movement(
            &session, map, 20, now, &movement));
        now += 20;
    }
    assert(calls < 2000);
    assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(session.flow.route_facts.count > 0);
    rich4_app_route_prepare_dispatch(&session, session.state.current_player);

    for (calls = 0; calls < 256; calls++) {
        GameEvent event;
        rich4_game_event_queue_reset(&presentation_events);
        rich4_audio_event_queue_reset(&audio_events);
        execution_result = step_real_map_route_events(
            &session, now, 0, &presentation_events, &audio_events);
        while (rich4_game_event_queue_pop(&presentation_events, &event)) {
            /* Money/level notifications remain visible at the app seam, but
             * AI property actions must not leave a human interaction modal. */
            assert(event.type != GAME_EVENT_INTERACTION_REQUESTED);
        }
        drain_real_map_property_presentation(&session, &now, NULL);
        if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED &&
            session.flow.route_events.queue.count == 0 &&
            session.flow.property_turn_end_ticks == 0)
            break;
        assert(execution_result != ROUTE_EVENT_EXECUTION_INVALID);
        now += 20;
    }
    assert(calls < 256);
    assert(execution_result == ROUTE_EVENT_EXECUTION_FINISHED);
    assert(session.flow.route_events.queue.count == 0);
    assert(session.state.current_player == 0);
    assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(!rich4_app_route_event_finish(&session, now));
    memset(&rotation, 0, sizeof(rotation));
    assert(complete_real_map_route_turn(&session, map, &now, &rotation));
    assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
    assert(object_index >= 0);
    if (object_type == GAME_ROUTE_OBJECT_LAND) {
        assert(object_index < session.state.land_count);
        assert(session.state.land_owner[object_index] == 0);
        if (out_level != NULL) *out_level = session.state.land_level[object_index];
    } else {
        assert(object_index < session.state.facility_count);
        assert(session.state.facility_owner[object_index] == 0);
        if (out_level != NULL)
            *out_level = session.state.facility_level[object_index];
    }
    if (out_index != NULL) *out_index = object_index;
    if (out_owner != NULL) *out_owner = 0;
}

static void prepare_real_map_property_stop(
    const MapData *map, int object_type, GameSession *session,
    uint32_t *out_now, int *out_index)
{
    Rich4AppTurnMovementResult movement;
    RouteMoveResult move_result;
    uint32_t now = 100;
    int previous;
    int target;
    int object_index;
    int calls;
    int actor;

    assert(map != NULL && session != NULL && out_now != NULL);
    assert(object_type == GAME_ROUTE_OBJECT_LAND ||
           object_type == GAME_ROUTE_OBJECT_FACILITY);
    assert(find_real_map_property_target(
        map, object_type, &previous, &target, &object_index));
    initialize_session(session, map);
    for (actor = 0; actor < session->state.player_count; actor++)
        session->state.players[actor].is_ai = 1;
    session->state.current_player = 0;
    session->state.players[0].previous_node = 0;
    session->state.players[0].node = previous;
    session->state.players[0].next_node = target;
    session->presentation.actors[0].move_active = 0;
    session->presentation.movement_tick = 0;
    move_result = rich4_route_move_begin_with_next(
        &session->flow.route_move, map, previous, 0, target, 1);
    assert(move_result == ROUTE_MOVE_READY);
    session->flow.turn_state = GAME_TURN_STATE_MOVE;
    session->flow.steps_remaining = 1;
    for (calls = 0; calls < 2000 &&
         session->flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
        assert(rich4_app_update_turn_movement(
            session, map, 20, now, &movement));
        now += 20;
    }
    assert(calls < 2000);
    assert(session->flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(session->flow.route_facts.count > 0);
    assert(rich4_route_event_collection_last_phase(
        &session->flow.route_facts, ROUTE_EVENT_PHASE_STOP) != NULL);
    if (out_now != NULL) *out_now = now;
    if (out_index != NULL) *out_index = object_index;
}

static void finish_real_map_property_stop(
    GameSession *session, const MapData *map, uint32_t now,
    RouteEventExecutionResult execution_result)
{
    Rich4AppTurnRotationResult rotation;

    assert(session != NULL && map != NULL);
    assert(execution_result == ROUTE_EVENT_EXECUTION_FINISHED);
    assert(session->flow.route_events.queue.count == 0);
    assert(session->state.current_player == 0);
    assert(session->flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(!rich4_app_route_event_finish(session, now));
    memset(&rotation, 0, sizeof(rotation));
    assert(complete_real_map_route_turn(session, map, &now, &rotation));
    assert(session->flow.turn_state == GAME_TURN_STATE_IDLE);
}

static void run_real_map_owned_land_charge(
    const MapData *map, RealMapReplaySummary *summary)
{
    GameSession session;
    GameLandAction action;
    GameEventQueue presentation_events;
    Rich4AudioEventQueue audio_events;
    RouteEventExecutionResult execution_result =
        ROUTE_EVENT_EXECUTION_INVALID;
    uint32_t now;
    int object_index;
    int calls;
    int actor_cash_before;
    int owner_bank_before;
    int saw_charge = 0;

    assert(map != NULL && summary != NULL);
    prepare_real_map_property_stop(
        map, GAME_ROUTE_OBJECT_LAND, &session, &now, &object_index);
    assert(object_index >= 0 && object_index < session.state.land_count);
    session.state.land_owner[object_index] = 1;
    session.state.land_type[object_index] = 0;
    session.state.land_group[object_index] = 0;
    session.state.land_level[object_index] = 1;
    session.state.land_closed_status[object_index] = 0;
    session.state.land_price_status[object_index] = 0;
    /* Keep the real-map property record, but make the selected level's toll
     * explicit so this smoke always proves a non-zero owner transfer. */
    session.state.land_toll_by_level[object_index][1] = 100;
    session.state.players[0].cash = 10000;
    session.state.players[1].cash = 10000;
    assert(rich4_land_inspect(
        &session.state, 0, object_index, &action));
    assert(action.kind == GAME_LAND_ACTION_CHARGE);
    assert(action.amount > 0 && action.owner == 1);
    actor_cash_before = session.state.players[0].cash;
    owner_bank_before = session.state.players[1].bank;
    rich4_app_route_prepare_dispatch(&session, session.state.current_player);

    for (calls = 0; calls < 256; calls++) {
        GameEvent event;
        rich4_game_event_queue_reset(&presentation_events);
        rich4_audio_event_queue_reset(&audio_events);
        execution_result = step_real_map_route_events(
            &session, now, 0, &presentation_events, &audio_events);
        if (session.presentation.property.phase !=
            GAME_PROPERTY_PRESENT_NONE)
            saw_charge = 1;
        while (rich4_game_event_queue_pop(&presentation_events, &event))
            assert(event.type != GAME_EVENT_INTERACTION_REQUESTED);
        drain_real_map_property_presentation(&session, &now, NULL);
        if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED &&
            session.flow.route_events.queue.count == 0 &&
            session.flow.property_turn_end_ticks == 0)
            break;
        assert(execution_result != ROUTE_EVENT_EXECUTION_INVALID);
        now += 20;
    }
    assert(calls < 256);
    assert(saw_charge);
    finish_real_map_property_stop(&session, map, now, execution_result);
    assert(session.state.land_owner[object_index] == 1);
    assert(session.state.players[0].cash < actor_cash_before);
    assert(session.state.players[1].bank > owner_bank_before);
    summary->owned_land_index = object_index;
    summary->owned_land_owner = session.state.land_owner[object_index];
    summary->owned_land_actor_cash_before = actor_cash_before;
    summary->owned_land_actor_cash_after = session.state.players[0].cash;
    summary->owned_land_owner_bank_before = owner_bank_before;
    summary->owned_land_owner_bank_after = session.state.players[1].bank;
    summary->owned_land_route_result = ROUTE_EVENT_APPLIED;
}

static void run_real_map_owned_facility_roulette(
    const MapData *map, RealMapReplaySummary *summary)
{
    GameSession session;
    GameEventQueue presentation_events;
    Rich4AudioEventQueue audio_events;
    RouteEventExecutionResult execution_result =
        ROUTE_EVENT_EXECUTION_INVALID;
    uint32_t now;
    int object_index;
    int calls;
    int actor_cash_before;
    int owner_bank_before;
    int roulette_outcome = -1;
    int saw_roulette = 0;

    assert(map != NULL && summary != NULL);
    prepare_real_map_property_stop(
        map, GAME_ROUTE_OBJECT_FACILITY, &session, &now, &object_index);
    assert(object_index >= 0 && object_index < session.state.facility_count);
    session.state.facility_owner[object_index] = 1;
    session.state.facility_type[object_index] = 1;
    session.state.facility_level[object_index] = 1;
    session.state.facility_closed_status[object_index] = 0;
    session.state.facility_price_status[object_index] = 0;
    /* Type 1 uses the app roulette result as a multiplier. */
    session.state.facility_charge_by_level[object_index][1] = 100;
    session.state.players[0].cash = 10000;
    session.state.players[1].cash = 10000;
    {
        GameFacilityAction action;
        assert(rich4_facility_inspect(
            &session.state, 0, object_index, &action));
        assert(action.kind == GAME_FACILITY_ACTION_DEFERRED);
        assert(action.owner == 1);
    }
    actor_cash_before = session.state.players[0].cash;
    owner_bank_before = session.state.players[1].bank;
    rich4_app_route_prepare_dispatch(&session, session.state.current_player);

    for (calls = 0; calls < 256; calls++) {
        GameEvent event;
        rich4_game_event_queue_reset(&presentation_events);
        rich4_audio_event_queue_reset(&audio_events);
        execution_result = step_real_map_route_events(
            &session, now, 0, &presentation_events, &audio_events);
        if (session.presentation.facility_roulette.active)
            saw_roulette = 1;
        while (rich4_game_event_queue_pop(&presentation_events, &event))
            assert(event.type != GAME_EVENT_INTERACTION_REQUESTED);
        drain_real_map_property_presentation(
            &session, &now, &roulette_outcome);
        if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED &&
            session.flow.route_events.queue.count == 0 &&
            session.flow.property_turn_end_ticks == 0)
            break;
        assert(execution_result != ROUTE_EVENT_EXECUTION_INVALID);
        now += 20;
    }
    assert(calls < 256);
    assert(saw_roulette);
    assert(roulette_outcome >= 1 && roulette_outcome <= 4);
    finish_real_map_property_stop(&session, map, now, execution_result);
    assert(session.state.facility_owner[object_index] == 1);
    assert(session.state.players[0].cash < actor_cash_before);
    assert(session.state.players[1].bank > owner_bank_before);
    summary->owned_facility_index = object_index;
    summary->owned_facility_owner = session.state.facility_owner[object_index];
    summary->owned_facility_type = session.state.facility_type[object_index];
    summary->owned_facility_roulette_outcome = roulette_outcome;
    summary->owned_facility_actor_cash_before = actor_cash_before;
    summary->owned_facility_actor_cash_after = session.state.players[0].cash;
    summary->owned_facility_owner_bank_before = owner_bank_before;
    summary->owned_facility_owner_bank_after = session.state.players[1].bank;
    summary->owned_facility_route_result = ROUTE_EVENT_APPLIED;
}

static int find_real_map_action_target(const MapData *map, int action,
                                       int *out_previous, int *out_target)
{
    int target;

    if (out_previous != NULL) *out_previous = 0;
    if (out_target != NULL) *out_target = 0;
    if (map == NULL || map->routes == NULL || action <= 0 || action > 16)
        return 0;
    for (target = 1; (size_t)target < map->route_count; target++) {
        int previous;
        int edge;
        if (rich4_app_route_action_for_node(map, target) != action)
            continue;
        for (previous = 1; (size_t)previous < map->route_count; previous++) {
            for (edge = 0; edge < 4; edge++) {
                if (map->routes[previous].next[edge] == target) {
                    if (out_previous != NULL) *out_previous = previous;
                    if (out_target != NULL) *out_target = target;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void prepare_real_map_action_stop(
    const MapData *map, int action, GameSession *session,
    uint32_t *out_now, int *out_target)
{
    Rich4AppTurnMovementResult movement;
    RouteMoveResult move_result;
    uint32_t now = 100;
    int previous;
    int target;
    int calls;
    int actor;

    assert(map != NULL && session != NULL && out_now != NULL);
    assert(find_real_map_action_target(
        map, action, &previous, &target));
    initialize_session(session, map);
    for (actor = 0; actor < session->state.player_count; actor++) {
        session->state.players[actor].is_ai = 1;
        /* Several original branches use play_state 1 vs 2 to choose the
         * human minigame/modal path, so make the real-map actor unambiguously
         * AI for this headless route-family sweep. */
        session->state.players[actor].play_state = 2;
    }
    session->state.current_player = 0;
    session->state.players[0].previous_node = 0;
    session->state.players[0].node = previous;
    session->state.players[0].next_node = target;
    session->presentation.actors[0].move_active = 0;
    session->presentation.movement_tick = 0;
    move_result = rich4_route_move_begin_with_next(
        &session->flow.route_move, map, previous, 0, target, 1);
    assert(move_result == ROUTE_MOVE_READY);
    session->flow.turn_state = GAME_TURN_STATE_MOVE;
    session->flow.steps_remaining = 1;
    for (calls = 0; calls < 2000 &&
         session->flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
        assert(rich4_app_update_turn_movement(
            session, map, 20, now, &movement));
        now += 20;
    }
    assert(calls < 2000);
    assert(session->flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(session->flow.route_facts.count > 0);
    {
        const RouteEvent *fact = rich4_route_event_collection_last_phase(
            &session->flow.route_facts, ROUTE_EVENT_PHASE_STOP);
        assert(fact != NULL && fact->node == target &&
               fact->snapshot.route_action == action);
    }
    if (out_now != NULL) *out_now = now;
    if (out_target != NULL) *out_target = target;
}

static void drain_real_map_special_presentation(
    GameSession *session, uint32_t *io_now)
{
    int guard;

    assert(session != NULL && io_now != NULL);
    for (guard = 0; guard < 512; guard++) {
        Rich4SpecialEventUpdateInput input;
        Rich4SpecialEventUpdateResult result;
        int active = session->presentation.special_event.active ||
            session->presentation.auction.active ||
            session->presentation.magic_house.active ||
            session->presentation.speech.active ||
            session->presentation.tool_animation_count > 0 ||
            session->presentation.special_place_notice.queue.count > 0 ||
            session->presentation.bank_rejection.active ||
            session->presentation.insurance.count > 0;
        if (!active) return;
        *io_now += 2000;
        if (session->presentation.speech.active) {
            rich4_speech_request_skip(session);
            (void)rich4_speech_finish_update(session, *io_now, 0);
        }
        if (session->presentation.special_place_notice.queue.count > 0)
            (void)rich4_special_place_notice_update(session, *io_now);
        if (session->presentation.bank_rejection.active)
            (void)rich4_bank_rejection_update(session, *io_now, NULL);
        if (session->presentation.auction.active) {
            Rich4AudioEventQueue audio;
            GameEventQueue events;
            rich4_audio_event_queue_reset(&audio);
            rich4_game_event_queue_reset(&events);
            /* Drive 0x43BDE5 through the same app modal as SDL. AI bids
             * normally; a human slot in this replay declines to bid. */
            if (rich4_auction_flow_is_human_turn(session))
                assert(rich4_auction_flow_submit(
                    session, GAME_AUCTION_PASS, *io_now, &audio) !=
                    GAME_AUCTION_STEP_INVALID);
            (void)rich4_auction_flow_update(
                session, *io_now, 0, &audio, &events);
        }
        if (session->presentation.tool_animation_count > 0) {
            Rich4ToolAnimationCompletion completion;
            GameEvent event;
            /* 0x43D390/0x43ED62 confinement can be reached by a fortune
             * nested in magic action 1. Drive the existing app queue with
             * one headless frame, as the SDL animation adapter does. */
            (void)rich4_tool_animation_update(
                session, 1, 1, *io_now, NULL, &completion);
            if (completion.sync_player >= 0)
                rich4_tool_animation_actor_synced(session, completion.sync_player);
            while (rich4_game_event_queue_pop(&completion.events, &event)) {
                if (event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED)
                    assert(rich4_tool_animation_enqueue(
                        session, &event, 1, 1, 0, 0, 0, *io_now));
                else if (event.type == GAME_EVENT_VOICE_REQUESTED)
                    (void)rich4_tool_animation_handle_voice_event(session, &event, *io_now);
                else if (event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED)
                    rich4_insurance_flow_enqueue_confinement_claim(session, &event, *io_now);
            }
        }
        if (session->presentation.magic_house.active) {
            Rich4AudioEventQueue audio;
            GameEventQueue events;
            GameEvent event;
            rich4_audio_event_queue_reset(&audio);
            rich4_game_event_queue_reset(&events);
            /* 0x4339BD waits before 0x431CAA; let the app finish the AI
             * notice and target pages before resuming action 16. The
             * one-frame finale is the headless resource stand-in. */
            (void)rich4_magic_house_update(
                session, 1, 1, *io_now, 0, &audio, &events);
            while (rich4_game_event_queue_pop(&events, &event)) {
                /* Action 1 continues into three fortunes per target. */
                assert(event.type == GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED);
                assert(rich4_special_event_begin_from_event(
                    session, &event, *io_now));
            }
        }
        if (session->presentation.special_event.active) {
            memset(&input, 0, sizeof(input));
            input.page_frame_count = 1;
            input.page_delay_ms = 1;
            input.consequence_frame_count = 1;
            input.consequence_delay_ms = 1;
            input.fortune_frame_count = 1;
            input.fortune_delay_ms = 1;
            input.confinement_frame_count = 1;
            input.confinement_delay_ms = 1;
            input.speech_active = session->presentation.speech.active;
            input.audio_speech_playing = 0;
            if (rich4_special_event_prepare_news_area_view(
                    session, *io_now, input.audio_speech_playing)) {
                /* This headless fixture has an empty projected-object
                 * list. Do not approximate resource anchors from world
                 * positions. Actual news 4/20 projection is covered by
                 * tests/sdl/test_ai_pixel_candidates.c. */
                input.news_area_available = 1;
            }
            memset(&result, 0, sizeof(result));
            if (rich4_special_event_update(
                    session, &input, *io_now, &result)) {
                /* The SDL adapter normally turns these semantic actions
                 * into resource-backed timelines.  Replay has no renderer,
                 * so use one deterministic frame while preserving the app
                 * phase transitions. */
                if (result.action ==
                        RICH4_SPECIAL_EVENT_UPDATE_CONFINEMENT_ANIMATION ||
                    result.action ==
                        RICH4_SPECIAL_EVENT_UPDATE_FORTUNE_ANIMATION ||
                    (result.action == RICH4_SPECIAL_EVENT_UPDATE_REACTION &&
                     (result.reaction.action ==
                          RICH4_SPECIAL_EVENT_REACTION_CONFINEMENT_ANIMATION ||
                      result.reaction.action ==
                          RICH4_SPECIAL_EVENT_REACTION_FORTUNE_ANIMATION))) {
                    int phase = result.action ==
                            RICH4_SPECIAL_EVENT_UPDATE_FORTUNE_ANIMATION ||
                            result.reaction.action ==
                                RICH4_SPECIAL_EVENT_REACTION_FORTUNE_ANIMATION
                        ? RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION
                        : RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION;
                    assert(rich4_special_event_begin_timed_phase(
                        session, phase, 1, 1, *io_now, 0));
                }
            }
        }
        if (session->presentation.insurance.count > 0)
            (void)rich4_insurance_flow_update(session, *io_now);
    }
    assert(!session->presentation.special_event.active);
    assert(!session->presentation.magic_house.active);
    assert(!session->presentation.auction.active);
    assert(!session->presentation.speech.active);
    assert(session->presentation.insurance.count == 0);
}

static void consume_real_map_action_events(
    GameSession *session, GameEventQueue *events, uint32_t now,
    int *out_special, int *out_special_place)
{
    GameEvent event;

    assert(session != NULL && events != NULL);
    while (rich4_game_event_queue_pop(events, &event)) {
        if (event.type == GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED) {
            assert(rich4_special_event_begin_from_event(
                session, &event, now));
            if (out_special != NULL) *out_special = 1;
        } else if (event.type ==
                   GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED) {
            GameEventQueue resumed_events;
            RouteEventExecutionResult result;
            rich4_game_event_queue_reset(&resumed_events);
            result = rich4_route_event_executor_resume_special_place(
                &session->flow.route_events, &session->state,
                &resumed_events);
            assert(result != ROUTE_EVENT_EXECUTION_INVALID);
            if (out_special_place != NULL) *out_special_place = 1;
            while (rich4_game_event_queue_pop(&resumed_events, &event))
                assert(event.type == GAME_EVENT_INVENTORY_CHANGED ||
                       event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED);
        } else if (event.type == GAME_EVENT_INTERACTION_REQUESTED) {
            /* AI route decisions are submitted below.  Special-event pages
             * intentionally retain this interaction until their timed page
             * update submits pending_accept. */
            assert(session->presentation.route_interaction.active);
        } else if (event.type == GAME_EVENT_PROPERTY_LEVEL_CHANGED) {
            /* The SDL asset callback supplies the graph frame count.  The
             * headless replay uses one deterministic frame while preserving
             * the same app presentation state and end-delay transition. */
            rich4_property_begin_level_presentation(
                session, &event, 1, now);
        } else {
            assert(event.type != GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED);
        }
    }
}

static void submit_real_map_ai_interaction(GameSession *session)
{
    GameCommand command;
    GameEventQueue events;
    RouteEventCommandResult result;

    if (session == NULL || !session->presentation.route_interaction.active ||
        session->presentation.special_event.active)
        return;
    command = session->presentation.route_interaction.accept;
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_submit(
        &session->flow.route_events, &session->state, &command, &events);
    assert(result != ROUTE_EVENT_COMMAND_INVALID);
    session->presentation.route_interaction.active = 0;
    while (rich4_game_event_queue_pop(&events, NULL)) {
        /* The next app step owns any newly requested page. */
    }
}

typedef struct RealMapAiCommandContext {
    Rich4AiCommandSink sink;
    GameSession *session;
    uint32_t now;
} RealMapAiCommandContext;

static int submit_real_map_ai_command(
    Rich4AiCommandSink *sink, const GameCommand *command, uint32_t now)
{
    RealMapAiCommandContext *context = (RealMapAiCommandContext *)sink;
    GameEventQueue events;
    GameEvent event;
    int accepted;

    if (context == NULL || context->session == NULL || command == NULL)
        return 0;
    context->now = now;
    rich4_game_event_queue_reset(&events);
    accepted = rich4_app_apply_game_command(
        context->session, command, &events);
    while (rich4_game_event_queue_pop(&events, &event)) {
        Rich4CommandEventEffect effect;

        /* This is the same app-side command/event handoff used by the SDL
         * command adapter.  Resource-backed effects are intentionally left
         * out of this replay, but dice/turn-flow events must still enter the
         * real app state machines. */
        (void)rich4_app_apply_command_event(
            context->session, command, &event, now);
        effect = rich4_app_command_event_effect(
            context->session, command, &event);
        if (effect.kind == RICH4_COMMAND_EVENT_EFFECT_PROPERTY_LEVEL)
            rich4_property_begin_level_presentation(
                context->session, &event, 1, now);
        else if (effect.kind == RICH4_COMMAND_EVENT_EFFECT_STATUS_SYNC &&
                 event.player >= 0 &&
                 event.player < context->session->state.player_count)
            (void)rich4_app_process_pending_status_events(
                context->session, event.player);
    }
    return accepted;
}

static void run_real_map_ai_preactions(
    GameSession *session, const MapData *map, int player, uint32_t now)
{
    GameDomainRef current_property;
    GameAiMapTargetContext target_context;
    RealMapAiCommandContext command_context;
    uint8_t player_on_map[GAME_MAX_PLAYERS];
    int object_id;
    int object_type;
    int object_index;
    int god_type;
    int i;

    assert(session != NULL && map != NULL);
    memset(player_on_map, 0, sizeof(player_on_map));
    memset(&command_context, 0, sizeof(command_context));
    current_property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    target_context.nearest_dynamic_god =
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    target_context.nearest_dynamic_god_type = 0;
    rich4_app_route_object_at(
        map, session->state.players[player].node,
        &object_id, &object_type, &object_index);
    (void)object_id;
    if (object_type == GAME_ROUTE_OBJECT_LAND)
        current_property = rich4_game_domain_ref(
            GAME_DOMAIN_LAND, object_index);
    else if (object_type == GAME_ROUTE_OBJECT_FACILITY)
        current_property = rich4_game_domain_ref(
            GAME_DOMAIN_FACILITY, object_index);
    for (i = 0; i < session->state.player_count; i++)
        player_on_map[i] = (uint8_t)(
            (session->state.players[i].play_state &
             GAME_PLAYER_PLAY_STATE_MASK) != 0 &&
            session->state.players[i].days_disappearing == 0 &&
            session->state.players[i].node >= 0);
    god_type = 0;
    object_id = rich4_app_find_nearest_dynamic_god(
        &session->presentation, &session->state, player, &god_type);
    if (object_id >= 0) {
        target_context.nearest_dynamic_god =
            rich4_game_domain_ref(GAME_DOMAIN_GOD, object_id);
        target_context.nearest_dynamic_god_type = god_type;
    }
    command_context.sink.submit = submit_real_map_ai_command;
    command_context.session = session;
    command_context.now = now;
    rich4_ai_run_preactions(
        session, current_property, player_on_map, &target_context,
        player, now, &command_context.sink);
}

static int real_map_node_direction(
    const MapData *map, int node, int next_node)
{
    int dx;
    int dy;

    if (map == NULL || map->routes == NULL || node <= 0 || next_node <= 0 ||
        (size_t)node >= map->route_count ||
        (size_t)next_node >= map->route_count)
        return 0;
    dx = map->routes[next_node].x - map->routes[node].x;
    dy = map->routes[next_node].y - map->routes[node].y;
    return rich4_game_actor_direction_from_delta(dx, dy);
}

static void finish_real_map_first_landing(
    GameSession *session, const MapData *map, uint32_t now)
{
    Rich4FirstTurnPresentationRequest request;
    GamePlayerState *current_player;
    GamePlayerState *landed_player;
    int current;
    int landed;

    assert(session != NULL && map != NULL && map->routes != NULL);
    current = session->state.current_player % session->state.player_count;
    if (current < 0) current += session->state.player_count;
    landed = session->flow.landing_pending_player - 1;
    if (landed < 0 || landed >= session->state.player_count)
        landed = current;
    current_player = &session->state.players[current];
    landed_player = &session->state.players[landed];
    assert(rich4_app_update_first_landing(
        session, current, landed,
        map->routes[current_player->node].x,
        map->routes[current_player->node].y,
        map->routes[landed_player->node].x,
        map->routes[landed_player->node].y,
        real_map_node_direction(map, landed_player->node,
                                landed_player->next_node),
        0, 1, now, &request));
}

static void consume_real_map_natural_movement_events(
    GameSession *session, GameEventQueue *events)
{
    GameEvent event;

    assert(session != NULL && events != NULL);
    while (rich4_game_event_queue_pop(events, &event)) {
        if (event.type == GAME_EVENT_STATUS_CHANGED &&
            event.player >= 0 && event.player < session->state.player_count)
            (void)rich4_app_process_pending_status_events(
                session, event.player);
        /* GOD_ACQUIRED, tool presentation and voice events are semantic
         * evidence in this headless run.  The corresponding SDL overlays are
         * deliberately not started, so they cannot block the next turn. */
    }
}

static void record_real_map_natural_ai_turn(
    GameSession *session, RealMapReplaySummary *summary)
{
    assert(session != NULL && summary != NULL);
    summary->natural_ai_turn_count++;
    summary->natural_ai_stop_count++;
    summary->natural_ai_route_fact_count += session->flow.route_facts.count;
}

static void run_real_map_natural_ai(
    const MapData *map, RealMapReplaySummary *summary)
{
    GameSession session;
    Rich4AppTurnMovementResult movement;
    GameEventQueue presentation_events;
    Rich4AudioEventQueue audio_events;
    uint32_t now = 100;
    int outer;

    assert(map != NULL && summary != NULL);
    initialize_session(&session, map);
    for (outer = 0; outer < session.state.player_count; outer++)
        session.state.players[outer].is_ai = 1;
    session.presentation.intro.active = 0;
    session.state.day_limit = 4;
    session.state.win_target = 0;
    session.state.current_player = 0;
    session.presentation.actors[0].dirty_flags |= GAME_PLAYER_DIRTY_TURN;
    summary->natural_ai_turn_count = 0;
    summary->natural_ai_stop_count = 0;
    summary->natural_ai_route_fact_count = 0;
    summary->natural_ai_full_round_count = 0;
    summary->natural_ai_elapsed_days = 0;
    summary->natural_ai_game_over = 0;
    summary->natural_ai_winner_player = -1;
    summary->natural_ai_winner_reason = GAME_WIN_CANDIDATE_NONE;
    summary->natural_ai_state_digest = 0;

    /* The outer guard counts completed AI turns.  Each inner loop is the
     * production ordering in miniature: turn-start/status, AI preactions,
     * dice, movement, route events and rotation. */
    for (outer = 0; outer < 48 && !session.state.game_over; outer++) {
        int inner;

        for (inner = 0; inner < 20000 && !session.state.game_over; inner++) {
            Rich4TurnStartRequest request;

            if (!session.flow.turn_flow_active &&
                session.flow.turn_state == GAME_TURN_STATE_IDLE) {
                if (!rich4_turn_start_prepare_request(
                        &session, now, &request))
                    break;
                if (request.action == RICH4_TURN_START_STARTED) {
                    finish_real_map_first_landing(&session, map, now);
                    now += 20;
                    continue;
                }
                if (request.action == RICH4_TURN_START_AI) {
                    run_real_map_ai_preactions(
                        &session, map, request.player_index, now);
                    now += 20;
                    continue;
                }
                if (request.action == RICH4_TURN_START_CONTROL_LOCKED) {
                    Rich4AppTurnRotationResult rotation;
                    memset(&rotation, 0, sizeof(rotation));
                    assert(rich4_app_finish_control_locked_turn(
                        &session, map, now, &rotation));
                    now += 20;
                    continue;
                }
                break;
            }

            if (session.flow.turn_state == GAME_TURN_STATE_PREROLL) {
                assert(rich4_dice_update(
                    &session, map, 0, 1, 1, 1, now));
                now += 20;
                continue;
            }
            if (session.flow.turn_state == GAME_TURN_STATE_MOVE) {
                assert(rich4_app_update_turn_movement(
                    &session, map, 20, now, &movement));
                consume_real_map_natural_movement_events(
                    &session, &movement.events);
                if (session.presentation.tool_trigger_turn_pending) {
                    Rich4AppTurnRotationResult rotation;
                    memset(&rotation, 0, sizeof(rotation));
                    assert(rich4_tool_followup_finish_trigger_turn_and_advance(
                        &session, map, now, &rotation));
                }
                if (session.flow.turn_state == GAME_TURN_STATE_IDLE)
                    record_real_map_natural_ai_turn(&session, summary);
                now += 20;
                continue;
            }
            if (session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS) {
                RouteEventExecutionResult execution_result;
                GameEvent event;

                rich4_game_event_queue_reset(&presentation_events);
                rich4_audio_event_queue_reset(&audio_events);
                execution_result = step_real_map_route_events(
                    &session, now, 0, &presentation_events, &audio_events);
                consume_real_map_action_events(
                    &session, &presentation_events, now, NULL, NULL);
                submit_real_map_ai_interaction(&session);
                drain_real_map_property_presentation(&session, &now, NULL);
                drain_real_map_special_presentation(&session, &now);
                while (rich4_game_event_queue_pop(&presentation_events,
                                                  &event)) {
                    (void)event;
                }
                {
                    Rich4AudioEvent audio;
                    while (rich4_audio_event_pop(&audio_events, &audio)) {
                        assert(audio.type == RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY);
                        assert(audio.value >= 2 && audio.value <= 16);
                    }
                }
                if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED &&
                    session.flow.route_events.queue.count == 0 &&
                    !session.presentation.route_interaction.active &&
                    !session.presentation.special_event.active &&
                    session.presentation.insurance.count == 0 &&
                    session.flow.property_turn_end_ticks == 0) {
                    if (!rich4_app_route_event_finish(&session, now)) {
                        Rich4AppTurnRotationResult rotation;
                        memset(&rotation, 0, sizeof(rotation));
                        assert(complete_real_map_route_turn(&session, map, &now, &rotation));
                    }
                    if (session.flow.turn_state == GAME_TURN_STATE_IDLE)
                        record_real_map_natural_ai_turn(&session, summary);
                } else {
                    assert(execution_result != ROUTE_EVENT_EXECUTION_INVALID);
                }
                now += 20;
                continue;
            }
            if (session.flow.turn_state == GAME_TURN_STATE_IDLE) {
                break;
            }
            assert(0 && "unexpected natural AI turn state");
        }
        assert(inner < 20000);
    }

    assert(summary->natural_ai_turn_count >= session.state.player_count * 2);
    assert(summary->natural_ai_route_fact_count > 0);
    assert(session.state.game_over);
    assert(session.state.winner_player >= 0 &&
           session.state.winner_player < session.state.player_count);
    assert(session.state.winner_reason == GAME_WIN_CANDIDATE_DAY_LIMIT);
    assert(session.state.full_round_count > 0);
    summary->natural_ai_full_round_count = session.state.full_round_count;
    summary->natural_ai_elapsed_days = session.state.elapsed_days;
    summary->natural_ai_game_over = session.state.game_over;
    summary->natural_ai_winner_player = session.state.winner_player;
    summary->natural_ai_winner_reason = session.state.winner_reason;
    summary->natural_ai_state_digest = game_state_digest(&session.state);
}

static void run_real_map_route_action_smoke(
    const MapData *map, RealMapReplaySummary *summary)
{
    int action;
    uint32_t digest = UINT32_C(2166136261);

    assert(map != NULL && summary != NULL);
    summary->route_action_smoke_count = 0;
    summary->route_action_smoke_mask = 0;
    for (action = 1; action <= 16; action++) {
        GameSession session;
        GameEventQueue presentation_events;
        Rich4AudioEventQueue audio_events;
        RouteEventExecutionResult execution_result =
            ROUTE_EVENT_EXECUTION_INVALID;
        uint32_t now;
        int target;
        int calls;
        int saw_special = 0;
        int saw_special_place = 0;
        uint32_t action_digest;

        if (!find_real_map_action_target(map, action, NULL, &target))
            continue;
        prepare_real_map_action_stop(map, action, &session, &now, &target);
        rich4_app_route_prepare_dispatch(&session, session.state.current_player);
        for (calls = 0; calls < 512; calls++) {
            rich4_game_event_queue_reset(&presentation_events);
            rich4_audio_event_queue_reset(&audio_events);
            execution_result = step_real_map_route_events(
                &session, now, 0, &presentation_events, &audio_events);
            consume_real_map_action_events(
                &session, &presentation_events, now,
                &saw_special, &saw_special_place);
            if (action == 16 && calls == 0) {
                assert(session.presentation.magic_house.active);
                assert(session.presentation.magic_house.phase ==
                       RICH4_MAGIC_HOUSE_AI_NOTICE);
                assert(session.flow.route_events.state.waiting_for_presentation);
            }
            submit_real_map_ai_interaction(&session);
            drain_real_map_special_presentation(&session, &now);
            if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED &&
                session.flow.route_events.queue.count == 0 &&
                !session.presentation.route_interaction.active &&
                !session.presentation.special_event.active &&
                !session.presentation.magic_house.active &&
                session.presentation.insurance.count == 0)
                break;
            assert(execution_result != ROUTE_EVENT_EXECUTION_INVALID);
            now += 20;
        }
        assert(calls < 512);
        assert(execution_result == ROUTE_EVENT_EXECUTION_FINISHED);
        assert(session.flow.route_events.queue.count == 0);
        assert(!session.presentation.route_interaction.active);
        assert(!session.presentation.special_event.active);
        assert(!session.presentation.magic_house.active);
        assert(!session.flow.route_events.state.waiting_for_presentation);
        assert(session.presentation.insurance.count == 0);
        assert(session.flow.route_events.state.last_result ==
               ROUTE_EVENT_APPLIED);
        if (action == 2 || action == 3)
            assert(saw_special);
        if (action >= 10 && action <= 13)
            assert(saw_special_place);
        finish_real_map_property_stop(&session, map, now, execution_result);
        action_digest = game_state_digest(&session.state);
        digest ^= (uint32_t)action;
        digest *= UINT32_C(16777619);
        digest ^= action_digest;
        digest *= UINT32_C(16777619);
        summary->route_action_smoke_count++;
        summary->route_action_smoke_mask |= 1u << action;
        (void)saw_special;
        (void)saw_special_place;
    }
    assert(summary->route_action_smoke_count > 0);
    summary->route_action_smoke_digest = digest;
}

static int real_map_next_matches(const MapData *map, int node, int previous,
                                 int next)
{
    int options[4];
    int count;
    int i;

    count = rich4_route_collect_non_return(map, node, previous, options);
    for (i = 0; i < count; i++)
        if (options[i] == next) return 1;
    return 0;
}

static void run_real_map_asset_walk(const MapData *map,
                                    RealMapReplaySummary *summary)
{
    GameSession session;
    Rich4AppTurnMovementResult movement;
    Rich4AudioEventQueue audio_events;
    GameEventQueue presentation_events;
    Rich4AppTurnRotationResult rotation;
    RouteEventExecutionResult execution_result;
    uint32_t now = 100;
    int land_target;
    int facility_target;
    int land_index;
    int facility_index;
    int land_visit_mask = 0;
    int facility_visit_mask = 0;
    int phase = 0;
    int turn;
    int actor;

    assert(map != NULL && summary != NULL);
    assert(find_real_map_asset_target(
        map, GAME_ROUTE_OBJECT_LAND, &land_target, &land_index));
    assert(find_real_map_asset_target(
        map, GAME_ROUTE_OBJECT_FACILITY,
        &facility_target, &facility_index));
    initialize_session(&session, map);
    session.state.day_limit = 300;
    /* Keep the asset-competition walk focused on land/facility ownership;
     * the dedicated dynamic-object replay already covers god acquisition.
     * Clearing the presentation pool prevents an unrelated trade-blocking
     * god from making an otherwise valid AI purchase command reject. */
    memset(session.presentation.dynamic_objects, 0,
           sizeof(session.presentation.dynamic_objects));
    for (actor = 0; actor < session.state.player_count; actor++) {
        session.state.players[actor].is_ai = 1;
        session.state.players[actor].play_state = 2;
        session.state.players[actor].cash = 20000;
        session.state.players[actor].bank = 500000;
    }
    session.flow.turn_flow_active = 1;
    session.state.current_player = 0;

    summary->asset_turn_count = 0;
    summary->asset_land_visit_mask = 0;
    summary->asset_facility_visit_mask = 0;
    summary->asset_land_index = land_index;
    summary->asset_land_owner = -1;
    summary->asset_facility_index = facility_index;
    summary->asset_facility_owner = -1;
    summary->asset_land_charge_count = 0;
    summary->asset_facility_charge_count = 0;
    summary->asset_game_over = 0;
    summary->asset_winner_player = -1;
    summary->asset_winner_reason = GAME_WIN_CANDIDATE_NONE;
    summary->asset_state_digest = 0;

    for (turn = 0; turn < 1200 && !session.state.game_over; turn++) {
        GamePlayerState *player;
        RouteMoveResult move_result;
        int target;
        int next;
        int calls;

        actor = session.state.current_player;
        assert(actor >= 0 && actor < session.state.player_count);
        player = &session.state.players[actor];
        target = phase == 0 ? land_target :
                 phase == 1 ? facility_target : 0;
        if (target != 0 && player->node != target)
            next = real_map_path_next(
                map, player->previous_node, player->node, target);
        else
            next = 0;
        if (next == 0 || !real_map_next_matches(
                map, player->node, player->previous_node, next))
            next = real_map_safe_next(
                map, player->node, player->previous_node);
        if (next == 0) {
            int options[4];
            int option_count = rich4_route_collect_non_return(
                map, player->node, player->previous_node, options);
            if (option_count > 0) next = options[0];
        }
        assert(next > 0);

        rich4_app_route_reset_facts(&session);
        player->next_node = next;
        session.presentation.actors[actor].move_active = 0;
        session.presentation.movement_tick = 0;
        move_result = rich4_route_move_begin_with_next(
            &session.flow.route_move, map, player->node,
            player->previous_node, next, 1);
        assert(move_result == ROUTE_MOVE_READY &&
               session.flow.route_move.next_node == next);
        session.flow.turn_state = GAME_TURN_STATE_MOVE;
        session.flow.steps_remaining = 1;
        for (calls = 0; calls < 2000 &&
             session.flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
            assert(rich4_app_update_turn_movement(
                &session, map, 20, now, &movement));
            now += 20;
        }
        assert(calls < 2000);
        assert(session.state.players[actor].node == next);
        if (session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS) {
            for (calls = 0; calls < 512; calls++) {
                GameEvent event;
                rich4_game_event_queue_reset(&presentation_events);
                rich4_audio_event_queue_reset(&audio_events);
                execution_result = step_real_map_route_events(
                    &session, now, 0, &presentation_events, &audio_events);
                consume_real_map_action_events(
                    &session, &presentation_events, now, NULL, NULL);
                submit_real_map_ai_interaction(&session);
                drain_real_map_property_presentation(&session, &now, NULL);
                drain_real_map_special_presentation(&session, &now);
                while (rich4_game_event_queue_pop(&presentation_events,
                                                  &event))
                    (void)event;
                if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED &&
                    session.flow.route_events.queue.count == 0 &&
                    session.flow.property_turn_end_ticks == 0 &&
                    session.presentation.property.phase ==
                        GAME_PROPERTY_PRESENT_NONE &&
                    !session.presentation.facility_roulette.active &&
                    !session.presentation.route_interaction.active &&
                    !session.presentation.special_event.active &&
                    session.presentation.insurance.count == 0)
                    break;
                assert(execution_result != ROUTE_EVENT_EXECUTION_INVALID);
                now += 20;
            }
            assert(calls < 512);
            assert(execution_result == ROUTE_EVENT_EXECUTION_FINISHED);
            assert(session.flow.route_events.queue.count == 0);
            while (rich4_game_event_queue_pop(&presentation_events, NULL)) {}
            assert(!rich4_app_route_event_finish(&session, now));
            memset(&rotation, 0, sizeof(rotation));
            assert(complete_real_map_route_turn(&session, map, &now, &rotation));
            assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
        } else {
            assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
        }

        if (target != 0 && session.state.players[actor].node == target) {
            if (phase == 0)
                land_visit_mask |= 1 << actor;
            else if (phase == 1)
                facility_visit_mask |= 1 << actor;
        }
        if (phase == 0 && land_visit_mask ==
                (1 << session.state.player_count) - 1)
            phase = 1;
        else if (phase == 1 && facility_visit_mask ==
                (1 << session.state.player_count) - 1)
            phase = 2;

        {
            int fact_index;
            for (fact_index = 0;
                 fact_index < session.flow.route_facts.count; fact_index++) {
                const RouteEvent *fact = rich4_route_event_collection_at(
                    &session.flow.route_facts, fact_index);
                if (fact == NULL || fact->phase != ROUTE_EVENT_PHASE_STOP ||
                    fact->snapshot.property_branch !=
                        GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER)
                    continue;
                if (fact->snapshot.event_kind == GAME_ROUTE_EVENT_LAND)
                    summary->asset_land_charge_count++;
                else if (fact->snapshot.event_kind == GAME_ROUTE_EVENT_FACILITY)
                    summary->asset_facility_charge_count++;
            }
        }
        summary->asset_turn_count++;
    }

    assert(land_visit_mask == (1 << session.state.player_count) - 1);
    assert(facility_visit_mask == (1 << session.state.player_count) - 1);
    assert(summary->asset_land_charge_count > 0);
    assert(summary->asset_facility_charge_count > 0);
    assert(session.state.game_over);
    summary->asset_land_visit_mask = land_visit_mask;
    summary->asset_facility_visit_mask = facility_visit_mask;
    summary->asset_land_owner = session.state.land_owner[land_index];
    summary->asset_facility_owner = session.state.facility_owner[facility_index];
    summary->asset_game_over = session.state.game_over;
    summary->asset_winner_player = session.state.winner_player;
    summary->asset_winner_reason = session.state.winner_reason;
    summary->asset_state_digest = game_state_digest(&session.state);
}

static void run_real_map_asset_target_walk(
    const MapData *map, RealMapReplaySummary *summary)
{
    GameSession session;
    Rich4AppTurnMovementResult movement;
    Rich4AudioEventQueue audio_events;
    GameEventQueue presentation_events;
    Rich4AppTurnRotationResult rotation;
    RouteEventExecutionResult execution_result;
    uint32_t now = 100;
    int land_target;
    int land_index;
    int land_visit_mask = 0;
    int land_charge_count = 0;
    int target_initial_assets;
    int turn;
    int actor;

    assert(map != NULL && summary != NULL);
    assert(find_real_map_asset_target(
        map, GAME_ROUTE_OBJECT_LAND, &land_target, &land_index));
    initialize_session(&session, map);
    /* Use a fixed alternate placement seed for this target replay so the
     * live dynamic-object pool itself is part of the evidence while the
     * other real-map smokes retain the common setup seed. */
    session.state.ai_rng_state = REAL_MAP_REPLAY_SEED;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    rich4_app_initialize_dynamic_map_objects_with_rng(
        &session.presentation, map, &session.state.ai_rng_state);
    {
        int dynamic_count = 0;
        for (actor = 0; actor < GAME_MAP_DYNAMIC_OBJECT_COUNT; actor++)
            if (session.presentation.dynamic_objects[actor].node != 0)
                dynamic_count++;
        assert(dynamic_count == 8);
        summary->target_dynamic_initial_count = dynamic_count;
    }
    /* Use the lowest real new-game asset target (X3) and leave the day limit
     * disabled.  Unlike the preceding buy/charge walk, this run keeps the
     * new-game cash/bank split and lets the normal route executor's repeated
     * visits and ledger settlements create the value needed to reach target. */
    session.state.day_limit = 0;
    target_initial_assets = rich4_player_total_assets(&session.state, 0, NULL);
    assert(target_initial_assets > 0);
    session.state.win_target = session.state.initial_fund * 3;
    assert(session.state.win_target > target_initial_assets);
    /* Keep the eight dynamic objects created by the real-map runtime.  This
     * target route therefore exercises the normal road-god landing effects
     * instead of isolating the property/ledger path from that pool. */
    for (actor = 0; actor < session.state.player_count; actor++) {
        session.state.players[actor].is_ai = 1;
        session.state.players[actor].play_state = 2;
    }
    session.flow.turn_flow_active = 1;
    session.state.current_player = 0;

    summary->target_turn_count = 0;
    summary->target_land_visit_mask = 0;
    summary->target_land_index = land_index;
    summary->target_land_owner = -1;
    summary->target_land_level = 0;
    summary->target_land_charge_count = 0;
    summary->target_initial_assets = target_initial_assets;
    summary->target_win_target = session.state.win_target;
    summary->target_game_over = 0;
    summary->target_winner_player = -1;
    summary->target_winner_reason = GAME_WIN_CANDIDATE_NONE;
    summary->target_state_digest = 0;

    for (turn = 0; turn < 2400 && !session.state.game_over; turn++) {
        GamePlayerState *player;
        RouteMoveResult move_result;
        int next;
        int calls;

        actor = session.state.current_player;
        assert(actor >= 0 && actor < session.state.player_count);
        player = &session.state.players[actor];
        if (player->node != land_target)
            next = real_map_path_next(
                map, player->previous_node, player->node, land_target);
        else
            next = 0;
        if (next == 0 || !real_map_next_matches(
                map, player->node, player->previous_node, next))
            next = real_map_safe_next(
                map, player->node, player->previous_node);
        if (next == 0) {
            int options[4];
            int option_count = rich4_route_collect_non_return(
                map, player->node, player->previous_node, options);
            if (option_count > 0) next = options[0];
        }
        assert(next > 0);

        rich4_app_route_reset_facts(&session);
        player->next_node = next;
        session.presentation.actors[actor].move_active = 0;
        session.presentation.movement_tick = 0;
        move_result = rich4_route_move_begin_with_next(
            &session.flow.route_move, map, player->node,
            player->previous_node, next, 1);
        assert(move_result == ROUTE_MOVE_READY &&
               session.flow.route_move.next_node == next);
        session.flow.turn_state = GAME_TURN_STATE_MOVE;
        session.flow.steps_remaining = 1;
        for (calls = 0; calls < 2000 &&
             session.flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
            assert(rich4_app_update_turn_movement(
                &session, map, 20, now, &movement));
            now += 20;
        }
        assert(calls < 2000);
        assert(session.state.players[actor].node == next);
        if (session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS) {
            for (calls = 0; calls < 512; calls++) {
                GameEvent event;
                rich4_game_event_queue_reset(&presentation_events);
                rich4_audio_event_queue_reset(&audio_events);
                execution_result = step_real_map_route_events(
                    &session, now, 0, &presentation_events, &audio_events);
                consume_real_map_action_events(
                    &session, &presentation_events, now, NULL, NULL);
                submit_real_map_ai_interaction(&session);
                drain_real_map_property_presentation(
                    &session, &now, NULL);
                drain_real_map_special_presentation(&session, &now);
                while (rich4_game_event_queue_pop(&presentation_events,
                                                  &event))
                    (void)event;
                if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED &&
                    session.flow.route_events.queue.count == 0 &&
                    session.flow.property_turn_end_ticks == 0 &&
                    session.presentation.property.phase ==
                        GAME_PROPERTY_PRESENT_NONE &&
                    !session.presentation.facility_roulette.active &&
                    !session.presentation.route_interaction.active &&
                    !session.presentation.special_event.active &&
                    session.presentation.insurance.count == 0)
                    break;
                assert(execution_result != ROUTE_EVENT_EXECUTION_INVALID);
                now += 20;
            }
            assert(calls < 512);
            assert(execution_result == ROUTE_EVENT_EXECUTION_FINISHED);
            assert(session.flow.route_events.queue.count == 0);
            while (rich4_game_event_queue_pop(&presentation_events, NULL)) {}
            assert(!rich4_app_route_event_finish(&session, now));
            memset(&rotation, 0, sizeof(rotation));
            if (!complete_real_map_route_turn(&session, map, &now, &rotation))
                assert(session.state.game_over);
            if (!session.state.game_over)
                assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
        } else {
            assert(session.flow.turn_state == GAME_TURN_STATE_IDLE ||
                   session.state.game_over);
        }

        if (session.state.players[actor].node == land_target)
            land_visit_mask |= 1 << actor;
        {
            int fact_index;
            for (fact_index = 0;
                 fact_index < session.flow.route_facts.count; fact_index++) {
                const RouteEvent *fact = rich4_route_event_collection_at(
                    &session.flow.route_facts, fact_index);
                if (fact != NULL && fact->phase == ROUTE_EVENT_PHASE_STOP &&
                    fact->snapshot.event_kind == GAME_ROUTE_EVENT_LAND &&
                    fact->snapshot.property_branch ==
                        GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER)
                    land_charge_count++;
            }
        }
        summary->target_turn_count++;
    }

    assert(session.state.game_over);
    assert(session.state.winner_reason ==
           GAME_WIN_CANDIDATE_TARGET_ASSETS);
    assert(session.state.winner_player >= 0 &&
           session.state.winner_player < session.state.player_count);
    /* The asset target can end play while another actor is confined.
     * All-player visit coverage is asserted by the separate asset walk. */
    assert(land_visit_mask != 0);
    assert(land_charge_count > 0);
    assert(rich4_player_total_assets(
               &session.state, session.state.winner_player, NULL) >=
           session.state.win_target);
    summary->target_land_visit_mask = land_visit_mask;
    summary->target_land_owner = session.state.land_owner[land_index];
    summary->target_land_level = session.state.land_level[land_index];
    summary->target_land_charge_count = land_charge_count;
    summary->target_game_over = session.state.game_over;
    summary->target_winner_player = session.state.winner_player;
    summary->target_winner_reason = session.state.winner_reason;
    summary->target_state_digest = game_state_digest(&session.state);
    summary->target_dynamic_attached_count = 0;
    for (actor = 0; actor < GAME_MAP_DYNAMIC_OBJECT_COUNT; actor++)
        if (session.presentation.dynamic_objects[actor].attached_player != 0)
            summary->target_dynamic_attached_count++;
}

static void run_real_map_long_walk(const MapData *map,
                                   RealMapReplaySummary *summary)
{
    GameSession session;
    Rich4AppTurnMovementResult movement;
    Rich4AudioEventQueue audio_events;
    GameEventQueue presentation_events;
    Rich4AppTurnRotationResult rotation;
    RouteEventExecutionResult execution_result;
    uint32_t now = 100;
    int turn;

    assert(map != NULL && summary != NULL);
    initialize_session(&session, map);
    /* Keep this smoke focused on the real-map/app lifecycle.  AI actors use
     * the same route executor but do not stop the run on a human modal. */
    for (turn = 0; turn < session.state.player_count; turn++)
        session.state.players[turn].is_ai = 1;
    /* Use a real, short game configuration so this replay proves the full
     * day-end -> victory -> terminal-state path without depending on save
     * files or a guessed asset threshold. */
    session.state.day_limit = 4;
    session.flow.turn_flow_active = 1;
    session.state.current_player = 0;
    summary->long_turn_count = 0;
    summary->long_stop_count = 0;
    summary->long_route_fact_count = 0;
    summary->long_full_round_count = 0;
    summary->long_elapsed_days = 0;
    summary->long_special_place_count = 0;
    summary->long_game_over = 0;
    summary->long_winner_player = -1;
    summary->long_winner_reason = GAME_WIN_CANDIDATE_NONE;
    summary->long_state_digest = 0;
    memset(summary->long_nodes, 0, sizeof(summary->long_nodes));
    memset(summary->long_players, 0, sizeof(summary->long_players));

    for (turn = 0; turn < (int)(sizeof(summary->long_nodes) /
                               sizeof(summary->long_nodes[0])); turn++) {
        GamePlayerState *player;
        RouteMoveResult move_result;
        int actor;
        int target;
        int calls;

        actor = session.state.current_player;
        assert(actor >= 0 && actor < session.state.player_count);
        player = &session.state.players[actor];
        target = real_map_safe_next(map, player->node, player->previous_node);
        if (target == 0) break;

        rich4_app_route_reset_facts(&session);
        session.flow.turn_flow_active = 1;
        player->next_node = target;
        session.presentation.actors[actor].move_active = 0;
        session.presentation.movement_tick = 0;
        move_result = rich4_route_move_begin_with_next(
            &session.flow.route_move, map, player->node,
            player->previous_node, target, 1);
        assert(move_result == ROUTE_MOVE_READY &&
               session.flow.route_move.next_node == target);
        session.flow.turn_state = GAME_TURN_STATE_MOVE;
        session.flow.steps_remaining = 1;
        for (calls = 0; calls < 2000 &&
             session.flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
            assert(rich4_app_update_turn_movement(
                &session, map, 20, now, &movement));
            now += 20;
        }
        assert(calls < 2000);
        assert(session.state.players[actor].node == target);
        if (session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS) {
            for (calls = 0; calls < 64; calls++) {
                GameEvent event;
                Rich4AppRoutePresentation presentation;
                rich4_audio_event_queue_reset(&audio_events);
                execution_result = step_real_map_route_events(
                    &session, now, 0, &presentation_events, &audio_events);
                while (rich4_game_event_queue_pop(&presentation_events,
                                                  &event)) {
                    GameEventQueue resumed_events;
                    assert(event.type ==
                           GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED);
                    assert(rich4_app_route_event_presentation(
                        &event, &presentation));
                    assert(presentation.kind ==
                           RICH4_ROUTE_PRESENTATION_SPECIAL_PLACE);
                    summary->long_special_place_count++;
                    rich4_game_event_queue_reset(&resumed_events);
                    execution_result =
                        rich4_route_event_executor_resume_special_place(
                            &session.flow.route_events, &session.state,
                            &resumed_events);
                    assert(execution_result ==
                           ROUTE_EVENT_EXECUTION_PROGRESSED);
                    while (rich4_game_event_queue_pop(
                               &resumed_events, &event)) {
                        assert(event.type == GAME_EVENT_INVENTORY_CHANGED ||
                               event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED);
                    }
                }
                {
                    Rich4AudioEvent audio;
                    while (rich4_audio_event_pop(&audio_events, &audio)) {
                        /* 0x419884 plays the shared entry effect for
                         * rewards even when animation is disabled. */
                        assert(audio.type == RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY);
                        assert(audio.value >= 10 && audio.value <= 13);
                    }
                }
                if (execution_result == ROUTE_EVENT_EXECUTION_FINISHED)
                    break;
                assert(execution_result == ROUTE_EVENT_EXECUTION_PROGRESSED);
                now += 20;
            }
            assert(calls < 64 &&
                   execution_result == ROUTE_EVENT_EXECUTION_FINISHED);
            assert(!rich4_app_route_event_finish(&session, now));
            memset(&rotation, 0, sizeof(rotation));
            assert(complete_real_map_route_turn(&session, map, &now, &rotation));
            assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
        } else {
            /* Empty map nodes have no route fact; the app already rotates
             * the turn in the movement flow after the stop is recorded. */
            assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
        }

        summary->long_players[turn] = actor;
        summary->long_nodes[turn] = target;
        summary->long_turn_count++;
        summary->long_route_fact_count += session.flow.route_facts.count;
        summary->long_stop_count++;
    }

    assert(summary->long_turn_count >= 8);
    assert(summary->long_stop_count == summary->long_turn_count);
    assert(summary->long_route_fact_count > 0);
    assert(summary->long_special_place_count > 0);
    assert(summary->long_turn_count >= session.state.player_count * 2);
    assert(session.state.game_over);
    assert(session.state.winner_player >= 0 &&
           session.state.winner_player < session.state.player_count);
    assert(session.state.winner_reason == GAME_WIN_CANDIDATE_DAY_LIMIT);
    assert(session.state.full_round_count > 0);
    summary->long_full_round_count = session.state.full_round_count;
    summary->long_elapsed_days = session.state.elapsed_days;
    summary->long_game_over = session.state.game_over;
    summary->long_winner_player = session.state.winner_player;
    summary->long_winner_reason = session.state.winner_reason;
    summary->long_state_digest = game_state_digest(&session.state);
}

static void run_bootstrap(const MapData *map, RealMapReplaySummary *summary)
{
    GameSession session;
    Rich4AppTurnMovementResult result;
    RouteMoveResult move_result;
    int start_node;
    int old_node;
    int moved_steps = 0;
    int calls;
    uint32_t now = 100;
    int i;

    assert(map != NULL && summary != NULL);
    memset(summary, 0, sizeof(*summary));
    summary->special_reward_card = -1;
    summary->special_reward_pool_remaining = -1;
    initialize_session(&session, map);
    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        const GameMapDynamicObjectState *object =
            &session.presentation.dynamic_objects[i];
        RealMapDynamicSummary *saved = &summary->dynamic_objects[i];
        saved->type = object->type;
        saved->node_direction = object->node_direction;
        saved->node = object->node;
        saved->countdown = object->countdown;
        saved->attached_player = object->attached_player;
        saved->flight_active = object->flight_active;
        saved->flight_direction = object->flight_direction;
        saved->world_x = object->world_x;
        saved->world_y = object->world_y;
        saved->velocity_x = object->velocity_x;
        saved->velocity_y = object->velocity_y;
        if (object->node != 0) summary->dynamic_count++;
    }
    assert(summary->dynamic_count == 8);
    start_node = session.state.players[0].node;
    old_node = start_node;
    move_result = rich4_route_move_begin_with_next(
        &session.flow.route_move, map, start_node,
        session.state.players[0].previous_node,
        session.state.players[0].next_node, 3);
    assert(move_result == ROUTE_MOVE_READY);
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.steps_remaining = 3;

    for (calls = 0; calls < 2000 &&
         session.flow.turn_state == GAME_TURN_STATE_MOVE; calls++) {
        assert(rich4_app_update_turn_movement(
            &session, map, 20, now, &result));
        now += 20;
        if (session.state.players[0].node != old_node) {
            moved_steps++;
            old_node = session.state.players[0].node;
        }
    }

    assert(calls < 2000);
    assert(moved_steps == 3);
    /* 0x40D7C4 drains arrival and five idle ticks before 0x418E7F
     * dispatches the stopped square, even after remaining steps reach 0. */
    assert(session.flow.steps_remaining == 0);
    assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(session.state.players[0].node > 0 &&
           (size_t)session.state.players[0].node < map->route_count);
    /* 0x4198C3 returns without an interaction for action 0 / raw 0.
     * An ordinary stopped square has exactly one STOP fact otherwise. */
    assert(session.flow.property_turn_end_ticks == 0);
    assert(session.flow.route_facts.count ==
           ((map->routes[session.state.players[0].node].flags & 0xffu) != 0 ||
            map->routes[session.state.players[0].node].object.raw_object_id != 0));
    summary->moved_steps = moved_steps;
    summary->final_node = session.state.players[0].node;
    summary->previous_node = session.state.players[0].previous_node;
    summary->next_node = session.state.players[0].next_node;
    summary->route_fact_count = session.flow.route_facts.count;
    assert(summary->route_fact_count <= GAME_MAX_TURN_ROUTE_EVENTS);
    for (i = 0; i < summary->route_fact_count; i++) {
        const RouteEvent *fact = rich4_route_event_collection_at(
            &session.flow.route_facts, i);
        RealMapFactSummary *saved = &summary->facts[i];
        assert(fact != NULL);
        assert(fact->phase == ROUTE_EVENT_PHASE_STOP);
        assert(fact->node == summary->final_node);
        saved->phase = fact->phase;
        saved->node = fact->node;
        saved->raw_object_id = fact->object_ref.raw_object_id;
        saved->event_kind = fact->snapshot.event_kind;
        saved->route_action = fact->snapshot.route_action;
        saved->property_branch = fact->snapshot.property_branch;
        saved->property_owner = fact->snapshot.property_owner;
        saved->property_value = fact->snapshot.property_value;
        saved->dice_total = fact->snapshot.dice_total;
    }
    run_real_map_special_stop(&session, map, summary);
    summary->property_land_index = -1;
    summary->property_land_owner = -1;
    summary->property_land_level = -1;
    summary->property_facility_index = -1;
    summary->property_facility_owner = -1;
    summary->property_facility_level = -1;
    run_real_map_property_stop(
        map, GAME_ROUTE_OBJECT_LAND, &summary->property_land_index,
        &summary->property_land_owner, &summary->property_land_level);
    run_real_map_property_stop(
        map, GAME_ROUTE_OBJECT_FACILITY, &summary->property_facility_index,
        &summary->property_facility_owner, &summary->property_facility_level);
    summary->owned_land_index = -1;
    summary->owned_land_owner = -1;
    summary->owned_land_actor_cash_before = -1;
    summary->owned_land_actor_cash_after = -1;
    summary->owned_land_owner_bank_before = -1;
    summary->owned_land_owner_bank_after = -1;
    summary->owned_land_route_result = ROUTE_EVENT_EXECUTION_INVALID;
    summary->owned_facility_index = -1;
    summary->owned_facility_owner = -1;
    summary->owned_facility_type = -1;
    summary->owned_facility_roulette_outcome = -1;
    summary->owned_facility_actor_cash_before = -1;
    summary->owned_facility_actor_cash_after = -1;
    summary->owned_facility_owner_bank_before = -1;
    summary->owned_facility_owner_bank_after = -1;
    summary->owned_facility_route_result = ROUTE_EVENT_EXECUTION_INVALID;
    run_real_map_owned_land_charge(map, summary);
    run_real_map_owned_facility_roulette(map, summary);
    run_real_map_route_action_smoke(map, summary);
    run_real_map_asset_walk(map, summary);
    run_real_map_asset_target_walk(map, summary);
    run_real_map_long_walk(map, summary);
    run_real_map_natural_ai(map, summary);
}

static void assert_replay_equal(const RealMapReplaySummary *first,
                                const RealMapReplaySummary *second)
{
    int i;

    assert(first != NULL && second != NULL);
    assert(first->moved_steps == second->moved_steps);
    assert(first->final_node == second->final_node);
    assert(first->previous_node == second->previous_node);
    assert(first->next_node == second->next_node);
    assert(first->route_fact_count == second->route_fact_count);
    assert(first->dynamic_count == second->dynamic_count);
    assert(first->route_event_applied == second->route_event_applied);
    assert(first->route_action_applied == second->route_action_applied);
    assert(first->points_after_route_event == second->points_after_route_event);
    assert(first->special_request_resource == second->special_request_resource);
    assert(first->special_request_hold_ms == second->special_request_hold_ms);
    assert(first->special_reward_card == second->special_reward_card);
    assert(first->special_reward_amount == second->special_reward_amount);
    assert(first->special_reward_inventory_count ==
           second->special_reward_inventory_count);
    assert(first->special_reward_pool_remaining ==
           second->special_reward_pool_remaining);
    assert(first->route_event_result == second->route_event_result);
    assert(first->property_land_index == second->property_land_index);
    assert(first->property_land_owner == second->property_land_owner);
    assert(first->property_land_level == second->property_land_level);
    assert(first->property_facility_index == second->property_facility_index);
    assert(first->property_facility_owner == second->property_facility_owner);
    assert(first->property_facility_level == second->property_facility_level);
    assert(first->owned_land_index == second->owned_land_index);
    assert(first->owned_land_owner == second->owned_land_owner);
    assert(first->owned_land_actor_cash_before ==
           second->owned_land_actor_cash_before);
    assert(first->owned_land_actor_cash_after ==
           second->owned_land_actor_cash_after);
    assert(first->owned_land_owner_bank_before ==
           second->owned_land_owner_bank_before);
    assert(first->owned_land_owner_bank_after ==
           second->owned_land_owner_bank_after);
    assert(first->owned_land_route_result == second->owned_land_route_result);
    assert(first->owned_facility_index == second->owned_facility_index);
    assert(first->owned_facility_owner == second->owned_facility_owner);
    assert(first->owned_facility_type == second->owned_facility_type);
    assert(first->owned_facility_roulette_outcome ==
           second->owned_facility_roulette_outcome);
    assert(first->owned_facility_actor_cash_before ==
           second->owned_facility_actor_cash_before);
    assert(first->owned_facility_actor_cash_after ==
           second->owned_facility_actor_cash_after);
    assert(first->owned_facility_owner_bank_before ==
           second->owned_facility_owner_bank_before);
    assert(first->owned_facility_owner_bank_after ==
           second->owned_facility_owner_bank_after);
    assert(first->owned_facility_route_result ==
           second->owned_facility_route_result);
    assert(first->route_action_smoke_count ==
           second->route_action_smoke_count);
    assert(first->route_action_smoke_mask == second->route_action_smoke_mask);
    assert(first->route_action_smoke_digest ==
           second->route_action_smoke_digest);
    assert(first->asset_turn_count == second->asset_turn_count);
    assert(first->asset_land_visit_mask == second->asset_land_visit_mask);
    assert(first->asset_facility_visit_mask ==
           second->asset_facility_visit_mask);
    assert(first->asset_land_index == second->asset_land_index);
    assert(first->asset_land_owner == second->asset_land_owner);
    assert(first->asset_facility_index == second->asset_facility_index);
    assert(first->asset_facility_owner == second->asset_facility_owner);
    assert(first->asset_land_charge_count ==
           second->asset_land_charge_count);
    assert(first->asset_facility_charge_count ==
           second->asset_facility_charge_count);
    assert(first->asset_game_over == second->asset_game_over);
    assert(first->asset_winner_player == second->asset_winner_player);
    assert(first->asset_winner_reason == second->asset_winner_reason);
    assert(first->asset_state_digest == second->asset_state_digest);
    assert(first->target_turn_count == second->target_turn_count);
    assert(first->target_land_visit_mask == second->target_land_visit_mask);
    assert(first->target_land_index == second->target_land_index);
    assert(first->target_land_owner == second->target_land_owner);
    assert(first->target_land_level == second->target_land_level);
    assert(first->target_land_charge_count ==
           second->target_land_charge_count);
    assert(first->target_initial_assets == second->target_initial_assets);
    assert(first->target_win_target == second->target_win_target);
    assert(first->target_game_over == second->target_game_over);
    assert(first->target_winner_player == second->target_winner_player);
    assert(first->target_winner_reason == second->target_winner_reason);
    assert(first->target_state_digest == second->target_state_digest);
    assert(first->target_dynamic_initial_count ==
           second->target_dynamic_initial_count);
    assert(first->target_dynamic_attached_count ==
           second->target_dynamic_attached_count);
    assert(first->long_turn_count == second->long_turn_count);
    assert(first->long_stop_count == second->long_stop_count);
    assert(first->long_route_fact_count == second->long_route_fact_count);
    assert(first->long_full_round_count == second->long_full_round_count);
    assert(first->long_elapsed_days == second->long_elapsed_days);
    assert(first->long_special_place_count ==
           second->long_special_place_count);
    assert(first->long_game_over == second->long_game_over);
    assert(first->long_winner_player == second->long_winner_player);
    assert(first->long_winner_reason == second->long_winner_reason);
    assert(first->long_state_digest == second->long_state_digest);
    assert(first->natural_ai_turn_count == second->natural_ai_turn_count);
    assert(first->natural_ai_stop_count == second->natural_ai_stop_count);
    assert(first->natural_ai_route_fact_count ==
           second->natural_ai_route_fact_count);
    assert(first->natural_ai_full_round_count ==
           second->natural_ai_full_round_count);
    assert(first->natural_ai_elapsed_days == second->natural_ai_elapsed_days);
    assert(first->natural_ai_game_over == second->natural_ai_game_over);
    assert(first->natural_ai_winner_player ==
           second->natural_ai_winner_player);
    assert(first->natural_ai_winner_reason ==
           second->natural_ai_winner_reason);
    assert(first->natural_ai_state_digest == second->natural_ai_state_digest);
    for (i = 0; i < first->long_turn_count; i++) {
        assert(first->long_nodes[i] == second->long_nodes[i]);
        assert(first->long_players[i] == second->long_players[i]);
    }
    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        const RealMapDynamicSummary *a = &first->dynamic_objects[i];
        const RealMapDynamicSummary *b = &second->dynamic_objects[i];
        assert(a->type == b->type &&
               a->node_direction == b->node_direction &&
               a->node == b->node &&
               a->countdown == b->countdown &&
               a->attached_player == b->attached_player &&
               a->flight_active == b->flight_active &&
               a->flight_direction == b->flight_direction &&
               a->world_x == b->world_x && a->world_y == b->world_y &&
               a->velocity_x == b->velocity_x &&
               a->velocity_y == b->velocity_y);
    }
    for (i = 0; i < first->route_fact_count; i++) {
        const RealMapFactSummary *a = &first->facts[i];
        const RealMapFactSummary *b = &second->facts[i];
        assert(a->phase == b->phase && a->node == b->node &&
               a->raw_object_id == b->raw_object_id &&
               a->event_kind == b->event_kind &&
               a->route_action == b->route_action &&
               a->property_branch == b->property_branch &&
               a->property_owner == b->property_owner &&
               a->property_value == b->property_value &&
               a->dice_total == b->dice_total);
    }
}

int main(void)
{
    const char *path = "assets/original/Map.mkf";
    MapData map;
    RealMapReplaySummary first;
    RealMapReplaySummary second;

    memset(&map, 0, sizeof(map));
    {
        FILE *file = fopen(path, "rb");
        if (file == NULL) {
            puts("real-map bootstrap replay: skipped (Map.mkf unavailable)");
            return 0;
        }
        fclose(file);
    }
    assert(load_map(path, &map));
    assert(map.route_count > 100 && map.land_count > 1 &&
           map.facility_count > 1);
    run_bootstrap(&map, &first);
    run_bootstrap(&map, &second);
    assert_replay_equal(&first, &second);
    printf("real-map bootstrap replay: gmap0 routes=%lu lands=%lu facilities=%lu "
           "dynamic=%d moved=%d facts=%d route_action=%d applied=%d points=%d "
           "final_node=%d request=%d/%d reward_card=%d reward_count=%d "
           "pool=%d result=%d deterministic=2x\n",
           (unsigned long)map.route_count, (unsigned long)map.land_count,
           (unsigned long)map.facility_count, first.dynamic_count,
           first.moved_steps, first.route_fact_count,
           first.route_action_applied, first.route_event_applied,
           first.points_after_route_event,
           first.final_node, first.special_request_resource,
           first.special_request_hold_ms, first.special_reward_card,
           first.special_reward_inventory_count,
           first.special_reward_pool_remaining, first.route_event_result);
    printf("real-map long walk: turns=%d stops=%d facts=%d rounds=%d "
           "days=%d special_places=%d game_over=%d winner=%d reason=%d "
           "state=%08x deterministic=2x\n",
           first.long_turn_count, first.long_stop_count,
           first.long_route_fact_count, first.long_full_round_count,
           first.long_elapsed_days, first.long_special_place_count,
           first.long_game_over, first.long_winner_player,
           first.long_winner_reason,
           (unsigned int)first.long_state_digest);
    printf("real-map natural AI: turns=%d stops=%d facts=%d rounds=%d "
           "days=%d game_over=%d winner=%d reason=%d state=%08x "
           "deterministic=2x\n",
           first.natural_ai_turn_count, first.natural_ai_stop_count,
           first.natural_ai_route_fact_count,
           first.natural_ai_full_round_count, first.natural_ai_elapsed_days,
           first.natural_ai_game_over, first.natural_ai_winner_player,
           first.natural_ai_winner_reason,
           (unsigned int)first.natural_ai_state_digest);
    printf("real-map property stops: land=%d/owner%d/level%d "
           "facility=%d/owner%d/level%d deterministic=2x\n",
           first.property_land_index, first.property_land_owner,
           first.property_land_level, first.property_facility_index,
           first.property_facility_owner, first.property_facility_level);
    printf("real-map owned property: land=%d owner=%d cash=%d->%d/bank=%d->%d "
           "result=%d facility=%d/type%d owner=%d outcome=%d "
           "cash=%d->%d/bank=%d->%d result=%d deterministic=2x\n",
           first.owned_land_index, first.owned_land_owner,
           first.owned_land_actor_cash_before, first.owned_land_actor_cash_after,
           first.owned_land_owner_bank_before, first.owned_land_owner_bank_after,
           first.owned_land_route_result, first.owned_facility_index,
           first.owned_facility_type, first.owned_facility_owner,
           first.owned_facility_roulette_outcome,
           first.owned_facility_actor_cash_before,
           first.owned_facility_actor_cash_after,
           first.owned_facility_owner_bank_before,
           first.owned_facility_owner_bank_after,
           first.owned_facility_route_result);
    printf("real-map route actions: count=%d mask=%04x digest=%08x "
           "deterministic=2x\n", first.route_action_smoke_count,
           first.route_action_smoke_mask,
           (unsigned int)first.route_action_smoke_digest);
    printf("real-map asset walk: turns=%d land=%d/owner%d visits=%d "
           "land_charges=%d facility=%d/owner%d visits=%d "
           "facility_charges=%d game_over=%d winner=%d reason=%d "
           "state=%08x deterministic=2x\n",
           first.asset_turn_count, first.asset_land_index,
           first.asset_land_owner, first.asset_land_visit_mask,
           first.asset_land_charge_count, first.asset_facility_index,
           first.asset_facility_owner, first.asset_facility_visit_mask,
           first.asset_facility_charge_count, first.asset_game_over,
           first.asset_winner_player, first.asset_winner_reason,
           (unsigned int)first.asset_state_digest);
    printf("real-map asset target: turns=%d land=%d/owner%d/level%d visits=%d "
           "charges=%d initial=%d target=%d dynamic=%d/attached%d "
           "game_over=%d winner=%d reason=%d "
           "state=%08x deterministic=2x\n",
           first.target_turn_count, first.target_land_index,
           first.target_land_owner, first.target_land_level,
           first.target_land_visit_mask, first.target_land_charge_count,
           first.target_initial_assets, first.target_win_target,
           first.target_dynamic_initial_count,
           first.target_dynamic_attached_count,
           first.target_game_over, first.target_winner_player,
           first.target_winner_reason,
           (unsigned int)first.target_state_digest);
    rich4_map_data_free(&map);
    return 0;
}
