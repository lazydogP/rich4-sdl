#ifndef RICH4_PLATFORM_SDL1_GAME_CONFINEMENT_RENDER_VIEW_H
#define RICH4_PLATFORM_SDL1_GAME_CONFINEMENT_RENDER_VIEW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/game_constants.h"
#include "data/character_speech.h"

typedef struct GameConfinementRenderView {
    int player_count;
    int player_character[GAME_MAX_PLAYERS];
    uint16_t player_points[GAME_MAX_PLAYERS];
    unsigned int visit_candidate_mask;
    int confinement_status_active;
    uint32_t confinement_status_until;
    int confinement_status_player;
    int confinement_status_slot;
    int confinement_status_days;
    int confinement_visit_active;
    int confinement_visit_kind;
    int confinement_visit_nurse_side;
    int confinement_visit_nurse_gesture_frame;
    int confinement_visit_nurse_idle_chunk;
    int confinement_visit_phase;
    int confinement_visit_selected;
    int confinement_visit_hover;
    int confinement_visit_confirm_hover;
    int confinement_visit_speech_active;
    const Rich4CharacterSpeech *confinement_visit_speech;
    int confinement_visit_speech_special_actor;
    int speech_active;
    int route_execution_actor;
} GameConfinementRenderView;

int build_confinement_render_view(const GameSession *session,
                                  GameConfinementRenderView *out_view);

#endif
