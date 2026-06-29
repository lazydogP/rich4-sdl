#include "core/facility.h"
#include "core/facility_internal.h"
#include "core/ai.h"
#include "core/card.h"
#include "core/god.h"
#include "core/insurance.h"
#include "core/stock.h"
#include "core/game_event_payload.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

/* ref_00474940, indexed by the facility subtype byte. */
const unsigned char rich4_facility_max_level[5] = {1, 5, 5, 1, 5};

int rich4_facility_bounded_value(int64_t value, int *out_value)
{
    if (out_value == NULL || value < 0 || value > INT_MAX) return 0;
    *out_value = (int)value;
    return 1;
}

static int valid_facility(const GameState *state, int facility)
{
    return state != NULL && state->player_count > 0 &&
           state->player_count <= GAME_MAX_PLAYERS &&
           state->facility_count >= 0 &&
           state->facility_count <= GAME_MAX_MAP_PROPERTIES &&
           facility >= 0 && facility < state->facility_count &&
           state->price_index > 0 && state->facility_level[facility] >= 0 &&
           state->facility_level[facility] <= 5 &&
           state->facility_type[facility] >= 0 &&
           state->facility_type[facility] <= 4 &&
           state->facility_price[facility] >= 0 &&
           state->facility_price_per_level[facility] >= 0;
}

static int owner_cannot_collect(const GamePlayerState *owner, int actor)
{
    return owner->linked_player == actor || rich4_god_suppresses_collection(owner) ||
           owner->delayed_status_block != 0 || owner->days_disappearing != 0 ||
           owner->days_in_prison != 0 || owner->days_in_hospital != 0 ||
           owner->days_sleeping != 0 || owner->days_sleep_walking != 0;
}

static int facility_closed(const GameState *state, int facility)
{
    int status = state->facility_price_status[facility];
    /* 0x41D59C checks both nibbles of the raw +0x1C byte. */
    return state->facility_closed_status[facility] != 0 ||
           ((status & 0xf0) != 0 && (status & 0x0f) != 0);
}

/* 0x40DF69: charge/stay hostility belongs to the landing actor, even when
 * another player receives the payment and hotel-status consequences. */
static void facility_relation_credit(GameState *state, int actor, int owner,
                                      int64_t delta)
{
    int64_t value;
    int linked;
    if (actor == owner || delta <= 0) return;
    value = (int64_t)state->players[actor].relation_balance[owner] + delta;
    state->players[actor].relation_balance[owner] = value > INT_MAX ? INT_MAX : (int)value;
    if (state->players[actor].linked_player != owner) return;
    linked = state->players[actor].linked_player;
    state->players[actor].linked_player = -1;
    state->players[actor].god_status_days = 0;
    if (linked >= 0 && linked < state->player_count) {
        state->players[linked].linked_player = -1;
        state->players[linked].god_status_days = 0;
    }
}

int rich4_facility_active_player(const GameState *state, int player)
{
    return state != NULL && player >= 0 && player < state->player_count &&
           (state->players[player].play_state & 0xff) != 0;
}

int rich4_facility_valid_company(const GameState *state, int company)
{
    int stock;
    if (state == NULL || company < 0 || company >= state->company_count ||
        company >= GAME_MAX_MAP_PROPERTIES || state->price_index <= 0)
        return 0;
    stock = state->company_stock_index[company];
    return stock >= 0 && stock < GAME_STOCK_COUNT &&
           state->company_type[company] >= 0 &&
           state->company_base_fee[company] >= 0;
}

int rich4_facility_purchase_price(const GameState *state, int facility,
                                  int *out_price)
{
    if (!valid_facility(state, facility)) return 0;
    return rich4_facility_bounded_value((int64_t)state->facility_price[facility] *
                         state->price_index, out_price);
}

int rich4_facility_upgrade_price(const GameState *state, int facility,
                                 int *out_price)
{
    int type, level;
    if (!valid_facility(state, facility)) return 0;
    type = state->facility_type[facility];
    level = state->facility_level[facility];
    if (level == 0)
        return rich4_facility_purchase_price(state, facility, out_price);
    if (type < 1 || type > 4 || level >= rich4_facility_max_level[type]) return 0;
    return rich4_facility_bounded_value((int64_t)state->facility_price_per_level[facility] *
                         state->price_index, out_price);
}

int rich4_facility_asset_value(const GameState *state, int facility,
                               int *out_value)
{
    if (!valid_facility(state, facility)) return 0;
    return rich4_facility_bounded_value((int64_t)state->facility_price[facility] +
                         (int64_t)state->facility_level[facility] *
                         state->facility_price_per_level[facility], out_value);
}

int rich4_facility_traffic_charge_amount(
    const GameState *state, int actor, int facility, int dice_total,
    int *out_amount)
{
    int owner;
    int traffic;
    int base;
    int64_t amount;

    if (out_amount == NULL || !valid_facility(state, facility) ||
        !rich4_facility_active_player(state, actor) || dice_total <= 0)
        return 0;
    owner = state->facility_owner[facility];
    if (owner < 0 || owner >= state->player_count || owner == actor ||
        !rich4_facility_active_player(state, owner) ||
        state->facility_level[facility] == 0 ||
        facility_closed(state, facility) ||
        state->facility_type[facility] != 3 ||
        owner_cannot_collect(&state->players[owner], actor))
        return 0;
    traffic = state->players[actor].traffic_method & 3;
    if (traffic == 0) return 0;
    amount = (int64_t)500 * state->price_index * dice_total *
             (1 << (traffic - 1));
    if (!rich4_facility_bounded_value(amount, &base) ||
        !rich4_ledger_adjust_fee(
            base, rich4_god_player_type(&state->players[actor]), out_amount))
        return 0;
    return 1;
}

int rich4_facility_inspect(const GameState *state, int actor, int facility,
                           GameFacilityAction *out_action)
{
    GameFacilityAction action;
    int price;
    memset(&action, 0, sizeof(action));
    action.facility = facility; action.actor = actor; action.owner = -1;
    if (out_action == NULL || !valid_facility(state, facility) || actor < 0 ||
        actor >= state->player_count ||
        (state->players[actor].play_state & 0xff) == 0) return 0;
    action.owner = state->facility_owner[facility];
    action.kind = GAME_FACILITY_ACTION_NONE;
    if (action.owner < 0) {
        if (state->players[actor].days_sleep_walking == 0 &&
            !rich4_god_blocks_purchase(&state->players[actor]) &&
            rich4_facility_purchase_price(state, facility, &price) &&
            state->players[actor].cash >= price &&
            (!state->players[actor].is_ai ||
             rich4_ai_purchase_allowed(state, actor, price))) {
            action.kind = GAME_FACILITY_ACTION_PURCHASE;
            action.amount = price;
        }
    } else if (action.owner == actor) {
        if (state->players[actor].days_sleep_walking == 0 &&
            rich4_facility_upgrade_price(state, facility, &price) &&
            state->players[actor].cash >= price) {
            action.kind = state->facility_level[facility] == 0 ?
                GAME_FACILITY_ACTION_BUILD : GAME_FACILITY_ACTION_UPGRADE;
            action.amount = price;
        } else if (       state->players[actor].days_sleep_walking == 0 &&
                   state->facility_closed_status[facility] == 0 &&
                   state->facility_type[facility] == 4 &&
                   state->facility_level[facility] > 0) {
            action.kind = GAME_FACILITY_ACTION_RESEARCH;
        }
    } else {
        if (action.owner >= state->player_count ||
            (state->players[action.owner].play_state & 0xff) == 0) return 0;
        if (state->facility_level[facility] != 0 &&
            !facility_closed(state, facility) &&
            !owner_cannot_collect(&state->players[action.owner], actor) &&
            state->facility_type[facility] >= 1 &&
            state->facility_type[facility] <= 3) {
            /* 1/2 use the random 0x44090E interaction.  Type 3 is an
             * immediate traffic charge; its amount is resolved from the
             * actor's low two traffic bits and dice total. */
            action.kind = state->facility_type[facility] == 3 ?
                GAME_FACILITY_ACTION_TRAFFIC_CHARGE :
                GAME_FACILITY_ACTION_DEFERRED;
        }
    }
    *out_action = action;
    return 1;
}

int rich4_facility_apply_command(GameState *state, const GameCommand *command,
                                 GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int facility, price, value;
    if (state == NULL || command == NULL || events == NULL ||
        !rich4_game_command_is_valid(command) ||
        (command->type != GAME_COMMAND_PURCHASE_PROPERTY &&
         command->type != GAME_COMMAND_UPGRADE_PROPERTY) ||
        command->subject.kind != GAME_DOMAIN_FACILITY) return 0;
    facility = command->subject.id;
    if (!valid_facility(state, facility) ||
        command->player >= state->player_count ||
        rich4_god_blocks_trade(&state->players[command->player]) ||
        state->players[command->player].days_sleep_walking != 0) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (command->type == GAME_COMMAND_PURCHASE_PROPERTY) {
        if (command->value != 0 || next.facility_owner[facility] != -1 ||
            rich4_god_blocks_purchase(&next.players[command->player]) ||
            !rich4_facility_purchase_price(&next, facility, &price) ||
            !rich4_ledger_spend_cash(&next, command->player, price, &staged) ||
            !rich4_facility_asset_value(&next, facility, &value)) return 0;
        next.facility_owner[facility] = command->player;
        next.facility_value[facility] = value;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_property_owner_changed_from_fields,
                command->player,
                command->subject,
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, command->player),
                command->player, value, 0)) return 0;
    } else {
        int level = next.facility_level[facility];
        if (next.facility_owner[facility] != command->player ||
            !rich4_facility_upgrade_price(&next, facility, &price)) return 0;
        if ((level == 0 && (command->value < 0 || command->value > 4)) ||
            (level != 0 && command->value != 0) ||
            !rich4_ledger_spend_cash(&next, command->player, price, &staged))
            return 0;
        if (level == 0) next.facility_type[facility] = command->value;
        next.facility_level[facility]++;
        if (!rich4_facility_asset_value(&next, facility, &value)) return 0;
        next.facility_value[facility] = value;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_property_level_changed_from_fields,
                command->player,
                command->subject,
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                next.facility_level[facility], value,
                (unsigned int)next.facility_type[facility])) return 0;
    }
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_scene_refresh_requested_from_fields,
            command->player,
            command->subject, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            0, 0, 0) || events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY)
        return 0;
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    return 1;
}

GamePaymentResult rich4_facility_resolve_landing(
    GameState *state, int actor, int facility, int outcome, int dice_total,
    int accept_free, int redirect_target, GameFacilityLandingResult *out,
    GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    GamePaymentResult payment;
    GameFacilityLandingResult result;
    int owner, type, level, payer, fee, adjusted;
    int64_t amount;
    memset(&result, 0, sizeof(result));
    result.kind = GAME_LANDING_INVALID;
    result.payer = -1; result.owner = -1; result.outcome = outcome;
    if (state == NULL || events == NULL || !valid_facility(state, facility) ||
        !rich4_facility_active_player(state, actor)) return GAME_PAYMENT_REJECTED;
    owner = state->facility_owner[facility];
    type = state->facility_type[facility]; level = state->facility_level[facility];
    result.owner = owner;
    if (owner < 0 || owner == actor || !rich4_facility_active_player(state, owner) || level == 0 ||
        facility_closed(state, facility) || type == 0 || type == 4 ||
        owner_cannot_collect(&state->players[owner], actor)) {
        result.kind = GAME_LANDING_NONE;
        if (out != NULL) *out = result;
        return GAME_PAYMENT_COMMITTED;
    }
    if (type == 1 || type == 2) {
        int maximum = type == 1 ? 4 : 6;
        if (outcome < 1 || outcome > maximum) return GAME_PAYMENT_REJECTED;
        amount = (int64_t)state->facility_charge_by_level[facility][level] *
                 state->price_index * outcome;
        if (state->facility_price_status[facility] != 0) amount *= 2;
    } else if (type == 3) {
        int traffic = state->players[actor].traffic_method & 3;
        if (traffic == 0) {
            result.kind = GAME_LANDING_NONE;
            if (out != NULL) *out = result;
            return GAME_PAYMENT_COMMITTED;
        }
        if (dice_total <= 0) return GAME_PAYMENT_REJECTED;
        amount = (int64_t)500 * state->price_index * dice_total *
                 (1 << (traffic - 1));
    } else return GAME_PAYMENT_REJECTED;
    if (!rich4_facility_bounded_value(amount, &fee) ||
        !rich4_ledger_adjust_fee(fee,
             rich4_god_player_type(&state->players[actor]), &adjusted))
        return GAME_PAYMENT_REJECTED;
    /* 0x41A598 skips the entire charge/hotel-stay block when god-adjusted
     * cost is zero (distinct from an exemption card used later). */
    if (adjusted == 0) {
        result.kind = GAME_LANDING_NONE;
        result.payer = actor;
        if (out != NULL) *out = result;
        return GAME_PAYMENT_COMMITTED;
    }
    next = *state;
    rich4_game_event_queue_reset(&staged);
    /* 0x41A60E/0x41A670 gate both human and AI passive-card attempts. */
    facility_relation_credit(&next, actor, owner, adjusted / 100);
    if ((int64_t)adjusted < (int64_t)2000 * state->price_index &&
        (int64_t)adjusted <= (int64_t)state->players[actor].cash +
                            state->players[actor].bank) {
        accept_free = 0;
        redirect_target = -1;
    }
    if (!rich4_card_resolve_landing_charge(&next, actor, owner, adjusted, type != 1,
                                   type == 1 ? 0 : accept_free,
                                   redirect_target, &payer, &fee, &staged))
        return GAME_PAYMENT_REJECTED;
    payment = fee == 0 || payer == owner ? GAME_PAYMENT_COMMITTED :
        rich4_ledger_pay(&next, payer, rich4_ledger_player(owner), fee, 0,
                         &staged);
    if (payment != GAME_PAYMENT_COMMITTED && payment != GAME_PAYMENT_BANKRUPTCY)
        return payment;
    if (payer != owner) next.facility_runtime_value[facility] = fee;
    if (type == 1 && rich4_facility_active_player(&next, payer)) {
        int claim_company = next.players[payer].days_assurance != 0
            ? rich4_insurance_find_company(&next) : -1;
        facility_relation_credit(&next, actor, owner,
            (int64_t)next.price_index * 20 * outcome);
        /* 0x40D761 clears only +0x32..+0x35; vehicles/dice survive. */
        next.players[payer].days_disappearing = 0;
        next.players[payer].days_in_prison = 0;
        next.players[payer].days_in_hospital = 0;
        next.players[payer].confinement_actor_type = GAME_CONFINEMENT_NONE;
        next.players[payer].delayed_status_block = outcome == 1 ? 0x80 : outcome - 1;
        /* 0x40D650 copies the landing actor's road records for substitutes.
         * The road object's facility index retains the hotel anchor on load. */
        if (payer != actor) {
            next.players[payer].node = next.players[actor].node;
            next.players[payer].previous_node = next.players[actor].previous_node;
            next.players[payer].next_node = next.players[actor].next_node;
        }
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_status_changed_from_fields, payer,
                rich4_game_domain_ref(GAME_DOMAIN_STATUS, GAME_STATUS_DELAYED_BLOCK),
                rich4_game_domain_ref(GAME_DOMAIN_FACILITY, facility),
                next.players[payer].delayed_status_block, 0, 0))
            return GAME_PAYMENT_REJECTED;
        /* 0x44BAC1 waits two seconds before 0x44BAD8 credits the claim.
         * The app holds this receipt before starting 0x40D5A5. */
        if (claim_company >= 0) {
            int claim_amount;
            GameEvent notice;
            if (!rich4_facility_bounded_value(
                    (int64_t)2000 * next.price_index * outcome, &claim_amount))
                return GAME_PAYMENT_REJECTED;
            notice = rich4_game_event_insurance_claim(payer, claim_company,
                payer, claim_amount, 2000, GAME_INSURANCE_CLAIM_HOTEL |
                GAME_INSURANCE_CLAIM_DEFERRED |
                ((unsigned int)outcome << GAME_INSURANCE_CLAIM_DAYS_SHIFT));
            if (!rich4_game_event_queue_push(&staged, &notice)) return GAME_PAYMENT_REJECTED;
        } else {
            next.players[payer].total_confinement_days =
                (next.players[payer].total_confinement_days + outcome) & 0xff;
        }
        /* 0x41982D sets +21 bit 0x20 through 0x40D5A5 after the type-1
         * facility delay, but only when that active payer is the current
         * landing actor. This keeps the actor in the original draw queue
         * while the delayed block is pending. */
        if (payer == actor && actor == next.current_player)
            next.players[payer].play_state |=
                GAME_PLAYER_PLAY_STATE_DELAYED_BLOCK_VISIBLE;
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_PAYMENT_REJECTED;
    result.kind = payment == GAME_PAYMENT_BANKRUPTCY ?
        GAME_LANDING_BANKRUPTCY : GAME_LANDING_CHARGED;
    result.payer = payer; result.amount = fee;
    *state = next;
    if (out != NULL) *out = result;
    return payment;
}

int rich4_facility_start_research(GameState *state, int actor, int facility,
                                  int selection, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    if (state == NULL || events == NULL || !valid_facility(state, facility) ||
        !rich4_facility_active_player(state, actor) || state->facility_owner[facility] != actor ||
        state->facility_type[facility] != 4 ||
        state->facility_closed_status[facility] != 0 || selection < 0 ||
        selection >= state->facility_level[facility]) return 0;
    next = *state;
    next.facility_research_selection[facility] = selection + 1;
    next.facility_research_turns[facility] = 5;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_status_changed_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, facility),
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, selection + 8), 5,
            selection + 1, 0) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    return 1;
}

int rich4_facility_tick_research(GameState *state, int actor,
                                 GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int i, changed = 0;
    if (state == NULL || events == NULL || !rich4_facility_active_player(state, actor)) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    for (i = 0; i < next.facility_count; i++) {
        int selected = next.facility_research_selection[i];
        int tool;
        if (next.facility_owner[i] != actor || selected == 0) continue;
        if (next.facility_type[i] != 4 || selected > next.facility_level[i]) {
            next.facility_research_selection[i] = 0;
            next.facility_research_turns[i] = 0;
            changed = 1;
            continue;
        }
        if (next.facility_research_turns[i] <= 0) {
            next.facility_research_selection[i] = 0;
            changed = 1;
            continue;
        }
        next.facility_research_turns[i]--;
        changed = 1;
        if (next.facility_research_turns[i] != 0) continue;
        tool = selected + 7;
        next.facility_research_selection[i] = 0;
        if (tool < 0 || tool >= GAME_PLAYER_TOOL_SLOTS ||
            next.players[actor].tool_inventory[tool] == 255) return 0;
        next.players[actor].tool_inventory[tool]++;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_inventory_changed_from_fields, actor,
                rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
                rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i), 1,
                next.players[actor].tool_inventory[tool], 0)) return 0;
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return 0;
    if (changed) *state = next;
    return 1;
}
