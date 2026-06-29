#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

RouteEventExecutionResult request_facility_roulette(
    RouteEventExecutor *executor, GameDomainRef subject, int mode,
    GameEventQueue *events)
{
    RouteEventExecutor next;
    GameEvent request;
    if (executor == NULL || events == NULL || mode < 0 || mode > 3 ||
        (subject.kind != GAME_DOMAIN_FACILITY &&
         subject.kind != GAME_DOMAIN_COMPANY))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next = *executor;
    next.state.waiting_for_presentation = 1;
    next.state.presentation_action = 0;
    next.state.presentation_mode = mode;
    next.state.presentation_land =
        subject.kind == GAME_DOMAIN_FACILITY ? subject.id : -1;
    next.state.presentation_company =
        subject.kind == GAME_DOMAIN_COMPANY ? subject.id : -1;
    next.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    request = rich4_game_event_facility_roulette_request(
        executor->state.execution_actor, subject, mode, 68 + mode, 517,
        executor->state.turn_token);
    if (!rich4_game_event_queue_push(events, &request))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next;
    return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
}

static int company_issue_quantity_ai(
    const GameState *state, int actor, int company)
{
    int price;
    int maximum;
    int reserve;
    int spendable;
    int quantity;
    if (!rich4_company_max_issue_purchase(
            state, actor, company, &price, &maximum) ||
        maximum <= 0 || state->players[actor].days_sleep_walking != 0 ||
        state->price_index <= 0)
        return 0;
    /*
     * 0x41D839 reserves trunc(initial_fund * 0.3) * price_index cash,
     * then buys min(maximum, spendable / price).
     */
    reserve = (int)((double)state->initial_fund * 0.3);
    if (reserve > INT_MAX / state->price_index)
        return 0;
    reserve *= state->price_index;
    spendable = state->players[actor].cash - reserve;
    if (spendable <= 0) return 0;
    quantity = spendable / price;
    return quantity < maximum ? quantity : maximum;
}

RouteEventExecutionResult finish_company_landing(
    RouteEventExecutor *executor, GameState *state, int company,
    GamePaymentResult payment, GameEventQueue *staged,
    GameEventQueue *events)
{
    const RouteEvent *event;
    RouteEventExecutor next_queue;
    int actor;
    int price;
    int maximum;
    int quantity;
    if (executor == NULL || state == NULL || staged == NULL || events == NULL)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL) return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue = *executor;
    actor = executor->state.execution_actor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_land = -1;
    next_queue.state.presentation_company = -1;
    next_queue.state.presentation_mode = -1;
    if (payment == GAME_PAYMENT_LIQUIDATION) {
        next_queue.state.waiting_for_liquidation = 1;
        next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    } else if (payment == GAME_PAYMENT_BANKRUPTCY) {
        if (!emit_actor_eliminated(&next_queue, event, staged))
            return ROUTE_EVENT_EXECUTION_INVALID;
    } else if (state->players[actor].days_sleep_walking == 0 &&
               rich4_company_max_issue_purchase(
                   state, actor, company, &price, &maximum) &&
               maximum > 0) {
        if (state->players[actor].is_ai || (state->players[actor].play_state & 4)) {
            quantity = company_issue_quantity_ai(state, actor, company);
            if (quantity > 0 &&
                !rich4_company_buy_issued_stock(
                    state, actor, company, quantity, staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
            if (!rich4_route_event_executor_complete_events(
                    &next_queue, ROUTE_EVENT_APPLIED, staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
        } else {
            GameEvent request;
            request = rich4_game_event_company_stock_issue_request(
                actor, company,
                rich4_game_domain_ref(
                    GAME_DOMAIN_STOCK, state->company_stock_index[company]),
                price, maximum, executor->state.turn_token);
            next_queue.state.waiting_for_presentation = 1;
            next_queue.state.presentation_company = company;
            next_queue.state.presentation_mode = 4;
            next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
            if (!rich4_game_event_queue_push(staged, &request))
                return ROUTE_EVENT_EXECUTION_INVALID;
        }
    } else if (!rich4_route_event_executor_complete_events(
                   &next_queue, ROUTE_EVENT_APPLIED, staged)) {
        return ROUTE_EVENT_EXECUTION_INVALID;
    }
    if (events->count + staged->count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next_queue;
    if (payment == GAME_PAYMENT_LIQUIDATION ||
        (next_queue.state.waiting_for_presentation &&
         next_queue.state.presentation_mode == 4))
        return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
    return payment == GAME_PAYMENT_BANKRUPTCY ?
        ROUTE_EVENT_EXECUTION_FINISHED : ROUTE_EVENT_EXECUTION_PROGRESSED;
}


/* 0x41AEB4 -> 0x41AEC5: quote, god notice, then the payment.  Return
 * zero for an unchanged quote; -1 preserves the transaction on capacity error. */
int request_god_company_charge(RouteEventExecutor *executor, GameState *state,
    int company, int outcome, int dice_total, GameEventQueue *events)
{
    int actor = executor->state.execution_actor;
    int type = state->company_type[company];
    int god = type == 4 ? (state->players[actor].god_info & 0xff) :
        rich4_god_player_type(&state->players[actor]);
    int stock = state->company_stock_index[company];
    int owner;
    int base, amount;
    GameState next;
    GameEventQueue staged;
    if (stock < 0 || stock >= GAME_STOCK_COUNT) return -1;
    owner = state->stock_company_owner[stock];
    if ((type != 4 && god != 1 && god != 2 && god != 5 && god != 6) || owner < 0 || owner == actor ||
        type == 2 || type == 7 || type == 10 ||
        !rich4_company_landing_amount(state, actor, company, outcome, dice_total, &base)) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (type == 11) {
        GameCompanyLandingResult landing;
        GameEventQueue built;
        GameEvent event;
        int building = 0;
        GameDomainRef target = choose_construction_target(&next, actor, &building);
        rich4_game_event_queue_reset(&built);
        if (rich4_company_prepare_construction(&next, actor, company, target,
                building, &landing, &built) != GAME_PAYMENT_COMMITTED) return -1;
        base = landing.amount;
        while (rich4_game_event_queue_pop(&built, &event)) {
            if (event.type == GAME_EVENT_PROPERTY_LEVEL_CHANGED) {
                if (!rich4_game_event_emit_typed_domain(&staged,
                        rich4_game_event_tool_presentation_requested_from_fields,
                        actor, rich4_game_domain_ref(GAME_DOMAIN_GOD, god), target, 553, 91, 0)) return -1;
                if (rich4_game_event_property_level(&event) == 5 &&
                    !rich4_game_event_emit_typed_domain(&staged,
                        rich4_game_event_tool_presentation_requested_from_fields,
                        actor, rich4_game_domain_ref(GAME_DOMAIN_GOD, god), target, 523, 90, 0)) return -1;
            }
        }
    }
    if (!rich4_ledger_adjust_fee(base, god, &amount)) return -1;
    if (type == 4 && base == 0) return 0; /* 0x41AE37 skips a zero quote. */
    if (base == amount && type != 11 && type != 4) return 0;
    if (type == 4 && !rich4_company_prepare_insurance(
            &next, actor, company, outcome, &staged)) return -1;
    if (!rich4_game_event_emit_typed_domain(&staged,
            rich4_game_event_property_charge_presentation_requested_from_fields,
            actor, rich4_game_domain_ref(GAME_DOMAIN_COMPANY, company),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, owner), amount, 0,
            executor->state.turn_token) ||
        !rich4_game_event_queue_append(events, &staged)) return -1;
    *state = next;
    executor->state.presentation_land = base;
    executor->state.presentation_company = company;
    executor->state.presentation_mode = 20 + outcome;
    executor->state.waiting_for_presentation = 1;
    return 1;
}
