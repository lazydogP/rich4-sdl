#include "core/auction.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "core/game_event_payload.h"
#include "core/ledger.h"

/* Original entry 0x43BDE5 and modal callback 0x43A2DD. */
static int property_facts(
    const GameState *state, GameDomainRef target, int *owner, int *level,
    int *base_price, int *asset_value)
{
    int64_t value;
    if (state == NULL || owner == NULL || level == NULL ||
        base_price == NULL || asset_value == NULL || state->price_index <= 0)
        return 0;
    if (state->land_count < 0 ||
        state->land_count > GAME_MAX_MAP_PROPERTIES ||
        state->facility_count < 0 ||
        state->facility_count > GAME_MAX_MAP_PROPERTIES)
        return 0;
    if (target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
        target.id < state->land_count) {
        *owner = state->land_owner[target.id];
        *level = state->land_level[target.id];
        *base_price = state->land_price[target.id];
        value = state->land_value[target.id];
        if (value <= 0)
            value = (int64_t)*base_price +
                (int64_t)*level * state->land_price_per_level[target.id];
    } else if (target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
               target.id < state->facility_count) {
        *owner = state->facility_owner[target.id];
        *level = state->facility_level[target.id];
        *base_price = state->facility_price[target.id];
        value = state->facility_value[target.id];
        if (value <= 0)
            value = (int64_t)*base_price +
                (int64_t)*level * state->facility_price_per_level[target.id];
    } else {
        return 0;
    }
    if (*level < 0 || *base_price < 0 || value < 0 || value > INT_MAX)
        return 0;
    *asset_value = (int)value;
    return 1;
}

int rich4_auction_reserve_price(
    const GameState *state, GameDomainRef target, int *out_price)
{
    int owner, level, base_price, asset_value;
    int64_t scaled;
    if (out_price == NULL || !property_facts(
            state, target, &owner, &level, &base_price, &asset_value))
        return 0;
    (void)owner;
    (void)asset_value;
    scaled = (int64_t)state->price_index * base_price * (level + 2);
    scaled /= 2;
    if (scaled < 0 || scaled > INT_MAX) return 0;
    *out_price = (int)scaled;
    return 1;
}

static int player_is_eligible(
    const GameState *state, int player, int seller, int reserve)
{
    const GamePlayerState *candidate;
    if (player < 0 || player >= state->player_count || player == seller)
        return 0;
    candidate = &state->players[player];
    return (candidate->play_state & 0xff) != 0 &&
           candidate->cash > reserve &&
           candidate->delayed_status_block == 0 &&
           candidate->days_disappearing == 0 &&
           candidate->days_in_prison == 0 &&
           candidate->days_in_hospital == 0 &&
           candidate->days_sleeping == 0 &&
           candidate->days_sleep_walking == 0;
}

static int count_unowned(const GameState *state, GameDomainRef target)
{
    int count = 0;
    int i;
    if (target.kind == GAME_DOMAIN_LAND) {
        for (i = 0; i < state->land_count; i++)
            if (state->land_owner[i] < 0) count++;
    } else {
        for (i = 0; i < state->facility_count; i++)
            if (state->facility_owner[i] < 0) count++;
    }
    return count;
}

/* Original AI ceiling 0x439F0D.  land_group is populated from the original
 * fixed-width land name, so it preserves the strcmp grouping used there. */
static int ai_ceiling(
    const GameState *state, GameDomainRef target, int current_price,
    int player, Rich4Rng *rng)
{
    int owner, level, base_price, asset_value;
    int total, unowned, same_group = 0, factor, first_roll, second_roll, i;
    double scarcity, random_factor, estimate, cap;
    if (rng == NULL || player < 0 || player >= state->player_count ||
        !property_facts(state, target, &owner, &level, &base_price,
                        &asset_value))
        return 0;
    (void)owner;
    (void)asset_value;
    total = target.kind == GAME_DOMAIN_LAND ?
        state->land_count : state->facility_count;
    if (total <= 0) return 0;
    if (target.kind == GAME_DOMAIN_LAND) {
        int group = state->land_group[target.id];
        for (i = 0; i < state->land_count; i++)
            if (state->land_owner[i] == player &&
                state->land_group[i] == group)
                same_group++;
    }
    unowned = count_unowned(state, target);
    first_roll = rich4_rng_original_state_next(rng);
    random_factor = (double)first_roll / 32767.0 * 0.3 + 0.5;
    scarcity = 6.0 - (double)unowned / (double)total * 4.0;
    factor = same_group + level / 2 + 1;
    estimate = (double)state->price_index * current_price * factor *
        scarcity * random_factor;
    second_roll = rich4_rng_original_state_next(rng);
    cap = (double)state->price_index * base_price *
        ((double)second_roll / 65536.0 + 3.0);
    if (estimate > cap) estimate = cap;
    if (estimate > state->players[player].cash)
        estimate = state->players[player].cash;
    if (estimate <= 0.0) return 0;
    if (estimate > INT_MAX) return INT_MAX;
    return (int)estimate;
}

int rich4_auction_begin(
    GameAuctionState *auction, const GameState *state, int seller,
    GameDomainRef target, int manage_music, Rich4Rng *rng)
{
    int reserve;
    int player;
    if (auction == NULL || state == NULL || state->player_count <= 0 ||
        state->player_count > GAME_MAX_PLAYERS ||
        (seller < -1 || seller >= state->player_count) ||
        (manage_music != 0 && manage_music != 1) ||
        !rich4_auction_reserve_price(state, target, &reserve))
        return 0;
    memset(auction, 0, sizeof(*auction));
    auction->active = 1;
    auction->phase = GAME_AUCTION_BIDDING;
    auction->seller = seller;
    auction->target = target;
    auction->manage_music = manage_music;
    auction->reserve_price = reserve;
    auction->current_price = reserve;
    auction->current_player = -1;
    auction->leader = -1;
    auction->last_player = -1;
    for (player = 0; player < state->player_count; player++) {
        if (!player_is_eligible(state, player, seller, reserve)) continue;
        auction->participant[player] = GAME_AUCTION_PARTICIPANT_READY;
        auction->eligible_count++;
        if (auction->current_player < 0) auction->current_player = player;
        if (state->players[player].is_ai)
            auction->ai_ceiling[player] = ai_ceiling(
                state, target, reserve, player, rng);
    }
    if (auction->eligible_count == 0) {
        auction->phase = GAME_AUCTION_UNSOLD;
        auction->current_player = -1;
    }
    return 1;
}

int rich4_auction_action_amount(GameAuctionAction action)
{
    static const int amounts[] = {0, 100, 500, 1000, 5000, 10000, 0};
    if (action < GAME_AUCTION_PASS || action > GAME_AUCTION_GIVE_UP)
        return -1;
    return amounts[action];
}

static GameAuctionAction action_for_difference(int64_t difference)
{
    if (difference >= 10000) return GAME_AUCTION_BID_10000;
    if (difference >= 5000) return GAME_AUCTION_BID_5000;
    if (difference >= 1000) return GAME_AUCTION_BID_1000;
    if (difference >= 500) return GAME_AUCTION_BID_500;
    if (difference >= 100) return GAME_AUCTION_BID_100;
    return GAME_AUCTION_PASS;
}

GameAuctionAction rich4_auction_ai_action(
    const GameAuctionState *auction, const GameState *state, int player)
{
    GameAuctionAction action;
    int64_t threshold;
    if (auction == NULL || state == NULL || !auction->active ||
        auction->phase != GAME_AUCTION_BIDDING ||
        player != auction->current_player || player < 0 ||
        player >= state->player_count || !state->players[player].is_ai)
        return GAME_AUCTION_PASS;
    if (auction->current_price > state->players[player].cash)
        action = GAME_AUCTION_GIVE_UP;
    else
        action = action_for_difference(
            (int64_t)auction->ai_ceiling[player] - auction->current_price);
    if (auction->leader >= 0 && action >= GAME_AUCTION_BID_100 &&
        action <= GAME_AUCTION_BID_10000) {
        threshold = (int64_t)state->players[auction->leader].cash + 500 -
            auction->current_price;
        if (rich4_auction_action_amount(action) > threshold &&
            threshold <= 5000)
            action = action_for_difference(threshold);
    }
    if (auction->eligible_count == 1 && action != GAME_AUCTION_PASS)
        action = GAME_AUCTION_BID_100;
    return action;
}

static GameAuctionStepResult choose_next(GameAuctionState *auction, int after)
{
    int present = 0;
    int passed = 0;
    int offset;
    int player;
    for (player = 0; player < GAME_MAX_PLAYERS; player++) {
        if (auction->participant[player] == GAME_AUCTION_PARTICIPANT_READY ||
            auction->participant[player] == GAME_AUCTION_PARTICIPANT_PASSED)
            present++;
        if (auction->participant[player] == GAME_AUCTION_PARTICIPANT_PASSED)
            passed++;
    }
    if (present == 0 || present == passed) {
        auction->phase = GAME_AUCTION_UNSOLD;
        auction->current_player = -1;
        return GAME_AUCTION_STEP_UNSOLD;
    }
    if (auction->leader >= 0 && (present == 1 || present - passed == 1)) {
        auction->phase = GAME_AUCTION_SOLD;
        auction->current_player = -1;
        return GAME_AUCTION_STEP_SOLD;
    }
    for (offset = 1; offset <= GAME_MAX_PLAYERS; offset++) {
        player = (after + offset) % GAME_MAX_PLAYERS;
        if (auction->participant[player] == GAME_AUCTION_PARTICIPANT_READY &&
            player != auction->leader) {
            auction->current_player = player;
            return GAME_AUCTION_STEP_NEXT;
        }
    }
    return GAME_AUCTION_STEP_INVALID;
}

GameAuctionStepResult rich4_auction_apply_action(
    GameAuctionState *auction, const GameState *state, int player,
    GameAuctionAction action)
{
    int amount;
    int i;
    if (auction == NULL || state == NULL || !auction->active ||
        auction->phase != GAME_AUCTION_BIDDING ||
        player != auction->current_player || player < 0 ||
        player >= state->player_count ||
        auction->participant[player] != GAME_AUCTION_PARTICIPANT_READY)
        return GAME_AUCTION_STEP_INVALID;
    amount = rich4_auction_action_amount(action);
    if (amount < 0) return GAME_AUCTION_STEP_INVALID;
    if (action == GAME_AUCTION_PASS) {
        auction->participant[player] = GAME_AUCTION_PARTICIPANT_PASSED;
    } else if (action == GAME_AUCTION_GIVE_UP) {
        auction->participant[player] = GAME_AUCTION_PARTICIPANT_WITHDRAWN;
    } else {
        if ((int64_t)auction->current_price + amount >
            state->players[player].cash)
            return GAME_AUCTION_STEP_INVALID;
        auction->current_price += amount;
        auction->leader = player;
        for (i = 0; i < GAME_MAX_PLAYERS; i++)
            if (auction->participant[i] == GAME_AUCTION_PARTICIPANT_PASSED)
                auction->participant[i] = GAME_AUCTION_PARTICIPANT_READY;
    }
    auction->last_player = player;
    auction->last_action = action;
    auction->last_bid = amount;
    return choose_next(auction, player);
}

static int update_fixed_assets(
    GameState *state, int previous_owner, int buyer, int value)
{
    int64_t previous_value;
    int64_t buyer_value;
    if (previous_owner == buyer) return 1;
    previous_value = previous_owner >= 0 &&
        previous_owner < state->player_count ?
        (int64_t)state->fixed_asset_value[previous_owner] - value : 0;
    buyer_value = buyer >= 0 && buyer < state->player_count ?
        (int64_t)state->fixed_asset_value[buyer] + value : 0;
    if (previous_value < 0 || previous_value > INT_MAX ||
        buyer_value < 0 || buyer_value > INT_MAX)
        return 0;
    if (previous_owner >= 0 && previous_owner < state->player_count)
        state->fixed_asset_value[previous_owner] = (int)previous_value;
    if (buyer >= 0 && buyer < state->player_count)
        state->fixed_asset_value[buyer] = (int)buyer_value;
    return 1;
}

GameAuctionSettlementResult rich4_auction_settle(
    GameAuctionState *auction, GameState *state, unsigned int caller_flags,
    GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GamePaymentResult payment;
    GameLedgerParty payee;
    int owner, level, base_price, asset_value;
    int buyer;
    if (auction == NULL || state == NULL || events == NULL ||
        !auction->active ||
        (auction->phase != GAME_AUCTION_SOLD &&
         auction->phase != GAME_AUCTION_UNSOLD))
        return GAME_AUCTION_SETTLEMENT_REJECTED;
    if (auction->phase == GAME_AUCTION_UNSOLD) {
        if ((caller_flags & GAME_AUCTION_CLEAR_OWNER_IF_UNSOLD) != 0) {
            if (!property_facts(state, auction->target, &owner, &level,
                                &base_price, &asset_value))
                return GAME_AUCTION_SETTLEMENT_REJECTED;
            (void)level;
            (void)base_price;
            next = *state;
            rich4_game_event_queue_reset(&staged);
            if (!update_fixed_assets(&next, owner, -1, asset_value))
                return GAME_AUCTION_SETTLEMENT_REJECTED;
            if (auction->target.kind == GAME_DOMAIN_LAND)
                next.land_owner[auction->target.id] = -1;
            else
                next.facility_owner[auction->target.id] = -1;
            if (owner >= 0 &&
                (!rich4_game_event_emit_typed_domain(
                     &staged,
                     rich4_game_event_property_owner_changed_from_fields,
                     auction->seller, auction->target,
                     rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), -1, 0, 0) ||
                 events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                 !rich4_game_event_queue_append(events, &staged)))
                return GAME_AUCTION_SETTLEMENT_REJECTED;
            *state = next;
        }
        auction->phase = GAME_AUCTION_SETTLED;
        auction->active = 0;
        return GAME_AUCTION_SETTLEMENT_UNSOLD;
    }
    buyer = auction->leader;
    if (buyer < 0 || buyer >= state->player_count ||
        !property_facts(state, auction->target, &owner, &level, &base_price,
                        &asset_value))
        return GAME_AUCTION_SETTLEMENT_REJECTED;
    (void)level;
    (void)base_price;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!update_fixed_assets(&next, owner, buyer, asset_value))
        return GAME_AUCTION_SETTLEMENT_REJECTED;
    if (auction->target.kind == GAME_DOMAIN_LAND)
        next.land_owner[auction->target.id] = buyer;
    else
        next.facility_owner[auction->target.id] = buyer;
    payee = auction->seller >= 0 ?
        rich4_ledger_player(auction->seller) : rich4_ledger_system();
    payment = rich4_ledger_pay(
        &next, buyer, payee, auction->current_price, 0, &staged);
    if (payment == GAME_PAYMENT_REJECTED)
        return GAME_AUCTION_SETTLEMENT_REJECTED;
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_property_owner_changed_from_fields,
            auction->seller, auction->target,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, buyer), buyer,
            auction->current_price, 0) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_AUCTION_SETTLEMENT_REJECTED;
    *state = next;
    auction->phase = GAME_AUCTION_SETTLED;
    auction->active = 0;
    return payment == GAME_PAYMENT_BANKRUPTCY ?
        GAME_AUCTION_SETTLEMENT_BANKRUPTCY : GAME_AUCTION_SETTLEMENT_SOLD;
}
