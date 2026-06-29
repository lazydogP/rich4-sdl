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

    puts("app turn-start flow: ok");
    return 0;
}
