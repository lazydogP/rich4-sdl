#include "core/property.h"
#include "core/ai.h"
#include "core/card.h"
#include "core/god.h"
#include "core/game_event_payload.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static int valid_land(const GameState *state, int land)
{
    return state != NULL && state->player_count > 0 &&
           state->player_count <= GAME_MAX_PLAYERS && state->land_count >= 0 &&
           state->land_count <= GAME_MAX_MAP_PROPERTIES && land >= 0 &&
           land < state->land_count && state->price_index > 0 &&
           state->land_level[land] >= 0 && state->land_level[land] <= 5 &&
           state->land_price[land] >= 0 &&
           state->land_price_per_level[land] >= 0;
}

static int bounded_price(int64_t value, int *out_value)
{
    if (out_value == NULL || value < 0 || value > INT_MAX) return 0;
    *out_value = (int)value;
    return 1;
}

static int asset_value(const GameState *state, int land, int *out_value)
{
    int64_t value;
    if (!valid_land(state, land)) return 0;
    value = state->land_price[land];
    if (state->land_type[land] != 0)
        value += state->land_price_per_level[land];
    else
        value += (int64_t)state->land_level[land] *
                 state->land_price_per_level[land];
    return bounded_price(value, out_value);
}

int rich4_land_purchase_price(const GameState *state, int land, int *out_price)
{
    int64_t base;
    int ok;
    if (!valid_land(state, land)) return 0;
    base = state->land_price[land] +
           (int64_t)state->land_level[land] * state->land_price_per_level[land];
    ok = bounded_price(base * state->price_index, out_price);
    return ok;
}

int rich4_land_upgrade_price(const GameState *state, int land, int *out_price)
{
    int ok;
    if (!valid_land(state, land) || state->land_type[land] != 0 ||
        state->land_level[land] >= 5) return 0;
    ok = bounded_price((int64_t)state->land_price_per_level[land] *
                       state->price_index, out_price);
    return ok;
}

static int land_toll_for_owner(const GameState *state, int land, int owner,
                               int *out_toll)
{
    int i;
    int64_t total = 0;
    if (!valid_land(state, land) || out_toll == NULL ||
        owner < 0 || owner >= state->player_count)
        return 0;
    if (state->land_type[land] == 0) {
        if (state->land_group[land] < 0) return 0;
        for (i = 0; i < state->land_count; i++) {
            int level = state->land_level[i];
            if (state->land_owner[i] != owner || state->land_type[i] != 0 ||
                state->land_group[i] != state->land_group[land]) continue;
            if (level < 0 || level > 5 ||
                state->land_toll_by_level[i][level] < 0) return 0;
            total += state->land_toll_by_level[i][level];
        }
    } else {
        for (i = 0; i < state->land_count; i++)
            if (state->land_owner[i] == owner && state->land_type[i] != 0)
                total += 2000;
    }
    if (total > INT_MAX / state->price_index) return 0;
    total *= state->price_index;
    return bounded_price(total, out_toll);
}

int rich4_land_toll(const GameState *state, int land, int *out_toll)
{
    int owner, toll;
    if (!valid_land(state, land) || out_toll == NULL) return 0;
    owner = state->land_owner[land];
    if (owner < 0 || owner >= state->player_count ||
        !land_toll_for_owner(state, land, owner, &toll)) return 0;
    if (state->land_price_status[land] != 0) {
        if (toll > INT_MAX / 2) return 0;
        toll *= 2;
    }
    *out_toll = toll;
    return 1;
}

static GamePropertyCollectionBlockReason owner_collection_block_reason(
    const GamePlayerState *owner, int actor)
{
    if (owner == NULL) return GAME_PROPERTY_COLLECTION_BLOCK_NONE;
    if (owner->linked_player == actor)
        return GAME_PROPERTY_COLLECTION_BLOCK_ALLIED;
    if (rich4_god_suppresses_collection(owner))
        return GAME_PROPERTY_COLLECTION_BLOCK_GOD;
    if (owner->delayed_status_block != 0)
        return GAME_PROPERTY_COLLECTION_BLOCK_HOTEL;
    if (owner->days_disappearing != 0)
        return GAME_PROPERTY_COLLECTION_BLOCK_DISAPPEARING;
    if (owner->days_in_prison != 0)
        return GAME_PROPERTY_COLLECTION_BLOCK_PRISON;
    if (owner->days_in_hospital != 0)
        return GAME_PROPERTY_COLLECTION_BLOCK_HOSPITAL;
    if (owner->days_sleeping != 0)
        return GAME_PROPERTY_COLLECTION_BLOCK_SLEEPING;
    if (owner->days_sleep_walking != 0)
        return GAME_PROPERTY_COLLECTION_BLOCK_SLEEPWALKING;
    return GAME_PROPERTY_COLLECTION_BLOCK_NONE;
}

int rich4_land_inspect(const GameState *state, int actor, int land,
                       GameLandAction *out_action)
{
    GameLandAction action;
    int price, related_owner, owner_toll, related_toll;
    int64_t total_toll;
    memset(&action, 0, sizeof(action));
    action.land = land; action.actor = actor; action.owner = -1;
    action.related_owner = -1;
    if (out_action == NULL || !valid_land(state, land) || actor < 0 ||
        actor >= state->player_count ||
        (state->players[actor].play_state & 0xff) == 0)
        return 0;
    action.owner = state->land_owner[land];
    action.kind = GAME_LAND_ACTION_NONE;
    if (action.owner < 0) {
        if (state->players[actor].days_sleeping != 0 ||
            state->players[actor].days_sleep_walking != 0 ||
            rich4_god_blocks_purchase(&state->players[actor]) ||
            !rich4_land_purchase_price(state, land, &price)) {
        } else if (state->players[actor].cash < price) {
            action.amount = price;
        } else if (state->players[actor].is_ai &&
                   !rich4_ai_purchase_allowed(state, actor, price)) {
            action.amount = price;
        } else {
            action.kind = GAME_LAND_ACTION_PURCHASE;
            action.amount = price;
        }
        if (action.kind == GAME_LAND_ACTION_NONE) {
            *out_action = action;
            return 1;
        }
    } else if (action.owner == actor) {
        if (state->players[actor].days_sleeping != 0 ||
            state->players[actor].days_sleep_walking != 0 ||
            !rich4_land_upgrade_price(state, land, &price)) {
        } else if (state->players[actor].cash < price) {
            action.amount = price;
        } else {
            action.kind = GAME_LAND_ACTION_UPGRADE;
            action.amount = price;
        }
    } else {
        if (action.owner >= state->player_count ||
            (state->players[action.owner].play_state & 0xff) == 0)
            return 0;
        if (state->land_closed_status[land] != 0 ||
            ((state->land_price_status[land] & 0xf0) != 0 &&
             (state->land_price_status[land] & 0x0f) != 0)) {
            /* 0x41D559 checks the seal before owner protection states. */
            action.collection_block_reason = GAME_PROPERTY_COLLECTION_BLOCK_SEALED;
            action.kind = GAME_LAND_ACTION_COLLECTION_BLOCKED;
        } else if ((action.collection_block_reason =
                 owner_collection_block_reason(
                     &state->players[action.owner], actor)) !=
                   GAME_PROPERTY_COLLECTION_BLOCK_NONE) {
            if (rich4_land_toll(state, land, &price)) {
                action.kind = GAME_LAND_ACTION_COLLECTION_BLOCKED;
                action.amount = price;
            }
        } else {
            related_owner = state->players[action.owner].linked_player;
            if (related_owner >= 0) {
                if (related_owner >= state->player_count ||
                    related_owner == action.owner ||
                    (state->players[related_owner].play_state & 0xff) == 0 ||
                    !land_toll_for_owner(state, land, action.owner,
                                         &owner_toll) ||
                    !land_toll_for_owner(state, land, related_owner,
                                         &related_toll))
                    return 0;
                if (state->land_price_status[land] != 0) {
                    if (owner_toll > INT_MAX / 2) return 0;
                    owner_toll *= 2;
                }
                total_toll = (int64_t)owner_toll + related_toll;
                if (total_toll > INT_MAX ||
                    !rich4_ledger_adjust_fee(
                        (int)total_toll,
                        rich4_god_player_type(&state->players[actor]),
                        &price))
                    return 0;
                action.kind = GAME_LAND_ACTION_CHARGE;
                action.amount = price;
                action.related_owner = related_owner;
                /* Preserve the original owner-first split when possible.
                 * God modifiers can theoretically make the adjusted total
                 * smaller than the related raw toll; normalize that edge
                 * case to two nonnegative transfers whose sum is total. */
                action.owner_amount = price >= related_toll
                    ? price - related_toll : 0;
                action.related_amount = price - action.owner_amount;
            } else if (!rich4_land_toll(state, land, &price)) {
                /* Invalid toll data leaves the action as NONE. */
            } else if (!rich4_ledger_adjust_fee(
                           price, rich4_god_player_type(&state->players[actor]),
                           &price)) {
                /* An overflowing god adjustment is not actionable. */
            } else {
                action.kind = GAME_LAND_ACTION_CHARGE;
                action.amount = price;
                action.owner_amount = price;
            }
        }
    }
    *out_action = action;
    return 1;
}

int rich4_land_apply_command(GameState *state, const GameCommand *command,
                             GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int land, price, value;
    const char *reason = "ok";
    if (state == NULL || command == NULL || events == NULL ||
        !rich4_game_command_is_valid(command) ||
        (command->type != GAME_COMMAND_PURCHASE_PROPERTY &&
         command->type != GAME_COMMAND_UPGRADE_PROPERTY) ||
        command->subject.kind != GAME_DOMAIN_LAND)
        return 0;
    land = command->subject.id;
    if (!valid_land(state, land) || command->player >= state->player_count ||
        rich4_god_blocks_trade(&state->players[command->player]) ||
        state->players[command->player].days_sleeping != 0 ||
        state->players[command->player].days_sleep_walking != 0)
        return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (command->type == GAME_COMMAND_PURCHASE_PROPERTY) {
        if (next.land_owner[land] != -1) reason = "already_owned";
        else if (rich4_god_blocks_purchase(&next.players[command->player]))
            reason = "god_blocks_purchase";
        else if (!rich4_land_purchase_price(&next, land, &price))
            reason = "purchase_price_fail";
        else if (!rich4_ledger_spend_cash(&next, command->player, price, &staged))
            reason = "spend_cash_fail";
        else if (!asset_value(&next, land, &value)) reason = "asset_value_fail";
        else {
            next.land_owner[land] = command->player;
            next.land_value[land] = value;
            if (!rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_property_owner_changed_from_fields,
                    command->player,
                    command->subject, rich4_game_domain_ref(GAME_DOMAIN_PLAYER,
                                                            command->player),
                    command->player, value, 0)) reason = "emit_owner_fail";
            else reason = "ok";
        }
        if (strcmp(reason, "ok") != 0) {
            return 0;
        }
    } else {
        if (next.land_owner[land] != command->player) reason = "not_owner";
        else if (!rich4_land_upgrade_price(&next, land, &price))
            reason = "upgrade_price_fail";
        else if (!rich4_ledger_spend_cash(&next, command->player, price, &staged))
            reason = "spend_cash_fail";
        else {
            next.land_level[land]++;
            if (!asset_value(&next, land, &value)) reason = "asset_value_fail";
            else {
                next.land_value[land] = value;
                if (!rich4_game_event_emit_typed_domain(
                        &staged, rich4_game_event_property_level_changed_from_fields,
                        command->player,
                        command->subject,
                        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                        next.land_level[land], value, 0))
                    reason = "emit_level_fail";
                else reason = "ok";
            }
        }
        if (strcmp(reason, "ok") != 0) {
            return 0;
        }
    }
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_scene_refresh_requested_from_fields,
            command->player,
            command->subject, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            0, 0, 0) || events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY)
        return 0;
    if (!rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

GamePaymentResult rich4_land_charge(GameState *state, int actor, int land,
                                    int *out_fee, GameEventQueue *events)
{
    return rich4_land_charge_with_cards(state, actor, land, 0, -1,
                                        out_fee, events);
}

GamePaymentResult rich4_land_charge_with_cards(GameState *state, int actor,
                                               int land, int accept_free,
                                               int redirect_target,
                                               int *out_fee,
                                               GameEventQueue *events)
{
    GameLandAction action;
    GamePaymentResult payment;
    GameState next;
    GameEventQueue staged;
    int inspect_ok, payer, fee;
    memset(&action, 0, sizeof(action));
    inspect_ok = rich4_land_inspect(state, actor, land, &action);
    if (!inspect_ok || action.kind != GAME_LAND_ACTION_CHARGE)
        return GAME_PAYMENT_REJECTED;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_card_resolve_landing_charge(&next, actor, action.owner, action.amount, 1,
                                   accept_free, redirect_target, &payer, &fee,
                                   &staged))
        return GAME_PAYMENT_REJECTED;
    if (out_fee != NULL) *out_fee = fee;
    if (fee == 0 || payer == action.owner || payer == action.related_owner)
        payment = GAME_PAYMENT_COMMITTED;
    else if (action.related_owner >= 0)
        payment = rich4_ledger_pay_split(
            &next, payer, rich4_ledger_player(action.owner),
            action.owner_amount, rich4_ledger_player(action.related_owner),
            action.related_amount, 0, &staged);
    else
        payment = rich4_ledger_pay(&next, payer,
                                   rich4_ledger_player(action.owner), fee, 0,
                                   &staged);
    if ((payment != GAME_PAYMENT_COMMITTED &&
         payment != GAME_PAYMENT_BANKRUPTCY) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return (payment == GAME_PAYMENT_COMMITTED ||
                payment == GAME_PAYMENT_BANKRUPTCY)
            ? GAME_PAYMENT_REJECTED : payment;
    *state = next;
    return payment;
}
