#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/auction.h"

static void setup(GameState *state, int players)
{
    int i;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->player_count = players;
    state->price_index = 1;
    state->system_cash = 100;
    for (i = 0; i < players; i++) {
        state->players[i].play_state = 1;
        state->players[i].cash = 20000;
        state->players[i].bank = 100;
    }
    state->land_count = 1;
    state->land_owner[0] = -1;
    state->land_group[0] = 0;
    state->land_level[0] = 0;
    state->land_price[0] = 1000;
    state->land_price_per_level[0] = 500;
    state->land_value[0] = 1000;
}

static GameDomainRef land(void)
{
    return rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
}

static void reserve_and_eligibility(void)
{
    GameState state;
    GameAuctionState auction;
    Rich4Rng rng;
    int reserve;
    setup(&state, 4);
    state.price_index = 2;
    state.land_level[0] = 3;
    state.land_owner[0] = 1;
    state.players[0].cash = 5000;
    state.players[1].cash = 5001;
    state.players[3].days_sleeping = 1;
    rich4_rng_state_seed(&rng, 1);
    assert(rich4_auction_reserve_price(&state, land(), &reserve));
    assert(reserve == 5000);
    assert(rich4_auction_begin(&auction, &state, 2, land(), 1, &rng));
    assert(auction.eligible_count == 1 && auction.current_player == 1);
    assert(auction.participant[0] == GAME_AUCTION_PARTICIPANT_EXCLUDED);
    assert(auction.participant[1] == GAME_AUCTION_PARTICIPANT_READY);
    assert(auction.participant[2] == GAME_AUCTION_PARTICIPANT_EXCLUDED);
    assert(auction.participant[3] == GAME_AUCTION_PARTICIPANT_EXCLUDED);
}

static void passes_reset_after_bid(void)
{
    GameState state;
    GameAuctionState auction;
    Rich4Rng rng;
    setup(&state, 3);
    rich4_rng_state_seed(&rng, 2);
    assert(rich4_auction_begin(&auction, &state, -1, land(), 1, &rng));
    assert(rich4_auction_apply_action(
               &auction, &state, 0, GAME_AUCTION_BID_100) ==
           GAME_AUCTION_STEP_NEXT);
    assert(auction.current_player == 1 && auction.current_price == 1100);
    assert(rich4_auction_apply_action(
               &auction, &state, 1, GAME_AUCTION_PASS) ==
           GAME_AUCTION_STEP_NEXT);
    assert(auction.participant[1] == GAME_AUCTION_PARTICIPANT_PASSED);
    assert(rich4_auction_apply_action(
               &auction, &state, 2, GAME_AUCTION_BID_500) ==
           GAME_AUCTION_STEP_NEXT);
    assert(auction.leader == 2 && auction.current_price == 1600);
    assert(auction.participant[1] == GAME_AUCTION_PARTICIPANT_READY);
    assert(rich4_auction_apply_action(
               &auction, &state, 0, GAME_AUCTION_PASS) ==
           GAME_AUCTION_STEP_NEXT);
    assert(rich4_auction_apply_action(
               &auction, &state, 1, GAME_AUCTION_PASS) ==
           GAME_AUCTION_STEP_SOLD);
    assert(auction.phase == GAME_AUCTION_SOLD && auction.leader == 2);
}

static void unsold_and_single_bidder(void)
{
    GameState state;
    GameAuctionState auction;
    Rich4Rng rng;
    setup(&state, 2);
    rich4_rng_state_seed(&rng, 3);
    assert(rich4_auction_begin(&auction, &state, -1, land(), 1, &rng));
    assert(rich4_auction_apply_action(
               &auction, &state, 0, GAME_AUCTION_PASS) ==
           GAME_AUCTION_STEP_NEXT);
    assert(rich4_auction_apply_action(
               &auction, &state, 1, GAME_AUCTION_PASS) ==
           GAME_AUCTION_STEP_UNSOLD);

    setup(&state, 2);
    rich4_rng_state_seed(&rng, 4);
    assert(rich4_auction_begin(&auction, &state, 0, land(), 1, &rng));
    assert(auction.eligible_count == 1 && auction.current_player == 1);
    assert(rich4_auction_apply_action(
               &auction, &state, 1, GAME_AUCTION_BID_100) ==
           GAME_AUCTION_STEP_SOLD);
    assert(auction.current_price == auction.reserve_price + 100);
}

static void ai_policy(void)
{
    GameState state;
    GameAuctionState auction;
    Rich4Rng rng;
    setup(&state, 2);
    state.players[0].is_ai = 1;
    state.players[0].cash = 10000;
    state.land_owner[0] = 1;
    rich4_rng_state_seed(&rng, 5);
    assert(rich4_auction_begin(&auction, &state, 1, land(), 1, &rng));
    assert(auction.ai_ceiling[0] >= 3000 && auction.ai_ceiling[0] < 3500);
    /* 0x43B220 reduces every non-pass AI choice to +100 when the auction
     * started with exactly one eligible participant. */
    assert(rich4_auction_ai_action(&auction, &state, 0) ==
           GAME_AUCTION_BID_100);
    assert(rich4_auction_apply_action(
               &auction, &state, 0,
               rich4_auction_ai_action(&auction, &state, 0)) ==
           GAME_AUCTION_STEP_SOLD);
}

static void settlement_uses_seller(void)
{
    GameState state;
    GameAuctionState auction;
    GameEventQueue events;
    Rich4Rng rng;
    setup(&state, 3);
    state.land_owner[0] = 2;
    state.land_value[0] = 2000;
    state.fixed_asset_value[2] = 2000;
    state.players[0].cash = 0;
    state.players[0].bank = 50;
    rich4_rng_state_seed(&rng, 6);
    assert(rich4_auction_begin(&auction, &state, 0, land(), 1, &rng));
    assert(rich4_auction_apply_action(
               &auction, &state, 1, GAME_AUCTION_BID_100) ==
           GAME_AUCTION_STEP_NEXT);
    assert(rich4_auction_apply_action(
               &auction, &state, 2, GAME_AUCTION_PASS) ==
           GAME_AUCTION_STEP_SOLD);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_settle(&auction, &state, 0, &events) ==
           GAME_AUCTION_SETTLEMENT_SOLD);
    assert(state.land_owner[0] == 1);
    assert(state.players[1].cash == 18900);
    assert(state.players[0].bank == 1150);
    assert(state.players[2].bank == 100);
    assert(state.fixed_asset_value[1] == 2000);
    assert(state.fixed_asset_value[2] == 0);
    assert(events.count == 2);
}

static void owner_can_buy_back_and_system_can_sell(void)
{
    GameState state;
    GameAuctionState auction;
    GameEventQueue events;
    Rich4Rng rng;
    setup(&state, 2);
    state.land_owner[0] = 1;
    state.fixed_asset_value[1] = 1000;
    rich4_rng_state_seed(&rng, 7);
    assert(rich4_auction_begin(&auction, &state, 0, land(), 1, &rng));
    assert(auction.participant[1] == GAME_AUCTION_PARTICIPANT_READY);
    assert(rich4_auction_apply_action(
               &auction, &state, 1, GAME_AUCTION_BID_100) ==
           GAME_AUCTION_STEP_SOLD);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_settle(&auction, &state, 0, &events) ==
           GAME_AUCTION_SETTLEMENT_SOLD);
    assert(state.land_owner[0] == 1 && state.fixed_asset_value[1] == 1000);
    assert(state.players[0].bank == 1200);

    setup(&state, 1);
    rich4_rng_state_seed(&rng, 8);
    assert(rich4_auction_begin(&auction, &state, -1, land(), 1, &rng));
    assert(rich4_auction_apply_action(
               &auction, &state, 0, GAME_AUCTION_BID_100) ==
           GAME_AUCTION_STEP_SOLD);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_settle(&auction, &state, 0, &events) ==
           GAME_AUCTION_SETTLEMENT_SOLD);
    assert(state.system_cash == 1200 && state.land_owner[0] == 0);
}

static void card_unsold_policy_clears_owner(void)
{
    GameState state;
    GameAuctionState auction;
    GameEventQueue events;
    Rich4Rng rng;
    setup(&state, 1);
    state.land_owner[0] = 0;
    state.fixed_asset_value[0] = 1000;
    rich4_rng_state_seed(&rng, 9);
    assert(rich4_auction_begin(&auction, &state, 0, land(), 1, &rng));
    assert(auction.phase == GAME_AUCTION_UNSOLD);
    rich4_game_event_queue_reset(&events);
    assert(rich4_auction_settle(
               &auction, &state, GAME_AUCTION_CLEAR_OWNER_IF_UNSOLD,
               &events) == GAME_AUCTION_SETTLEMENT_UNSOLD);
    assert(state.land_owner[0] == -1);
    assert(state.fixed_asset_value[0] == 0);
    assert(events.count == 1 &&
           events.events[0].type == GAME_EVENT_PROPERTY_OWNER_CHANGED);
}

int main(void)
{
    reserve_and_eligibility();
    passes_reset_after_bid();
    unsold_and_single_bidder();
    ai_policy();
    settlement_uses_seller();
    owner_can_buy_back_and_system_can_sell();
    card_unsold_policy_clears_owner();
    puts("auction core tests: ok");
    return 0;
}
