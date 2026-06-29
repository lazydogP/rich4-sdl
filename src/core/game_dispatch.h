#ifndef RICH4_CORE_GAME_DISPATCH_H
#define RICH4_CORE_GAME_DISPATCH_H

#include "core/game_event.h"
#include "core/game_flow_state.h"
#include "core/game_state.h"
#include "core/route_event_executor.h"

int rich4_game_dispatch_command(GameState *state,
                                GameFlowState *flow,
                                const GameCommand *command,
                                GameEventQueue *events);
int rich4_game_dispatch_command_deferred_confinement(GameState *state,
    GameFlowState *flow, const GameCommand *command, GameEventQueue *events);

#endif
