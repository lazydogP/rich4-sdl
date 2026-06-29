#ifndef RICH4_CORE_BANK_H
#define RICH4_CORE_BANK_H

#include "core/game_event.h"
#include "core/game_state.h"

typedef struct Rich4BankSettlement {
    unsigned int active_mask;
    unsigned int loan_mask;
    int bank_before[GAME_MAX_PLAYERS];
    int interest[GAME_MAX_PLAYERS];
    int bank_after[GAME_MAX_PLAYERS];
} Rich4BankSettlement;

/* sub_4239B9: liquid net worth plus stocks and owned property values. */
int rich4_bank_total_assets(const GameState *state, int player);
/* sub_436EF8: the pass-bank window transfers money between bank and cash. */
int rich4_bank_withdraw(GameState *state, int player, int amount);
int rich4_bank_deposit(GameState *state, int player, int amount);
int rich4_bank_run_ai_transfer(GameState *state, int player);
/* Event-producing variant used by the route executor.  When the original
 * balance-band gate requests a transfer, it also runs the owner shortfall
 * collection from 0x436B0A(1). */
int rich4_bank_run_ai_transfer_with_events(
    GameState *state, int player, GameEventQueue *events);
/* 0x436B0A: mode 0 collects special-finance balances from active non-owner
 * players; mode 1 collects a company owner's uncovered balance shortfall. */
int rich4_bank_settle_special_finance(
    GameState *state, int mode, int current_player, GameEventQueue *events);
int rich4_bank_loan_available(const GameState *state, int player);
int rich4_bank_apply_loan(GameState *state, int player, int amount);
int rich4_bank_repay_loan(GameState *state, int player, int amount);
int rich4_bank_special_available(const GameState *state, int player);
int rich4_bank_special_borrow(GameState *state, int player, int amount);
int rich4_bank_special_repay(GameState *state, int player, int amount);
int rich4_bank_run_ai(GameState *state, int player);
/* sub_439BFA/sub_437E61: monthly 10% savings interest.  Ordinary
 * borrowers are listed as "loan in progress" and receive no interest. */
int rich4_bank_monthly_settle(GameState *state,
                              Rich4BankSettlement *out_settlement);
/* Player record +44 stores the first-loan deadline.  In this port it is an
 * absolute elapsed-day value; later borrowing does not extend it. */
int rich4_bank_loan_days_remaining(const GameState *state, int player);

#endif
