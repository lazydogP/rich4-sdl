#ifndef RICH4_APP_TURN_STATUS_FLOW_H
#define RICH4_APP_TURN_STATUS_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_event.h"

/* App-owned status work performed when the core selects the next turn.  The
 * core supplies deterministic status/facility/victory rules; this module
 * records app debug facts and presentation dirtiness without SDL. Evidence:
 * A4/A5, tests/unit/test_turn_status_flow.c. */
void rich4_app_update_player_status_countdowns(GameSession *session,
                                               int player_index);
int rich4_app_process_pending_status_events(GameSession *session,
                                            int player_index);
void rich4_app_update_win_candidate(GameSession *session);
void rich4_app_capture_game_ended(GameSession *session,
                                  const GameEvent *event);
void rich4_app_update_special_player_status_countdowns(GameSession *session,
                                                       int slot);
int rich4_app_begin_confinement_status_presentation(
    GameSession *session, int player_index, int status_slot, uint32_t now);
int rich4_app_update_confinement_status_presentation(
    GameSession *session, uint32_t now, int speech_playing);

#endif
