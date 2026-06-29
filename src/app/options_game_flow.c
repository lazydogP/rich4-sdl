#include "app/options_game_flow.h"

#include "app/bankruptcy_flow.h"
#include "app/magic_house_flow.h"
#include "app/command_event_flow.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/ledger.h"

static int next_active_player(const GameState *state, int player)
{
    int offset;
    if (state == NULL || state->player_count <= 0) return -1;
    for (offset = 1; offset <= state->player_count; offset++) {
        int candidate = (player + offset) % state->player_count;
        if ((state->players[candidate].play_state &
             GAME_PLAYER_PLAY_STATE_MASK) != 0)
            return candidate;
    }
    return -1;
}

static void prepare_next_turn(GameSession *session, int player)
{
    int next;
    if (session == NULL || session->state.game_over) return;
    next = next_active_player(&session->state, player);
    if (next < 0) return;
    session->state.current_player = next;
    session->state.turn_rotation_index = next;
    session->flow.turn_flow_active = 0;
    session->flow.turn_state = GAME_TURN_STATE_IDLE;
    session->flow.turn_state_frame = 0;
    session->flow.turn_wait = 0;
    session->flow.steps_remaining = 0;
    session->flow.dice_value = 0;
    session->flow.property_turn_end_ticks = 0;
    session->presentation.dice.popup_active = 0;
    session->presentation.dice.status_visible = 0;
    session->presentation.dice.status_dragging = 0;
    session->presentation.dice.action_hint_active = 0;
    session->presentation.dice.input_refresh_active = 0;
    session->presentation.actors[next].dirty_flags |=
        GAME_PLAYER_DIRTY_TURN;
}

int rich4_app_surrender_current_player(
    GameSession *session, Rich4AudioEventQueue *audio_events,
    uint32_t now)
{
    GameEventQueue events;
    int player;

    if (session == NULL || !session->state.active ||
        session->state.game_over || session->state.player_count <= 0)
        return 0;
    player = session->state.current_player;
    if (player < 0 || player >= session->state.player_count)
        return 0;
    rich4_game_event_queue_reset(&events);
    if (!rich4_ledger_surrender_player(
            &session->state, player, &events))
        return 0;
    rich4_bankruptcy_capture_events(session, &events, now);
    session->presentation.bankruptcy.surrender_followup = 1;
    (void)audio_events;
    if (!session->state.game_over) {
        prepare_next_turn(session, player);
        /* The bankruptcy presentation snapshots the current player before
         * this options command advances the turn.  Keep the post-animation
         * cursor on the next active player instead of restoring the
         * eliminated actor when the deferred auction/speech work completes. */
        if (session->presentation.bankruptcy.active)
            session->presentation.bankruptcy.previous_current_player =
                session->state.current_player;
    }
    return 1;
}

void rich4_app_surrender_followup(GameSession *session, uint32_t now,
                                 Rich4AudioEventQueue *audio_events)
{
    int player, count = 0, humans = 0;
    unsigned int targets = 0;
    GameBankruptcyPresentationState *b;
    if (session == NULL) return;
    b = &session->presentation.bankruptcy;
    if (!b->surrender_followup || b->active || b->awaiting_auction ||
        session->presentation.speech.active) return;
    b->surrender_followup = 0;
    /* 0x411AE0 -> 0x40DFDA -> 0x4339D9. Only the multiplayer branch
     * with a remaining human and at least two other active players opens it. */
    if (session->state.human_player_count <= 1) return;
    for (player = 0; player < session->state.player_count; ++player) {
        if (!session->state.players[player].play_state) continue;
        if (session->state.players[player].play_state & 1) ++humans;
        if (player != b->player) { targets |= 1u << player; ++count; }
    }
    if (humans && count >= 2)
        rich4_magic_house_begin_surrender(session, b->player, targets, now, audio_events);
}
