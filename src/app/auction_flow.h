#ifndef RICH4_APP_AUCTION_FLOW_H
#define RICH4_APP_AUCTION_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "core/auction.h"
#include "core/game_event.h"

/* 0x43BB25: seven inclusive hit boxes centered at y=133+48*n. */
int rich4_auction_action_at(int x, int y);
void rich4_auction_flow_set_pressed(GameSession *session, int action);
int rich4_auction_flow_is_human_turn(const GameSession *session);

/* Starts immediately when idle and otherwise preserves request order. */
int rich4_auction_flow_enqueue(
    GameSession *session, const GameEvent *event,
    Rich4AudioEventQueue *audio_events);

GameAuctionStepResult rich4_auction_flow_submit(
    GameSession *session, GameAuctionAction action, uint32_t now,
    Rich4AudioEventQueue *audio_events);

/* Advances one original modal transition per call.  Settlement events are
 * appended to out_events when supplied. */
int rich4_auction_flow_update(
    GameSession *session, uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events);

#endif
