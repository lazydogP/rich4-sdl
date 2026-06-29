#ifndef RICH4_CORE_CONFINEMENT_H
#define RICH4_CORE_CONFINEMENT_H

#include <stdint.h>

#include "core/game_event.h"
#include "core/game_state.h"

typedef enum GameConfinementResult {
    GAME_CONFINEMENT_APPLIED = 1,
    GAME_CONFINEMENT_INVALID,
    GAME_CONFINEMENT_EVENT_CAPACITY
} GameConfinementResult;

typedef struct GameStatusPlayerSnapshot {
    int node, next_node, previous_node, play_state;
    int status[12];
    int god_info, god_type;
    int god_income_modifier, god_investment_modifier, god_land_modifier;
    int linked_player;
    int confinement_actor_type, total_confinement_days;
} GameStatusPlayerSnapshot;

typedef struct GameStatusSnapshot {
    uint32_t version;
    int player_count;
    int prison_node, hospital_node;
    GameStatusPlayerSnapshot players[GAME_MAX_PLAYERS];
    int special_node[GAME_SPECIAL_PLAYER_COUNT];
    int special_actor_type[GAME_SPECIAL_PLAYER_COUNT];
    int special_confinement[GAME_SPECIAL_PLAYER_COUNT];
} GameStatusSnapshot;

#define GAME_STATUS_SNAPSHOT_VERSION 1u

GameConfinementResult rich4_confinement_enter(GameState *state, int actor,
                                               int kind, int days,
                                               GameEventQueue *events);
/* Enter while the caller retains the claim and total-days continuation. */
GameConfinementResult rich4_confinement_enter_deferred_claim(
    GameState *state, int actor, int kind, int days, GameEventQueue *events);
/* Validate admission now; ordinary actors commit after their days reaction. */
GameConfinementResult rich4_confinement_request_enter(
    GameState *state, int actor, int kind, int days, GameEventQueue *events);
void rich4_confinement_clear_status(GamePlayerState *player);
void rich4_confinement_injure_player(GameState *state, int actor);
/* 0x40D375: travel/alien disappearance; extensions have no second claim. */
GameConfinementResult rich4_confinement_disappear(GameState *state, int actor,
    int days, int alien, int defer_claim, GameEventQueue *events);
GameConfinementResult rich4_confinement_release(GameState *state, int actor,
                                                 int kind,
                                                 GameEventQueue *events);
int rich4_confinement_visit_cost(GameDomainRef target);
int rich4_confinement_visit_candidate(const GameState *state,
                                      GameDomainRef target, int kind);
GameConfinementResult rich4_confinement_visit_release(
    GameState *state, int visitor, GameDomainRef target, int kind,
    GameEventQueue *events);
int rich4_status_snapshot_export(const GameState *state, GameStatusSnapshot *out);
int rich4_status_snapshot_import(GameState *state, const GameStatusSnapshot *in);

#endif
