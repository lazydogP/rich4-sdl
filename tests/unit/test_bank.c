#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/bank.h"

int main(void)
{
    GameState state;
    Rich4BankSettlement settlement;
    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 2;
    state.players[0].cash = 100;
    state.players[0].bank = 200;
    state.players[1].bank = 1000;
    state.land_count = 1;
    state.land_owner[0] = 0;
    state.land_value[0] = 400;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_value[0] = 300;
    state.player_stock_amount[0][0] = 2;
    state.stocks[0].f12 = 999.0f;
    state.stocks[0].f20 = 50.0f;
    assert(rich4_bank_total_assets(&state, 0) == 1100);
    state.stocks[0].f20 = 50.75f;
    assert(rich4_bank_total_assets(&state, 0) == 1101);
    state.stocks[0].f20 = 50.0f;
    assert(rich4_bank_withdraw(&state, 0, 50));
    assert(state.players[0].cash == 150 && state.players[0].bank == 150);
    assert(rich4_bank_deposit(&state, 0, 50));
    assert(state.players[0].cash == 100 && state.players[0].bank == 200);
    assert(rich4_bank_loan_available(&state, 0) == 1100);
    assert(rich4_bank_apply_loan(&state, 0, 500));
    assert(state.players[0].bank == 700 && state.players[0].loan == 500);
    assert(state.players[0].loan_due_date == 90);
    assert(rich4_bank_loan_days_remaining(&state, 0) == 90);
    assert(rich4_bank_repay_loan(&state, 0, 500));
    assert(state.players[0].bank == 200 && state.players[0].loan == 0 &&
           state.players[0].loan_due_date == 0);
    assert(rich4_bank_special_available(&state, 0) == 1000);
    assert(rich4_bank_special_borrow(&state, 0, 600));
    assert(state.players[0].bank == 800 && state.players[0].special_finance == 600);
    assert(rich4_bank_special_repay(&state, 0, 600));
    assert(state.players[0].bank == 200 && state.players[0].special_finance == 0);
    state.players[0].days_bank_no_loans = 1;
    assert(!rich4_bank_apply_loan(&state, 0, 1));
    assert(!rich4_bank_special_borrow(&state, 0, 1));

    memset(&state, 0, sizeof(state));
    state.player_count = 1;
    state.stage = 0;
    state.map = 0;
    state.calendar_start_year = 2000;
    state.calendar_start_month = 1;
    state.calendar_start_day = 3;
    state.players[0].cash = 100;
    assert(rich4_bank_apply_loan(&state, 0, 10));
    assert(state.players[0].loan_due_date == 91); /* day 90 is Sunday */

    memset(&state, 0, sizeof(state));
    state.player_count = 1;
    state.stage = 0;
    state.map = 0;
    state.calendar_start_year = 2000;
    state.calendar_start_month = 1;
    state.calendar_start_day = 1;
    state.stock_market_closed = 1;
    state.players[0].cash = 100;
    assert(rich4_bank_apply_loan(&state, 0, 10));
    assert(state.players[0].loan_due_date == 90);

    memset(&state, 0, sizeof(state));
    state.player_count = 4;
    state.players[0].play_state = 1;
    state.players[0].bank = 101;
    state.players[1].play_state = 1;
    state.players[1].bank = 200;
    state.players[1].loan = 1;
    state.players[2].bank = 300; /* inactive: not listed or credited */
    state.players[3].play_state = 1;
    state.players[3].bank = 1000;
    state.players[3].special_finance = 500;
    assert(rich4_bank_monthly_settle(&state, &settlement));
    assert(settlement.active_mask == ((1u << 0) | (1u << 1) | (1u << 3)));
    assert(settlement.loan_mask == (1u << 1));
    assert(state.players[0].bank == 111 && settlement.interest[0] == 10);
    assert(state.players[1].bank == 200 && settlement.interest[1] == 0);
    assert(state.players[2].bank == 300);
    assert(state.players[3].bank == 1100 && settlement.interest[3] == 100);

    memset(&state, 0, sizeof(state));
    state.player_count = 1;
    state.elapsed_days = 10;
    state.players[0].bank = 1000;
    state.players[0].loan = 500;
    state.players[0].loan_due_date = 16;
    assert(rich4_bank_run_ai(&state, 0));
    assert(state.players[0].bank == 500 && state.players[0].loan == 0 &&
           state.players[0].loan_due_date == 16);

    memset(&state, 0, sizeof(state));
    state.player_count = 1;
    state.calendar_start_year = 2026;
    state.calendar_start_month = 8;
    state.calendar_start_day = 15;
    state.players[0].cash = 800;
    state.players[0].bank = 200;
    state.players[0].ai_cash_ratio = 50;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 500 && state.players[0].bank == 500);
    /* 0x457DBC truncates the product; .5 ties do not round upward. */
    state.players[0].cash = 0;
    state.players[0].bank = 1003;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 501 && state.players[0].bank == 502);

    /* 0x437B60 only replaces targets outside (0,1), after date scaling. */
    state.calendar_start_day = 26;
    state.players[0].ai_cash_ratio = 10;
    state.players[0].cash = 0;
    state.players[0].bank = 1000;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 50 && state.players[0].bank == 950);
    state.calendar_start_day = 15;
    state.players[0].ai_cash_ratio = 95;
    state.players[0].cash = 0;
    state.players[0].bank = 1000;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 949 && state.players[0].bank == 51);
    state.players[0].ai_cash_ratio = 100;
    state.players[0].cash = 0;
    state.players[0].bank = 1000;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 899 && state.players[0].bank == 101);

    state.players[0].days_rejected_by_bank = 1;
    state.players[0].cash = 0;
    state.players[0].bank = 1000;
    state.players[0].loan = 100;
    state.players[0].loan_due_date = 1;
    state.ai_rng_state = 42;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(rich4_bank_run_ai(&state, 0));
    assert(state.players[0].cash == 0 && state.players[0].bank == 1000 &&
           state.players[0].loan == 100 && state.ai_rng_state == 42);
    state.players[0].days_rejected_by_bank = 0;
    state.players[0].ai_cash_ratio = 50;

    /* A difference inside the original +/-25% band does not trigger a
     * transfer.  The exact boundary is inclusive and is covered below. */
    state.players[0].cash = 600;
    state.players[0].bank = 400;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 600 && state.players[0].bank == 400);
    state.players[0].cash = 750;
    state.players[0].bank = 250;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 500 && state.players[0].bank == 500);

    /* Days 1..7 multiply the target by 1.5. */
    state.calendar_start_day = 1;
    state.players[0].cash = 300;
    state.players[0].bank = 700;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 750 && state.players[0].bank == 250);

    /* Days 26..31 multiply it by 0.5. */
    state.calendar_start_day = 26;
    state.players[0].cash = 700;
    state.players[0].bank = 300;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 250 && state.players[0].bank == 750);

    /* Zero cash bypasses the ratio-difference gate. */
    state.calendar_start_day = 15;
    state.players[0].cash = 0;
    state.players[0].bank = 1000;
    assert(rich4_bank_run_ai_transfer(&state, 0));
    assert(state.players[0].cash == 500 && state.players[0].bank == 500);
    puts("bank loan/repayment/special finance: ok");
    return 0;
}
