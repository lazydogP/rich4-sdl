#ifndef RICH4_APP_INTRO_FLOW_H
#define RICH4_APP_INTRO_FLOW_H

/* App-owned intro timeline.  The platform adapter supplies immutable frame
 * timing extracted from assets and a borrowed map; no SDL/resource types
 * cross this boundary. Evidence/tests: A5/A7; test_intro_flow.c. */
#include <stdint.h>

#include "app/game_session.h"
#include "core/game_constants.h"
#include "data/map_data.h"
#include "data/resource_constants.h"
#include "view/view_constants.h"

typedef enum Rich4IntroStageKind {
    RICH4_INTRO_STAGE_WINDOW,
    RICH4_INTRO_STAGE_TAKEOFF,
    RICH4_INTRO_STAGE_PARACHUTE,
    RICH4_INTRO_STAGE_DONE
} Rich4IntroStageKind;

typedef struct Rich4IntroStage {
    Rich4IntroStageKind kind;
    int player_index;
    int static_player_index;
    int graph_frame;
    int frame_count;
    int delay_ms;
} Rich4IntroStage;

typedef struct Rich4IntroTiming {
    int window_frame_count;
    int window_delay_ms;
    int actor_frame_count[2][GAME_MAX_PLAYERS];
    int actor_delay_ms[2][GAME_MAX_PLAYERS];
} Rich4IntroTiming;

int rich4_intro_jump_character_index(const GamePlayerState *player,
                                     int fallback);
void rich4_intro_begin(GameSession *session);
void rich4_intro_set_avi_selection(GameSession *session, int stage, int map,
                                   const char *avi_name);
Rich4IntroStage rich4_intro_stage_for_frame(const Rich4IntroTiming *timing,
                                             const GameSession *session,
                                             int intro_frame);
int rich4_intro_total_frames(const Rich4IntroTiming *timing,
                             const GameSession *session);
void rich4_intro_finish(GameSession *session);
void rich4_intro_finish_at_tick(GameSession *session, uint32_t now);
void rich4_intro_request_skip(GameSession *session);
int rich4_intro_finish_if_requested(GameSession *session);
int rich4_intro_finish_if_requested_at_tick(GameSession *session, uint32_t now);
void rich4_intro_update(GameSession *session, const Rich4IntroTiming *timing,
                        const MapData *map, uint32_t now);

#endif
