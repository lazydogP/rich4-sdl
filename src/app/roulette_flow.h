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

/* Original 0x475CF8 mode-indexed prompt table and the result formats used by
 * the mode-specific callers after 0x44090E returns. */
const char *rich4_roulette_prompt_format_big5(int mode);
const char *rich4_roulette_result_format_big5(int mode);

#endif
