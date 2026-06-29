#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

const RouteEvent *rich4_route_event_executor_current(
    const RouteEventExecutor *executor)
{
    return executor == NULL ? NULL :
        rich4_route_event_queue_peek(&executor->queue);
}

int rich4_route_event_executor_complete_events(
    RouteEventExecutor *executor,
    RouteEventHandlerResult result,
    GameEventQueue *events)
{
    const RouteEvent *event;
    RouteEvent completed;

    if (executor == NULL || events == NULL ||
        result == ROUTE_EVENT_WAITING_FOR_COMMAND ||
        result == ROUTE_EVENT_INVALID_STATE) return 0;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL) return 0;
    completed = *event;
    if (!rich4_game_event_emit_route_event_resolved(
            events, completed.snapshot.player, completed.phase, result,
            completed.node, (unsigned int)completed.object_ref.raw_object_id))
        return 0;
    if (!rich4_route_event_queue_pop(&executor->queue, NULL)) return 0;
    executor->state.last_result = result;
    if (completed.phase == ROUTE_EVENT_PHASE_STOP)
        executor->state.beggar_checked = 0;
    return 1;
}

RouteEventExecutionResult rich4_route_event_executor_step(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events)
{
    const RouteEvent *event;
    RouteEventObjectFamily family;
    RouteEventHandlerResult result;

    if (executor == NULL || state == NULL || events == NULL ||
        !executor->state.execution_started || executor->state.execution_actor < 0 ||
        executor->state.execution_actor >= state->player_count)
        return ROUTE_EVENT_EXECUTION_INVALID;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL)
        return ROUTE_EVENT_EXECUTION_FINISHED;
    if ((state->players[executor->state.execution_actor].play_state & 0xff) == 0)
        return emit_actor_eliminated(executor, event, events) ?
            ROUTE_EVENT_EXECUTION_FINISHED : ROUTE_EVENT_EXECUTION_INVALID;
    if (executor->state.waiting_for_command || executor->state.waiting_for_liquidation ||
        executor->state.waiting_for_presentation)
        return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;

    if (event->phase == ROUTE_EVENT_PHASE_STOP &&
        !executor->state.beggar_checked) {
        RouteEventExecutor next_queue = *executor;
        int beggar = rich4_ledger_find_beggar(
            state, executor->state.execution_actor, event->node);
        int amount = rich4_ledger_beggar_amount(state);
        next_queue.state.beggar_checked = 1;
        if (beggar >= 0 && amount > 0) {
            next_queue.state.waiting_for_presentation = 1;
            next_queue.state.presentation_action =
                GAME_ROUTE_EVENT_ACTION_BEGGAR;
            next_queue.state.presentation_land = event->node;
            next_queue.state.presentation_company = beggar;
            next_queue.state.presentation_mode = amount;
            next_queue.state.last_result =
                ROUTE_EVENT_WAITING_FOR_COMMAND;
            if (!rich4_game_event_emit_typed_domain(
                    events,
                    rich4_game_event_beggar_donation_presentation_requested_from_fields,
                    executor->state.execution_actor,
                    rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, beggar),
                    amount, event->node, executor->state.turn_token))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
        }
        *executor = next_queue;
    }

    /* 0x41982D skips non-property action nodes while sleepwalking. */
    if (event->snapshot.route_action != 0 &&
        state->players[executor->state.execution_actor].days_sleep_walking) {
        return rich4_route_event_executor_complete_events(
            executor, ROUTE_EVENT_APPLIED, events) ?
            ROUTE_EVENT_EXECUTION_PROGRESSED : ROUTE_EVENT_EXECUTION_INVALID;
    }

    family = rich4_route_event_family_from_raw_id(event->object_ref.raw_object_id);
    if (event->snapshot.route_action != 0)
        family = ROUTE_EVENT_FAMILY_SPECIAL_TABLE_2;
    if (family == ROUTE_EVENT_FAMILY_LAND ||
        family == ROUTE_EVENT_FAMILY_FACILITY)
        return rich4_route_event_executor_step_property(
            executor, state, events, event, family);
    if (family == ROUTE_EVENT_FAMILY_ROAD)
        result = ROUTE_EVENT_APPLIED;
    else if (family == ROUTE_EVENT_FAMILY_SPECIAL_TABLE_1 ||
             family == ROUTE_EVENT_FAMILY_SPECIAL_TABLE_2)
        return rich4_route_event_executor_step_special(
            executor, state, events, event, family);
    else
        result = ROUTE_EVENT_UNKNOWN_SKIPPED;
    return rich4_route_event_executor_complete_events(executor, result, events) ?
        ROUTE_EVENT_EXECUTION_PROGRESSED : ROUTE_EVENT_EXECUTION_INVALID;
}



