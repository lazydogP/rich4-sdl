#ifndef RICH4_APP_ENGINEERING_ATTACK_FLOW_H
#define RICH4_APP_ENGINEERING_ATTACK_FLOW_H

#include "app/game_session.h"

/* 0x448A7E arms EA2=3; resource metadata is supplied by the SDL adapter. */
int rich4_engineering_attack_begin(GameSession *session, int player,
                                    int node, uint32_t now);
int rich4_engineering_attack_update(GameSession *session, int actor_frames,
                                     int effect_frames, int tick_ms,
                                     uint32_t now,
                                     Rich4AudioEventQueue *audio_events);

#endif
