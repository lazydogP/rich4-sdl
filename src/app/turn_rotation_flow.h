#ifndef RICH4_APP_TURN_ROTATION_FLOW_H
#define RICH4_APP_TURN_ROTATION_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/turn_rotation.h"
#include "data/map_data.h"

/* App-owned turn selection and status side effects.  The map is borrowed
 * data; a special-actor result is returned for the platform animation seam. */
typedef struct Rich4AppTurnRotationResult {
    int special_action_pending;
    TurnSpecialAutoActionResult special_action;
} Rich4AppTurnRotationResult;

int rich4_app_advance_turn_rotation(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result);
int rich4_app_resume_turn_events(GameSession *session, const MapData *map,
                                 uint32_t now);
int rich4_app_complete_route_event_turn(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result);
int rich4_app_finish_control_locked_turn(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result);
int rich4_app_update_confinement_exit(
    GameSession *session, uint32_t now, int frame_ms);

#endif
