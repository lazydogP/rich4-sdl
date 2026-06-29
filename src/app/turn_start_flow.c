#include "app/turn_start_flow.h"

#include "core/player_status.h"
#include "core/turn_flow.h"

int rich4_turn_start_begin(GameSession *session, uint32_t now)
{
    int current;
    int started;

    if (session == NULL || session->state.game_over ||
        session->presentation.intro.active ||
        session->state.player_count <= 0)
        return 0;
    current = session->state.current_player % session->state.player_count;
    if (current < 0) current += session->state.player_count;
    started = rich4_turn_flow_begin(
        &session->state, &session->flow,
        session->presentation.actors[current].first_turn_landing_pending,
        now);
    if (started) {
        session->presentation.has_view_override = 0;
        session->presentation.actors[current].walk_frame = 0;
        session->presentation.dice.status_visible = 0;
        session->presentation.dice.action_hint_active = 0;
        session->presentation.dice.input_refresh_active = 0;
    }
    return started;
}

Rich4TurnStartAction rich4_turn_start_prepare(GameSession *session,
                                              uint32_t now)
{
    int current;
    GamePlayerState *actor;
    PlayerStatusDecision decision;

    if (session == NULL || !session->state.active || session->state.game_over ||
        session->state.player_count <= 0 ||
        session->pending_turn_events.count > 0 ||
        session->presentation.periodic_finance.active ||
        session->presentation.bank_settlement.active ||
        session->flow.turn_flow_active || session->flow.steps_remaining > 0 ||
        session->presentation.dice.popup_active ||
        session->presentation.god_detachment.active ||
        session->presentation.god_detachment.speech_pending)
        return RICH4_TURN_START_NONE;
    current = session->state.current_player % session->state.player_count;
    if (current < 0) current += session->state.player_count;
    if ((session->presentation.actors[current].dirty_flags &
         GAME_PLAYER_DIRTY_TURN) == 0)
        return RICH4_TURN_START_NONE;

    actor = &session->state.players[current];
    session->presentation.actors[current].dirty_flags &=
        ~GAME_PLAYER_DIRTY_TURN;
    /* 0x418D69 -> 0x41D546 precedes status evaluation and AI preactions,
     * including redispatch after a human is manually hosted (state 5). */
    if ((actor->play_state & 0xff) != 0)
        session->presentation.has_view_override = 0;
    if (session->presentation.actors[current].first_turn_landing_pending) {
        return rich4_turn_start_begin(session, now)
            ? RICH4_TURN_START_STARTED : RICH4_TURN_START_NONE;
    }

    rich4_player_status_evaluate(&session->state, current, 0, &decision);
    session->flow.player_status_decision = decision;
    if (decision.result == 0 && decision.status_slot >= 50 &&
        decision.status_slot <= 54) {
        if (decision.status_slot == 50 || decision.status_slot == 52 || decision.status_slot == 53) {
            /* 0x418C55 clears the pending camera target before the status
             * presentation.  With no override, the ViewModel centers a
             * confined player on its facility anchor. */
            session->presentation.has_view_override = 0;
            return RICH4_TURN_START_CONFINEMENT_STATUS;
        }
        return RICH4_TURN_START_CONTROL_LOCKED;
    }
    if (decision.result == -1 && decision.status_slot == 55)
        return rich4_turn_start_begin(session, now)
            ? RICH4_TURN_START_STARTED : RICH4_TURN_START_NONE;
    return actor->is_ai ? RICH4_TURN_START_AI : RICH4_TURN_START_HUMAN;
}

int rich4_turn_start_prepare_request(GameSession *session, uint32_t now,
                                     Rich4TurnStartRequest *out_request)
{
    Rich4TurnStartRequest request;

    if (out_request == NULL) {
        return 0;
    }
    request.action = rich4_turn_start_prepare(session, now);
    request.player_index = -1;
    request.status_slot = -1;
    request.status_result = 0;
    if (session != NULL && session->state.player_count > 0) {
        request.player_index = session->state.current_player %
                               session->state.player_count;
        if (request.player_index < 0) {
            request.player_index += session->state.player_count;
        }
        request.status_slot = session->flow.player_status_decision.status_slot;
        request.status_result = session->flow.player_status_decision.result;
    }
    *out_request = request;
    return request.action != RICH4_TURN_START_NONE;
}
