#ifndef RICH4_CORE_PROPERTY_H
#define RICH4_CORE_PROPERTY_H

#include "core/ledger.h"

typedef enum GameLandActionKind {
    GAME_LAND_ACTION_INVALID = 0,
    GAME_LAND_ACTION_NONE,
    GAME_LAND_ACTION_DEFERRED,
    GAME_LAND_ACTION_PURCHASE,
    GAME_LAND_ACTION_UPGRADE,
    GAME_LAND_ACTION_CHARGE,
    GAME_LAND_ACTION_COLLECTION_BLOCKED
} GameLandActionKind;

typedef enum GamePropertyCollectionBlockReason {
    GAME_PROPERTY_COLLECTION_BLOCK_NONE = 0,
    GAME_PROPERTY_COLLECTION_BLOCK_ALLIED,
    GAME_PROPERTY_COLLECTION_BLOCK_GOD,
    GAME_PROPERTY_COLLECTION_BLOCK_HOTEL,
    GAME_PROPERTY_COLLECTION_BLOCK_DISAPPEARING,
    GAME_PROPERTY_COLLECTION_BLOCK_PRISON,
    GAME_PROPERTY_COLLECTION_BLOCK_HOSPITAL,
    GAME_PROPERTY_COLLECTION_BLOCK_SLEEPING,
    GAME_PROPERTY_COLLECTION_BLOCK_SLEEPWALKING,
    GAME_PROPERTY_COLLECTION_BLOCK_SEALED
} GamePropertyCollectionBlockReason;

typedef struct GameLandAction {
    GameLandActionKind kind;
    int land;
    int actor;
    int owner;
    int amount;
    GamePropertyCollectionBlockReason collection_block_reason;
    /* A +65 relation can split one charge across two recipients. */
    int related_owner;
    int owner_amount;
    int related_amount;
} GameLandAction;

int rich4_land_purchase_price(const GameState *state, int land, int *out_price);
int rich4_land_upgrade_price(const GameState *state, int land, int *out_price);
int rich4_land_toll(const GameState *state, int land, int *out_toll);
int rich4_land_inspect(const GameState *state, int actor, int land,
                       GameLandAction *out_action);
int rich4_land_apply_command(GameState *state, const GameCommand *command,
                             GameEventQueue *events);
GamePaymentResult rich4_land_charge(GameState *state, int actor, int land,
                                    int *out_fee, GameEventQueue *events);
GamePaymentResult rich4_land_charge_with_cards(GameState *state, int actor,
                                               int land, int accept_free,
                                               int redirect_target,
                                               int *out_fee,
                                               GameEventQueue *events);

#endif
