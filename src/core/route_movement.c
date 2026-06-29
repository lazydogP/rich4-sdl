#include "core/route_movement.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/tool.h"

#include <string.h>

static int route_node_valid(const MapData *map, int node)
{
    return map != NULL && map->routes != NULL && node > 0 &&
           (size_t)node < map->route_count;
}

int rich4_route_collect_non_return(const MapData *map,
                                   int node,
                                   int previous_node,
                                   int out_options[4])
{
    int count = 0;
    int fallback = 0;
    int i;
    if (out_options == NULL) return 0;
    memset(out_options, 0, sizeof(int) * 4);
    if (!route_node_valid(map, node)) return 0;
    for (i = 0; i < 4; i++) {
        int next = map->routes[node].next[i];
        uint32_t blocked_mask = 0x40000000u >> i;
        if (!route_node_valid(map, next)) continue;
        if (next == previous_node) fallback = next;
        if ((map->routes[node].flags & blocked_mask) != 0) continue;
        if (next == previous_node) continue;
        /* 0x40C05C keeps every legal adjacency slot, including duplicates. */
        out_options[count++] = next;
    }
    if (count == 0 && fallback != 0) out_options[count++] = fallback;
    return count;
}

static RouteMoveResult route_prepare(RouteMoveState *state, const MapData *map, uint32_t *rng_state)
{
    state->option_count = rich4_route_collect_non_return(
        map, state->node, state->previous_node, state->options);
    state->selected_index = -1;
    state->next_node = 0;
    if (state->option_count <= 0) return ROUTE_MOVE_INVALID;
    state->selected_index = rng_state != NULL ? rich4_rng_range_at(rng_state, state->option_count) : rich4_rng_range(state->option_count);
    state->next_node = state->options[state->selected_index];
    return ROUTE_MOVE_READY;
}

RouteMoveResult rich4_route_move_begin_with_rng(RouteMoveState *state,
                                       const MapData *map,
                                       int node,
                                       int previous_node,
                                       int steps, uint32_t *rng_state)
{
    if (state == NULL || !route_node_valid(map, node) || steps <= 0) {
        return ROUTE_MOVE_INVALID;
    }
    memset(state, 0, sizeof(*state));
    state->node = node;
    state->previous_node = previous_node;
    state->steps_remaining = steps;
    return route_prepare(state, map, rng_state);
}

RouteMoveResult rich4_route_move_begin_with_next_and_rng(RouteMoveState *state,
                                                 const MapData *map,
                                                 int node,
                                                 int previous_node,
                                                 int next_node,
                                                 int steps, uint32_t *rng_state)
{
    int i;
    if (state == NULL || !route_node_valid(map, node) || steps <= 0) {
        return ROUTE_MOVE_INVALID;
    }
    memset(state, 0, sizeof(*state));
    state->node = node;
    state->previous_node = previous_node;
    state->steps_remaining = steps;
    state->option_count = rich4_route_collect_non_return(
        map, node, previous_node, state->options);
    state->selected_index = -1;
    if (state->option_count <= 0) return ROUTE_MOVE_INVALID;
    for (i = 0; i < state->option_count; i++) {
        if (state->options[i] == next_node) {
            state->selected_index = i;
            state->next_node = next_node;
            return ROUTE_MOVE_READY;
        }
    }
    state->selected_index = rng_state != NULL ? rich4_rng_range_at(rng_state, state->option_count) : rich4_rng_range(state->option_count);
    state->next_node = state->options[state->selected_index];
    return ROUTE_MOVE_READY;
}

RouteMoveResult rich4_route_move_commit_with_rng(RouteMoveState *state, const MapData *map, uint32_t *rng_state)
{
    int old_node;
    if (state == NULL || state->steps_remaining <= 0 || state->next_node <= 0 ||
        state->selected_index < 0 || !route_node_valid(map, state->next_node)) {
        return ROUTE_MOVE_INVALID;
    }
    old_node = state->node;
    state->node = state->next_node;
    state->previous_node = old_node;
    state->steps_remaining--;
    if (state->steps_remaining == 0) {
        state->next_node = 0;
        state->option_count = 0;
        state->selected_index = -1;
        memset(state->options, 0, sizeof(state->options));
        return ROUTE_MOVE_DONE;
    }
    return route_prepare(state, map, rng_state);
}

RouteMoveResult rich4_route_move_begin_events(RouteMoveState *state,
                                              const MapData *map,
                                              int player,
                                              int node,
                                              int previous_node,
                                              int steps,
                                              GameEventQueue *events)
{
    RouteMoveResult result = rich4_route_move_begin(state, map, node, previous_node, steps);
    rich4_game_event_emit_route_move_resolved(
        events, player, result, node,
        state != NULL ? state->next_node : 0);
    return result;
}

RouteMoveResult rich4_route_move_begin_with_next_events(
    RouteMoveState *state,
    const MapData *map,
    int player,
    int node,
    int previous_node,
    int next_node,
    int steps,
    GameEventQueue *events)
{
    RouteMoveResult result = rich4_route_move_begin_with_next(
        state, map, node, previous_node, next_node, steps);
    rich4_game_event_emit_route_move_resolved(
        events, player, result, node,
        state != NULL ? state->next_node : 0);
    return result;
}

RouteMoveResult rich4_route_move_commit_events(RouteMoveState *state,
                                               const MapData *map,
                                               int player,
                                               GameEventQueue *events)
{
    RouteMoveResult result = rich4_route_move_commit(state, map);
    rich4_game_event_emit_route_move_resolved(
        events, player, result,
        state != NULL ? state->node : 0,
        state != NULL ? state->steps_remaining : 0);
    return result;
}

RouteMoveResult rich4_route_move_commit_tool_events(RouteMoveState *state,
                                                    const MapData *map,
                                                    GameState *game,
                                                    int player,
                                                    GameEventQueue *events)
{
    RouteMoveState next_move;
    GameState next_game;
    GameEventQueue staged;
    RouteMoveResult move_result;
    GameToolTriggerResult trigger;
    if (state == NULL || game == NULL || events == NULL) return ROUTE_MOVE_INVALID;
    next_move = *state;
    next_game = *game;
    rich4_game_event_queue_reset(&staged);
    move_result = rich4_route_move_commit_with_rng(&next_move, map, &next_game.ai_rng_state);
    rich4_game_event_emit_route_move_resolved(&staged, player, move_result,
        next_move.node, next_move.steps_remaining);
    if (move_result == ROUTE_MOVE_INVALID) return ROUTE_MOVE_INVALID;
    trigger = rich4_tool_trigger_at_node(
        &next_game, player, next_move.node,
        move_result == ROUTE_MOVE_DONE, &staged);
    if (trigger == GAME_TOOL_TRIGGER_INVALID ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return ROUTE_MOVE_INVALID;
    if (trigger == GAME_TOOL_TRIGGER_STOP ||
        trigger == GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT) {
        next_move.steps_remaining = 0;
        next_move.next_node = 0;
        next_move.option_count = 0;
        next_move.selected_index = -1;
        memset(next_move.options, 0, sizeof(next_move.options));
        move_result = ROUTE_MOVE_DONE;
    }
    *state = next_move;
    *game = next_game;
    return move_result;
}

RouteMoveResult rich4_route_move_begin(RouteMoveState *state, const MapData *map,
    int node, int previous, int steps)
{ return rich4_route_move_begin_with_rng(state, map, node, previous, steps, NULL); }
RouteMoveResult rich4_route_move_begin_with_next(RouteMoveState *state, const MapData *map,
    int node, int previous, int next, int steps)
{ return rich4_route_move_begin_with_next_and_rng(state, map, node, previous, next, steps, NULL); }
RouteMoveResult rich4_route_move_commit(RouteMoveState *state, const MapData *map)
{ return rich4_route_move_commit_with_rng(state, map, NULL); }
