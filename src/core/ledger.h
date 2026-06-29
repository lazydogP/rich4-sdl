#ifndef RICH4_CORE_LEDGER_H
#define RICH4_CORE_LEDGER_H

#include "core/game_event.h"
#include "core/game_state.h"

typedef enum GameLedgerPartyKind {
    GAME_LEDGER_SYSTEM = 0,
    GAME_LEDGER_PLAYER,
    GAME_LEDGER_COMPANY
} GameLedgerPartyKind;

typedef struct GameLedgerParty {
    GameLedgerPartyKind kind;
    int id;
} GameLedgerParty;

typedef enum GamePaymentResult {
    GAME_PAYMENT_REJECTED = 0,
    GAME_PAYMENT_COMMITTED,
    GAME_PAYMENT_LIQUIDATION,
    GAME_PAYMENT_BANKRUPTCY
} GamePaymentResult;

GameLedgerParty rich4_ledger_system(void);
GameLedgerParty rich4_ledger_player(int player);
GameLedgerParty rich4_ledger_company(int stock);
int rich4_ledger_adjust_fee(int amount, int god_type, int *out_amount);
int rich4_ledger_spend_cash(GameState *state, int payer, int amount,
                            GameEventQueue *events);
/* Applies the same 0x40CD87 player cleanup used by insufficient-liquidity
 * bankruptcy, but as an explicit player-initiated surrender transaction. */
int rich4_ledger_surrender_player(GameState *state, int player,
                                   GameEventQueue *events);
/* Original 0x41B42D/0x40D293 interaction: return the lowest-index inactive
 * player sharing the stopping node, or -1 when the node has no beggar. */
int rich4_ledger_find_beggar(const GameState *state, int actor, int node);
int rich4_ledger_beggar_amount(const GameState *state);
/* Settles the original 1000 * price_index donation and then relocates the
 * beggar using the 0x40AA6C candidate scan.  The payer may go bankrupt; in
 * that case the normal ledger cleanup and auction events are staged first. */
GamePaymentResult rich4_ledger_beggar_donate(
    GameState *state, int payer, int beggar, int node, int amount,
    GameEventQueue *events);
GamePaymentResult rich4_ledger_pay(GameState *state, int payer,
                                   GameLedgerParty payee, int amount,
                                   unsigned int flags, GameEventQueue *events);
/* 0x431DBC continues the selected player's three fortunes after bankruptcy.
 * 0x41D2C6 still settles that player's accounts; 0x40CD87 is then a no-op
 * for an already inactive payer. Other inactive parties remain invalid. */
GamePaymentResult rich4_ledger_pay_fortune(GameState *state, int payer,
    GameLedgerParty payee, int amount, unsigned int flags, GameEventQueue *events);
/* Atomically settles the two-recipient property-charge path.  The first
 * recipient is paid before the second recipient, matching Rich4.exe's two
 * calls into 0x41D2C6; an insufficient payer balance is allocated in that
 * order and bankruptcy is committed only after both partial transfers have
 * been staged. */
GamePaymentResult rich4_ledger_pay_split(
    GameState *state, int payer,
    GameLedgerParty first_payee, int first_amount,
    GameLedgerParty second_payee, int second_amount,
    unsigned int flags, GameEventQueue *events);
int rich4_ledger_sell_asset(GameState *state, const GameCommand *command,
                            GameEventQueue *events);
int rich4_ledger_apply_market_command(
    GameState *state, const GameCommand *command, GameEventQueue *events);

#endif
