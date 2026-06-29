#ifndef RICH4_CORE_AUCTION_H
#define RICH4_CORE_AUCTION_H

#include "core/game_event.h"
#include "core/game_state.h"
#include "core/rng.h"

typedef enum GameAuctionTrigger {
    GAME_AUCTION_TRIGGER_NONE = 0,
    GAME_AUCTION_TRIGGER_CARD,
    GAME_AUCTION_TRIGGER_NEWS,
    GAME_AUCTION_TRIGGER_MAGIC_HOUSE,
    GAME_AUCTION_TRIGGER_BANKRUPTCY
} GameAuctionTrigger;

#define GAME_AUCTION_CLEAR_OWNER_IF_UNSOLD 0x01u

typedef enum GameAuctionPhase {
    GAME_AUCTION_IDLE = 0,
    GAME_AUCTION_BIDDING,
    GAME_AUCTION_SOLD,
    GAME_AUCTION_UNSOLD,
    GAME_AUCTION_SETTLED
} GameAuctionPhase;

/* The numeric values match 0x43A2DD's private message values. */
typedef enum GameAuctionAction {
    GAME_AUCTION_PASS = 0,
    GAME_AUCTION_BID_100,
    GAME_AUCTION_BID_500,
    GAME_AUCTION_BID_1000,
    GAME_AUCTION_BID_5000,
    GAME_AUCTION_BID_10000,
    GAME_AUCTION_GIVE_UP
} GameAuctionAction;

typedef enum GameAuctionParticipantState {
    GAME_AUCTION_PARTICIPANT_EXCLUDED = 0,
    GAME_AUCTION_PARTICIPANT_READY,
    GAME_AUCTION_PARTICIPANT_PASSED,
    GAME_AUCTION_PARTICIPANT_WITHDRAWN
} GameAuctionParticipantState;

typedef enum GameAuctionStepResult {
    GAME_AUCTION_STEP_INVALID = 0,
    GAME_AUCTION_STEP_NEXT,
    GAME_AUCTION_STEP_SOLD,
    GAME_AUCTION_STEP_UNSOLD
} GameAuctionStepResult;

typedef enum GameAuctionSettlementResult {
    GAME_AUCTION_SETTLEMENT_REJECTED = 0,
    GAME_AUCTION_SETTLEMENT_SOLD,
    GAME_AUCTION_SETTLEMENT_UNSOLD,
    GAME_AUCTION_SETTLEMENT_BANKRUPTCY
} GameAuctionSettlementResult;

typedef struct GameAuctionState {
    int active;
    GameAuctionPhase phase;
    int seller;
    GameDomainRef target;
    int manage_music;
    int reserve_price;
    int current_price;
    int current_player;
    int leader;
    int eligible_count;
    int participant[GAME_MAX_PLAYERS];
    int ai_ceiling[GAME_MAX_PLAYERS];
    int last_player;
    GameAuctionAction last_action;
    int last_bid;
} GameAuctionState;

int rich4_auction_reserve_price(
    const GameState *state, GameDomainRef target, int *out_price);
int rich4_auction_begin(
    GameAuctionState *auction, const GameState *state, int seller,
    GameDomainRef target, int manage_music, Rich4Rng *rng);
int rich4_auction_action_amount(GameAuctionAction action);
GameAuctionAction rich4_auction_ai_action(
    const GameAuctionState *auction, const GameState *state, int player);
GameAuctionStepResult rich4_auction_apply_action(
    GameAuctionState *auction, const GameState *state, int player,
    GameAuctionAction action);
GameAuctionSettlementResult rich4_auction_settle(
    GameAuctionState *auction, GameState *state, unsigned int caller_flags,
    GameEventQueue *events);

#endif
