#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/first_turn_flow.h"
#include "core/game_constants.h"

static void setup(GameSession *session)
{
    memset(session, 0, sizeof(*session));
    session->state.active = 1;
    session->state.player_count = 2;
    session->state.players[0].linked_player = -1;
    session->state.players[1].linked_player = -1;
    session->state.players[0].attached_timed_bomb_slot = -1;
    session->state.players[1].attached_timed_bomb_slot = -1;
    session->flow.turn_flow_active = 1;
    session->flow.turn_state = GAME_TURN_STATE_LANDING;
}

int main(void)
{
    GameSession session;
    FirstTurnDispatchPlan plan;
    Rich4FirstTurnPresentationRequest request;

    setup(&session);
    session.state.current_player = 0;
    session.state.players[0].node = 3;
    session.state.players[0].next_node = 4;
    session.state.players[0].play_state = 1;
    session.flow.landing_pending_player = 1;
    session.flow.turn_state_tick = 0;
    assert(rich4_app_update_first_landing(
               &session, 0, 0, 10, 20, 30, 40, 2, 1, 10, 10,
               &request));
    assert(request == RICH4_FIRST_TURN_PRESENTATION_HUMAN_DICE);
    assert(session.flow.landing_pending_player == 0);
    assert(session.presentation.has_view_override == 0);
    assert(session.presentation.actors[0].visual_x == 30 &&
           session.presentation.actors[0].visual_y == 40 &&
           session.presentation.actors[0].direction == 2);
    assert(session.presentation.dice.status_visible == 1 &&
           session.presentation.dice.input_refresh_active == 1);

    setup(&session);
    plan = rich4_app_prepare_first_turn_dispatch(&session, 0, 1);
    request = rich4_app_apply_first_turn_dispatch(&session, &plan, 0, 0,
                                                  1000);
    assert(request == RICH4_FIRST_TURN_PRESENTATION_HUMAN_DICE);
    assert(session.flow.turn_flow_active == 0);
    assert(session.flow.turn_state == 0);
    assert(session.flow.turn_state_tick == 1000);

    setup(&session);
    session.state.players[1].is_ai = 1;
    plan = rich4_app_prepare_first_turn_dispatch(&session, 0, 1);
    request = rich4_app_apply_first_turn_dispatch(&session, &plan, 0, 1,
                                                  1100);
    assert(request == RICH4_FIRST_TURN_PRESENTATION_AI_DICE);
    assert((session.presentation.actors[1].dirty_flags &
            GAME_PLAYER_DIRTY_TURN) != 0);
    assert(session.presentation.dice.status_visible == 0);

    setup(&session);
    session.state.card_amount[0] = 2;
    session.state.tool_amount[0] = 3;
    session.state.players[0].card_inventory[0] = 1;
    session.state.players[0].tool_inventory[0] = 4;
    plan = rich4_app_prepare_first_turn_dispatch(&session, 0, 2);
    request = rich4_app_apply_first_turn_dispatch(&session, &plan, 0, 0,
                                                  1200);
    assert(request == RICH4_FIRST_TURN_PRESENTATION_NONE);
    assert(session.flow.turn_flow_active == 1);
    assert(session.flow.turn_state == 2);
    assert(session.debug.automatic_preaction_card_pool_total == 2);
    assert(session.debug.automatic_preaction_tool_pool_total == 3);
    assert(session.debug.automatic_preaction_player_card_total == 1);
    assert(session.debug.automatic_preaction_player_tool_total == 4);

    setup(&session);
    session.state.players[0].days_in_prison = 1;
    plan = rich4_app_prepare_first_turn_dispatch(&session, 0, 2);
    assert(plan.remark_dirty == 1);
    request = rich4_app_apply_first_turn_dispatch(&session, &plan, 0, 0,
                                                  1300);
    assert(request == RICH4_FIRST_TURN_PRESENTATION_NONE);
    assert(session.flow.turn_flow_active == 0);
    assert((session.presentation.actors[0].dirty_flags &
            GAME_PLAYER_DIRTY_TURN) != 0);
    assert(session.debug.automatic_preaction_status_remark_dirty == 1);

    setup(&session);
    session.flow.player_status_decision.status_slot = 55;
    plan = rich4_app_prepare_first_turn_dispatch(&session, 0, -1);
    request = rich4_app_apply_first_turn_dispatch(&session, &plan, 0, 0,
                                                  1400);
    assert(request == RICH4_FIRST_TURN_PRESENTATION_NONE);
    assert(session.flow.turn_state == GAME_TURN_STATE_PREROLL);
    assert(session.flow.turn_flow_active == 1);

    setup(&session);
    session.flow.player_status_decision.status_slot = 50;
    plan = rich4_app_prepare_first_turn_dispatch(&session, 0, 0);
    request = rich4_app_apply_first_turn_dispatch(&session, &plan, 0, 0,
                                                  1500);
    assert(request == RICH4_FIRST_TURN_PRESENTATION_CONTROL_LOCKED);
    assert(session.flow.turn_state == GAME_TURN_STATE_LANDING);
    assert(session.flow.turn_flow_active == 1);

    puts("app first turn flow: ok");
    return 0;
}
