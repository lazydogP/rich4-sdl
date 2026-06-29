#include "core/special_actor_action.h"

#include <limits.h>
#include <string.h>

#include "core/card.h"
#include "core/ledger.h"
#include "core/rng.h"

static int active_player(const GameState *state, int player)
{
    return state != NULL && player >= 0 && player < state->player_count &&
           (state->players[player].play_state & 0xff) != 0;
}

static int bounded_amount(uint64_t value)
{
    return value > (uint64_t)INT_MAX ? INT_MAX : (int)value;
}

static int append_action(GameSpecialActorActionBatch *batch,
                         GameSpecialActorActionKind kind,
                         int slot, int owner, int target,
                         int amount, int card, GameDomainRef property)
{
    GameSpecialActorAction *action;
    if (batch == NULL || batch->count >= GAME_SPECIAL_ACTOR_ACTION_BATCH_MAX)
        return 0;
    action = &batch->action[batch->count++];
    memset(action, 0, sizeof(*action));
    action->kind = kind;
    action->special_slot = slot;
    action->owner_player = owner;
    action->target_player = target;
    action->amount = amount;
    action->card = card;
    action->property = property;
    return 1;
}

static int encountered_player(GameState *state, int node, int owner,
                              Rich4Rng *rng)
{
    int candidates[GAME_MAX_PLAYERS];
    int count = 0;
    int i;
    for (i = 0; i < state->player_count; i++)
        if (i != owner && active_player(state, i) &&
            state->players[i].node == node)
            candidates[count++] = i;
    return count > 0
        ? candidates[rich4_rng_state_range(rng, count)] : -1;
}

static int random_owned_card(const GameState *state, int player,
                             Rich4Rng *rng)
{
    int slots[GAME_PLAYER_CARD_SLOTS];
    int count = 0;
    int i;
    if (!active_player(state, player)) return -1;
    for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
        if (state->players[player].card_inventory[i] != 0)
            slots[count++] = i;
    if (count == 0) return -1;
    i = slots[rich4_rng_state_range(rng, count)];
    return (int)state->players[player].card_inventory[i] - 1;
}

static int pay_property_fee(GameState *state, int payer, int owner,
                            int amount, GameEventQueue *events)
{
    GamePaymentResult result = rich4_ledger_pay(
        state, payer, rich4_ledger_player(owner), amount, 0, events);
    return result != GAME_PAYMENT_REJECTED;
}

int rich4_special_actor_apply_step(
    GameState *state, const MapData *map, int special_slot, int node,
    int is_endpoint, GameSpecialActorActionBatch *out_batch,
    GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GameSpecialActorActionBatch batch;
    Rich4Rng rng;
    int owner;
    int victim;
    int raw;
    if (state == NULL || map == NULL || out_batch == NULL || events == NULL ||
        special_slot < 0 || special_slot >= 4 || node <= 0 ||
        (size_t)node >= map->route_count ||
        state->special_player_f10[special_slot] != 0)
        return 0;
    memset(&batch, 0, sizeof(batch));
    if ((state->special_player_status[special_slot][1] & 0xff) != 0) {
        *out_batch = batch;
        return 1;
    }
    owner = state->special_player_play_state[special_slot];
    if (!active_player(state, owner)) {
        *out_batch = batch;
        return 1;
    }
    next = *state;
    rich4_game_event_queue_reset(&staged);
    rng.state = next.ai_rng_state;
    victim = encountered_player(&next, node, owner, &rng);
    if (special_slot == 0 && victim >= 0) {
        int amount = (int)next.players[victim].points / 2;
        if (amount > 0) {
            next.players[victim].points =
                (uint16_t)(next.players[victim].points - amount);
            next.players[owner].points =
                (uint16_t)(next.players[owner].points + amount);
            if (!append_action(&batch,
                    GAME_SPECIAL_ACTOR_ACTION_THIEF_POINTS,
                    special_slot, owner, victim, amount, -1,
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, victim)))
                return 0;
        }
    } else if (special_slot == 1 && victim >= 0) {
        int card = random_owned_card(&next, victim, &rng);
        if (card >= 0 && rich4_card_inventory_transfer(
                &next, victim, owner, card)) {
            if (!append_action(&batch,
                    GAME_SPECIAL_ACTOR_ACTION_ROBBER_CARD,
                    special_slot, owner, victim, 0, card,
                    rich4_game_domain_ref(GAME_DOMAIN_CARD, card)))
                return 0;
        }
    }
    if (special_slot == 1 &&
        (map->routes[node].flags & 0xffu) == 14u) {
        int total = 0;
        int player;
        for (player = 0; player < next.player_count; player++) {
            int amount;
            GamePaymentResult payment;
            if (player == owner || !active_player(&next, player)) continue;
            amount = next.players[player].bank / 5;
            payment = rich4_ledger_pay(
                &next, player, rich4_ledger_player(owner), amount,
                GAME_PAYMENT_RECEIVER_CASH |
                    GAME_PAYMENT_PAYER_BANK_FIRST,
                &staged);
            if (payment == GAME_PAYMENT_REJECTED) return 0;
            if (total <= INT_MAX - amount) total += amount;
            else total = INT_MAX;
        }
        if (!append_action(&batch, GAME_SPECIAL_ACTOR_ACTION_ROBBER_BANK,
                special_slot, owner, -1, total, -1,
                rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node)))
            return 0;
    }
    raw = map->routes[node].object.raw_object_id;
    if (is_endpoint && (special_slot == 2 || special_slot == 3)) {
        int target = -1;
        int amount = 0;
        GameDomainRef property = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        GameSpecialActorActionKind kind = GAME_SPECIAL_ACTOR_ACTION_NONE;
        if (raw >= 0x7d1 && raw < 0xfa0) {
            int land = raw - 0x7d1;
            if (land >= 0 && land < next.land_count) {
                target = next.land_owner[land];
                property = rich4_game_domain_ref(GAME_DOMAIN_LAND, land);
                if (special_slot == 2) {
                    uint64_t base = 0;
                    int i;
                    for (i = 0; i < next.land_count; i++)
                        if (next.land_owner[i] == target &&
                            ((next.land_group[land] >= 0 &&
                              next.land_group[i] == next.land_group[land]) ||
                             (next.land_group[land] < 0 && i == land)))
                            base += (unsigned int)next.land_price[i];
                    amount = bounded_amount(base *
                        (unsigned int)(next.price_index > 0
                            ? next.price_index : 1));
                    kind = GAME_SPECIAL_ACTOR_ACTION_HOOLIGAN_FEE;
                } else {
                    amount = bounded_amount(next.land_runtime_value[land]);
                    kind = GAME_SPECIAL_ACTOR_ACTION_SPY_TOLL;
                }
            }
        } else if (raw >= 0xfa1 && raw < 0x1770) {
            int facility = raw - 0xfa1;
            if (facility >= 0 && facility < next.facility_count) {
                target = next.facility_owner[facility];
                property = rich4_game_domain_ref(
                    GAME_DOMAIN_FACILITY, facility);
                if (special_slot == 2) {
                    amount = bounded_amount(
                        (uint64_t)(unsigned int)next.facility_price[facility] *
                        (unsigned int)(next.price_index > 0
                            ? next.price_index : 1));
                    kind = GAME_SPECIAL_ACTOR_ACTION_HOOLIGAN_FEE;
                } else {
                    amount = bounded_amount(
                        next.facility_runtime_value[facility]);
                    kind = GAME_SPECIAL_ACTOR_ACTION_SPY_TOLL;
                }
            }
        } else if (special_slot == 3 &&
                   raw >= 0x1771 && raw < 0x1f40) {
            int company = raw - 0x1771;
            if (company >= 0 && company < next.company_count) {
                target = next.company_owner[company];
                amount = bounded_amount(
                    next.company_period_business[company]);
                property = rich4_game_domain_ref(
                    GAME_DOMAIN_COMPANY, company);
                kind = GAME_SPECIAL_ACTOR_ACTION_SPY_DIVIDEND;
            }
        }
        if (kind != GAME_SPECIAL_ACTOR_ACTION_NONE && amount > 0 &&
            target != owner && active_player(&next, target)) {
            if (!pay_property_fee(&next, target, owner, amount, &staged) ||
                !append_action(&batch, kind, special_slot, owner, target,
                               amount, -1, property))
                return 0;
        }
    }
    next.ai_rng_state = rng.state;
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    *out_batch = batch;
    return 1;
}
