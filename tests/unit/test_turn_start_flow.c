#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/turn_start_flow.h"

int main(void)
{
    GameSession session;
    Rich4TurnStartRequest request;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].play_state = 2;
    session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
    assert(rich4_turn_start_prepare(&session, 100) ==
           RICH4_TURN_START_HUMAN);
    assert(session.flow.player_status_decision.result == 2);
    assert(rich4_turn_start_prepare_request(&session, 110, &request) == 0);

    session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
    session.presentation.actors[0].first_turn_landing_pending = 1;
    assert(rich4_turn_start_prepare_request(&session, 200, &request) != 0);
    assert(request.action == RICH4_TURN_START_STARTED);
    assert(request.player_index == 0);
    assert(session.flow.turn_flow_active != 0);

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].play_state = 1;
    session.state.players[0].days_in_hospital = 3;
    session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
    session.presentation.has_view_override = 1;
    session.presentation.view_world_x = 777;
    session.presentation.view_world_y = 888;
    assert(rich4_turn_start_prepare(&session, 300) ==
           RICH4_TURN_START_CONFINEMENT_STATUS);
    assert(session.presentation.has_view_override == 0);

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].play_state = 1;
    session.state.players[0].days_disappearing =
        GAME_DISAPPEARING_ALIEN_BIT | 2;
    session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
    session.presentation.has_view_override = 1;
    assert(rich4_turn_start_prepare_request(
        &session, 350, &request));
    assert(request.action == RICH4_TURN_START_CONFINEMENT_STATUS);
    assert(request.status_slot == 51);
    assert(session.presentation.has_view_override == 0);

    /* Manual hosting redispatches this same actor before selecting tools. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 5;
    session.state.players[0].is_ai = 1;
    session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
    session.presentation.has_view_override = 1;
    session.presentation.map_direction = 6;
    assert(rich4_turn_start_prepare(&session, 400) == RICH4_TURN_START_AI);
    assert(!session.presentation.has_view_override);
    assert(session.presentation.map_direction == 6);

    /* 0x418D70 -> 0x40CB02: sleeping humans and AI show the same notice
     * before their blocked turn completes; +21 bits 0x30 use actor refresh. */
    {
        int ai;
        for (ai = 0; ai < 2; ai++) {
            memset(&session, 0, sizeof(session));
            session.state.active = 1;
            session.state.player_count = 1;
            session.state.players[0].play_state = ai ? 2 : 1;
            session.state.players[0].is_ai = ai;
            session.state.players[0].days_sleeping = 4;
            session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
            assert(rich4_turn_start_prepare_request(&session, 500, &request));
            assert(request.action == RICH4_TURN_START_CONFINEMENT_STATUS);
            assert(request.status_slot == 54 && request.status_result == 0);
            assert(!session.flow.turn_flow_active);
            session.state.players[0].days_sleeping = GAME_STATUS_PENDING_BIT;
            session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
            assert(rich4_turn_start_prepare(&session, 600) ==
                   RICH4_TURN_START_CONFINEMENT_STATUS);
            session.state.players[0].play_state |= 0x10;
            session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
            assert(rich4_turn_start_prepare(&session, 700) ==
                   RICH4_TURN_START_CONTROL_LOCKED);
        }
    }
    puts("app turn-start flow: ok");
    return 0;
}
