#ifndef RICH4_APP_OPTIONS_GAME_FLOW_H
#define RICH4_APP_OPTIONS_GAME_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"

/* Apply the in-game surrender action at the app boundary.  The platform
 * supplies the shared audio handoff queue; domain events, elimination
 * presentation, and next-turn preparation stay app-owned. */
int rich4_app_surrender_current_player(
    GameSession *session, Rich4AudioEventQueue *audio_events,
    uint32_t now);
void rich4_app_surrender_followup(GameSession *session, uint32_t now,
                                 Rich4AudioEventQueue *audio_events);

#endif
