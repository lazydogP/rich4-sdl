#ifndef RICH4_APP_LOTTERY_FLOW_H
#define RICH4_APP_LOTTERY_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_event.h"

typedef enum Rich4LotteryAudioEffect {
    RICH4_LOTTERY_AUDIO_NONE = 0,
    RICH4_LOTTERY_AUDIO_VISIT,
    RICH4_LOTTERY_AUDIO_PURCHASE,
    RICH4_LOTTERY_AUDIO_CANCEL
} Rich4LotteryAudioEffect;

/* SDL consumes this request after the app updates the deterministic modal
 * state. The app never owns an audio handle. */
typedef struct Rich4LotteryAudioRequest {
    Rich4LotteryAudioEffect effect;
    int start_speech_resource;
    int stop_speech;
} Rich4LotteryAudioRequest;

void rich4_lottery_begin_visit(
    GameSession *session, const GameEvent *event, uint32_t now,
    int animation_enabled, Rich4LotteryAudioRequest *request);

int rich4_lottery_update(
    GameSession *session, int panel_frames, uint32_t now,
    int speech_playing, Rich4LotteryAudioRequest *request);

int rich4_lottery_number_at(const GameSession *session, int x, int y);
void rich4_lottery_set_hover(GameSession *session, int hover);

void rich4_lottery_cancel_visit(
    GameSession *session, uint32_t now, Rich4LotteryAudioRequest *request);

void rich4_lottery_left_button_down(
    GameSession *session, int pointer_x, int pointer_y, uint32_t now,
    Rich4LotteryAudioRequest *request);

void rich4_lottery_animation_prepared(GameSession *session, uint32_t now);
#endif
