#ifndef RICH4_APP_TURN_START_FLOW_H
#define RICH4_APP_TURN_START_FLOW_H

#include <stdint.h>

#include "app/game_session.h"

typedef enum Rich4TurnStartAction {
    RICH4_TURN_START_NONE = 0,
    RICH4_TURN_START_STARTED,
    RICH4_TURN_START_CONTROL_LOCKED,
    RICH4_TURN_START_CONFINEMENT_STATUS,
    RICH4_TURN_START_AI,
    RICH4_TURN_START_HUMAN
} Rich4TurnStartAction;

typedef struct Rich4TurnStartRequest {
    Rich4TurnStartAction action;
    int player_index;
    int status_slot;
    int status_result;
} Rich4TurnStartRequest;

int rich4_turn_start_begin(GameSession *session, uint32_t now);
Rich4TurnStartAction rich4_turn_start_prepare(GameSession *session,
                                              uint32_t now);
int rich4_turn_start_prepare_request(GameSession *session, uint32_t now,
                                     Rich4TurnStartRequest *out_request);

#endif
