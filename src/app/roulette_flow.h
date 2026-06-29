#ifndef RICH4_APP_ROULETTE_FLOW_H
#define RICH4_APP_ROULETTE_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "core/game_event.h"

void rich4_roulette_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    Rich4AudioEventQueue *audio_events);

void rich4_roulette_stop(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events);

int rich4_roulette_update(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events);

#endif
