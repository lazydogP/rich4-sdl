#ifndef RICH4_APP_GOD_MONEY_FLOW_H
#define RICH4_APP_GOD_MONEY_FLOW_H

#include "app/audio_event.h"
#include "app/game_session.h"

void rich4_god_money_begin(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events);
/* 0x43F23E consumes any key-up or either mouse-button release. */
void rich4_god_money_stop(GameSession *session);
int rich4_god_money_update(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events,
                            GameEventQueue *events);

#endif
