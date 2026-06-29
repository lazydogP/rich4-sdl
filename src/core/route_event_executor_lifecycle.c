#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

void rich4_route_event_executor_reset_dispatch(RouteEventExecutor *executor)
{
    if (executor == NULL) return;
    memset(&executor->state.dispatch, 0, sizeof(executor->state.dispatch));
    executor->state.dispatch.phase = GAME_ROUTE_EVENT_DISPATCH_NONE;
    executor->state.dispatch.index = -1;
    executor->state.dispatch.next_phase = GAME_ROUTE_EVENT_DISPATCH_NONE;
    executor->state.dispatch.next_index = -1;
}

void rich4_route_event_executor_reset(RouteEventExecutor *executor)
{
    if (executor == NULL) return;
    memset(executor, 0, sizeof(*executor));
    rich4_route_event_executor_reset_dispatch(executor);
}

void clear_pending(RouteEventExecutor *executor)
{
    memset(&executor->state.pending_accept, 0,
           sizeof(executor->state.pending_accept));
    memset(&executor->state.pending_decline, 0,
           sizeof(executor->state.pending_decline));
    executor->state.pending_accept.subject.id = -1;
    executor->state.pending_accept.target.id = -1;
    executor->state.pending_decline.subject.id = -1;
    executor->state.pending_decline.target.id = -1;
    executor->state.waiting_for_command = 0;
    executor->state.waiting_for_liquidation = 0;
    executor->state.waiting_for_presentation = 0;
    executor->state.presentation_land = -1;
    executor->state.presentation_action = 0;
    executor->state.presentation_company = -1;
    executor->state.presentation_mode = -1;
    executor->state.presentation_condition = -1;
    executor->state.presentation_targets = 0;
    executor->state.lottery_purchase_committed = 0;
}

int rich4_route_event_executor_begin(RouteEventExecutor *executor,
                                     int actor,
                                     unsigned int token)
{
    const RouteEvent *event;
    if (executor == NULL || executor->queue.count == 0 ||
        executor->state.execution_started ||
        actor < 0 || actor >= GAME_MAX_PLAYERS || token == 0) return 0;
    event = rich4_route_event_executor_current(executor);
    if (event == NULL) return 0;
    executor->state.execution_started = 1;
    executor->state.execution_actor = actor;
    executor->state.turn_token = token;
    clear_pending(executor);
    executor->state.beggar_checked = 0;
    executor->state.magic_fortune_sequence = 0;
    executor->state.magic_fortune_target = -1;
    executor->state.magic_fortunes_left = 0;
    executor->state.magic_fortune_targets_left = 0;
    return 1;
}

int emit_actor_eliminated(RouteEventExecutor *executor,
                                 const RouteEvent *event,
                                 GameEventQueue *events)
{
    if (!rich4_game_event_emit_route_event_resolved(
            events, executor->state.execution_actor, event->phase,
            ROUTE_EVENT_ACTOR_ELIMINATED, event->node,
            event->object_ref.raw_object_id)) return 0;
    executor->state.last_result = ROUTE_EVENT_ACTOR_ELIMINATED;
    rich4_route_event_queue_reset(&executor->queue);
    clear_pending(executor);
    return 1;
}



