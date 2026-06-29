#include "core/ledger.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "core/victory.h"
#include "core/stock.h"
#include "core/auction.h"
#include "core/card.h"
#include "core/tool.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/special_event.h"

static void queue_bankruptcy_auctions(
    GameState *state, int player, GameDomainRef *property,
    int property_count, GameEventQueue *events)
{
    Rich4Rng rng;
    int active = 0;
    int human = 0;
    int i;
    if (state == NULL || property == NULL || events == NULL ||
        property_count <= 3)
        return;
    for (i = 0; i < state->player_count; i++) {
        if ((state->players[i].play_state & 0xff) == 0) continue;
        active++;
        if (!state->players[i].is_ai) human++;
    }
    if (human == 0 || active == 1) return;
    rng.state = state->ai_rng_state;
    for (i = 0; i < 3; i++) {
        int selected;
        GameEvent request;
        do {
            selected = rich4_rng_state_range(&rng, property_count);
        } while (property[selected].kind == GAME_DOMAIN_NONE);
        request = rich4_game_event_auction_request(
            player, property[selected], -1, 1,
            GAME_AUCTION_TRIGGER_BANKRUPTCY, 0);
        (void)rich4_game_event_queue_push(events, &request);
        property[selected] =
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    }
    state->ai_rng_state = rng.state;
}

static GameDomainRef none_ref(void) { return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1); }
static int property_asset_value(const GameState *state, GameDomainRef ref,
                                int *out_value)
{
    int level;
    int base_price;
    int per_level;
    int64_t value;
    if (state == NULL || out_value == NULL ||
        state->land_count < 0 ||
        state->land_count > GAME_MAX_MAP_PROPERTIES ||
        state->facility_count < 0 ||
        state->facility_count > GAME_MAX_MAP_PROPERTIES)
        return 0;
    if (ref.kind == GAME_DOMAIN_LAND && ref.id >= 0 &&
        ref.id < state->land_count) {
        level = state->land_level[ref.id];
        base_price = state->land_price[ref.id];
        per_level = state->land_price_per_level[ref.id];
        value = state->land_value[ref.id];
    } else if (ref.kind == GAME_DOMAIN_FACILITY && ref.id >= 0 &&
               ref.id < state->facility_count) {
        level = state->facility_level[ref.id];
        base_price = state->facility_price[ref.id];
        per_level = state->facility_price_per_level[ref.id];
        value = state->facility_value[ref.id];
    } else {
        return 0;
    }
    if (level < 0 || base_price < 0 || per_level < 0) return 0;
    if (value <= 0)
        value = (int64_t)base_price + (int64_t)level * per_level;
    if (value < 0 || value > INT_MAX) return 0;
    *out_value = (int)value;
    return 1;
}

static int transfer_fixed_asset_value(
    GameState *state, int seller, int buyer, int value)
{
    int64_t seller_after;
    int64_t buyer_after;
    if (seller < 0 || seller >= state->player_count || value < 0)
        return 0;
    seller_after = (int64_t)state->fixed_asset_value[seller] - value;
    if (seller_after < 0 || seller_after > INT_MAX) return 0;
    if (buyer >= 0) {
        if (buyer >= state->player_count) return 0;
        buyer_after = (int64_t)state->fixed_asset_value[buyer] + value;
        if (buyer_after < 0 || buyer_after > INT_MAX) return 0;
        state->fixed_asset_value[buyer] = (int)buyer_after;
    }
    state->fixed_asset_value[seller] = (int)seller_after;
    return 1;
}

static int market_recompute_fixed_asset_values(GameState *state)
{
    int64_t totals[GAME_MAX_PLAYERS] = {0};
    int i;
    if (state == NULL || state->land_count < 0 ||
        state->land_count > GAME_MAX_MAP_PROPERTIES ||
        state->facility_count < 0 ||
        state->facility_count > GAME_MAX_MAP_PROPERTIES)
        return 0;
    for (i = 0; i < state->land_count; i++) {
        int owner = state->land_owner[i];
        int value;
        if (owner < 0 || owner >= state->player_count) continue;
        if (!property_asset_value(
                state, rich4_game_domain_ref(GAME_DOMAIN_LAND, i), &value))
            return 0;
        totals[owner] += value;
        if (totals[owner] > INT_MAX) return 0;
    }
    for (i = 0; i < state->facility_count; i++) {
        int owner = state->facility_owner[i];
        int value;
        if (owner < 0 || owner >= state->player_count) continue;
        if (!property_asset_value(
                state, rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i),
                &value))
            return 0;
        totals[owner] += value;
        if (totals[owner] > INT_MAX) return 0;
    }
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        state->fixed_asset_value[i] = (int)totals[i];
    return 1;
}

static GameDomainRef party_ref(GameLedgerParty party)
{
    if (party.kind == GAME_LEDGER_PLAYER)
        return rich4_game_domain_ref(GAME_DOMAIN_PLAYER, party.id);
    if (party.kind == GAME_LEDGER_COMPANY)
        return rich4_game_domain_ref(GAME_DOMAIN_STOCK, party.id);
    return none_ref();
}

GameLedgerParty rich4_ledger_system(void) { GameLedgerParty p = { GAME_LEDGER_SYSTEM, -1 }; return p; }
GameLedgerParty rich4_ledger_player(int id) { GameLedgerParty p = { GAME_LEDGER_PLAYER, id }; return p; }
GameLedgerParty rich4_ledger_company(int id) { GameLedgerParty p = { GAME_LEDGER_COMPANY, id }; return p; }

int rich4_ledger_adjust_fee(int amount, int god_type, int *out_amount)
{
    int64_t value;
    if (out_amount == NULL || amount < 0) return 0;
    value = amount;
    switch (god_type) {
    case 1: value /= 2; break;
    case 2: value = 0; break;
    case 3: case 4: break;
    case 5: value = value * 3 / 2; break;
    case 6: value *= 2; break;
    default: break;
    }
    if (value > INT_MAX) return 0;
    *out_amount = (int)value;
    return 1;
}

static int valid_party(const GameState *s, GameLedgerParty p)
{
    if (p.kind == GAME_LEDGER_SYSTEM) return p.id == -1;
    if (p.kind == GAME_LEDGER_COMPANY) return p.id >= 0 && p.id < GAME_STOCK_COUNT;
    return p.kind == GAME_LEDGER_PLAYER && p.id >= 0 && p.id < s->player_count &&
           (s->players[p.id].play_state & 0xff) != 0;
}

static int emit_transfer(GameEventQueue *q, int payer, GameLedgerParty payee,
                         int amount, unsigned int flags)
{
    return rich4_game_event_emit_typed_domain(
        q, rich4_game_event_money_transferred_from_fields, payer,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, payer), party_ref(payee),
        amount, payee.kind, flags);
}

static void credit(GameState *s, GameLedgerParty p, int amount, unsigned int flags)
{
    if (p.kind == GAME_LEDGER_SYSTEM) {
        s->system_cash += amount;
        /* 0x41D38C: the system recipient is the shared lottery pool. */
        s->lottery_pool = (int32_t)((uint32_t)s->lottery_pool + (uint32_t)amount);
    }
    else if (p.kind == GAME_LEDGER_COMPANY) s->company_cash[p.id] += amount;
    else if ((flags & GAME_PAYMENT_RECEIVER_CASH) != 0) s->players[p.id].cash += amount;
    else s->players[p.id].bank += amount;
    if (p.kind == GAME_LEDGER_PLAYER)
        s->players[p.id].monthly_income = (int32_t)(
            (uint32_t)s->players[p.id].monthly_income + (uint32_t)amount);
}

static int can_credit(const GameState *s, GameLedgerParty p, int amount,
                      unsigned int flags)
{
    int balance;
    if (p.kind == GAME_LEDGER_SYSTEM) balance = s->system_cash;
    else if (p.kind == GAME_LEDGER_COMPANY) balance = s->company_cash[p.id];
    else balance = (flags & GAME_PAYMENT_RECEIVER_CASH) != 0
        ? s->players[p.id].cash : s->players[p.id].bank;
    return amount >= 0 && balance >= 0 && balance <= INT_MAX - amount;
}

static int same_party(GameLedgerParty left, GameLedgerParty right)
{
    return left.kind == right.kind && left.id == right.id;
}

static void debit_liquid(GamePlayerState *p, int amount, unsigned int flags)
{
    int first;
    p->monthly_loss = (int32_t)((uint32_t)p->monthly_loss + (uint32_t)amount);
    if ((flags & GAME_PAYMENT_PAYER_BANK_FIRST) != 0) {
        first = p->bank < amount ? p->bank : amount; p->bank -= first; amount -= first;
        p->cash -= amount;
    } else {
        first = p->cash < amount ? p->cash : amount; p->cash -= first; amount -= first;
        p->bank -= amount;
    }
}

int rich4_ledger_spend_cash(GameState *state, int payer, int amount,
                            GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    if (state == NULL || events == NULL || payer < 0 ||
        payer >= state->player_count || amount < 0 ||
        (state->players[payer].play_state & 0xff) == 0 ||
        state->players[payer].cash < amount || state->system_cash < 0 ||
        state->system_cash > INT_MAX - amount) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    next.players[payer].cash -= amount;
    next.system_cash += amount;
    if (!emit_transfer(&staged, payer, rich4_ledger_system(), amount,
                       GAME_PAYMENT_PAYER_CASH_ONLY) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY) return 0;
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    return 1;
}

static int clear_player(GameState *s, int player, GameEventQueue *q)
{
    GamePlayerState *p = &s->players[player];
    GameDomainRef property[GAME_MAX_MAP_PROPERTIES * 2];
    int property_count = 0;
    int i, original_play_state;
    int land_first = -1, land_released = 0;
    int facility_first = -1, facility_released = 0;
    int64_t stock_proceeds = 0;

    /* Rich4.exe 0x40CD87 liquidates every holding through
     * 0x428E23(player, stock, quantity, 0) before clearing the player.
     * mode=0 returns the current-price proceeds to the system account and
     * restores both stock volume counters; it does not credit the player. */
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        int quantity = s->player_stock_amount[player][i];
        double value;
        int proceeds;
        if (quantity < 0) return 0;
        if (quantity == 0) continue;
        if ((unsigned int)s->stocks[i].f8 + (unsigned int)quantity > UINT16_MAX ||
            (unsigned int)s->stocks[i].f10 + (unsigned int)quantity > UINT16_MAX)
            return 0;
        value = (double)quantity * s->stocks[i].f20;
        if (value != value || value < 0.0 || value > (double)INT_MAX)
            return 0;
        proceeds = (int)value; /* original x87 conversion truncates toward zero */
        if (stock_proceeds > INT_MAX - proceeds) return 0;
        stock_proceeds += proceeds;
    }
    if (s->system_cash < 0 || stock_proceeds > INT_MAX - s->system_cash)
        return 0;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        int quantity = s->player_stock_amount[player][i];
        if (quantity == 0) continue;
        s->stocks[i].f8 = (uint16_t)(s->stocks[i].f8 + quantity);
        s->stocks[i].f10 = (uint16_t)(s->stocks[i].f10 + quantity);
        s->player_stock_amount[player][i] = 0;
        s->player_stock_average_cost[player][i] = 0.0f;
        rich4_stock_refresh_company_ownership(s, i, player);
    }
    s->system_cash += (int)stock_proceeds;
    /* 0x428EA7, the mode=0 liquidation stock proceeds join the prize pool. */
    s->lottery_pool = (int32_t)((uint32_t)s->lottery_pool + (uint32_t)stock_proceeds);
    for (i = 0; i < s->land_count; i++) if (s->land_owner[i] == player) {
        property[property_count++] =
            rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
        if (land_first < 0) land_first = i;
        land_released++;
        s->land_owner[i] = -1;
        s->land_runtime_value[i] = 0;
        s->land_value[i] = 0;
    }
    for (i = 0; i < s->facility_count; i++) if (s->facility_owner[i] == player) {
        property[property_count++] =
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
        if (facility_first < 0) facility_first = i;
        facility_released++;
        s->facility_owner[i] = -1;
        s->facility_runtime_value[i] = 0;
        s->facility_value[i] = 0;
        s->facility_last_change[i] = 0;
    }
    /* 0x498e7c is the map auxiliary/company pool.  0x40CD87 clears its
     * owner byte (record +0x18), while the business counters remain map
     * state.  The typed projection stores that owner as -1. */
    for (i = 0; i < s->company_count; i++)
        if (s->company_owner[i] == player) s->company_owner[i] = -1;
    if (land_released != 0)
        rich4_game_event_emit_typed_domain(
            q, rich4_game_event_property_owner_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_LAND, land_first), none_ref(),
            -1, land_released, 0);
    if (facility_released != 0)
        rich4_game_event_emit_typed_domain(
            q, rich4_game_event_property_owner_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, facility_first),
            none_ref(), -1, facility_released, 0);
    /* 0x441F21 returns each held card to card_amount before clearing the
     * player's 15-byte card slots.  0x445B3F does the same for the public
     * eight-tool pool (private/special tool slots 8..12 are not in that
     * pool), then restores the player's transport to walking. */
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
        int card = (int)p->card_inventory[i] - 1;
        if (card >= 0 && card < GAME_CARD_COUNT &&
            s->card_amount[card] < UINT8_MAX)
            s->card_amount[card]++;
    }
    for (i = 0; i < GAME_PLAYER_TOOL_SLOTS && i < 8; i++) {
        unsigned int available = (unsigned int)s->tool_amount[i] +
            (unsigned int)p->tool_inventory[i];
        s->tool_amount[i] = (uint8_t)(available > UINT8_MAX
            ? UINT8_MAX : available);
    }
    memset(p->card_inventory, 0, sizeof(p->card_inventory));
    memset(p->tool_inventory, 0, sizeof(p->tool_inventory));
    p->traffic_method = 0;
    p->ndices = 1;
    if (p->attached_timed_bomb_slot >= 0 &&
        p->attached_timed_bomb_slot < GAME_TOOL_PLACED_OBJECT_COUNT) {
        int slot = p->attached_timed_bomb_slot;
        if (s->tool_placed_objects[slot].kind == 0x12 &&
            s->tool_placed_objects[slot].attached_player == player + 1)
            memset(&s->tool_placed_objects[slot], 0,
                   sizeof(s->tool_placed_objects[slot]));
    }
    p->attached_timed_bomb_slot = -1;
    memset(p->relation_balance, 0, sizeof(p->relation_balance));
    p->cash = p->bank = p->loan = 0;
    p->loan_due_date = 0;
    p->special_finance = 0;
    p->points = 0;
    p->remote_dice_value = 0;
    p->saved_traffic_method = 0;
    p->saved_ndices = 0;
    p->route_reversed = 0;
    p->engineering_saved_traffic_method = 0;
    p->engineering_saved_ndices = 0;
    p->next_node = 0;
    p->previous_node = 0;
    s->fixed_asset_value[player] = 0;
    p->delayed_status_block = p->days_disappearing = p->days_in_prison = 0;
    p->days_in_hospital = p->days_sleeping = p->days_sleep_walking = 0;
    p->days_stopping = p->days_tortoise_walking = p->days_rejected_by_bank = 0;
    p->days_bank_no_loans = p->god_status_days = p->days_assurance = 0;
    p->god_info = 0;
    p->god_type = 0;
    p->god_income_modifier = 0;
    p->god_investment_modifier = 0;
    p->god_land_modifier = 0;
    p->linked_player = -1;
    p->confinement_actor_type = GAME_CONFINEMENT_NONE;
    p->total_confinement_days = 0;
    for (i = 0; i < GAME_NEWS_EVENT_COUNT; i++)
        if (s->news_event_occupant[i] == player + 1) s->news_event_occupant[i] = 0;
    for (i = 0; i < 36; i++)
        if (s->lottery_owner[i] == player + 1) s->lottery_owner[i] = 0;
    original_play_state = p->play_state;
    p->play_state = 0;
    queue_bankruptcy_auctions(s, player, property, property_count, q);
    rich4_game_event_inventory_changed_emit(q, player, 0, 0, 0, 0);
    rich4_game_event_status_changed_emit(q, player, 0, 0, 0, 0);
    rich4_game_event_player_eliminated_emit(
        q, player, original_play_state, 0, 0, 0);
    return 1;
}

static void maybe_end(GameState *s, GameEventQueue *q)
{
    VictoryResult r;
    int active = 0, last = -1, i;
    if (s->game_over) return;
    for (i = 0; i < s->player_count; i++) if ((s->players[i].play_state & 0xff) != 0) { active++; last = i; }
    if (active == 1) {
        s->game_over = 1; s->winner_player = last; s->winner_reason = GAME_WIN_CANDIDATE_LAST_PLAYER;
    } else if (rich4_victory_evaluate(s, &r) && r.event == VICTORY_EVENT_CANDIDATE) {
        s->game_over = 1; s->winner_player = r.player; s->winner_reason = r.reason;
    }
    if (s->game_over)
        rich4_game_event_game_ended_emit(
            q, s->winner_player, s->winner_reason,
            rich4_player_total_assets(s, s->winner_player, NULL), 0, 0);
}

int rich4_ledger_surrender_player(GameState *state, int player,
                                   GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;

    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count ||
        (state->players[player].play_state & 0xff) == 0 ||
        state->game_over)
        return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!clear_player(&next, player, &staged) ||
        staged.count >= GAME_EVENT_QUEUE_CAPACITY)
        return 0;
    maybe_end(&next, &staged);
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

int rich4_ledger_find_beggar(const GameState *state, int actor, int node)
{
    int player;
    if (state == NULL || actor < 0 || actor >= state->player_count ||
        (state->players[actor].play_state & 0xff) == 0 || node <= 0)
        return -1;
    for (player = 0; player < state->player_count; player++) {
        if (player == actor) continue;
        /* Bankruptcy/surrender deliberately leaves +8 (node) intact.  That
         * retained node is the original occupancy bit used by 0x41B42D. */
        /* 0x40D293 selects the first occupied slot, even if it is active. */
        if (state->players[player].node == node)
            return (state->players[player].play_state & 0xff) == 0 ? player : -1;
    }
    return -1;
}

int rich4_ledger_beggar_amount(const GameState *state)
{
    if (state == NULL || state->price_index <= 0 ||
        state->price_index > INT_MAX / 1000)
        return 0;
    return state->price_index * 1000;
}

static int beggar_abs_delta(int left, int right)
{
    int64_t delta = (int64_t)left - right;
    if (delta < 0) delta = -delta;
    return delta > INT_MAX ? INT_MAX : (int)delta;
}

static void relocate_beggar(GameState *state, int beggar, int reference_node)
{
    int candidates[GAME_MAX_MAP_ROUTE_NODES];
    int candidate_count = 0;
    int node_count;
    int node;
    int selected;
    Rich4Rng rng;
    GamePlayerState *player;

    if (state == NULL || beggar < 0 || beggar >= state->player_count)
        return;
    node_count = state->tool_route_node_count;
    if (node_count > GAME_MAX_MAP_ROUTE_NODES)
        node_count = GAME_MAX_MAP_ROUTE_NODES;
    if (node_count <= 1) return;

    /* sub_40AA6C scans the map's route records, not the active-player
     * occupancy bits.  It rejects object/blocked nodes and coordinates that
     * are not drawable, then keeps nodes outside the 300x300 local window. */
    for (node = 1; node < node_count; node++) {
        if ((state->tool_route_flags[node] & 0x80ffff00u) != 0 ||
            state->tool_route_x[node] == 0 ||
            state->tool_route_y[node] == 0)
            continue;
        if (reference_node > 0 && reference_node < node_count &&
            beggar_abs_delta(state->tool_route_x[node],
                             state->tool_route_x[reference_node]) < 300 &&
            beggar_abs_delta(state->tool_route_y[node],
                             state->tool_route_y[reference_node]) < 300)
            continue;
        candidates[candidate_count++] = node;
    }
    if (candidate_count <= 0) return;
    rng.state = state->ai_rng_state;

    selected = candidates[rich4_rng_state_range(&rng, candidate_count)];
    state->ai_rng_state = rng.state;
    player = &state->players[beggar];
    player->node = selected;
    player->previous_node = 0;
    player->next_node = 0;
    /* 0x40CC56 uses the first non-zero edge as the idle facing/route edge;
     * it does not reserve an edge or avoid another player's node. */
    for (node = 0; node < 4; node++) {
        if (state->tool_route_next[selected][node] != 0) {
            player->next_node = state->tool_route_next[selected][node];
            break;
        }
    }
}

GamePaymentResult rich4_ledger_beggar_donate(
    GameState *state, int payer, int beggar, int node, int amount,
    GameEventQueue *events)
{
    GamePaymentResult payment;
    if (state == NULL || events == NULL || payer < 0 ||
        payer >= state->player_count || beggar < 0 ||
        beggar >= state->player_count || payer == beggar || node <= 0 ||
        amount < 0 || (state->players[payer].play_state & 0xff) == 0 ||
        (state->players[beggar].play_state & 0xff) != 0 ||
        state->players[beggar].node != node)
        return GAME_PAYMENT_REJECTED;
    payment = rich4_ledger_pay(
        state, payer, rich4_ledger_system(), amount, 0, events);
    if (payment == GAME_PAYMENT_COMMITTED ||
        payment == GAME_PAYMENT_BANKRUPTCY)
        relocate_beggar(state, beggar, node);
    return payment;
}

static GamePaymentResult ledger_pay(GameState *state, int payer,
                                   GameLedgerParty payee, int amount,
                                   unsigned int flags, GameEventQueue *events,
                                   int inactive_fortune_actor)
{
    GameState next;
    GameEventQueue staged;
    int liquid, actual;
    int64_t liquid64;
    GamePaymentResult result;
    if (state == NULL || events == NULL || amount < 0 || payer < 0 ||
        payer >= state->player_count ||
        ((state->players[payer].play_state & 0xff) == 0 &&
         payer != inactive_fortune_actor) ||
        (!valid_party(state, payee) &&
         !(payee.kind == GAME_LEDGER_PLAYER && payee.id == inactive_fortune_actor &&
           payee.id >= 0 && payee.id < state->player_count)) || state->players[payer].cash < 0 ||
        state->players[payer].bank < 0 ||
        (flags & ~(GAME_PAYMENT_RECEIVER_CASH | GAME_PAYMENT_PAYER_BANK_FIRST)) != 0)
        return GAME_PAYMENT_REJECTED;
    next = *state; rich4_game_event_queue_reset(&staged);
    liquid64 = (int64_t)next.players[payer].cash + next.players[payer].bank;
    liquid = liquid64 > INT_MAX ? INT_MAX : (int)liquid64;
    if (liquid >= amount) {
        if (!can_credit(&next, payee, amount, flags)) return GAME_PAYMENT_REJECTED;
        debit_liquid(&next.players[payer], amount, flags); credit(&next, payee, amount, flags);
        emit_transfer(&staged, payer, payee, amount, flags); maybe_end(&next, &staged);
        result = GAME_PAYMENT_COMMITTED;
    } else {
        /* Rich4.exe 0x41D2C6 transfers the available liquid balance and then
         * immediately calls 0x40CD87.  It does not enter an asset-sale wait
         * state here. */
        actual = liquid < 0 ? 0 : liquid;
        if (!can_credit(&next, payee, actual, flags)) return GAME_PAYMENT_REJECTED;
        debit_liquid(&next.players[payer], actual, flags); credit(&next, payee, actual, flags);
        emit_transfer(&staged, payer, payee, actual, flags);
        if ((next.players[payer].play_state & 0xff) != 0) {
            if (!clear_player(&next, payer, &staged)) return GAME_PAYMENT_REJECTED;
            result = GAME_PAYMENT_BANKRUPTCY;
        } else result = GAME_PAYMENT_COMMITTED;
        maybe_end(&next, &staged);
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY) return GAME_PAYMENT_REJECTED;
    if (!rich4_game_event_queue_append(events, &staged)) return GAME_PAYMENT_REJECTED;
    *state = next; return result;
}

GamePaymentResult rich4_ledger_pay(GameState *state, int payer,
    GameLedgerParty payee, int amount, unsigned int flags, GameEventQueue *events)
{
    return ledger_pay(state,payer,payee,amount,flags,events,-1);
}

GamePaymentResult rich4_ledger_pay_fortune(GameState *state, int payer,
    GameLedgerParty payee, int amount, unsigned int flags, GameEventQueue *events)
{
    if (state == NULL || state->special_event_family != GAME_SPECIAL_EVENT_FORTUNE ||
        state->special_event_actor < 0 ||
        state->special_event_actor >= state->player_count) return GAME_PAYMENT_REJECTED;
    return ledger_pay(state,payer,payee,amount,flags,events,state->special_event_actor);
}

GamePaymentResult rich4_ledger_pay_split(
    GameState *state, int payer,
    GameLedgerParty first_payee, int first_amount,
    GameLedgerParty second_payee, int second_amount,
    unsigned int flags, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int64_t total64;
    int total, liquid, actual_first, actual_second, remaining;
    int64_t liquid64;

    if (state == NULL || events == NULL || payer < 0 ||
        payer >= state->player_count ||
        (state->players[payer].play_state & 0xff) == 0 ||
        first_amount < 0 || second_amount < 0 ||
        !valid_party(state, first_payee) ||
        !valid_party(state, second_payee) ||
        (state->players[payer].cash < 0 || state->players[payer].bank < 0) ||
        (flags & ~(GAME_PAYMENT_RECEIVER_CASH |
                   GAME_PAYMENT_PAYER_BANK_FIRST)) != 0 ||
        (first_amount > 0 && second_amount > 0 &&
         same_party(first_payee, second_payee)))
        return GAME_PAYMENT_REJECTED;

    total64 = (int64_t)first_amount + second_amount;
    if (total64 > INT_MAX) return GAME_PAYMENT_REJECTED;
    total = (int)total64;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    liquid64 = (int64_t)next.players[payer].cash + next.players[payer].bank;
    liquid = liquid64 > INT_MAX ? INT_MAX : (int)liquid64;

    if (liquid >= total) {
        if ((first_amount > 0 &&
             !can_credit(&next, first_payee, first_amount, flags)) ||
            (second_amount > 0 &&
             !can_credit(&next, second_payee, second_amount, flags)))
            return GAME_PAYMENT_REJECTED;
        debit_liquid(&next.players[payer], total, flags);
        if (first_amount > 0) {
            credit(&next, first_payee, first_amount, flags);
            if (!emit_transfer(&staged, payer, first_payee,
                               first_amount, flags))
                return GAME_PAYMENT_REJECTED;
        }
        if (second_amount > 0) {
            credit(&next, second_payee, second_amount, flags);
            if (!emit_transfer(&staged, payer, second_payee,
                               second_amount, flags))
                return GAME_PAYMENT_REJECTED;
        }
        maybe_end(&next, &staged);
        if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
            !rich4_game_event_queue_append(events, &staged))
            return GAME_PAYMENT_REJECTED;
        *state = next;
        return GAME_PAYMENT_COMMITTED;
    }

    /* Rich4.exe calls the ordinary payment entry once for each recipient.
     * Preserve that order when the payer cannot cover the aggregate: the
     * first recipient consumes its full component first, the second receives
     * the remainder, and only then is the payer cleared by bankruptcy. */
    actual_first = first_amount < liquid ? first_amount : liquid;
    remaining = liquid - actual_first;
    actual_second = second_amount < remaining ? second_amount : remaining;
    if ((actual_first > 0 &&
         !can_credit(&next, first_payee, actual_first, flags)) ||
        (actual_second > 0 &&
         !can_credit(&next, second_payee, actual_second, flags)))
        return GAME_PAYMENT_REJECTED;
    debit_liquid(&next.players[payer], actual_first + actual_second, flags);
    if (actual_first > 0) {
        credit(&next, first_payee, actual_first, flags);
        if (!emit_transfer(&staged, payer, first_payee,
                           actual_first, flags))
            return GAME_PAYMENT_REJECTED;
    }
    if (actual_second > 0) {
        credit(&next, second_payee, actual_second, flags);
        if (!emit_transfer(&staged, payer, second_payee,
                           actual_second, flags))
            return GAME_PAYMENT_REJECTED;
    }
    if (!clear_player(&next, payer, &staged)) return GAME_PAYMENT_REJECTED;
    maybe_end(&next, &staged);
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_PAYMENT_REJECTED;
    *state = next;
    return GAME_PAYMENT_BANKRUPTCY;
}

int rich4_ledger_sell_asset(GameState *state, const GameCommand *command,
                            GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int player, id, proceeds = 0, quantity = 0;
    GameDomainRef subject;
    if (state == NULL || command == NULL || events == NULL ||
        command->type != GAME_COMMAND_SELL_ASSET || !rich4_game_command_is_valid(command)) return 0;
    player = command->player; subject = command->subject; id = subject.id;
    if (player >= state->player_count || (state->players[player].play_state & 0xff) == 0) return 0;
    next = *state; rich4_game_event_queue_reset(&staged);
    if (subject.kind == GAME_DOMAIN_STOCK) {
        double value;
        quantity = command->value;
        if (quantity > next.player_stock_amount[player][id]) return 0;
        if ((unsigned int)next.stocks[id].f8 + (unsigned int)quantity > UINT16_MAX ||
            (unsigned int)next.stocks[id].f10 + (unsigned int)quantity > UINT16_MAX)
            return 0;
        value = (double)quantity * next.stocks[id].f20;
        if (value < 0.0 || value > (double)INT_MAX) return 0;
        proceeds = (int)value; next.player_stock_amount[player][id] -= quantity;
        next.stocks[id].f8 = (uint16_t)(next.stocks[id].f8 + quantity);
        next.stocks[id].f10 = (uint16_t)(next.stocks[id].f10 + quantity);
        if (next.player_stock_amount[player][id] == 0)
            next.player_stock_average_cost[player][id] = 0.0f;
        rich4_stock_refresh_company_ownership(&next, id, player);
        rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            subject, none_ref(), -quantity,
            next.player_stock_amount[player][id], 0);
    } else if (subject.kind == GAME_DOMAIN_LAND) {
        if (id >= next.land_count || next.land_owner[id] != player) return 0;
        if (!property_asset_value(&next, subject, &proceeds) ||
            !transfer_fixed_asset_value(&next, player, -1, proceeds))
            return 0;
        next.land_owner[id] = -1; next.land_value[id] = 0;
        rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_property_owner_changed_from_fields,
            player, subject, none_ref(), -1, 0, 0);
    } else {
        if (id >= next.facility_count || next.facility_owner[id] != player) return 0;
        if (!property_asset_value(&next, subject, &proceeds) ||
            !transfer_fixed_asset_value(&next, player, -1, proceeds))
            return 0;
        next.facility_owner[id] = -1;
        next.facility_value[id] = 0; next.facility_last_change[id] = 0;
        rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_property_owner_changed_from_fields,
            player, subject, none_ref(), -1, 0, 0);
    }
    if (proceeds < 0 || next.players[player].cash > INT_MAX - proceeds) return 0;
    next.players[player].cash += proceeds;
    emit_transfer(&staged, player, rich4_ledger_player(player), proceeds, GAME_PAYMENT_RECEIVER_CASH);
    if (next.liquidation_active && next.liquidation_player == player &&
        (int64_t)next.players[player].cash + next.players[player].bank >= next.liquidation_amount) {
        GameLedgerParty payee = { (GameLedgerPartyKind)next.liquidation_payee_kind, next.liquidation_payee_id };
        int amount = next.liquidation_amount; unsigned int flags = next.liquidation_flags;
        if (can_credit(&next, payee, amount, flags)) {
            next.liquidation_active = 0;
            debit_liquid(&next.players[player], amount, flags); credit(&next, payee, amount, flags);
            emit_transfer(&staged, player, payee, amount, flags); maybe_end(&next, &staged);
        }
    }
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next; return 1;
}

static GameDomainRef market_listing_ref(int seller, int slot)
{
    return rich4_game_domain_ref(
        GAME_DOMAIN_MARKET_LISTING,
        seller * GAME_MARKET_LISTING_SLOT_COUNT + slot);
}

static int market_item_type_and_id(GameDomainRef item, int *type, int *id)
{
    if (type == NULL || id == NULL ||
        !rich4_game_domain_ref_is_valid(item))
        return 0;
    switch (item.kind) {
    case GAME_DOMAIN_STOCK:
        *type = GAME_MARKET_LISTING_STOCK;
        *id = item.id;
        return 1;
    case GAME_DOMAIN_LAND:
        *type = GAME_MARKET_LISTING_ASSET;
        *id = 0x7d1 + item.id;
        return *id <= UINT16_MAX;
    case GAME_DOMAIN_FACILITY:
        *type = GAME_MARKET_LISTING_ASSET;
        *id = 0xfa1 + item.id;
        return *id <= UINT16_MAX;
    case GAME_DOMAIN_CARD:
        *type = GAME_MARKET_LISTING_CARD;
        *id = item.id + 1;
        return *id <= UINT16_MAX;
    case GAME_DOMAIN_TOOL:
        *type = GAME_MARKET_LISTING_TOOL;
        *id = item.id + 1;
        return *id <= UINT16_MAX;
    default:
        return 0;
    }
}

static int market_listing_item_ref(const GameMarketListing *listing,
                                   GameDomainRef *out_item)
{
    if (listing == NULL || out_item == NULL) return 0;
    switch (listing->type) {
    case GAME_MARKET_LISTING_STOCK:
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_STOCK, (int)listing->item_id);
        break;
    case GAME_MARKET_LISTING_ASSET:
        if (listing->item_id > 0x7d0 && listing->item_id < 0xfa0) {
            *out_item = rich4_game_domain_ref(
                GAME_DOMAIN_LAND, (int)listing->item_id - 0x7d1);
        } else if (listing->item_id >= 0xfa1 && listing->item_id < 0x1770) {
            *out_item = rich4_game_domain_ref(
                GAME_DOMAIN_FACILITY, (int)listing->item_id - 0xfa1);
        } else {
            return 0;
        }
        break;
    case GAME_MARKET_LISTING_CARD:
        if (listing->item_id == 0) return 0;
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_CARD, (int)listing->item_id - 1);
        break;
    case GAME_MARKET_LISTING_TOOL:
        if (listing->item_id == 0) return 0;
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_TOOL, (int)listing->item_id - 1);
        break;
    default:
        return 0;
    }
    return rich4_game_domain_ref_is_valid(*out_item);
}

static int market_emit_listing_changed(
    GameEventQueue *events, int seller, int slot,
    const GameMarketListing *listing)
{
    GameDomainRef item = none_ref();
    int price = 0;
    int quantity = 0;
    unsigned int type = GAME_MARKET_LISTING_EMPTY;
    if (listing != NULL && listing->type != GAME_MARKET_LISTING_EMPTY) {
        if (!market_listing_item_ref(listing, &item)) return 0;
        price = listing->ask_price;
        quantity = listing->quantity;
        type = listing->type;
    }
    return rich4_game_event_emit_typed_domain(
        events, rich4_game_event_listing_changed_from_fields, seller,
        market_listing_ref(seller, slot), item, price, quantity, type);
}

static int market_item_is_owned(const GameState *state, int player,
                                const GameMarketListing *listing,
                                GameDomainRef item)
{
    switch (item.kind) {
    case GAME_DOMAIN_STOCK:
        return listing->quantity > 0 &&
               state->player_stock_amount[player][item.id] >=
                   listing->quantity;
    case GAME_DOMAIN_LAND:
        return item.id < state->land_count &&
               state->land_owner[item.id] == player;
    case GAME_DOMAIN_FACILITY:
        return item.id < state->facility_count &&
               state->facility_owner[item.id] == player;
    case GAME_DOMAIN_CARD:
        return rich4_card_inventory_has(state, player, item.id);
    case GAME_DOMAIN_TOOL:
        return state->players[player].tool_inventory[item.id] != 0;
    default:
        return 0;
    }
}

static int market_add_listing(GameState *state, const GameCommand *command,
                              GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GameDomainRef item = command->subject;
    GameMarketListing listing;
    int player = command->player;
    int type;
    int item_id;
    int slot = -1;
    int i;
    if (player >= state->player_count ||
        (state->players[player].play_state & 0xff) == 0 ||
        !market_item_type_and_id(item, &type, &item_id))
        return 0;
    memset(&listing, 0, sizeof(listing));
    listing.type = (uint8_t)type;
    listing.item_id = (uint16_t)item_id;
    listing.ask_price = command->value;
    listing.quantity = (int16_t)command->value2;
    if (!market_item_is_owned(state, player, &listing, item)) return 0;
    for (i = 0; i < GAME_MARKET_LISTING_SLOT_COUNT; i++) {
        const GameMarketListing *current = &state->market_listings[player][i];
        if (current->type == listing.type &&
            current->item_id == listing.item_id) {
            slot = i;
            break;
        }
        if (slot < 0 && current->type == GAME_MARKET_LISTING_EMPTY)
            slot = i;
    }
    if (slot < 0) return 0;
    next = *state;
    next.market_listings[player][slot] = listing;
    rich4_game_event_queue_reset(&staged);
    if (!market_emit_listing_changed(
            &staged, player, slot, &next.market_listings[player][slot]) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

static int market_remove_listing(GameState *state, int seller, int slot,
                                 GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int i;
    if (seller < 0 || seller >= state->player_count ||
        slot < 0 || slot >= GAME_MARKET_LISTING_SLOT_COUNT ||
        state->market_listings[seller][slot].type == GAME_MARKET_LISTING_EMPTY)
        return 0;
    next = *state;
    memmove(&next.market_listings[seller][slot],
            &next.market_listings[seller][slot + 1],
            (size_t)(GAME_MARKET_LISTING_SLOT_COUNT - slot - 1) *
                sizeof(next.market_listings[seller][0]));
    memset(&next.market_listings[seller][GAME_MARKET_LISTING_SLOT_COUNT - 1],
           0, sizeof(next.market_listings[seller][0]));
    rich4_game_event_queue_reset(&staged);
    for (i = slot; i < GAME_MARKET_LISTING_SLOT_COUNT; i++) {
        if (!market_emit_listing_changed(
                &staged, seller, i, &next.market_listings[seller][i]))
            return 0;
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

static int market_reprice_listing(GameState *state,
                                  const GameCommand *command,
                                  int seller, int slot,
                                  GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GameMarketListing *listing;
    if (state == NULL || command == NULL || events == NULL ||
        command->source != GAME_COMMAND_SOURCE_AI ||
        seller < 0 || seller >= state->player_count ||
        seller != command->player || slot < 0 ||
        slot >= GAME_MARKET_LISTING_SLOT_COUNT ||
        !state->players[seller].is_ai || command->value <= 0)
        return 0;
    listing = &state->market_listings[seller][slot];
    if (listing->type != GAME_MARKET_LISTING_CARD &&
        listing->type != GAME_MARKET_LISTING_TOOL)
        return 0;
    next = *state;
    next.market_listings[seller][slot].ask_price = command->value;
    rich4_game_event_queue_reset(&staged);
    if (!market_emit_listing_changed(
            &staged, seller, slot, &next.market_listings[seller][slot]) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

static int market_transfer_item(GameState *state, int buyer, int seller,
                                const GameMarketListing *listing,
                                GameDomainRef item, GameEventQueue *events)
{
    GameDomainRef none = none_ref();
    switch (item.kind) {
    case GAME_DOMAIN_STOCK: {
        int quantity = listing->quantity;
        int buyer_before = state->player_stock_amount[buyer][item.id];
        int seller_before = state->player_stock_amount[seller][item.id];
        double previous_cost;
        int64_t total_cost;
        if (seller_before < quantity || buyer_before > INT_MAX - quantity)
            return 0;
        previous_cost = (double)buyer_before *
            (double)state->player_stock_average_cost[buyer][item.id];
        if (!(previous_cost >= 0.0 && previous_cost <= (double)INT_MAX))
            return 0;
        total_cost = (int64_t)(int)previous_cost + listing->ask_price;
        if (total_cost > INT_MAX) return 0;
        state->player_stock_amount[seller][item.id] -= quantity;
        state->player_stock_amount[buyer][item.id] += quantity;
        if (state->player_stock_amount[seller][item.id] == 0)
            state->player_stock_average_cost[seller][item.id] = 0.0f;
        state->player_stock_average_cost[buyer][item.id] =
            (float)((double)total_cost /
                    state->player_stock_amount[buyer][item.id]);
        rich4_stock_refresh_company_ownership(state, item.id, seller);
        rich4_stock_refresh_company_ownership(state, item.id, buyer);
        return rich4_game_event_emit_typed_domain(
                   events, rich4_game_event_inventory_changed_from_fields,
                   seller, item, none, -quantity,
                   state->player_stock_amount[seller][item.id], 0) &&
               rich4_game_event_emit_typed_domain(
                   events, rich4_game_event_inventory_changed_from_fields,
                   buyer, item, none, quantity,
                   state->player_stock_amount[buyer][item.id], 0);
    }
    case GAME_DOMAIN_LAND:
    {
        int value;
        if (item.id >= state->land_count ||
            state->land_owner[item.id] != seller ||
            !property_asset_value(state, item, &value))
            return 0;
        state->land_owner[item.id] = buyer;
        if (!market_recompute_fixed_asset_values(state)) return 0;
        return rich4_game_event_emit_typed_domain(
            events, rich4_game_event_property_owner_changed_from_fields,
            buyer, item, rich4_game_domain_ref(GAME_DOMAIN_PLAYER, seller),
            buyer, state->land_level[item.id], value);
    }
    case GAME_DOMAIN_FACILITY:
    {
        int value;
        if (item.id >= state->facility_count ||
            state->facility_owner[item.id] != seller ||
            !property_asset_value(state, item, &value))
            return 0;
        state->facility_owner[item.id] = buyer;
        if (!market_recompute_fixed_asset_values(state)) return 0;
        return rich4_game_event_emit_typed_domain(
            events, rich4_game_event_property_owner_changed_from_fields,
            buyer, item, rich4_game_domain_ref(GAME_DOMAIN_PLAYER, seller),
            buyer, state->facility_level[item.id], value);
    }
    case GAME_DOMAIN_CARD: {
        int count = 0;
        int i;
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
            if (state->players[buyer].card_inventory[i] == item.id + 1)
                count++;
        if (count >= 9 || !rich4_card_inventory_transfer(
                state, seller, buyer, item.id))
            return 0;
        return rich4_game_event_emit_typed_domain(
                   events, rich4_game_event_inventory_changed_from_fields,
                   seller, item, none, -1,
                   rich4_card_inventory_count(state, seller), 0) &&
               rich4_game_event_emit_typed_domain(
                   events, rich4_game_event_inventory_changed_from_fields,
                   buyer, item, none, 1,
                   rich4_card_inventory_count(state, buyer), 0);
    }
    case GAME_DOMAIN_TOOL:
        if (rich4_tool_inventory_count(state, buyer) >= 15 ||
            state->players[seller].tool_inventory[item.id] == 0 ||
            state->players[buyer].tool_inventory[item.id] == UINT8_MAX)
            return 0;
        state->players[seller].tool_inventory[item.id]--;
        state->players[buyer].tool_inventory[item.id]++;
        return rich4_game_event_emit_typed_domain(
                   events, rich4_game_event_inventory_changed_from_fields,
                   seller, item, none, -1,
                   rich4_tool_inventory_count(state, seller), 0) &&
               rich4_game_event_emit_typed_domain(
                   events, rich4_game_event_inventory_changed_from_fields,
                   buyer, item, none, 1,
                   rich4_tool_inventory_count(state, buyer), 0);
    default:
        return 0;
    }
}

static int market_buy_listing(GameState *state, const GameCommand *command,
                              GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int listing_id = command->subject.id;
    int seller = listing_id / GAME_MARKET_LISTING_SLOT_COUNT;
    int slot = listing_id % GAME_MARKET_LISTING_SLOT_COUNT;
    int buyer = command->player;
    GameMarketListing listing;
    GameDomainRef item;
    GamePaymentResult payment;
    if (buyer >= state->player_count || seller < 0 ||
        seller >= state->player_count || seller == buyer ||
        (state->players[buyer].play_state & 0xff) == 0 ||
        (state->players[seller].play_state & 0xff) == 0)
        return 0;
    listing = state->market_listings[seller][slot];
    if (listing.type == GAME_MARKET_LISTING_EMPTY || listing.ask_price <= 0 ||
        state->players[buyer].cash < listing.ask_price ||
        !market_listing_item_ref(&listing, &item) ||
        !market_item_is_owned(state, seller, &listing, item))
        return 0;
    if (item.kind == GAME_DOMAIN_CARD &&
        rich4_card_inventory_count(state, buyer) >= GAME_PLAYER_CARD_SLOTS)
        return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!market_transfer_item(&next, buyer, seller, &listing, item, &staged))
        return 0;
    payment = rich4_ledger_pay(
        &next, buyer, rich4_ledger_player(seller), listing.ask_price,
        0, &staged);
    if (payment != GAME_PAYMENT_COMMITTED) return 0;
    if (!market_remove_listing(&next, seller, slot, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

int rich4_ledger_apply_market_command(
    GameState *state, const GameCommand *command, GameEventQueue *events)
{
    int seller;
    int slot;
    if (state == NULL || command == NULL || events == NULL ||
        !rich4_game_command_is_valid(command))
        return 0;
    if (command->type == GAME_COMMAND_CREATE_MARKET_LISTING)
        return market_add_listing(state, command, events);
    if (command->type == GAME_COMMAND_REPRICE_MARKET_LISTING)
        return market_reprice_listing(
            state, command,
            command->subject.id / GAME_MARKET_LISTING_SLOT_COUNT,
            command->subject.id % GAME_MARKET_LISTING_SLOT_COUNT, events);
    if (command->type != GAME_COMMAND_CANCEL_MARKET_LISTING &&
        command->type != GAME_COMMAND_BUY_MARKET_LISTING)
        return 0;
    seller = command->subject.id / GAME_MARKET_LISTING_SLOT_COUNT;
    slot = command->subject.id % GAME_MARKET_LISTING_SLOT_COUNT;
    if (command->type == GAME_COMMAND_CANCEL_MARKET_LISTING) {
        if (seller >= state->player_count || seller != command->player ||
            (state->players[seller].play_state & 0xff) == 0)
            return 0;
        return market_remove_listing(state, seller, slot, events);
    }
    return market_buy_listing(state, command, events);
}
