#ifndef RICH4_CORE_INSURANCE_H
#define RICH4_CORE_INSURANCE_H

#include "core/game_event.h"
#include "core/game_state.h"

typedef enum GameInsuranceClaimCause {
    GAME_INSURANCE_CLAIM_NONE = 0,
    GAME_INSURANCE_CLAIM_DISAPPEARING,
    GAME_INSURANCE_CLAIM_HOTEL,
    GAME_INSURANCE_CLAIM_PRISON,
    GAME_INSURANCE_CLAIM_HOSPITAL,
    GAME_INSURANCE_CLAIM_FORTUNE
} GameInsuranceClaimCause;

/* Claim event metadata: low byte is the cause, bit 8 retains payment until
 * 0x440CAC returns, bits 16..23 retain the caller's +66 day continuation. */
#define GAME_INSURANCE_CLAIM_CAUSE_MASK 0xffu
#define GAME_INSURANCE_CLAIM_DEFERRED 0x100u
#define GAME_INSURANCE_CLAIM_DAYS_SHIFT 16

typedef struct GameInsuranceClaimResult {
    int applied;
    int player;
    int company;
    int stock;
    int amount;
    GameInsuranceClaimCause cause;
} GameInsuranceClaimResult;

/* Rich4.exe searches the auxiliary-company records from the beginning and
 * uses the first type-4 company for every claim. */
int rich4_insurance_find_company(const GameState *state);

/* Returns zero only for invalid input or event-capacity failure.  An
 * uninsured player is a successful no-op, reported through out->applied. */
int rich4_insurance_claim(GameState *state, int player, int amount,
                          GameInsuranceClaimCause cause,
                          GameInsuranceClaimResult *out,
                          GameEventQueue *events);

/* Confinement, hotel and travel claims use 2000 * days * price_index. */
int rich4_insurance_claim_for_days(GameState *state, int player, int days,
                                   GameInsuranceClaimCause cause,
                                   GameInsuranceClaimResult *out,
                                   GameEventQueue *events);

/* Runtime split of 0x44BA63: request emits only the 2000ms notice; settlement
 * credits cash and +0x60 income exactly once, without emitting another notice.
 * days is zero for monetary fortunes, otherwise the original +66 increment. */
int rich4_insurance_request_claim(GameState *state, int player, int amount,
    GameInsuranceClaimCause cause, int days, GameEventQueue *events);
int rich4_insurance_request_claim_for_days(GameState *state, int player, int days,
    GameInsuranceClaimCause cause, GameEventQueue *events);
int rich4_insurance_settle_claim(GameState *state, int player, int amount,
    GameInsuranceClaimCause cause, GameEventQueue *events);

/* Premiums are accumulated in auxiliary-company record +0x28/+0x2c.
 * This bookkeeping is separate from stock ownership and valuation. */
int rich4_insurance_record_business(GameState *state, int company, int amount);

#endif
