#ifndef RICH4_APP_BANK_FLOW_H
#define RICH4_APP_BANK_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "app/audio_event.h"
#include "core/route_event.h"

/* App-owned bank presentation timing. SDL supplies the clock and consumes the
 * returned blocking decision; it does not own the bank flow state machine. */
int rich4_bank_flow_update(
    GameSession *session, uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events);
void rich4_bank_flow_begin_interaction(
    GameSession *session, int accept_type, const RouteEvent *route_event,
    uint32_t now, int animation_enabled,
    Rich4AudioEventQueue *audio_events);
int rich4_bank_flow_begin_amount_prompt(
    GameSession *session, int mode, uint32_t now);
int rich4_bank_flow_begin_result(
    GameSession *session, GameCommandType command_type, uint32_t now);
void rich4_bank_flow_cancel_amount_entry(GameSession *session, uint32_t now);

#endif
