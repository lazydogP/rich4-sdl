#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/turn_status_flow.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/status_countdown.h"

static void setup(GameSession *session)
{
    memset(session, 0, sizeof(*session));
    session->state.active = 1;
    session->state.player_count = 2;
    session->state.players[0].play_state = 1;
    session->state.players[1].play_state = 1;
    session->state.players[0].linked_player = -1;
    session->state.players[1].linked_player = -1;
    session->state.players[0].attached_timed_bomb_slot = -1;
    session->state.players[1].attached_timed_bomb_slot = -1;
}

int main(void)
{
    GameSession session;

    setup(&session);
    session.state.players[0].days_in_prison = 2;
    rich4_app_update_player_status_countdowns(&session, 0);
    assert(session.state.players[0].days_in_prison == 1);

    session.state.price_index = 2;
    session.state.players[0].god_status_days = 3;
    session.state.players[0].linked_player = 1;
    session.state.players[1].linked_player = 0;
    session.state.players[0].relation_balance[1] = 100;
    session.state.players[1].relation_balance[0] = 80;
    rich4_app_update_player_status_countdowns(&session, 0);
    assert(session.state.players[0].relation_balance[1] == 60);
    assert(session.state.players[1].relation_balance[0] == 40);
    assert(session.debug.god_balance_tick_count == 2);

    session.state.players[0].days_in_prison = GAME_STATUS_PENDING_BIT;
    assert(rich4_status_player_has_pending(
               &session.state.players[0]));
    assert(rich4_app_process_pending_status_events(&session, 0) == 1);
    assert(session.state.players[0].days_in_prison == 0);
    assert(session.debug.status_event_processed_count == 1);

    session.state.special_player_status[0][0] = GAME_STATUS_PENDING_BIT;
    session.state.special_player_play_state[0] = GAME_PLAYER_PLAY_STATE_FROZEN;
    rich4_app_update_special_player_status_countdowns(&session, 0);
    assert(session.state.special_player_status[0][0] == 0);
    assert((session.state.special_player_play_state[0] &
            GAME_PLAYER_PLAY_STATE_FROZEN) == 0);
    assert(session.debug.last_special_status_actor_refreshed == 1);

    session.state.players[0].days_in_prison = 2;
    assert(rich4_app_begin_confinement_status_presentation(
        &session, 0, 52, 500));
    assert(session.presentation.confinement_status.active);
    assert(session.presentation.confinement_status.player == 0);
    assert(session.presentation.confinement_status.slot == 52);
    assert(session.presentation.confinement_status.days == 3);
    assert(session.presentation.confinement_status.until == 0);
    assert(rich4_app_update_confinement_status_presentation(
        &session, 900, 1) == 1);
    assert(session.presentation.confinement_status.until == 0);
    assert(rich4_app_update_confinement_status_presentation(
        &session, 900, 0) == 1);
    assert(session.presentation.confinement_status.until == 2400);
    assert(rich4_app_update_confinement_status_presentation(
        &session, 2399, 0) == 1);
    assert(rich4_app_update_confinement_status_presentation(
        &session, 2400, 0) == 2);

    {
        Rich4GameEventFields fields = { 0 };
        GameEvent ended;
        session.presentation.ending.active = 0;
        session.state.game_over = 0;
        session.state.players[1].character = 7;
        fields.player = 1;
        fields.subject = GAME_WIN_CANDIDATE_TARGET_ASSETS;
        fields.value = 1234;
        ended = rich4_game_event_game_ended_from_fields(fields);
        rich4_app_capture_game_ended(&session, &ended);
        assert(session.state.game_over && session.state.winner_player == 1);
        assert(session.presentation.ending.active &&
               session.presentation.ending.winner_character == 7 &&
               session.presentation.ending.reason ==
                   GAME_WIN_CANDIDATE_TARGET_ASSETS &&
               session.presentation.ending.assets == 1234 &&
               session.presentation.ending.winner_voice_pending &&
               session.presentation.ending.ending_video_pending);
    }

    puts("app turn status flow: ok");
    return 0;
}
