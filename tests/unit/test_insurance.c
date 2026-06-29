#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/insurance.h"

static void setup(GameState *state)
{
    memset(state, 0, sizeof(*state));
    state->player_count = 2;
    state->price_index = 3;
    state->players[0].play_state = 1;
    state->players[1].play_state = 1;
    state->players[0].cash = 1000;
    state->players[0].days_assurance = 4;
    state->company_count = 3;
    state->company_type[0] = 1;
    state->company_type[1] = 4;
    state->company_type[2] = 4;
    state->company_stock_index[0] = 2;
    state->company_stock_index[1] = 5;
    state->company_stock_index[2] = 6;
    state->company_period_business[1] = 10000;
    state->company_total_business[1] = 20000;
    state->company_period_business[2] = 30000;
    state->company_total_business[2] = 40000;
    state->stock_company_owner[5] = 1;
    state->stock_company_value[5] = 7654321;
    state->stock_company_available[5] = 321;
    state->player_stock_amount[0][5] = 88;
}

int main(void)
{
    GameState state;
    GameEventQueue events;
    GameEvent event;
    GameInsuranceClaimResult claim;
    unsigned int owner_value;
    unsigned int available;
    int shares;

    setup(&state);
    owner_value = state.stock_company_value[5];
    available = state.stock_company_available[5];
    shares = state.player_stock_amount[0][5];
    rich4_game_event_queue_reset(&events);
    assert(rich4_insurance_claim_for_days(
        &state, 0, 2, GAME_INSURANCE_CLAIM_HOSPITAL, &claim, &events));
    assert(claim.applied && claim.company == 1 && claim.stock == 5 &&
           claim.amount == 12000 && claim.cause == GAME_INSURANCE_CLAIM_HOSPITAL);
    assert(state.players[0].cash == 13000 &&
           state.players[0].days_assurance == 4);
    assert(state.company_period_business[1] == (uint32_t)(10000u - 12000u) &&
           state.company_total_business[1] == 8000u);
    assert(state.company_period_business[2] == 30000u &&
           state.company_total_business[2] == 40000u);
    assert(state.stock_company_owner[5] == 1 &&
           state.stock_company_value[5] == owner_value &&
           state.stock_company_available[5] == available &&
           state.player_stock_amount[0][5] == shares);
    assert(events.count == 2);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED &&
           event.player == 0 &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_COMPANY &&
           rich4_game_event_subject_ref(&event).id == 1 &&
           rich4_game_event_value(&event) == 12000 &&
           rich4_game_event_value2(&event) == 2000 &&
           rich4_game_event_flags(&event) == GAME_INSURANCE_CLAIM_HOSPITAL);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_MONEY_TRANSFERRED &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_COMPANY &&
           rich4_game_event_target_ref(&event).kind == GAME_DOMAIN_PLAYER);

    rich4_game_event_queue_reset(&events);
    state.players[0].days_assurance = 0;
    assert(rich4_insurance_claim(
        &state, 0, 777, GAME_INSURANCE_CLAIM_FORTUNE, &claim, &events));
    assert(!claim.applied && events.count == 0 &&
           state.players[0].cash == 13000);

    assert(rich4_insurance_record_business(&state, 1, 500));
    assert(state.company_period_business[1] ==
           (uint32_t)(10000u - 12000u + 500u));
    assert(state.company_total_business[1] == 8500u);
    puts("insurance claims and company accounting: ok");
    return 0;
}
