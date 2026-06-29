#ifndef RICH4_APP_FIRST_TURN_FLOW_H
#define RICH4_APP_FIRST_TURN_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/first_turn_dispatch.h"

typedef enum Rich4FirstTurnPresentationRequest {
    RICH4_FIRST_TURN_PRESENTATION_NONE = 0,
    RICH4_FIRST_TURN_PRESENTATION_CONTROL_LOCKED,
    RICH4_FIRST_TURN_PRESENTATION_HUMAN_DICE,
    RICH4_FIRST_TURN_PRESENTATION_AI_DICE
} Rich4FirstTurnPresentationRequest;

/* Combines the deterministic dispatch result with the current player's
 * status facts before the platform consumes the resulting plan. */
FirstTurnDispatchPlan rich4_app_prepare_first_turn_dispatch(
    const GameSession *session, int current_player, int dispatch_result);

/* Applies the platform-independent state transition after the first landing
 * dispatch. Resource-backed animation and panel work remain platform seams. */
Rich4FirstTurnPresentationRequest rich4_app_apply_first_turn_dispatch(
    GameSession *session,
    const FirstTurnDispatchPlan *dispatch_plan,
    int current_player,
    int landed_player,
    uint32_t now);

/* Advances the app-owned first-landing state. The platform supplies only
 * resource-derived timing and map geometry; this function owns actor/domain
 * commits, status evaluation, debug facts, and the next presentation request. */
int rich4_app_update_first_landing(
    GameSession *session,
    int current_player,
    int landed_player,
    int view_x,
    int view_y,
    int landed_x,
    int landed_y,
    int landed_direction,
    int landing_frames,
    int landing_delay_ms,
    uint32_t now,
    Rich4FirstTurnPresentationRequest *out_request);

#endif
