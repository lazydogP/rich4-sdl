#ifndef RICH4_CORE_TURN_AROUND_H
#define RICH4_CORE_TURN_AROUND_H

#include "core/game_domain.h"
#include "core/game_state.h"

/* 0x40C78C: choose a new route edge while excluding the actor's current
 * forward edge.  Presentation direction and Effect[56] stay in app/audio. */
int rich4_turn_around_route(GameState *state, GameDomainRef target);

#endif
