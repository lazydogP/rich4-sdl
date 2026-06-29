#include "core/facility_internal.h"
#include "core/facility.h"
#include "core/card.h"
#include "core/confinement.h"
#include "core/god.h"
#include "core/insurance.h"
#include "core/game_event_payload.h"
#include "core/ledger.h"
#include "core/stock.h"

#include <limits.h>
#include <string.h>

int rich4_company_landing_amount(const GameState *state, int actor, int company,
                                 int outcome, int dice_total, int *out_amount)
{
    int type, traffic = 0;
    int64_t amount;
    if (!rich4_facility_valid_company(state, company) || out_amount == NULL) return 0;
    type = state->company_type[company];
    if (type == 1) {
        if (outcome < 0 || outcome > 3 || (outcome != 0 && dice_total <= 0))
            return 0;
        /* 0x41ABDA..0x41ABDE: mode-0 result, without a dice multiplier. */
        amount = (int64_t)state->company_base_fee[company] *
                 outcome * state->price_index;
    } else if (type == 3) {
        amount = (int64_t)state->company_base_fee[company] * state->elapsed_days;
    } else if (type == 4) {
        if (outcome != 3 && outcome != 5 && outcome != 10 && outcome != 15 &&
            outcome != 20 && outcome != 30) return 0;
        amount = (int64_t)state->company_base_fee[company] * outcome *
                 state->price_index;
    } else if (type == 5 || type == 6) {
        if (!rich4_facility_active_player(state, actor) || dice_total <= 0)
            return 0;
        traffic = state->players[actor].traffic_method & 3;
        amount = traffic == 0 ? 0 :
            (int64_t)state->company_base_fee[company] * dice_total *
            (1 << (traffic - 1)) * state->price_index;
    } else if (type == 11) {
        amount = (int64_t)1000 * state->price_index;
    } else if (type == 12) {
        if (dice_total <= 0) return 0;
        amount = (int64_t)state->company_base_fee[company] * dice_total *
                 state->price_index;
    } else if (type >= 0 && type <= 10) amount = 0;
    else return 0;
    return rich4_facility_bounded_value(amount, out_amount);
}

static GamePaymentResult company_landing_stage(
    GameState *state, int actor, int company, int outcome, int dice_total,
    int accept_free, int redirect_target, GameCompanyLandingResult *out,
    GameEventQueue *events, int payment_only)
{
    GameState next;
    GameEventQueue staged;
    GameCompanyLandingResult result;
    GamePaymentResult payment;
    int stock, chairman, type, payer, fee, quoted_fee = 0;
    GameEvent insurance_event;
    int company_cash_before = 0;
    memset(&result, 0, sizeof(result));
    result.kind = GAME_LANDING_INVALID; result.company = company;
    result.payer = -1; result.chairman = -1; result.outcome = outcome;
    if (state == NULL || events == NULL || !rich4_facility_valid_company(state, company) ||
        !rich4_facility_active_player(state, actor)) return GAME_PAYMENT_REJECTED;
    stock = state->company_stock_index[company];
    chairman = state->stock_company_owner[stock];
    type = state->company_type[company];
    result.stock = stock; result.chairman = chairman;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (type == 4 && chairman >= 0 && !payment_only) {
        int days;
        if (outcome != 3 && outcome != 5 && outcome != 10 && outcome != 15 &&
            outcome != 20 && outcome != 30) return GAME_PAYMENT_REJECTED;
        days = ((next.players[actor].days_assurance & 0x7f) + outcome) & 0x7f;
        next.players[actor].days_assurance = days;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_status_changed_from_fields, actor,
                rich4_game_domain_ref(GAME_DOMAIN_STATUS, GAME_STATUS_ASSURANCE),
                rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock), days, 0, 0))
            return GAME_PAYMENT_REJECTED;
    }
    if (chairman < 0 || chairman == actor || (type == 1 && outcome == 0) ||
        type == 0 || type == 2 || (type >= 7 && type <= 10) ||
        ((type == 5 || type == 6) &&
         (next.players[actor].traffic_method & 3) == 0)) {
        if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
            !rich4_game_event_queue_append(events, &staged)) return GAME_PAYMENT_REJECTED;
        result.kind = staged.count != 0 || (type == 1 && outcome > 0) ?
            GAME_LANDING_APPLIED : GAME_LANDING_NONE;
        *state = next;
        if (out != NULL) *out = result;
        return GAME_PAYMENT_COMMITTED;
    }
    if (!rich4_company_landing_amount(&next, actor, company, outcome, dice_total, &fee))
        return GAME_PAYMENT_REJECTED;
    quoted_fee = fee;
    if (type == 4)
        insurance_event = rich4_game_event_insurance_fee(
            actor, company, chairman, quoted_fee, 1500, (unsigned int)stock);
    if (type == 4 && fee != 0 && !payment_only &&
        !rich4_game_event_queue_push(&staged, &insurance_event))
        return GAME_PAYMENT_REJECTED;
    if (!rich4_ledger_adjust_fee(fee,
             type == 4 ? (next.players[actor].god_info & 0xff) :
                 rich4_god_player_type(&next.players[actor]), &fee) ||
        !rich4_card_resolve_landing_charge(&next, actor, chairman, fee, 1, accept_free,
                                   redirect_target, &payer, &fee, &staged))
        return GAME_PAYMENT_REJECTED;
    /* 0x41AF96 also seeks a god substitute for a nonzero travel result
     * after a free card has removed the monetary fee. */
    if (type == 1 && outcome > 0 && fee == 0) {
        int i;
        for (i = 0; i < next.player_count; i++) {
            int god = rich4_god_player_type(&next.players[i]);
            if (i != payer && rich4_facility_active_player(&next, i) &&
                (god == 14 || god == 15)) {
                payer = i;
                break;
            }
        }
    }
    company_cash_before = next.company_cash[stock];
    payment = fee == 0 ? GAME_PAYMENT_COMMITTED :
        rich4_ledger_pay(&next, payer, rich4_ledger_company(stock), fee, 0,
                         &staged);
    if (payment != GAME_PAYMENT_COMMITTED && payment != GAME_PAYMENT_BANKRUPTCY)
        return payment;
    if (type == 4 && !rich4_insurance_record_business(
            &next, company, next.company_cash[stock] - company_cash_before))
        return GAME_PAYMENT_REJECTED;
    if (type == 1 && rich4_facility_active_player(&next, payer)) {
        if (rich4_confinement_disappear(&next, payer, outcome, 0, 1,
                &staged) != GAME_CONFINEMENT_APPLIED)
            return GAME_PAYMENT_REJECTED;
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return GAME_PAYMENT_REJECTED;
    result.kind = payment == GAME_PAYMENT_BANKRUPTCY ?
        GAME_LANDING_BANKRUPTCY : GAME_LANDING_CHARGED;
    result.payer = payer; result.amount = fee;
    *state = next;
    if (out != NULL) *out = result;
    return payment;
}

/* 0x41AC47..0x41AC80: coverage is committed before the fee window. */
int rich4_company_prepare_insurance(GameState *state, int actor, int company,
                                    int outcome, GameEventQueue *events)
{
    int fee, stock, days;
    if (events == NULL || !rich4_facility_valid_company(state, company) ||
        !rich4_facility_active_player(state, actor) ||
        state->company_type[company] != 4 ||
        !rich4_company_landing_amount(state, actor, company, outcome, 0, &fee))
        return 0;
    stock = state->company_stock_index[company];
    if (state->stock_company_owner[stock] < 0) return 0;
    days = (state->players[actor].days_assurance + outcome) & 0x7f;
    if (!rich4_game_event_emit_typed_domain(events,
            rich4_game_event_status_changed_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_STATUS, GAME_STATUS_ASSURANCE),
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock), days, 0, 0)) return 0;
    state->players[actor].days_assurance = days;
    return 1;
}

GamePaymentResult rich4_company_resolve_landing(
    GameState *state, int actor, int company, int outcome, int dice_total,
    int accept_free, int redirect_target, GameCompanyLandingResult *out,
    GameEventQueue *events)
{
    return company_landing_stage(state, actor, company, outcome, dice_total,
        accept_free, redirect_target, out, events, 0);
}

GamePaymentResult rich4_company_pay_insurance(
    GameState *state, int actor, int company, int outcome,
    GameCompanyLandingResult *out, GameEventQueue *events)
{
    if (!rich4_facility_valid_company(state, company) ||
        state->company_type[company] != 4) return GAME_PAYMENT_REJECTED;
    return company_landing_stage(state, actor, company, outcome, 0,
        0, -1, out, events, 1);
}

static GamePaymentResult company_construction_stage(
    GameState *state, int actor, int company, GameDomainRef target,
    int facility_type, int accept_free, int redirect_target,
    GameCompanyLandingResult *out, GameEventQueue *events, int stage, int quote)
{
    GameState next;
    GameEventQueue staged;
    GameCompanyLandingResult result;
    GamePaymentResult payment = GAME_PAYMENT_COMMITTED;
    int stock, chairman, payer = actor, fee, built = 0, level = 0;
    int property_owner = -1, old_value = 0, new_value = 0;
    int64_t amount = 1000;
    memset(&result, 0, sizeof(result));
    result.kind = GAME_LANDING_INVALID; result.company = company;
    result.payer = -1; result.chairman = -1;
    if (state == NULL || events == NULL || !rich4_facility_valid_company(state, company) ||
        state->company_type[company] != 11 || !rich4_facility_active_player(state, actor))
        return GAME_PAYMENT_REJECTED;
    stock = state->company_stock_index[company];
    chairman = state->stock_company_owner[stock];
    result.stock = stock; result.chairman = chairman;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (stage != 2 && chairman >= 0 && target.kind == GAME_DOMAIN_LAND && target.id >= 0 &&
        target.id < next.land_count && next.land_type[target.id] == 0 &&
        next.land_level[target.id] < 5) {
        amount = next.land_price[target.id];
        property_owner = next.land_owner[target.id];
        old_value = next.land_value[target.id];
        next.land_level[target.id]++;
        /* 0x41AAF0/0x41AAFC: self-owned construction company calls
         * 0x40B110 again unless the first build reached level five. */
        if (chairman == actor && next.players[actor].is_ai &&
            next.land_level[target.id] < 5)
            next.land_level[target.id]++;
        level = next.land_level[target.id];
        next.land_value[target.id] = next.land_price[target.id] +
            next.land_level[target.id] * next.land_price_per_level[target.id];
        new_value = next.land_value[target.id];
        built = 1;
    } else if (stage != 2 && chairman >= 0 && target.kind == GAME_DOMAIN_FACILITY && target.id >= 0 &&
               target.id < next.facility_count) {
        int old_level = next.facility_level[target.id];
        int old_type = next.facility_type[target.id];
        property_owner = next.facility_owner[target.id];
        old_value = next.facility_value[target.id];
        if (old_level == 0 && facility_type >= 0 && facility_type <= 4) {
            next.facility_type[target.id] = facility_type;
            next.facility_level[target.id] = 1;
            built = 1;
        } else if (old_level > 0 && old_type >= 0 && old_type <= 4 &&
                   old_level < rich4_facility_max_level[old_type]) {
            next.facility_level[target.id]++;
            built = 1;
        }
        if (built) {
            if (chairman == actor && next.players[actor].is_ai &&
                next.facility_level[target.id] <
                    rich4_facility_max_level[next.facility_type[target.id]])
                next.facility_level[target.id]++;
            amount = next.facility_price[target.id];
            level = next.facility_level[target.id];
            if (!rich4_facility_asset_value(&next, target.id,
                                            &next.facility_value[target.id]))
                return GAME_PAYMENT_REJECTED;
            new_value = next.facility_value[target.id];
        }
    }
    if (built && property_owner >= 0 && property_owner < next.player_count) {
        int64_t assets = (int64_t)next.fixed_asset_value[property_owner] -
                         old_value + new_value;
        if (assets < 0 || assets > INT_MAX) return GAME_PAYMENT_REJECTED;
        next.fixed_asset_value[property_owner] = (int)assets;
    }
    if (built && !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_property_level_changed_from_fields,
            actor, target,
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock), level,
            target.kind == GAME_DOMAIN_FACILITY ?
                next.facility_value[target.id] : next.land_value[target.id],
            target.kind == GAME_DOMAIN_FACILITY ?
                (unsigned int)next.facility_type[target.id] : 0))
        return GAME_PAYMENT_REJECTED;
    amount = stage == 2 ? quote : amount * next.price_index;
    if (!rich4_facility_bounded_value(amount, &fee) ||
        (stage != 1 && !rich4_ledger_adjust_fee(fee,
             rich4_god_player_type(&next.players[actor]), &fee)))
        return GAME_PAYMENT_REJECTED;
    if (stage != 1 && rich4_facility_active_player(&next, chairman) && chairman != actor) {
        if (!rich4_card_resolve_landing_charge(&next, actor, chairman, fee, 1, accept_free,
                                       redirect_target, &payer, &fee, &staged))
            return GAME_PAYMENT_REJECTED;
        payment = fee == 0 ? GAME_PAYMENT_COMMITTED :
            rich4_ledger_pay(&next, payer, rich4_ledger_company(stock), fee, 0,
                             &staged);
        if (payment != GAME_PAYMENT_COMMITTED &&
            payment != GAME_PAYMENT_BANKRUPTCY) return payment;
    } else if (stage != 1) fee = 0;
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return GAME_PAYMENT_REJECTED;
    result.kind = payment == GAME_PAYMENT_BANKRUPTCY ?
        GAME_LANDING_BANKRUPTCY : built ? GAME_LANDING_APPLIED :
        (fee != 0 ? GAME_LANDING_CHARGED : GAME_LANDING_NONE);
    if (rich4_facility_active_player(&next, chairman) && chairman != actor &&
        payment == GAME_PAYMENT_COMMITTED) result.kind = GAME_LANDING_CHARGED;
    result.payer = payer; result.amount = fee;
    *state = next;
    if (out != NULL) *out = result;
    return payment;
}

/* 0x41AD86 builds before the quote/0x41D709; resumption must not build twice. */
GamePaymentResult rich4_company_prepare_construction(
    GameState *state, int actor, int company, GameDomainRef target,
    int facility_type, GameCompanyLandingResult *out, GameEventQueue *events)
{
    return company_construction_stage(state, actor, company, target,
        facility_type, 0, -1, out, events, 1, 0);
}

GamePaymentResult rich4_company_pay_construction(
    GameState *state, int actor, int company, int quote,
    GameCompanyLandingResult *out, GameEventQueue *events)
{
    return company_construction_stage(state, actor, company,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, 0, -1, out, events, 2, quote);
}

GamePaymentResult rich4_company_resolve_construction(
    GameState *state, int actor, int company, GameDomainRef target,
    int facility_type, int accept_free, int redirect_target,
    GameCompanyLandingResult *out, GameEventQueue *events)
{
    return company_construction_stage(state, actor, company, target,
        facility_type, accept_free, redirect_target, out, events, 0, 0);
}

int rich4_company_max_issue_purchase(const GameState *state, int actor,
                                     int company, int *out_price,
                                     int *out_maximum)
{
    int stock, price, maximum;
    if (!rich4_facility_valid_company(state, company) || !rich4_facility_active_player(state, actor) ||
        out_price == NULL || out_maximum == NULL) return 0;
    stock = state->company_stock_index[company];
    /* 0x41D1C7: exchange listing/trading flags do not gate an issue. */
    if (state->players[actor].days_sleep_walking != 0) return 0;
    price = (int)(state->stock_company_value[stock] / 10000u);
    if (price <= 0) return 0;
    maximum = state->players[actor].cash / price;
    if (maximum > 1000) maximum = 1000;
    if ((unsigned int)maximum > state->stock_company_available[stock])
        maximum = (int)state->stock_company_available[stock];
    *out_price = price; *out_maximum = maximum;
    return 1;
}

int rich4_company_buy_issued_stock(GameState *state, int actor, int company,
                                   int quantity, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int stock, price, maximum, old_amount, old_owner;
    int64_t total;
    double average;
    if (state == NULL || events == NULL || quantity <= 0 ||
        !rich4_company_max_issue_purchase(state, actor, company, &price, &maximum) ||
        quantity > maximum) return 0;
    stock = state->company_stock_index[company];
    total = (int64_t)price * quantity;
    if (total > INT_MAX) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    old_amount = next.player_stock_amount[actor][stock];
    old_owner = next.stock_company_owner[stock];
    next.players[actor].cash -= (int)total;
    average = old_amount == 0 ? price :
        ((double)next.player_stock_average_cost[actor][stock] * old_amount +
         (double)price * quantity) / (old_amount + quantity);
    if (old_amount > INT_MAX - quantity) return 0;
    next.player_stock_amount[actor][stock] = old_amount + quantity;
    next.player_stock_average_cost[actor][stock] = (float)average;
    next.stock_company_available[stock] -= (unsigned int)quantity;
    rich4_stock_refresh_company_ownership(&next, stock, actor);
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_money_transferred_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, actor),
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock), -(int)total,
            quantity, 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), quantity,
            next.player_stock_amount[actor][stock], 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_market_changed_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock),
            rich4_game_domain_ref(GAME_DOMAIN_COMPANY, company), old_owner,
            next.stock_company_owner[stock], 0) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    return 1;
}
