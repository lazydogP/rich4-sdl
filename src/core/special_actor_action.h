#ifndef RICH4_CORE_SPECIAL_ACTOR_ACTION_H
#define RICH4_CORE_SPECIAL_ACTOR_ACTION_H

#include "core/game_event.h"
#include "core/game_state.h"
#include "data/map_data.h"

#define GAME_SPECIAL_ACTOR_ACTION_BATCH_MAX 5

typedef enum GameSpecialActorActionKind {
    GAME_SPECIAL_ACTOR_ACTION_NONE = 0,
    GAME_SPECIAL_ACTOR_ACTION_THIEF_POINTS,
    GAME_SPECIAL_ACTOR_ACTION_ROBBER_CARD,
    GAME_SPECIAL_ACTOR_ACTION_ROBBER_BANK,
    GAME_SPECIAL_ACTOR_ACTION_HOOLIGAN_FEE,
    GAME_SPECIAL_ACTOR_ACTION_SPY_TOLL,
    GAME_SPECIAL_ACTOR_ACTION_SPY_DIVIDEND
} GameSpecialActorActionKind;

typedef struct GameSpecialActorAction {
    GameSpecialActorActionKind kind;
    int special_slot;
    int owner_player;
    int target_player;
    int amount;
    int card;
    GameDomainRef property;
} GameSpecialActorAction;

typedef struct GameSpecialActorActionBatch {
    int count;
    GameSpecialActorAction action[GAME_SPECIAL_ACTOR_ACTION_BATCH_MAX];
} GameSpecialActorActionBatch;

/* Applies the original 0x41B42D effect for one visibly reached node.
 * is_endpoint gates hooligan/spy property effects.  Sleepwalking special
 * actors return success with an empty batch. */
int rich4_special_actor_apply_step(
    GameState *state, const MapData *map, int special_slot, int node,
    int is_endpoint, GameSpecialActorActionBatch *out_batch,
    GameEventQueue *events);

#endif
