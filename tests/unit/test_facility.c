#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "core/facility.h"

static void setup(GameState *state)
{
    memset(state, 0, sizeof(*state));
    state->player_count = 2;
    state->price_index = 2;
    state->initial_fund = 100000;
    state->players[0].play_state = 1;
    state->players[1].play_state = 1;
    state->players[0].cash = 100000;
    state->players[1].cash = 100000;
    state->players[0].linked_player = -1;
    state->players[1].linked_player = -1;
    state->facility_count = 2;
    state->facility_owner[0] = -1;
    state->facility_owner[1] = -1;
    state->facility_price[0] = 1000;
    state->facility_price_per_level[0] = 200;
    state->facility_price[1] = 700;
    state->facility_price_per_level[1] = 100;
}

static GameCommand command(GameCommandType type, int player, int facility,
                           int value)
{
    return rich4_game_domain_command(type, GAME_COMMAND_SOURCE_HUMAN, player,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, facility),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), value);
}

int main(void)
{
    GameState state, before;
    GameEventQueue events;
    GameFacilityAction action;
    GameCommand cmd;
    GameFacilityLandingResult landing;
    GameCompanyLandingResult company_landing;
    int value;
    setup(&state);
    assert(rich4_facility_purchase_price(&state, 0, &value) && value == 2000);
    assert(rich4_facility_asset_value(&state, 0, &value) && value == 1000);

    rich4_game_event_queue_reset(&events);
    cmd = command(GAME_COMMAND_PURCHASE_PROPERTY, 0, 0, 0);
    assert(rich4_facility_apply_command(&state, &cmd, &events));
    assert(state.facility_owner[0] == 0 && state.players[0].cash == 98000 &&
           state.facility_value[0] == 1000 && events.count == 3);

    assert(rich4_facility_inspect(&state, 0, 0, &action) &&
           action.kind == GAME_FACILITY_ACTION_BUILD && action.amount == 2000);
    rich4_game_event_queue_reset(&events);
    cmd = command(GAME_COMMAND_UPGRADE_PROPERTY, 0, 0, 2);
    assert(rich4_facility_apply_command(&state, &cmd, &events));
    assert(state.facility_type[0] == 2 && state.facility_level[0] == 1 &&
           state.facility_value[0] == 1200);

    cmd = command(GAME_COMMAND_UPGRADE_PROPERTY, 0, 0, 0);
    while (state.facility_level[0] < 5) {
        rich4_game_event_queue_reset(&events);
        assert(rich4_facility_apply_command(&state, &cmd, &events));
    }
    before = state;
    rich4_game_event_queue_reset(&events);
    assert(!rich4_facility_apply_command(&state, &cmd, &events));
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    state.facility_owner[1] = 0;
    state.facility_type[1] = 3;
    state.facility_level[1] = 1;
    assert(!rich4_facility_upgrade_price(&state, 1, &value));
    assert(rich4_facility_inspect(&state, 1, 1, &action) &&
           action.kind == GAME_FACILITY_ACTION_TRAFFIC_CHARGE);
    state.facility_type[1] = 4;
    assert(rich4_facility_inspect(&state, 1, 1, &action) &&
           action.kind == GAME_FACILITY_ACTION_NONE);
    state.facility_level[1] = 0;
    state.facility_type[1] = 0;
    assert(rich4_facility_inspect(&state, 1, 1, &action) &&
           action.kind == GAME_FACILITY_ACTION_NONE);

    setup(&state);
    state.facility_price[0] = INT_MAX;
    before = state;
    rich4_game_event_queue_reset(&events);
    cmd = command(GAME_COMMAND_PURCHASE_PROPERTY, 0, 0, 0);
    assert(!rich4_facility_apply_command(&state, &cmd, &events));
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    setup(&state);
    state.players[0].god_info = 12;
    state.players[0].god_type = 12;
    before = state;
    rich4_game_event_queue_reset(&events);
    cmd = command(GAME_COMMAND_PURCHASE_PROPERTY, 0, 0, 0);
    assert(rich4_facility_inspect(&state, 0, 0, &action) &&
           action.kind == GAME_FACILITY_ACTION_NONE);
    assert(!rich4_facility_apply_command(&state, &cmd, &events));
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    setup(&state);
    state.facility_owner[0] = 0;
    state.facility_type[0] = 1;
    state.facility_level[0] = 2;
    state.facility_charge_by_level[0][2] = 100;
    state.players[1].traffic_method = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&state, 1, 0, 3, 0, 1, -1,
                                          &landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(landing.kind == GAME_LANDING_CHARGED && landing.amount == 600 &&
           landing.payer == 1 && state.players[1].cash == 99400 &&
           state.players[0].cash == 100000 && state.players[0].bank == 600 &&
           state.players[1].delayed_status_block == 2 &&
           state.players[1].traffic_method == 2);
    state.facility_closed_status[0] = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&state, 1, 0, 2, 0, 0, -1,
                                          &landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(landing.kind == GAME_LANDING_NONE && events.count == 0);

    setup(&state);
    state.facility_owner[0] = 0;
    state.facility_type[0] = 3;
    state.facility_level[0] = 1;
    state.facility_charge_by_level[0][1] = 999;
    state.players[1].cash = 100000;
    state.players[1].traffic_method = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&state, 1, 0, 0, 4, 0, -1,
                                          &landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(landing.kind == GAME_LANDING_CHARGED && landing.amount == 4000 &&
           state.players[1].cash == 96000 && state.players[0].bank == 4000);
    state.players[1].traffic_method = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&state, 1, 0, 0, 4, 0, -1,
                                          &landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(landing.kind == GAME_LANDING_CHARGED && landing.amount == 8000 &&
           state.players[1].cash == 88000 && state.players[0].bank == 12000);
    state.players[1].traffic_method = 3;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&state, 1, 0, 0, 4, 0, -1,
                                          &landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(landing.kind == GAME_LANDING_CHARGED && landing.amount == 16000 &&
           state.players[1].cash == 72000 && state.players[0].bank == 28000);
    state.players[1].traffic_method = 0;
    before = state;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&state, 1, 0, 0, 4, 0, -1,
                                          &landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(landing.kind == GAME_LANDING_NONE && events.count == 0 &&
           memcmp(&state, &before, sizeof(state)) == 0);

    setup(&state);
    state.facility_owner[0] = 0;
    state.facility_type[0] = 4;
    state.facility_level[0] = 3;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_start_research(&state, 0, 0, 2, &events));
    assert(state.facility_research_selection[0] == 3 &&
           state.facility_research_turns[0] == 5);
    for (value = 0; value < 5; value++) {
        rich4_game_event_queue_reset(&events);
        assert(rich4_facility_tick_research(&state, 0, &events));
    }
    assert(state.facility_research_selection[0] == 0 &&
           state.players[0].tool_inventory[10] == 1);
    state.facility_level[0] = 5;
    assert(rich4_facility_inspect(&state, 0, 0, &action) &&
           action.kind == GAME_FACILITY_ACTION_RESEARCH);

    setup(&state);
    state.company_count = 1;
    state.company_stock_index[0] = 2;
    state.company_type[0] = 4;
    state.company_base_fee[0] = 100;
    state.stock_company_owner[2] = 0;
    state.stock_company_auxiliary[2] = 0;
    state.stocks[2].f4 = 1;
    state.stock_company_value[2] = 1000000;
    state.stock_company_available[2] = 500;
    rich4_game_event_queue_reset(&events);
    assert(rich4_company_resolve_landing(&state, 1, 0, 5, 0, 0, -1,
                                         &company_landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(company_landing.kind == GAME_LANDING_CHARGED &&
           company_landing.amount == 1000 && state.company_cash[2] == 1000 &&
           state.players[1].days_assurance == 5 &&
           state.company_period_business[0] == 1000u &&
           state.company_total_business[0] == 1000u);
    assert(rich4_company_max_issue_purchase(&state, 1, 0, &value,
                                             &company_landing.amount));
    assert(value == 100 && company_landing.amount == 500);
    rich4_game_event_queue_reset(&events);
    assert(rich4_company_buy_issued_stock(&state, 1, 0, 50, &events));
    assert(state.players[1].cash == 94000 &&
           state.player_stock_amount[1][2] == 50 &&
           state.stock_company_available[2] == 450 &&
           state.stock_company_owner[2] == 1);

    setup(&state);
    state.company_count = 1;
    state.company_stock_index[0] = 2;
    state.company_type[0] = 4;
    state.company_base_fee[0] = 100;
    state.stock_company_owner[2] = -1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_company_resolve_landing(
               &state, 1, 0, 0, 0, 0, -1, &company_landing, &events) ==
           GAME_PAYMENT_COMMITTED);
    assert(company_landing.kind == GAME_LANDING_NONE &&
           state.players[1].days_assurance == 0 && events.count == 0);

    setup(&state);
    state.land_count = 1;
    state.land_type[0] = 0;
    state.land_level[0] = 2;
    state.land_price[0] = 300;
    state.company_count = 1;
    state.company_stock_index[0] = 2;
    state.company_type[0] = 11;
    state.company_base_fee[0] = 100;
    state.stock_company_owner[2] = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_company_resolve_construction(
               &state, 1, 0, rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), 0,
               0, -1, &company_landing, &events) == GAME_PAYMENT_COMMITTED);
    assert(state.land_level[0] == 3 && company_landing.amount == 600 &&
           state.players[1].cash == 99400 && state.company_cash[2] == 600);
    /* 0x41ABDA/0x41AF96: travel fee ignores dice, and a free card
     * does not prevent a god substitute receiving the travel status. */
    for (value = 0; value < 2; value++) {
        setup(&state);
        state.company_count = 1;
        state.company_stock_index[0] = 2;
        state.company_type[0] = 1;
        state.company_base_fee[0] = 100;
        state.stock_company_owner[2] = 0;
        if (value) {
            state.players[1].card_inventory[0] = 20;
            state.players[0].god_info = state.players[0].god_type = 14;
        }
        rich4_game_event_queue_reset(&events);
        assert(rich4_company_resolve_landing(&state, 1, 0, 2, 7,
            value, -1, &company_landing, &events) == GAME_PAYMENT_COMMITTED);
        assert(company_landing.amount == (value ? 0 : 400));
        assert(company_landing.payer == (value ? 0 : 1));
        assert(state.players[value ? 0 : 1].days_disappearing == 2);
    }
    puts("facility rules: ok");
    return 0;
}
