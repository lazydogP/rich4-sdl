#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/turn_flow.h"
#include "app/game_presentation.h"

int main(void)
{
    GameState state;
    GameFlowState flow;
    GamePresentationState presentation;
    int i;
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    memset(&presentation, 0, sizeof(presentation));
    state.active = 1;
    state.player_count = 2;
    state.current_player = 1;
    presentation.dice.status_visible = 1;
    presentation.dice.action_hint_active = 1;
    presentation.dice.input_refresh_active = 1;
    presentation.actors[1].first_turn_landing_pending = 1;
    presentation.actors[1].walk_frame = 9;
    assert(rich4_turn_flow_begin(&state, &flow, 1, 1234));
    assert(flow.turn_flow_active && flow.turn_state == GAME_TURN_STATE_LANDING);
    assert(flow.landing_pending_player == 2 && flow.turn_state_tick == 1234);
    assert(presentation.dice.status_visible &&
           presentation.dice.action_hint_active &&
           presentation.dice.input_refresh_active &&
           presentation.actors[1].walk_frame == 9);
    assert(!rich4_turn_flow_begin(&state, &flow, 1, 1235));
    flow.turn_flow_active = 0;
    presentation.actors[1].first_turn_landing_pending = 0;
    assert(rich4_turn_flow_begin(&state, &flow, 0, 2000));
    assert(flow.turn_state == GAME_TURN_STATE_PREROLL);
    rich4_turn_flow_start_property_end_delay(&flow, 3000);
    assert(flow.property_turn_end_ticks == GAME_PROPERTY_TURN_END_TICKS);
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3119, 120));
    assert(flow.property_turn_end_ticks == 8);
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3120, 120));
    assert(flow.property_turn_end_ticks == 7);
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3240, 120));
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3360, 120));
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3480, 120));
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3600, 120));
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3720, 120));
    assert(rich4_turn_flow_update_property_end_delay(&flow, 3840, 120));
    assert(!rich4_turn_flow_update_property_end_delay(&flow, 3960, 120));
    assert(flow.property_turn_end_ticks == 0);
    rich4_turn_flow_start_property_end_delay(&flow, 4000);
    for (i = 1; i < 8; i++)
        assert(rich4_turn_flow_update_property_end_delay(
            &flow, 4000 + (uint32_t)i * 80u, 80));
    assert(!rich4_turn_flow_update_property_end_delay(&flow, 4640, 80));
    rich4_turn_flow_start_property_end_delay(&flow, 5000);
    for (i = 1; i < 8; i++)
        assert(rich4_turn_flow_update_property_end_delay(
            &flow, 5000 + (uint32_t)i * 40u, 40));
    assert(!rich4_turn_flow_update_property_end_delay(&flow, 5320, 40));
    rich4_turn_flow_start_property_end_delay(&flow, 6000);
    assert(rich4_turn_flow_update_property_end_delay(&flow, 8000, 40));
    assert(flow.property_turn_end_ticks == 7);
    assert(rich4_turn_flow_update_property_end_delay(&flow, 8000, 40));
    assert(flow.property_turn_end_ticks == 7);
    puts("turn flow landing transition: ok");
    return 0;
}
