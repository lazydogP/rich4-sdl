#ifndef RICH4_CORE_TURN_FLOW_H
#define RICH4_CORE_TURN_FLOW_H

#include <stdint.h>

#include "core/game_state.h"
#include "core/game_flow_state.h"

/* Starts the presentation flow for the current turn.  First turns enter the
 * landing state before the normal preroll/action state. */
int rich4_turn_flow_begin(const GameState *state, GameFlowState *flow,
                          int first_turn_landing_pending,
                          uint32_t now_ms);

/* Starts and advances the original 0x88 ordinary-property end-turn
 * countdown.  update returns nonzero while turn rotation must remain
 * blocked.  At most one original logic tick is consumed per call. */
void rich4_turn_flow_start_property_end_delay(GameFlowState *flow,
                                              uint32_t now_ms);
int rich4_turn_flow_update_property_end_delay(GameFlowState *flow,
                                              uint32_t now_ms,
                                              uint32_t logic_tick_ms);

#endif
