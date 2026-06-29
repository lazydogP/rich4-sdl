#ifndef RICH4_CORE_ROUTE_MOVEMENT_H
#define RICH4_CORE_ROUTE_MOVEMENT_H

#include "data/map_data.h"
#include "core/game_event.h"
#include "core/game_state.h"

typedef enum RouteMoveResult {
    ROUTE_MOVE_INVALID = 0,
    ROUTE_MOVE_READY,
    ROUTE_MOVE_MOVED,
    ROUTE_MOVE_DONE
} RouteMoveResult;

typedef struct RouteMoveState {
    int node;
    int previous_node;
    int next_node;
    int steps_remaining;
    int option_count;
    int options[4];
    int selected_index;
} RouteMoveState;

int rich4_route_collect_non_return(const MapData *map,
                                   int node,
                                   int previous_node,
                                   int out_options[4]);
RouteMoveResult rich4_route_move_begin_with_rng(RouteMoveState *state,
    const MapData *map, int node, int previous_node, int steps, uint32_t *rng_state);
RouteMoveResult rich4_route_move_begin_with_next_and_rng(RouteMoveState *state,
    const MapData *map, int node, int previous_node, int next_node, int steps,
    uint32_t *rng_state);
RouteMoveResult rich4_route_move_commit_with_rng(RouteMoveState *state,
    const MapData *map, uint32_t *rng_state);
RouteMoveResult rich4_route_move_begin(RouteMoveState *state,
                                       const MapData *map,
                                       int node,
                                       int previous_node,
                                       int steps);
/* Starts movement with an already selected first edge when that edge is one
 * of the legal options.  The original stores this edge before first landing. */
RouteMoveResult rich4_route_move_begin_with_next(RouteMoveState *state,
                                                 const MapData *map,
                                                 int node,
                                                 int previous_node,
                                                 int next_node,
                                                 int steps);
RouteMoveResult rich4_route_move_begin_events(RouteMoveState *state,
                                              const MapData *map,
                                              int player,
                                              int node,
                                              int previous_node,
                                              int steps,
                                              GameEventQueue *events);
RouteMoveResult rich4_route_move_begin_with_next_events(
    RouteMoveState *state,
    const MapData *map,
    int player,
    int node,
    int previous_node,
    int next_node,
    int steps,
    GameEventQueue *events);
RouteMoveResult rich4_route_move_commit(RouteMoveState *state,
                                        const MapData *map);
RouteMoveResult rich4_route_move_commit_events(RouteMoveState *state,
                                               const MapData *map,
                                               int player,
                                               GameEventQueue *events);
/* Commits one step and resolves one-shot tool objects as a single transaction. */
RouteMoveResult rich4_route_move_commit_tool_events(RouteMoveState *state,
                                                    const MapData *map,
                                                    GameState *game,
                                                    int player,
                                                    GameEventQueue *events);

#endif
