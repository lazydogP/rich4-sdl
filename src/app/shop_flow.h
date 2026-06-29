#ifndef RICH4_APP_SHOP_FLOW_H
#define RICH4_APP_SHOP_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "core/game_event.h"

const char *rich4_shop_message_big5(int index);
void rich4_shop_begin(GameSession *session, const GameEvent *event,
                      uint32_t now, int animation_enabled,
                      Rich4AudioEventQueue *audio_events);
int rich4_shop_update(GameSession *session, uint32_t now,
                      int speech_playing,
                      Rich4AudioEventQueue *audio_events);
int rich4_shop_skip_message(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events);
int rich4_shop_hit(const GameSession *session, int x, int y);
int rich4_shop_pointer_down(GameSession *session, int x, int y, uint32_t now,
                            Rich4AudioEventQueue *audio_events);
void rich4_shop_pointer_up(GameSession *session, int released_control,
                           int x, int y, uint32_t now,
                           Rich4AudioEventQueue *audio_events);
void rich4_shop_cancel(GameSession *session,
                       Rich4AudioEventQueue *audio_events);
int rich4_shop_apply_transaction(GameSession *session, int hit, uint32_t now,
                                 Rich4AudioEventQueue *audio_events);
void rich4_shop_switch_page(GameSession *session, uint32_t now,
                            Rich4AudioEventQueue *audio_events);
void rich4_shop_begin_exit(GameSession *session, uint32_t now,
                           Rich4AudioEventQueue *audio_events);
void rich4_shop_finish(GameSession *session);
void rich4_shop_mark_music_started(GameSession *session);

#endif
