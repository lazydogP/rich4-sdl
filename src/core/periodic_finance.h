#ifndef RICH4_CORE_PERIODIC_FINANCE_H
#define RICH4_CORE_PERIODIC_FINANCE_H

#include "core/game_state.h"
#include "core/game_event.h"

/* 0x42BA97: signed company +0x28, float32 share ratios, per-player CHP. */
typedef struct Rich4CompanyDividend {
    unsigned int player_mask;
    unsigned int stock_mask;
    int company[GAME_STOCK_COUNT];
    int profit[GAME_STOCK_COUNT];
    int amount[GAME_STOCK_COUNT][GAME_MAX_PLAYERS];
    int total[GAME_MAX_PLAYERS];
} Rich4CompanyDividend;

int rich4_company_dividend_prepare(GameState *state, Rich4CompanyDividend *out);
/* The original credits aggregated totals only after Panel[76] closes. */
int rich4_company_dividend_commit(GameState *state,
    const Rich4CompanyDividend *dividend, GameEventQueue *events);
int rich4_lottery_has_tickets(const GameState *state);
/* 0x430B2A: >10 tickets held by one player forces the sold-number branch. */
int rich4_lottery_draw(GameState *state, int *out_number, int *out_player);
/* 0x430AD0: no winner preserves both the pool and the owners. */
int rich4_lottery_claim(GameState *state, int number, GameEventQueue *events);
int rich4_monthly_unlucky_player(const GameState *state, unsigned int mask);
int rich4_monthly_richest_player(const GameState *state, unsigned int mask);
void rich4_monthly_clear_statistics(GameState *state, unsigned int mask);

#endif
