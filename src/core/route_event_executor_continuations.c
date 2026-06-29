#include "core/route_event_executor_internal.h"
#include <limits.h>
#include "core/game_event_payload.h"

RouteEventExecutionResult rich4_route_event_executor_finish_bank_rejection(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    const RouteEvent *event;
    RouteEventExecutor next;
    GameEventQueue staged;
    (void)state;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_action != 14)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL || event->snapshot.route_action != 14 ||
        (event->phase != ROUTE_EVENT_PHASE_PASS &&
         event->phase != ROUTE_EVENT_PHASE_STOP))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next = *executor;
    next.state.waiting_for_presentation = 0;
    next.state.presentation_action = 0;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_route_event_executor_complete_events(
            &next, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

RouteEventExecutionResult rich4_route_event_executor_finish_god_trade_rejection(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    const RouteEvent *event;
    RouteEventObjectFamily family;
    RouteEventExecutor next;
    GameEventQueue staged;

    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_action !=
            GAME_ROUTE_EVENT_PRESENTATION_GOD_TRADE_REJECTION)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    family = event == NULL ? ROUTE_EVENT_FAMILY_UNKNOWN :
        rich4_route_event_family_from_raw_id(event->object_ref.raw_object_id);
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        (family != ROUTE_EVENT_FAMILY_LAND &&
         family != ROUTE_EVENT_FAMILY_FACILITY))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next = *executor;
    next.state.waiting_for_presentation = 0;
    next.state.presentation_action = 0;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_route_event_executor_complete_events(
            &next, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

RouteEventExecutionResult rich4_route_event_executor_resume_company_roulette(
    RouteEventExecutor *executor, GameState *state, int outcome,
    GameEventQueue *events)
{
    const RouteEvent *event;
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    GamePaymentResult payment;
    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_mode < 0 ||
        executor->state.presentation_mode > 3)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_state = *state;
    next_queue = *executor;
    rich4_game_event_queue_reset(&staged);
    if (executor->state.presentation_company >= 0) {
        int company = executor->state.presentation_company;
        GameCompanyLandingResult landing;
        if (event->snapshot.route_action != 0 ||
            company >= state->company_count)
            return ROUTE_EVENT_EXECUTION_INVALID;
        {
            int notice = request_god_company_charge(executor, state, company, outcome,
                event->snapshot.dice_total, events);
            if (notice) return notice > 0 ? ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                                          ROUTE_EVENT_EXECUTION_INVALID;
        }
        payment = rich4_company_resolve_landing(
            &next_state, executor->state.execution_actor, company, outcome,
            event->snapshot.dice_total, 0, -1, &landing, &staged);
        if (payment == GAME_PAYMENT_REJECTED)
            return ROUTE_EVENT_EXECUTION_INVALID;
        {
            RouteEventExecutionResult result = finish_company_landing(
                &next_queue, &next_state, company, payment,
                &staged, events);
            if (result == ROUTE_EVENT_EXECUTION_INVALID)
                return result;
            *state = next_state;
            *executor = next_queue;
            return result;
        }
    }
    if (executor->state.presentation_land >= 0) {
        int facility = executor->state.presentation_land;
        if (facility >= state->facility_count || outcome < 1 ||
            (state->facility_type[facility] != 1 && state->facility_type[facility] != 2) ||
            outcome > (state->facility_type[facility] == 1 ? 4 : 6))
            return ROUTE_EVENT_EXECUTION_INVALID;
        {
            int actor = executor->state.execution_actor;
            int god = rich4_god_player_type(&state->players[actor]);
            int adjusted;
            int64_t base = (int64_t)state->facility_charge_by_level[facility][
                state->facility_level[facility]] * state->price_index * outcome;
            if (state->facility_price_status[facility]) base *= 2;
            if (base < 0 || base > INT_MAX ||
                !rich4_ledger_adjust_fee((int)base, god, &adjusted))
                return ROUTE_EVENT_EXECUTION_INVALID;
            /* 0x41A478/0x41A56F: every hotel result is quoted before payment. */
            if (!rich4_game_event_emit_typed_domain(events,
                    rich4_game_event_property_charge_presentation_requested_from_fields,
                    actor, rich4_game_domain_ref(GAME_DOMAIN_FACILITY, facility),
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, state->facility_owner[facility]),
                    adjusted, 0, executor->state.turn_token))
                return ROUTE_EVENT_EXECUTION_INVALID;
            executor->state.presentation_mode = 10 + outcome;
            return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
        }

    }
    return ROUTE_EVENT_EXECUTION_INVALID;
}

RouteEventExecutionResult rich4_route_event_executor_buy_company_issue(
    RouteEventExecutor *executor, GameState *state, int quantity,
    GameEventQueue *events)
{
    const RouteEvent *event;
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    int company;
    if (executor == NULL || state == NULL || events == NULL || quantity < 0 ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_mode != 4 ||
        executor->state.presentation_company < 0)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    company = executor->state.presentation_company;
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->snapshot.route_action != 0 ||
        company >= state->company_count)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_state = *state;
    next_queue = *executor;
    rich4_game_event_queue_reset(&staged);
    if (quantity > 0 &&
        !rich4_company_buy_issued_stock(
            &next_state, executor->state.execution_actor, company,
            quantity, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_company = -1;
    next_queue.state.presentation_mode = -1;
    if (!rich4_route_event_executor_complete_events(
            &next_queue,
            quantity > 0 ? ROUTE_EVENT_APPLIED :
                           ROUTE_EVENT_DECISION_DECLINED,
            &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

static RouteEventExecutionResult rich4_route_event_executor_resume_magic_house_impl(
    RouteEventExecutor *executor, GameState *state, int action,
    GameEventQueue *events, int defer_confinement)
{
    const RouteEvent *event;
    GameState next_state;
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    if (executor == NULL || state == NULL || events == NULL ||
        action < 0 || action >= 12 ||
        !executor->state.execution_started ||
        !executor->state.waiting_for_presentation ||
        executor->state.presentation_action != 16 ||
        executor->state.presentation_condition < 0 ||
        executor->state.presentation_condition >= 12 ||
        executor->state.presentation_targets == 0)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL || event->phase != ROUTE_EVENT_PHASE_STOP ||
        event->snapshot.route_action != 16)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_state = *state;
    next_queue = *executor;
    rich4_game_event_queue_reset(&staged);
    if (action == 1) {
        int started;
        next_queue.state.waiting_for_presentation = 0;
        next_queue.state.presentation_action = 0;
        next_queue.state.presentation_condition = -1;
        next_queue.state.presentation_targets = 0;
        next_queue.state.magic_fortune_sequence = 1;
        next_queue.state.magic_fortune_target = -1;
        next_queue.state.magic_fortunes_left = 0;
        next_queue.state.magic_fortune_targets_left =
            executor->state.presentation_targets;
        started=begin_next_magic_fortune(
            &next_queue,&next_state,&staged);
        if (started < 0)
            return ROUTE_EVENT_EXECUTION_INVALID;
        if (started == 0) {
            if (!rich4_route_event_executor_complete_events(
                    &next_queue,ROUTE_EVENT_APPLIED,&staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
        }
        if (events->count+staged.count>GAME_EVENT_QUEUE_CAPACITY ||
            !rich4_game_event_queue_append(events,&staged))
            return ROUTE_EVENT_EXECUTION_INVALID;
        *state=next_state;
        *executor=next_queue;
        return started ?
            ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
            ROUTE_EVENT_EXECUTION_PROGRESSED;
    }
    if ((defer_confinement ? rich4_magic_house_apply_action_deferred_confinement : rich4_magic_house_apply_action)(
            &next_state, executor->state.execution_actor,
            executor->state.presentation_condition, action,
            executor->state.presentation_targets, &staged) !=
            GAME_SPECIAL_EVENT_APPLIED)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    next_queue.state.presentation_condition = -1;
    next_queue.state.presentation_targets = 0;
    if (!rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}





RouteEventExecutionResult rich4_route_event_executor_resume_magic_house(RouteEventExecutor *executor, GameState *state, int action, GameEventQueue *events)
{
    return rich4_route_event_executor_resume_magic_house_impl(executor, state, action, events, 0);
}

RouteEventExecutionResult rich4_route_event_executor_resume_magic_house_deferred_confinement(RouteEventExecutor *executor, GameState *state, int action, GameEventQueue *events)
{
    return rich4_route_event_executor_resume_magic_house_impl(executor, state, action, events, 1);
}
