#ifndef RICH4_APP_BANKRUPTCY_FLOW_H
#define RICH4_APP_BANKRUPTCY_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_event.h"

/* The original 0x40CD87 path commits the economic cleanup immediately, then
 * blocks the outer turn loop while its Data.MKF[555] bankruptcy animation and
 * 2000 ms hold are shown.  Auction and terminal events are therefore kept in
 * transient app queues until that presentation boundary has completed. */
void rich4_bankruptcy_capture_events(GameSession *session,
                                     GameEventQueue *events,
                                     uint32_t now);

/* Capture one event from command/special-event paths that do not expose a
 * complete queue to the caller.  Returns nonzero when the event was consumed
 * by the bankruptcy follow-up machinery. */
int rich4_bankruptcy_capture_event(GameSession *session,
                                   const GameEvent *event,
                                   uint32_t now);

/* Platform supplies the decoded Data.MKF[555] frame metadata. */
int rich4_bankruptcy_update(GameSession *session, int frame_count,
                            int delay_ms, uint32_t now);

/* Called after the last bankruptcy auction has settled.  For the original
 * multi-player path this starts the eliminated player's type-2 speech first;
 * terminal events are released only after that speech has finished. */
void rich4_bankruptcy_finish_auctions(GameSession *session, uint32_t now);

/* Called by the SDL speech adapter after the post-auction speech is done. */
void rich4_bankruptcy_finish_speech(GameSession *session);

/* Release a delayed GAME_ENDED queue without starting speech.  This remains
 * useful for the one-player terminal path and for suppressed speech. */
void rich4_bankruptcy_release_terminal_events(GameSession *session);

#endif
