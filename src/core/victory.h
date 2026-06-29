#ifndef RICH4_CORE_VICTORY_H
#define RICH4_CORE_VICTORY_H

#include "core/game_state.h"
#include "core/game_event.h"

typedef struct PlayerAssetBreakdown {
    int cash_and_bank;
    int loan_deduction;
    int stock_value;
    int land_value;
    int facility_value;
    int total;
} PlayerAssetBreakdown;

typedef enum VictoryDomainEvent {
    VICTORY_EVENT_NONE = 0,
    VICTORY_EVENT_CANDIDATE
} VictoryDomainEvent;

typedef struct VictoryResult {
    VictoryDomainEvent event;
    int player;
    int reason;
    PlayerAssetBreakdown assets;
    int special_finance_excluded;
} VictoryResult;

int rich4_player_cash_bank_assets(const GameState *state, int player);
int rich4_player_stock_assets(const GameState *state, int player);
int rich4_player_land_assets(const GameState *state, int player);
int rich4_player_facility_assets(const GameState *state, int player);
int rich4_player_total_assets(const GameState *state,
                              int player,
                              PlayerAssetBreakdown *out_breakdown);
int rich4_victory_evaluate(GameState *state, VictoryResult *out_result);
int rich4_victory_evaluate_events(GameState *state,
                                  VictoryResult *out_result,
                                  GameEventQueue *events);
/* Commit a newly reached day-limit/asset-target victory.  The operation is
 * transactional: it appends VICTORY_CANDIDATE followed by GAME_ENDED and
 * only then updates the winner fields.  Bankruptcy/last-player endings stay
 * in ledger.c because they are discovered during payment settlement. */
int rich4_victory_finalize_events(GameState *state,
                                  VictoryResult *out_result,
                                  GameEventQueue *events);

#endif
