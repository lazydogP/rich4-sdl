#include "core/bank.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "core/rng.h"
#include "core/ledger.h"
#include "core/stock.h"
#include "core/game_event_payload.h"

static int clamp_money(int64_t value)
{
    if (value < 0) return 0;
    if (value > INT_MAX) return INT_MAX;
    return (int)value;
}

int rich4_bank_total_assets(const GameState *state, int player)
{
    int i;
    int64_t value;
    if (state == 0 || player < 0 || player >= state->player_count) return 0;
    value = (int64_t)state->players[player].cash +
            state->players[player].bank - state->players[player].loan;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        /* 0x423A0A..0x423A17: convert the accumulator to float, add
         * quantity times the unrounded quote, then __CHP truncates. */
        double subtotal = (double)(float)value +
            (double)state->player_stock_amount[player][i] *
            (double)state->stocks[i].f20;
        if (subtotal >= INT_MAX) value = INT_MAX;
        else if (subtotal <= INT_MIN) value = INT_MIN;
        else value = (int)subtotal;
    }
    for (i = 0; i < state->land_count; i++)
        if (state->land_owner[i] == player) value += state->land_value[i];
    for (i = 0; i < state->facility_count; i++)
        if (state->facility_owner[i] == player) value += state->facility_value[i];
    return clamp_money(value);
}

int rich4_bank_withdraw(GameState *state, int player, int amount)
{
    GamePlayerState *p;
    if (state == 0 || player < 0 || player >= state->player_count || amount < 0)
        return 0;
    p = &state->players[player];
    if (amount > p->bank || amount > INT_MAX - p->cash) return 0;
    p->bank -= amount;
    p->cash += amount;
    return 1;
}

int rich4_bank_deposit(GameState *state, int player, int amount)
{
    GamePlayerState *p;
    if (state == 0 || player < 0 || player >= state->player_count || amount < 0)
        return 0;
    p = &state->players[player];
    if (amount > p->cash || amount > INT_MAX - p->bank) return 0;
    p->cash -= amount;
    p->bank += amount;
    return 1;
}

static int bank_run_ai_transfer_impl(GameState *state, int player,
                                     int *out_rebalanced)
{
    GamePlayerState *p;
    int64_t total;
    int target_cash;
    int day;
    double cash_product;
    float current_ratio;
    float target_ratio;
    double difference;
    if (out_rebalanced != NULL) *out_rebalanced = 0;
    if (state == 0 || player < 0 || player >= state->player_count) return 0;
    p = &state->players[player];
    total = (int64_t)p->cash + p->bank;
    /* 0x4379DA rejects service before entering the AI ratio branch. */
    if (p->days_rejected_by_bank != 0) return 1;
    if (total <= 0) return 1;
    /* 0x437AFB/0x437B1C store both ratios as single precision. */
    current_ratio = (float)((double)p->cash / (double)total);
    target_ratio = (float)((double)p->ai_cash_ratio / 100.0);
    day = 0;
    (void)rich4_stock_calendar_date(state, NULL, NULL, &day, NULL);
    if (day >= 1 && day <= 7) {
        target_ratio = (float)((double)target_ratio * 1.5);
    } else if (day >= 26) {
        target_ratio = (float)((double)target_ratio * 0.5);
    }
    /* 0x437B60..0x437B7D only replaces <=0 and >=1 AFTER the date
     * multiplier.  Positive targets below 10% or above 90% survive. */
    if (target_ratio >= 1.0f) target_ratio = 0.9f;
    else if (target_ratio <= 0.0f) target_ratio = 0.1f;
    difference = (double)current_ratio - (double)target_ratio;
    /* 0x437B88 only rebalances outside the inclusive +/-25% band, except
     * that a zero cash balance always enters the transfer path. */
    if (p->cash != 0 && difference > -0.25 && difference < 0.25)
        return 1;
    if (out_rebalanced != NULL) *out_rebalanced = 1;
    /* 0x437BD8 -> __CHP (0x457DBC) truncates the float target product. */
    cash_product = (double)total * (double)target_ratio;
    target_cash = cash_product > INT_MAX ? INT_MAX : (int)cash_product;
    if (p->cash > target_cash)
        return rich4_bank_deposit(state, player, p->cash - target_cash);
    if (p->cash < target_cash)
        return rich4_bank_withdraw(state, player, target_cash - p->cash);
    return 1;
}

int rich4_bank_run_ai_transfer(GameState *state, int player)
{
    return bank_run_ai_transfer_impl(state, player, NULL);
}

int rich4_bank_run_ai_transfer_with_events(
    GameState *state, int player, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int rebalanced;

    if (state == NULL || events == NULL) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!bank_run_ai_transfer_impl(&next, player, &rebalanced)) return 0;
    if (rebalanced &&
        !rich4_bank_settle_special_finance(&next, 1, player, &staged))
        return 0;
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    return 1;
}

static int bank_special_finance_owner(const GameState *state)
{
    int owner = -1;
    int i;

    if (state == NULL) return -1;
    for (i = 0; i < state->company_count &&
                i < GAME_MAX_MAP_PROPERTIES; i++) {
        int candidate;
        if (state->company_type[i] != 7) continue;
        candidate = state->company_owner[i];
        if (candidate >= 0 && candidate < state->player_count)
            owner = candidate;
    }
    return owner;
}

int rich4_bank_settle_special_finance(
    GameState *state, int mode, int current_player, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int owner;
    int i;

    if (state == NULL || events == NULL || state->player_count < 0 ||
        state->player_count > GAME_MAX_PLAYERS || current_player < 0 ||
        current_player >= state->player_count || (mode != 0 && mode != 1))
        return 0;

    next = *state;
    rich4_game_event_queue_reset(&staged);
    owner = bank_special_finance_owner(&next);
    if (mode == 0) {
        for (i = 0; i < next.player_count; i++) {
            int amount;
            GamePaymentResult payment;
            if (i == owner || (next.players[i].play_state & 0xff) == 0)
                continue;
            amount = next.players[i].special_finance;
            if (amount <= 0) continue;
            payment = rich4_ledger_pay(
                &next, i, rich4_ledger_system(), amount,
                GAME_PAYMENT_PAYER_BANK_FIRST, &staged);
            if (payment == GAME_PAYMENT_REJECTED) return 0;
            next.players[i].special_finance = 0;
        }
    } else if (owner >= 0 && owner != current_player &&
               (next.players[owner].play_state & 0xff) != 0 &&
               next.players[owner].special_finance > 0) {
        int64_t other_banks = 0;
        int amount;
        GamePaymentResult payment;
        for (i = 0; i < next.player_count; i++) {
            if (i != owner && (next.players[i].play_state & 0xff) != 0)
                other_banks += next.players[i].bank;
        }
        if (other_banks < next.players[owner].special_finance) {
            amount = next.players[owner].special_finance -
                     (int)other_banks;
            /* 0x436C1F: the bank owner's uncovered special finance has
             * its own map notice, even when reached through an AI pass. */
            {
                GameEvent notice = rich4_game_event_special_place_notice(
                    owner, GAME_SPECIAL_NOTICE_BANK_OWNER_SHORTFALL,
                    rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), amount, 2500);
                if (!rich4_game_event_queue_push(&staged, &notice)) return 0;
            }
            payment = rich4_ledger_pay(
                &next, owner, rich4_ledger_system(), amount,
                GAME_PAYMENT_PAYER_BANK_FIRST, &staged);
            if (payment == GAME_PAYMENT_REJECTED) return 0;
            next.players[owner].special_finance -= amount;
            if (next.players[owner].special_finance < 0)
                next.players[owner].special_finance = 0;
        }
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    return 1;
}

int rich4_bank_loan_available(const GameState *state, int player)
{
    int64_t available;
    if (state == 0 || player < 0 || player >= state->player_count) return 0;
    available = (int64_t)rich4_bank_total_assets(state, player) -
                state->players[player].loan;
    return clamp_money(available);
}

int rich4_bank_apply_loan(GameState *state, int player, int amount)
{
    GamePlayerState *p;
    int due_date = 0;
    if (state == 0 || player < 0 || player >= state->player_count || amount < 0)
        return 0;
    p = &state->players[player];
    if (p->days_bank_no_loans != 0 || amount > rich4_bank_loan_available(state, player) ||
        amount > INT_MAX - p->bank || amount > INT_MAX - p->loan)
        return 0;
    if (amount > 0 && p->loan_due_date == 0) {
        GameState due_day;
        if (state->elapsed_days > INT_MAX - 90) return 0;
        due_date = state->elapsed_days + 90;
        due_day = *state;
        due_day.elapsed_days = due_date;
        while (rich4_stock_calendar_is_closed(&due_day)) {
            if (due_date == INT_MAX) return 0;
            due_day.elapsed_days = ++due_date;
        }
    }
    p->bank += amount;
    p->loan += amount;
    /* sub_433B7E installs and calendar-normalizes the deadline only for the
     * first non-zero loan.  Later borrowing never extends it. */
    if (due_date != 0) p->loan_due_date = due_date;
    return 1;
}

int rich4_bank_repay_loan(GameState *state, int player, int amount)
{
    GamePlayerState *p;
    int from_bank;
    if (state == 0 || player < 0 || player >= state->player_count || amount < 0)
        return 0;
    p = &state->players[player];
    if (amount > p->loan || (int64_t)amount > (int64_t)p->cash + p->bank)
        return 0;
    from_bank = p->bank < amount ? p->bank : amount;
    p->bank -= from_bank;
    p->cash -= amount - from_bank;
    p->loan -= amount;
    if (p->loan == 0) p->loan_due_date = 0;
    return 1;
}

int rich4_bank_special_available(const GameState *state, int player)
{
    int i;
    int64_t value = 0;
    if (state == 0 || player < 0 || player >= state->player_count) return 0;
    /* sub_43453A scans the four fixed player records, not the active count. */
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        if (i != player) value += state->players[i].bank;
    value -= state->players[player].special_finance;
    return clamp_money(value);
}

int rich4_bank_special_borrow(GameState *state, int player, int amount)
{
    GamePlayerState *p;
    if (state == 0 || player < 0 || player >= state->player_count || amount < 0)
        return 0;
    p = &state->players[player];
    /* sub_434492/0x434C60 cancels the special-finance button under the same
     * +60 bank-refusal status that masks the control in sub_434186. */
    if (p->days_bank_no_loans != 0 ||
        amount > rich4_bank_special_available(state, player)) return 0;
    if (amount > INT_MAX - p->bank || amount > INT_MAX - p->special_finance) return 0;
    p->bank += amount;
    p->special_finance += amount;
    return 1;
}

int rich4_bank_special_repay(GameState *state, int player, int amount)
{
    GamePlayerState *p;
    int from_bank;
    if (state == 0 || player < 0 || player >= state->player_count || amount < 0)
        return 0;
    p = &state->players[player];
    if (amount > p->special_finance ||
        (int64_t)amount > (int64_t)p->cash + p->bank) return 0;
    from_bank = p->bank < amount ? p->bank : amount;
    p->bank -= from_bank;
    p->cash -= amount - from_bank;
    p->special_finance -= amount;
    return 1;
}

int rich4_bank_run_ai(GameState *state, int player)
{
    GamePlayerState *p;
    Rich4Rng rng;
    int liquid;
    int amount;
    int due_soon;
    if (state == 0 || player < 0 || player >= state->player_count) return 0;
    p = &state->players[player];
    /* 0x43667B: bank refusal suppresses both borrowing and repayment. */
    if (p->days_rejected_by_bank != 0) return 1;
    liquid = clamp_money((int64_t)p->cash + p->bank);
    if (p->loan > 0) {
        due_soon = p->loan_due_date > 0 && p->loan_due_date - state->elapsed_days <= 6;
        if ((int64_t)p->bank > (int64_t)p->loan * 2 ||
            (due_soon && (double)p->loan * 1.1 <= (double)liquid)) {
            int amount = p->loan;
            int from_bank = p->bank < amount ? p->bank : amount;
            /* 0x43682D pays principal bank-first.  Unlike the human helper,
             * this original AI branch clears +36 but leaves the +44 deadline
             * untouched; preserve that observable quirk. */
            p->bank -= from_bank;
            p->cash -= amount - from_bank;
            p->loan = 0;
            return 1;
        }
        return 1;
    }
    rng.state = state->ai_rng_state;

    if (rich4_rng_original_state_range(&rng, 10) != 0 && liquid >= 30000) {
        state->ai_rng_state = rng.state;
        return 1;
    }
    state->ai_rng_state = rng.state;
    if (p->days_bank_no_loans != 0 || p->ai_trait <= 0) return 1;
    amount = (int)(((int64_t)rich4_bank_total_assets(state, player) * p->ai_trait) / 100);
    if (amount > rich4_bank_loan_available(state, player))
        amount = rich4_bank_loan_available(state, player);
    return amount == 0 || rich4_bank_apply_loan(state, player, amount);
}

int rich4_bank_monthly_settle(GameState *state,
                              Rich4BankSettlement *out_settlement)
{
    Rich4BankSettlement settlement;
    int i;
    if (state == NULL || out_settlement == NULL || state->player_count < 0 ||
        state->player_count > GAME_MAX_PLAYERS) return 0;
    memset(&settlement, 0, sizeof(settlement));
    for (i = 0; i < state->player_count; i++) {
        GamePlayerState *player = &state->players[i];
        double credited;
        int after;
        if ((player->play_state & 0xff) == 0) continue;
        settlement.active_mask |= 1u << i;
        settlement.bank_before[i] = player->bank;
        if (player->loan != 0) {
            settlement.loan_mask |= 1u << i;
            settlement.bank_after[i] = player->bank;
            continue;
        }
        /* 0x4381F5 multiplies the complete bank field by the qword 1.1,
         * then converts it through __CHP/fistp.  special_finance remains a
         * separate field and therefore does not disqualify this credit. */
        credited = (double)player->bank * 1.1;
        after = credited > (double)INT_MAX ? INT_MAX :
                credited < 0.0 ? 0 : (int)credited;
        player->bank = after;
        settlement.bank_after[i] = after;
        settlement.interest[i] = after - settlement.bank_before[i];
    }
    *out_settlement = settlement;
    return 1;
}

int rich4_bank_loan_days_remaining(const GameState *state, int player)
{
    int64_t remaining;
    if (state == NULL || player < 0 || player >= state->player_count ||
        state->players[player].loan_due_date == 0) return INT_MAX;
    remaining = (int64_t)state->players[player].loan_due_date -
                state->elapsed_days;
    if (remaining < INT_MIN) return INT_MIN;
    if (remaining > INT_MAX) return INT_MAX;
    return (int)remaining;
}
