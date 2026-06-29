#ifndef RICH4_APP_MONTHLY_FINANCE_FLOW_H
#define RICH4_APP_MONTHLY_FINANCE_FLOW_H

#include "app/game_session.h"

enum { RICH4_FINANCE_DIVIDEND = 0, RICH4_FINANCE_AFTER_DIVIDEND = -1,
       RICH4_FINANCE_NO_WINNER_REVEAL = 11 };
void rich4_periodic_finance_begin(GameSession *session, uint32_t now);
/* SDL supplies asset-derived frame timing and speech status only. */
int rich4_periodic_finance_update(GameSession *session, uint32_t now,
    int animation_enabled, int speech_playing, int animation_frames,
    int animation_delay, GameEventQueue *events);
void rich4_monthly_finance_begin(GameSession *session);
int rich4_monthly_finance_update(GameSession *session, uint32_t now,
    int animation_enabled, int speech_playing, int animation_frames,
    int animation_delay);
/* Original left/right release fast-forwards monthly speech/holds only;
 * Panel[76] closes. The lottery callback does not handle button releases. */
int rich4_monthly_finance_release(GameSession *session);
void rich4_periodic_finance_mark_music_started(GameSession *session);
/* -1 stops the current voice, 0 has no request, positive is Speaking.MKF. */
int rich4_monthly_finance_take_speech(GameSession *session, int animation_enabled);
int rich4_monthly_finance_take_effect(GameSession *session);
int rich4_monthly_animation_repeats(int character, int unlucky);
void rich4_monthly_banker_update(GameSession *session, int speaking);

#endif
