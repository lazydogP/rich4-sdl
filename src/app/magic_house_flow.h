#ifndef RICH4_APP_MAGIC_HOUSE_FLOW_H
#define RICH4_APP_MAGIC_HOUSE_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "core/game_event.h"

/* Separate original callback 0x433088, sharing Panel[18]/[20]. */
#define RICH4_MAGIC_HOUSE_SURRENDER 12
void rich4_magic_house_begin_surrender(GameSession *session, int player,
    unsigned int targets, uint32_t now, Rich4AudioEventQueue *audio_events);
int rich4_magic_house_surrender_target_at(const GameSession *session, int x, int y);
int rich4_magic_house_surrender_target_x(unsigned int targets, int player);
void rich4_magic_house_surrender_choose(GameSession *session, int player,
    uint32_t now, Rich4AudioEventQueue *audio_events);
void rich4_magic_house_surrender_cancel(GameSession *session,
    Rich4AudioEventQueue *audio_events);

const char *rich4_magic_house_action_big5(int action);
const char *rich4_magic_house_condition_big5(int condition);
const char *rich4_magic_house_phase_big5(int phase, int condition);
int rich4_magic_house_facility_at(int x, int y);
void rich4_magic_house_set_hover(GameSession *session, int hover);
void rich4_magic_house_set_facility_choice_hover(
    GameSession *session, int hover);
void rich4_magic_house_request_return_to_selection(GameSession *session);
int rich4_magic_house_choose_facility(
    GameSession *session, int choice, uint32_t now,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events);

void rich4_magic_house_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    int animation_enabled, Rich4AudioEventQueue *audio_events);
void rich4_magic_house_start_phase(
    GameSession *session, int phase, uint32_t now,
    Rich4AudioEventQueue *audio_events);
void rich4_magic_house_skip_intro(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events);
void rich4_magic_house_select_action(
    GameSession *session, int action, uint32_t now,
    Rich4AudioEventQueue *audio_events);
void rich4_magic_house_finish(
    GameSession *session, int action, uint32_t now,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events);
int rich4_magic_house_update(
    GameSession *session, int finale_frames, int finale_delay_ms,
    uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events, GameEventQueue *out_events);

#endif
