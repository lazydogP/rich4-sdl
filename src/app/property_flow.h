#ifndef RICH4_APP_PROPERTY_FLOW_H
#define RICH4_APP_PROPERTY_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_event.h"

/* App-owned property presentation. Resource frame counts and audio playback
 * are observed at the platform boundary and passed as scalar inputs. */
int rich4_property_start_max_level_animation(
    GameSession *session, int frame_count, uint32_t now);
int rich4_property_update_max_level_animation(
    GameSession *session, int frame_count, int delay_ms, uint32_t now);
void rich4_property_begin_level_presentation(
    GameSession *session, const GameEvent *event, int max_level_frames,
    uint32_t now);
void rich4_property_begin_charge_presentation(
    GameSession *session, const GameEvent *event, uint32_t now);
void rich4_property_begin_beggar_presentation(
    GameSession *session, const GameEvent *event, uint32_t now);
void rich4_property_begin_god_trade_rejection(
    GameSession *session, const GameEvent *event, uint32_t now);
int rich4_property_begin_god_landing(GameSession *session, int player,
                                      int node, uint32_t now);
int rich4_property_choose_god_building(GameSession *session,
                                       const GameCommand *command);
int rich4_property_skip_wait(GameSession *session, uint32_t now);
void rich4_property_flash_frame_presented(GameSession *session, uint32_t now);
int rich4_property_update_charge_presentation(
    GameSession *session, uint32_t now);
/* -1 declines; otherwise a player ID (ignored by the free-card prompt). */
int rich4_property_insurance_choose(GameSession *session, int target, uint32_t now);
int rich4_property_insurance_hover(GameSession *session, int target);

#endif
