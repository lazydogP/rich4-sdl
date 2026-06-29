#ifndef RICH4_APP_SPECIAL_EVENT_TIMELINE_H
#define RICH4_APP_SPECIAL_EVENT_TIMELINE_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_command.h"

const GamePlayerState *rich4_special_event_original_player(
    const GameSession *session, int player);
int rich4_special_event_prepare_next_news_victim(GameSession *session);
int rich4_special_event_begin_fortune_card_targets(
    GameSession *session, uint32_t now);
int rich4_special_event_advance_fortune_card_target(
    GameSession *session, uint32_t now);
void rich4_special_event_set_reaction_stage(
    GameSession *session, int reaction_stage);
void rich4_special_event_set_first_confinement(
    GameSession *session, const GamePlayerState *before);
int rich4_special_event_begin_timed_phase(
    GameSession *session, int phase, int frame_count, int delay_ms,
    uint32_t now, int scene_refresh_frame);
int rich4_special_event_begin_consequence(
    GameSession *session, int logical_id, int consequence_index,
    int frame_count, int delay_ms, uint32_t now);
void rich4_special_event_update_timeline(
    GameSession *session, int frame_count, int delay_ms, uint32_t now);
void rich4_special_event_mark_scene_refreshed(GameSession *session);
int rich4_special_event_start_reaction_delay(
    GameSession *session, uint32_t now);
int rich4_special_event_finish_callback(GameSession *session, uint32_t now);
void rich4_special_event_begin_hold(GameSession *session, uint32_t now);
void rich4_special_event_center_callback_view(GameSession *session);
void rich4_special_event_deactivate(GameSession *session);
void rich4_special_event_complete_route_interaction(GameSession *session);
void rich4_special_event_restore_route_interaction(
    GameSession *session, const GameCommand *command);

#endif
