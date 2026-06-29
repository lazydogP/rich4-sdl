#include <string.h>

#include "app/game_session.h"
#include "app/confinement_flow.h"
#include "core/confinement.h"
#include "platform/sdl1/game_confinement_render_view.h"
#include "platform/sdl1/game_flow_api.h"

int build_confinement_render_view(const GameSession *session,
                                  GameConfinementRenderView *out_view)
{
    memset(out_view, 0, sizeof(*out_view));
    if (session == NULL) return 0;
    out_view->player_count = session->state.player_count;
    if (out_view->player_count < 0) out_view->player_count = 0;
    if (out_view->player_count > GAME_MAX_PLAYERS)
        out_view->player_count = GAME_MAX_PLAYERS;
    {
        int player;
        for (player = 0; player < GAME_MAX_PLAYERS; player++) {
            out_view->player_character[player] =
                session->state.players[player].character;
            out_view->player_points[player] =
                session->state.players[player].points;
        }
    }
    {
        int slot;
        for (slot = 0; slot < 8; slot++) {
            GameDomainRef target = rich4_confinement_visit_target(slot);
            if (rich4_confinement_visit_candidate(
                    &session->state, target,
                    session->presentation.confinement_visit.kind))
                out_view->visit_candidate_mask |= 1u << slot;
        }
    }
    out_view->confinement_status_active =
        session->presentation.confinement_status.active;
    out_view->confinement_status_until =
        session->presentation.confinement_status.until;
    out_view->confinement_status_player =
        session->presentation.confinement_status.player;
    out_view->confinement_status_slot =
        session->presentation.confinement_status.slot;
    out_view->confinement_status_days =
        session->presentation.confinement_status.days;
    out_view->confinement_visit_active = session->presentation.confinement_visit.active;
    out_view->confinement_visit_kind = session->presentation.confinement_visit.kind;
    out_view->confinement_visit_nurse_side = session->presentation.confinement_visit.nurse_side;
    out_view->confinement_visit_nurse_gesture_frame =
        session->presentation.confinement_visit.nurse_gesture_frame;
    out_view->confinement_visit_nurse_idle_chunk =
        session->presentation.confinement_visit.nurse_idle_chunk;
    out_view->confinement_visit_phase = session->presentation.confinement_visit.phase;
    out_view->confinement_visit_selected = session->presentation.confinement_visit.selected;
    out_view->confinement_visit_hover = session->presentation.confinement_visit.hover;
    out_view->confinement_visit_confirm_hover =
        session->presentation.confinement_visit.confirm_hover;
    out_view->confinement_visit_speech_active =
        session->presentation.confinement_visit.speech_active;
    out_view->confinement_visit_speech = session->presentation.confinement_visit.speech;
    out_view->confinement_visit_speech_special_actor =
        session->presentation.confinement_visit.speech_special_actor;
    out_view->speech_active = session->presentation.speech.active;
    out_view->route_execution_actor =
        session->flow.route_events.state.execution_actor;
    return 1;
}
