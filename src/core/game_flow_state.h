#ifndef RICH4_CORE_GAME_FLOW_STATE_H
#define RICH4_CORE_GAME_FLOW_STATE_H

#include <stdint.h>

#include "core/game_constants.h"
#include "core/player_status.h"
#include "core/route_event_executor.h"
#include "core/route_movement.h"
#include "core/victory.h"

/* Deterministic route-event and turn-coordination facts. Runtime camera,
 * interaction, animation, and feature-presentation state lives in app. */
typedef struct GameFlowState {
    VictoryResult victory_result;
    PlayerStatusDecision player_status_decision;
    RouteMoveState route_move;
    RouteEventExecutor route_events;
    RouteEventCollection route_facts;
    unsigned int route_event_turn_token;
    int dice_value;
    int steps_remaining;
    int route_count;
    int landing_pending_player;
    int turn_flow_active;
    int turn_state;
    int turn_state_frame;
    int turn_wait;
    uint32_t turn_state_tick;
    int property_turn_end_ticks;
    uint32_t property_turn_end_tick;
} GameFlowState;

#endif
