#ifndef RICH4_APP_TURN_MOVEMENT_FLOW_H
#define RICH4_APP_TURN_MOVEMENT_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_event.h"
#include "core/turn_rotation.h"
#include "data/map_data.h"

/* turn_wait is already serialized. Negative values are continuations;
 * positive values during MOVE are the original EA5 stop countdown. */
enum {
    RICH4_MOVE_ARRIVAL = -1,
    RICH4_MOVE_AFTER_BANK = -2,
    RICH4_MOVE_BEGGAR = -3,
    RICH4_MOVE_AFTER_BEGGAR = -4,
    RICH4_MOVE_FORCED_STOP = -5,
    RICH4_ROUTE_END_EFFECTS = -10,
    RICH4_ROUTE_END_COUNTDOWN = -11
};

/* App-owned movement state machine.  MapData is borrowed immutable input;
 * GameSession owns deterministic movement and actor presentation state.
 * Resource-backed event effects are returned for the platform adapter. */
typedef struct Rich4AppTurnMovementResult {
    GameEventQueue events;
    int special_action_pending;
    TurnSpecialAutoActionResult special_action;
} Rich4AppTurnMovementResult;

int rich4_app_update_turn_movement(
    GameSession *session, const MapData *map, int frame_ms, uint32_t now,
    Rich4AppTurnMovementResult *out_result);

#endif
