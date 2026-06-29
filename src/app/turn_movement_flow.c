#include "app/turn_movement_flow.h"

#include <stddef.h>
#include <string.h>

#include "app/dice_flow.h"
#include "app/special_presentation_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/route_facts.h"
#include "app/route_flow.h"
#include "app/property_flow.h"
#include "core/ledger.h"
#include "core/player_status.h"
#include "core/route_event_executor.h"
#include "app/tool_followup_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/god.h"
#include "core/route_movement.h"
#include "core/tool.h"
#include "debug/trace.h"
#include "view/game_view_model.h"

static const int rich4_move_speed[4] = { 8, 12, 16, 8 };

static void route_fallback_xy(int node, int *out_x, int *out_y)
{
    static const int points[GAME_ROUTE_NODE_COUNT][2] = {
        { 96, 380 }, { 150, 380 }, { 204, 380 }, { 258, 380 },
        { 312, 380 }, { 366, 380 }, { 420, 380 }, { 474, 380 },
        { 528, 380 }, { 528, 330 }, { 528, 280 }, { 528, 230 },
        { 528, 180 }, { 474, 180 }, { 420, 180 }, { 366, 180 },
        { 312, 180 }, { 258, 180 }, { 204, 180 }, { 150, 180 },
        { 96, 180 }, { 96, 230 }, { 96, 280 }, { 96, 330 }
    };
    node = node < 0 ? 0 : node % GAME_ROUTE_NODE_COUNT;
    if (out_x != NULL) *out_x = points[node][0];
    if (out_y != NULL) *out_y = points[node][1];
}

static int route_node_valid(const MapData *map, int node)
{
    if (map == NULL || map->routes == NULL || map->route_count <= 1)
        return node >= 0 && node < GAME_ROUTE_NODE_COUNT;
    return node >= 1 && (size_t)node < map->route_count &&
           (map->routes[node].next[0] != 0 ||
            map->routes[node].next[1] != 0 ||
            map->routes[node].next[2] != 0 ||
            map->routes[node].next[3] != 0);
}

static int route_first_node(const MapData *map)
{
    return map != NULL && map->routes != NULL && map->route_count > 1 ? 1 : 0;
}

static void route_node_xy(const MapData *map, int node,
                          int *out_x, int *out_y)
{
    if (map != NULL && map->routes != NULL && map->route_count > 1) {
        if (node <= 0 || (size_t)node >= map->route_count)
            node = route_first_node(map);
        if (out_x != NULL) *out_x = map->routes[node].x;
        if (out_y != NULL) *out_y = map->routes[node].y;
        return;
    }
    route_fallback_xy(node, out_x, out_y);
}

static int original_actor_uses_ea1_movement_lane(
    const GamePlayerState *player,
    const MapData *map,
    int destination_node)
{
    if (player == NULL) return 0;
    /* sub_40B93B selects EA1=1 for an inactive/frozen actor or a signed
     * high-bit route node. sub_40C05C writes the selected destination to
     * word_496B74 before calling sub_40B93B, so the lane check is on the
     * destination node rather than the edge's source. EA1 then uses its
     * fixed 1/8 world-unit step, independent of the traffic-method table. */
    if ((player->play_state & 0xff) == 0 ||
        (player->play_state & GAME_PLAYER_PLAY_STATE_FROZEN) != 0)
        return 1;
    return map != NULL && map->routes != NULL && destination_node > 0 &&
        (size_t)destination_node < map->route_count &&
        (map->routes[destination_node].flags & 0x80000000u) != 0;
}

static int route_next_node(const MapData *map, int node, int previous_node)
{
    int options[4];
    int count;

    if (map == NULL || map->routes == NULL || map->route_count <= 1)
        return (node + 1) % GAME_ROUTE_NODE_COUNT;
    if (!route_node_valid(map, node)) return route_first_node(map);
    count = rich4_route_collect_non_return(map, node, previous_node, options);
    return count > 0 ? options[0] : route_first_node(map);
}

static uint64_t int_sqrt_floor_app(uint64_t value)
{
    uint64_t bit = UINT64_C(1) << 62;
    uint64_t result = 0;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

static long double original_route_distance(int dx, int dy)
{
    uint64_t squared = (uint64_t)((int64_t)dx * dx) +
                       (uint64_t)((int64_t)dy * dy);
    long double value = (long double)squared;
    long double root;
    int i;
    if (squared == 0) return 0.0L;
    root = (long double)int_sqrt_floor_app(squared);
    if (root <= 0.0L) root = 1.0L;
    /* Seed with the integer root, then recover the fractional part. The
     * original calls x87 FSQRT on this integer squared distance. */
    for (i = 0; i < 5; i++) root = (root + value / root) * 0.5L;
    return root;
}

static float original_route_duration(int dx, int dy,
                                     int fixed_speed, int speed)
{
    long double distance = original_route_distance(dx, dy);
    long double duration = fixed_speed
        ? distance * 0.125L
        : distance / (long double)(speed > 0 ? speed : 1);
    /* sub_40C05C stores the duration in a single-precision local before
     * converting its integral part into the frame counter. */
    return (float)duration;
}

static int original_route_frame_count(float duration)
{
    int frames = (int)duration;
    return frames > 0 ? frames : 1;
}

static int original_actor_uses_fixed_movement_speed(
    const GamePlayerState *player, const MapData *map, int destination_node)
{
    return original_actor_uses_ea1_movement_lane(
               player, map, destination_node) ||
           (player != NULL && (player->play_state & 0x30) != 0);
}

static float original_active_route_duration(
    const GamePlayerState *player,
    const GameActorPresentationState *actor,
    const MapData *map)
{
    int dx = actor->move_to_x - actor->move_from_x;
    int dy = actor->move_to_y - actor->move_from_y;
    int speed = rich4_move_speed[player->traffic_method & 3];
    float fixed_duration = original_route_duration(dx, dy, 1, 8);
    float traffic_duration = original_route_duration(dx, dy, 0, speed);
    int frames = actor->move_distance;
    int fixed_frames = original_route_frame_count(fixed_duration);
    int traffic_frames = original_route_frame_count(traffic_duration);

    if (original_actor_uses_fixed_movement_speed(
            player, map, actor->move_to_node))
        return fixed_duration;
    /* A +21 status can clear itself halfway through a move. The original
     * keeps its start-of-step duration; for any move with an intermediate
     * frame the two speed paths have distinct integer frame counts unless
     * the traffic speed is already 8, so the captured count recovers it. */
    if (frames > 1 && frames == fixed_frames && frames != traffic_frames)
        return fixed_duration;
    return traffic_duration;
}

static int original_interpolated_axis(int from, int to, int progress,
                                      float duration)
{
    volatile float position = (float)from;
    volatile float step;
    int i;
    if (duration <= 0.0f) return to;
    /* 0x40C05C stores dx/duration into a float global, then adds that float
     * to a float accumulator once per movement call before truncating it to
     * the actor's 16-bit world coordinate. */
    step = (float)((long double)(to - from) / (long double)duration);
    for (i = 0; i < progress; i++)
        position = position + step;
    return (int)position;
}

static void prepare_route_for_step(GameSession *session, GamePlayerState *player)
{
    if (session == NULL || player == NULL) return;
    if (session->flow.route_move.steps_remaining > 0 &&
        session->flow.route_move.node == player->node &&
        session->flow.route_move.next_node > 0)
        player->next_node = session->flow.route_move.next_node;
}

static void start_player_step(GamePlayerState *player,
                              GameActorPresentationState *actor,
                              const MapData *map)
{
    int sx;
    int sy;
    int tx;
    int ty;
    int dx;
    int dy;
    int speed;
    int fixed_speed;
    float duration;

    if (player == NULL || actor == NULL) return;
    if (!route_node_valid(map, player->node))
        player->node = route_first_node(map);
    if (!route_node_valid(map, player->next_node))
        player->next_node = route_next_node(map, player->node,
                                             player->previous_node);
    route_node_xy(map, player->node, &sx, &sy);
    route_node_xy(map, player->next_node, &tx, &ty);
    dx = tx - sx;
    dy = ty - sy;
    actor->move_active = 1;
    actor->move_from_node = player->node;
    actor->move_to_node = player->next_node;
    rich4_trace_emit(RICH4_TRACE_MOVE_BEGIN, -1,
                     actor->move_from_node, actor->move_to_node);
    actor->move_from_x = sx;
    actor->move_from_y = sy;
    actor->move_to_x = tx;
    actor->move_to_y = ty;
    actor->move_progress = 0;
    /* sub_40C05C uses distance * 0.125 when EA1 is set or +21 & 0x30 is
     * nonzero; otherwise byte_4749D8 selects 8/12/16/8 by traffic method. */
    fixed_speed = original_actor_uses_fixed_movement_speed(
        player, map, player->next_node);
    speed = fixed_speed ? 8 : rich4_move_speed[player->traffic_method & 3];
    duration = original_route_duration(dx, dy, fixed_speed, speed);
    actor->move_distance = original_route_frame_count(duration);
    actor->visual_x = sx;
    actor->visual_y = sy;
    /* sub_40C05C stores the old node in +14 and the selected target in +12
     * before 0x40C410 calls sub_407A8C(+14,+12).  Since sub_407A8C returns
     * arg2-arg1, walking faces the same forward vector used here. */
    actor->direction = rich4_game_actor_direction_from_delta(dx, dy);
}

static int update_player_step(GamePlayerState *player,
                              GameActorPresentationState *actor,
                              const MapData *map)
{
    float duration;
    if (player == NULL || actor == NULL || !actor->move_active) return 0;
    duration = original_active_route_duration(player, actor, map);
    actor->move_progress++;
    actor->walk_frame++;
    if (actor->move_progress >= actor->move_distance) {
        player->previous_node = actor->move_from_node;
        player->node = actor->move_to_node;
        player->next_node = route_next_node(map, player->node,
                                             player->previous_node);
        actor->visual_x = actor->move_to_x;
        actor->visual_y = actor->move_to_y;
        actor->move_active = 0;
        actor->move_progress = 0;
        actor->move_distance = 0;
        rich4_trace_emit(RICH4_TRACE_MOVE_END, -1,
                         actor->move_from_node, actor->move_to_node);
        return 1;
    }
    actor->visual_x = original_interpolated_axis(
        actor->move_from_x, actor->move_to_x, actor->move_progress, duration);
    actor->visual_y = original_interpolated_axis(
        actor->move_from_y, actor->move_to_y, actor->move_progress, duration);
    return 0;
}

static void retain_movement_events(GameEventQueue *out_events,
                                   GameEventQueue *source,
                                   GameSession *session,
                                   int player_index)
{
    GameEvent event;

    while (rich4_game_event_queue_pop(source, &event)) {
        if (event.type == GAME_EVENT_MAP_OBJECT_CHANGED &&
            rich4_game_event_map_object_subject(&event).kind ==
                GAME_DOMAIN_TOOL &&
            (rich4_game_event_map_object_tool(&event) == 1 ||
             rich4_game_event_map_object_tool(&event) == 2 ||
             (rich4_game_event_map_object_tool(&event) == 3 &&
              rich4_game_event_map_object_effect(&event) == 0)))
            rich4_tool_followup_mark_turn_pending(session);
        if (event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED &&
            event.player >= 0 && event.player < session->state.player_count)
            session->presentation.actors[event.player].dirty_flags |=
                GAME_PLAYER_DIRTY_TURN;
        else if (event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED &&
                 player_index >= 0 &&
                 player_index < session->state.player_count)
            session->presentation.actors[player_index].dirty_flags |=
                GAME_PLAYER_DIRTY_TURN;
        (void)rich4_game_event_queue_push(out_events, &event);
    }
    /* 0x41BD18 -> 0x41D476 removes the roadblock from the displayed map
     * before 0x41BD60 starts its blocking reaction speech. */
    rich4_app_sync_tool_dynamic_objects(&session->presentation, &session->state);
}

/* 0x41B42D checks the first occupied player before advancing an attached
 * bomb or dispatching the single dynamic object at this node. Reuse the
 * executor's donation continuation, but do not execute the square yet. */
static int begin_arrival_beggar(GameSession *session, int node, uint32_t now)
{
    RouteEvent fact;
    GameEventQueue events;
    GameEvent event;
    if (rich4_ledger_find_beggar(&session->state,
            session->state.current_player, node) < 0 ||
        rich4_ledger_beggar_amount(&session->state) <= 0)
        return 0;
    rich4_app_route_reset_facts(session);
    memset(&fact, 0, sizeof(fact));
    fact.phase = ROUTE_EVENT_PHASE_STOP;
    fact.node = node;
    fact.snapshot.player = session->state.current_player;
    fact.snapshot.event_kind = GAME_ROUTE_EVENT_ROAD_NODE;
    rich4_route_event_collection_push(&session->flow.route_facts, &fact);
    rich4_app_route_prepare_dispatch(session, session->state.current_player);
    rich4_game_event_queue_reset(&events);
    rich4_route_event_executor_step(&session->flow.route_events,
                                    &session->state, &events);
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED)
            rich4_property_begin_beggar_presentation(session, &event, now);
    }
    session->flow.turn_wait = RICH4_MOVE_BEGGAR;
    session->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    return 1;
}

static int roadblock_at(const GameState *state, int node)
{
    int i;
    for (i = 0; i < GAME_TOOL_PLACED_OBJECT_COUNT; ++i)
        if (state->tool_placed_objects[i].kind == 16 &&
            state->tool_placed_objects[i].node == node)
            return 1;
    return 0;
}

static void begin_stopped_square(GameSession *session, const MapData *map,
                                  uint32_t now)
{
    PlayerStatusDecision decision;
    int player = session->state.current_player;
    /* 0x418E7F calls 0x40C912(1) after EA5's five ticks. A hospital,
     * sleeping or otherwise blocked actor returns 0x83 without landing. */
    rich4_app_route_reset_facts(session);
    rich4_player_status_evaluate(&session->state, player, 1, &decision);
    if (decision.result != 0)
        rich4_app_route_record_stop(session, map,
                                    session->state.players[player].node);
    rich4_app_route_prepare_dispatch(session, player);
    session->flow.route_events.state.beggar_checked = 1;
    session->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    session->flow.turn_state_frame = 0;
    session->flow.turn_state_tick = now;
    session->presentation.dice.popup_active = 0;
    session->flow.turn_wait = decision.result == 0 ?
        RICH4_ROUTE_END_COUNTDOWN : 0;
    session->flow.property_turn_end_ticks = decision.result == 0 ? 3 : 0;
    session->flow.property_turn_end_tick = now;
}

int rich4_app_update_turn_movement(
    GameSession *session, const MapData *map, int frame_ms, uint32_t now,
    Rich4AppTurnMovementResult *out_result)
{
    Rich4AppTurnMovementResult local_result;
    GamePlayerState *player;
    GameActorPresentationState *actor;
    int phase;
    if (out_result == NULL) out_result = &local_result;
    memset(out_result, 0, sizeof(*out_result));
    rich4_game_event_queue_reset(&out_result->events);
    if (session == NULL || session->state.player_count <= 0 || frame_ms <= 0 ||
        session->flow.turn_state != GAME_TURN_STATE_MOVE)
        return 0;
    player = &session->state.players[session->state.current_player];
    actor = &session->presentation.actors[session->state.current_player];
    if (session->presentation.movement_tick == 0)
        session->presentation.movement_tick = session->flow.turn_state_tick;
    phase = session->flow.turn_wait;
    /* Bank/donation returns resume the same 0x41B42D call. They do not
     * consume a new timer pulse; a pulse raised inside the modal remains
     * pending for the next outer movement/idle case. */
    if (phase != RICH4_MOVE_AFTER_BANK && phase != RICH4_MOVE_AFTER_BEGGAR) {
        if (now - session->presentation.movement_tick < (uint32_t)frame_ms)
            return 1;
        /* 0x401F98 exposes one pending byte, not a catch-up counter. */
        session->presentation.movement_tick +=
            ((now - session->presentation.movement_tick) / (uint32_t)frame_ms) *
            (uint32_t)frame_ms;
    }
    if (phase > 0) {
        if (--session->flow.turn_wait == 0)
            begin_stopped_square(session, map, now);
        return 1;
    }
    if (phase == RICH4_MOVE_FORCED_STOP ||
        (phase == 0 && session->flow.steps_remaining <= 0)) {
        /* A roadblock cleared n9_0 inside 0x41B42D. EA2 becomes idle on
         * this next tick; the five decrements start on the following tick. */
        session->flow.turn_wait = 5;
        return 1;
    }
    if (phase == RICH4_MOVE_ARRIVAL || phase == RICH4_MOVE_AFTER_BANK ||
        phase == RICH4_MOVE_AFTER_BEGGAR) {
        int node = actor->move_to_node;
        int natural_stop = session->flow.steps_remaining <= 0;
        int object_type = GAME_ROUTE_OBJECT_NONE;
        GameToolTriggerResult trigger = GAME_TOOL_TRIGGER_NONE;
        GameEventQueue events;
        if (phase == RICH4_MOVE_ARRIVAL && !natural_stop &&
            !player->days_sleep_walking && !roadblock_at(&session->state, node) &&
            rich4_app_route_action_for_node(map, node) == 14) {
            rich4_app_route_reset_facts(session);
            rich4_app_route_record_pass(session, map, node);
            rich4_app_route_prepare_dispatch(session, session->state.current_player);
            session->flow.route_events.state.dispatch.resume_movement = 1;
            session->flow.turn_wait = RICH4_MOVE_AFTER_BANK;
            session->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
            return 1;
        }
        if (phase != RICH4_MOVE_AFTER_BEGGAR && natural_stop &&
            begin_arrival_beggar(session, node, now))
            return 1;
        rich4_game_event_queue_reset(&events);
        rich4_app_route_object_at(map, node, NULL, &object_type, NULL);
        if (player->attached_timed_bomb_slot >= 0 ||
            (player->traffic_method > 3 && (player->traffic_method & 3) == 3)) {
            trigger = rich4_tool_advance_step_deferred_hospital(
                &session->state, session->state.current_player, node,
                object_type == GAME_ROUTE_OBJECT_LAND ||
                object_type == GAME_ROUTE_OBJECT_FACILITY, &events);
        }
        /* 0x41B42D returns immediately on an attached-bomb explosion. */
        if (trigger != GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT)
            trigger = rich4_tool_trigger_at_node_deferred_hospital(
                &session->state, session->state.current_player, node,
                natural_stop, &events);
        retain_movement_events(&out_result->events, &events, session,
                               session->state.current_player);
        if (trigger == GAME_TOOL_TRIGGER_STOP ||
            trigger == GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT) {
            session->flow.steps_remaining = 0;
            session->flow.route_move.steps_remaining = 0;
        }
        if (natural_stop && player->node == node &&
            trigger != GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT) {
            /* Building effects (0x40F381) belong to 0x41982D's common
             * property tail, after payment/interaction, not to arrival. */
            (void)rich4_road_gift_begin(
                session, session->state.current_player, now);
            (void)rich4_road_chest_begin(
                session, map, session->state.current_player, now);
            (void)rich4_app_collect_dynamic_god(
                &session->presentation, &session->state,
                session->state.current_player, &out_result->events);
            if (rich4_app_resolve_dynamic_dog(
                    &session->presentation, &session->state, map,
                    session->state.current_player, &out_result->events) > 0)
                rich4_tool_followup_mark_turn_pending(session);
        }
        session->flow.turn_wait = natural_stop ? 5 :
            (session->flow.steps_remaining <= 0 ? RICH4_MOVE_FORCED_STOP : 0);
        if (session->flow.steps_remaining <= 0) return 1;
        /* An ordinary pass resumes 0x40C05C in this same timer case. */
    }
    if (!actor->move_active) {
        prepare_route_for_step(session, player);
        start_player_step(player, actor, map);
    }
    if (update_player_step(player, actor, map)) {
        RouteMoveResult result = rich4_route_move_commit_with_rng(
            &session->flow.route_move, map, &session->state.ai_rng_state);
        if (result == ROUTE_MOVE_INVALID)
            session->flow.steps_remaining--;
        else {
            session->flow.steps_remaining = session->flow.route_move.steps_remaining;
            if (session->flow.route_move.next_node > 0)
                player->next_node = session->flow.route_move.next_node;
        }
        /* 0x40C05C sets byte_48BB00; 0x41B42D runs next timer tick. */
        session->flow.turn_wait = RICH4_MOVE_ARRIVAL;
    }
    session->flow.turn_state_frame++;
    return 1;
}
