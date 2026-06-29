#include "core/turn_flow.h"

static int wrap_index(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    return value < 0 ? value + count : value;
}

int rich4_turn_flow_begin(const GameState *state, GameFlowState *flow,
                          int first_turn_landing_pending,
                          uint32_t now_ms)
{
    int current;
    if (state == NULL || flow == NULL || !state->active ||
        state->player_count <= 0 || state->game_over ||
        flow->turn_flow_active || flow->steps_remaining > 0) return 0;
    current = wrap_index(state->current_player, state->player_count);
    if (first_turn_landing_pending)
        flow->landing_pending_player = current + 1;
    flow->turn_flow_active = 1;
    flow->turn_state = first_turn_landing_pending ?
        GAME_TURN_STATE_LANDING : GAME_TURN_STATE_PREROLL;
    flow->turn_state_frame = 0;
    flow->turn_wait = 0;
    flow->turn_state_tick = now_ms;
    flow->dice_value = 0;
    flow->steps_remaining = 0;

    return 1;
}

void rich4_turn_flow_start_property_end_delay(GameFlowState *flow,
                                              uint32_t now_ms)
{
    if (flow == NULL) return;
    flow->property_turn_end_ticks = GAME_PROPERTY_TURN_END_TICKS;
    flow->property_turn_end_tick = now_ms;
}

int rich4_turn_flow_update_property_end_delay(GameFlowState *flow,
                                              uint32_t now_ms,
                                              uint32_t logic_tick_ms)
{
    if (flow == NULL || flow->property_turn_end_ticks <= 0)
        return 0;
    if (logic_tick_ms == 0 ||
        now_ms - flow->property_turn_end_tick < logic_tick_ms)
        return 1;
    flow->property_turn_end_ticks--;
    /* 0x401F98 exposes one pending bit, not a backlog of ticks. */
    flow->property_turn_end_tick +=
        ((now_ms - flow->property_turn_end_tick) / logic_tick_ms) * logic_tick_ms;
    if (flow->property_turn_end_ticks <= 0) {
        flow->property_turn_end_ticks = 0;
        return 0;
    }
    return 1;
}
