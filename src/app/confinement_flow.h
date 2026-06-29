#ifndef RICH4_APP_CONFINEMENT_FLOW_H
#define RICH4_APP_CONFINEMENT_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "core/game_domain.h"
#include "core/game_event.h"
#include "data/map_data.h"

GameDomainRef rich4_confinement_visit_target(int slot);
int rich4_confinement_anchor_xy(const MapData *map, int kind,
                                int *out_x, int *out_y);
int rich4_confinement_sync_player_actor_to_node(
    GameSession *session, const MapData *map, int player_index);
int rich4_confinement_visit_slot_at(
    const GameSession *session, int x, int y);
void rich4_confinement_set_hover(GameSession *session, int hover);
void rich4_confinement_set_confirm_hover(GameSession *session, int hover);
void rich4_confinement_begin(
    GameSession *session, const GameEvent *event, uint32_t now,
    Rich4AudioEventQueue *audio_events);
int rich4_confinement_visit_finish_route(GameSession *session, int cancel);
void rich4_confinement_finish(GameSession *session, int cancel);
int rich4_confinement_handle_select(
    GameSession *session, int slot, uint32_t now,
    Rich4AudioEventQueue *audio_events);
int rich4_confinement_handle_confirm(
    GameSession *session, int accept, uint32_t now,
    Rich4AudioEventQueue *audio_events);
int rich4_confinement_handle_cancel(
    GameSession *session, uint32_t now,
    Rich4AudioEventQueue *audio_events);
void rich4_confinement_visit_start_hospital_exit(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events);
void rich4_confinement_visit_skip_speech(
    GameSession *session, uint32_t now, Rich4AudioEventQueue *audio_events);
void rich4_confinement_visit_start_speech(
    GameSession *session, const Rich4CharacterSpeech *speech,
    int special_actor, uint32_t now, Rich4AudioEventQueue *audio_events);
int rich4_confinement_update(
    GameSession *session, uint32_t now, int speech_playing,
    Rich4AudioEventQueue *audio_events);

#endif
