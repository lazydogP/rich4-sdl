#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"
#include "core/insurance.h"

RouteEventExecutionResult rich4_route_event_executor_settle_insurance(
    RouteEventExecutor *executor, GameState *state, int payer, int amount,
    GameEventQueue *events)
{
    RouteEventExecutor next_queue;
    GameState next;
    GameEventQueue staged;
    GamePaymentResult payment;
    RouteEventExecutionResult result;
    int company, stock, received;
    if (executor == NULL || state == NULL || events == NULL || amount < 0 ||
        !executor->state.execution_started || !executor->state.waiting_for_presentation ||
        executor->state.presentation_mode < 20 ||
        payer < 0 || payer >= state->player_count ||
        (state->players[payer].play_state & 0xff) == 0) return ROUTE_EVENT_EXECUTION_INVALID;
    company = executor->state.presentation_company;
    if (company < 0 || company >= state->company_count ||
        state->company_type[company] != 4) return ROUTE_EVENT_EXECUTION_INVALID;
    stock = state->company_stock_index[company];
    if (stock < 0 || stock >= GAME_STOCK_COUNT) return ROUTE_EVENT_EXECUTION_INVALID;
    next = *state;
    next_queue = *executor;
    rich4_game_event_queue_reset(&staged);
    received = next.company_cash[stock];
    payment = amount == 0 ? GAME_PAYMENT_COMMITTED : rich4_ledger_pay(
        &next, payer, rich4_ledger_company(stock), amount, 0, &staged);
    if (payment != GAME_PAYMENT_COMMITTED && payment != GAME_PAYMENT_BANKRUPTCY)
        return ROUTE_EVENT_EXECUTION_INVALID;
    received = next.company_cash[stock] - received;
    if (!rich4_insurance_record_business(&next, company, received))
        return ROUTE_EVENT_EXECUTION_INVALID;
    /* A substitute's bankruptcy does not eliminate the landing actor. */
    if (payment == GAME_PAYMENT_BANKRUPTCY && payer != executor->state.execution_actor)
        payment = GAME_PAYMENT_COMMITTED;
    result = finish_company_landing(&next_queue, &next, company, payment, &staged, events);
    if (result != ROUTE_EVENT_EXECUTION_INVALID) { *state = next; *executor = next_queue; }
    return result;
}

int rich4_route_event_executor_is_waiting(const RouteEventExecutor *executor)
{
    return executor != NULL && executor->state.execution_started &&
           (executor->state.waiting_for_command ||
            executor->state.waiting_for_liquidation ||
            executor->state.waiting_for_presentation);
}

RouteEventExecutionResult rich4_route_event_executor_resume_charge(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    const RouteEvent *event;
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    GamePaymentResult payment;
    RouteEventObjectFamily family;
    int property_id;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    property_id = executor->state.presentation_land;
    family = event == NULL ? ROUTE_EVENT_FAMILY_UNKNOWN :
        rich4_route_event_family_from_raw_id(event->object_ref.raw_object_id);
    if (event != NULL && executor->state.presentation_mode >= 20 &&
        executor->state.presentation_company >= 0) {
        int company = executor->state.presentation_company;
        int outcome = executor->state.presentation_mode - 20;
        GameCompanyLandingResult landing;
        RouteEventExecutionResult result;
        next_state = *state;
        next_queue = *executor;
        rich4_game_event_queue_reset(&staged);
        if (state->company_type[company] == 11) {
            payment = rich4_company_pay_construction(&next_state,
                executor->state.execution_actor, company, executor->state.presentation_land,
                &landing, &staged);
        } else if (state->company_type[company] == 4) {
            payment = rich4_company_pay_insurance(&next_state,
                executor->state.execution_actor, company, outcome, &landing, &staged);
        } else payment = rich4_company_resolve_landing(&next_state,
            executor->state.execution_actor, company, outcome, event->snapshot.dice_total,
            0, -1, &landing, &staged);
        if (payment == GAME_PAYMENT_REJECTED) return ROUTE_EVENT_EXECUTION_INVALID;
        result = finish_company_landing(&next_queue, &next_state, company, payment, &staged, events);
        if (result != ROUTE_EVENT_EXECUTION_INVALID) { *state = next_state; *executor = next_queue; }
        return result;
    }
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        property_id < 0 ||
        (family != ROUTE_EVENT_FAMILY_LAND &&
         family != ROUTE_EVENT_FAMILY_FACILITY))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_state = *state;
    next_queue = *executor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_land = -1;
    rich4_game_event_queue_reset(&staged);
    if (family == ROUTE_EVENT_FAMILY_LAND)
        payment = rich4_land_charge(
            &next_state, executor->state.execution_actor, property_id,
            NULL, &staged);
    else {
        GameFacilityLandingResult landing;
        payment = rich4_facility_resolve_landing(
            &next_state, executor->state.execution_actor, property_id,
            executor->state.presentation_mode >= 10 ? executor->state.presentation_mode - 10 : 0,
            event->snapshot.dice_total, 0, -1, &landing, &staged);
    }
    if (payment == GAME_PAYMENT_REJECTED)
        return ROUTE_EVENT_EXECUTION_INVALID;
    if (payment == GAME_PAYMENT_LIQUIDATION) {
        next_queue.state.waiting_for_liquidation = 1;
        next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    } else if (payment == GAME_PAYMENT_BANKRUPTCY) {
        if (!emit_actor_eliminated(&next_queue, event, &staged))
            return ROUTE_EVENT_EXECUTION_INVALID;
    } else if (!rich4_route_event_executor_complete_events(
                   &next_queue, ROUTE_EVENT_APPLIED, &staged)) {
        return ROUTE_EVENT_EXECUTION_INVALID;
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return payment == GAME_PAYMENT_LIQUIDATION ?
        ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
        (payment == GAME_PAYMENT_BANKRUPTCY ? ROUTE_EVENT_EXECUTION_FINISHED :
                                             ROUTE_EVENT_EXECUTION_PROGRESSED);
}

RouteEventExecutionResult rich4_route_event_executor_resume_beggar(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    const RouteEvent *event;
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    GamePaymentResult payment;
    int beggar;
    int node;
    int amount;

    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_action !=
            GAME_ROUTE_EVENT_ACTION_BEGGAR)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    beggar = executor->state.presentation_company;
    node = executor->state.presentation_land;
    amount = executor->state.presentation_mode;
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->node != node || beggar < 0 || amount <= 0)
        return ROUTE_EVENT_EXECUTION_INVALID;

    next_state = *state;
    next_queue = *executor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    next_queue.state.presentation_land = -1;
    next_queue.state.presentation_company = -1;
    next_queue.state.presentation_mode = -1;
    rich4_game_event_queue_reset(&staged);
    payment = rich4_ledger_beggar_donate(
        &next_state, executor->state.execution_actor, beggar, node, amount,
        &staged);
    if (payment == GAME_PAYMENT_REJECTED)
        return ROUTE_EVENT_EXECUTION_INVALID;
    if (payment == GAME_PAYMENT_BANKRUPTCY) {
        if (!emit_actor_eliminated(&next_queue, event, &staged))
            return ROUTE_EVENT_EXECUTION_INVALID;
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return payment == GAME_PAYMENT_BANKRUPTCY
        ? ROUTE_EVENT_EXECUTION_FINISHED
        : ROUTE_EVENT_EXECUTION_PROGRESSED;
}

RouteEventExecutionResult rich4_route_event_executor_resume_special_place(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    const RouteEvent *event;
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    int action;
    int applied;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    action = executor->state.presentation_action;
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->snapshot.route_action != action ||
        action < 10 || action > 13)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_state = *state;
    next_queue = *executor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    rich4_game_event_queue_reset(&staged);
    if (action == 13)
        applied = gain_weighted_random_card(
            &next_state, executor->state.execution_actor, NULL, &staged);
    else
        applied = apply_points_reward(
            &next_state, executor->state.execution_actor,
            action == 10 ? 50 : action == 11 ? 30 : 10, &staged);
    if (!applied ||
        !rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

RouteEventExecutionResult rich4_route_event_executor_resume_confinement(
    RouteEventExecutor *executor, GameState *state, GameDomainRef target,
    GameEventQueue *events)
{
    const RouteEvent *event;
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    int action;
    int kind;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    action = executor->state.presentation_action;
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->snapshot.route_action != action ||
        (action != 4 && action != 5))
        return ROUTE_EVENT_EXECUTION_INVALID;
    kind = action == 4 ? GAME_CONFINEMENT_PRISON :
                         GAME_CONFINEMENT_HOSPITAL;
    next_state = *state;
    next_queue = *executor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    rich4_game_event_queue_reset(&staged);
    if (rich4_confinement_visit_release(
            &next_state, executor->state.execution_actor, target, kind, &staged) !=
            GAME_CONFINEMENT_APPLIED ||
        !rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

RouteEventExecutionResult rich4_route_event_executor_cancel_confinement(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    const RouteEvent *event;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    int action;
    (void)state;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    action = executor->state.presentation_action;
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->snapshot.route_action != action ||
        (action != 4 && action != 5))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue = *executor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_DECISION_DECLINED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

RouteEventExecutionResult rich4_route_event_executor_purchase_lottery(
    RouteEventExecutor *executor, GameState *state, int number,
    GameEventQueue *events)
{
    const RouteEvent *event;
    GameState next_state;
    GameEventQueue staged;
    int player;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_action != 9 ||
        executor->state.lottery_purchase_committed ||
        number < 0 || number >= 36)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->snapshot.route_action != 9)
        return ROUTE_EVENT_EXECUTION_INVALID;
    /* 0x42FF1D: occupied numbers are ignored without closing the modal. */
    if (state->lottery_owner[number] != 0)
        return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
    player = executor->state.execution_actor;
    next_state = *state;
    rich4_game_event_queue_reset(&staged);
    next_state.lottery_owner[number] = (uint8_t)(player + 1);
    /* The callback deliberately does not repeat its entrance cash check. */
    next_state.players[player].cash = (int32_t)(
        (uint32_t)next_state.players[player].cash - 1000u);
    next_state.lottery_pool = (int32_t)(
        (uint32_t)next_state.lottery_pool + 1000u);
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_player_cash_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            next_state.players[player].cash, -1000, 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            number, next_state.lottery_pool, 0) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    executor->state.lottery_purchase_committed = 1;
    return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
}

RouteEventExecutionResult rich4_route_event_executor_finish_lottery(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    const RouteEvent *event;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_action != 9)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->snapshot.route_action != 9)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue = *executor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    next_queue.state.lottery_purchase_committed = 0;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

int presentation_matches(const RouteEventExecutor *executor, int action)
{
    const RouteEvent *event;
    if (executor == NULL || !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_action != action)
        return 0;
    event = rich4_route_event_executor_current(executor);
    return event != NULL && event->phase == ROUTE_EVENT_PHASE_STOP &&
           event->snapshot.route_action == action;
}

RouteEventExecutionResult rich4_route_event_executor_finish_point_minigame(
    RouteEventExecutor *executor, GameState *state, int points,
    GameEventQueue *events)
{
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    int action;
    if (executor == NULL || state == NULL || events == NULL || points < 0)
        return ROUTE_EVENT_EXECUTION_INVALID;
    action = executor->state.presentation_action;
    if (action < 6 || action > 8 ||
        !presentation_matches(executor, action))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_state = *state;
    next_queue = *executor;
    rich4_game_event_queue_reset(&staged);
    if (!apply_points_reward(
            &next_state, executor->state.execution_actor, points, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    if (!rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}




