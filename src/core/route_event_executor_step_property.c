#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

RouteEventExecutionResult rich4_route_event_executor_step_property(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events,
    const RouteEvent *event,
    RouteEventObjectFamily family)
{
    RouteEventHandlerResult result = ROUTE_EVENT_UNKNOWN_SKIPPED;
    if (family == ROUTE_EVENT_FAMILY_LAND) {
        GameDomainRef ref;
        GameLandAction action;
        if (event->phase == ROUTE_EVENT_PHASE_PASS) {
            result = ROUTE_EVENT_APPLIED;
        } else {
            int ref_ok = property_domain_ref(event, state, &ref);
            int inspect_ok = ref_ok &&
                rich4_land_inspect(state, executor->state.execution_actor,
                                   ref.id, &action);
            if (!inspect_ok) {
            return ROUTE_EVENT_EXECUTION_INVALID;
        } else if (action.kind == GAME_LAND_ACTION_PURCHASE ||
                   action.kind == GAME_LAND_ACTION_UPGRADE) {
            GameCommandType accept_type = action.kind == GAME_LAND_ACTION_PURCHASE ?
                GAME_COMMAND_PURCHASE_PROPERTY : GAME_COMMAND_UPGRADE_PROPERTY;
            GameCommandType decline_type = action.kind == GAME_LAND_ACTION_PURCHASE ?
                GAME_COMMAND_DECLINE_PROPERTY_PURCHASE :
                GAME_COMMAND_DECLINE_PROPERTY_UPGRADE;
            if (!state->players[executor->state.execution_actor].is_ai)
                return request_property_decision(executor, event, state,
                    accept_type, decline_type, events) ?
                    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                    ROUTE_EVENT_EXECUTION_INVALID;
            else {
                if (rich4_god_blocks_trade(
                        &state->players[executor->state.execution_actor]))
                    return rich4_route_event_executor_begin_god_trade_rejection(
                        executor, state, event, events) ?
                        ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                        ROUTE_EVENT_EXECUTION_INVALID;
                GameState next_state = *state;
                RouteEventExecutor next_queue = *executor;
                GameEventQueue staged;
                GameCommand command = rich4_game_domain_command(
                    accept_type, GAME_COMMAND_SOURCE_AI,
                    executor->state.execution_actor, ref,
                    rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
                rich4_game_event_queue_reset(&staged);
                command.turn_token = executor->state.turn_token;
                if (!rich4_land_apply_command(&next_state, &command, &staged) ||
                    !rich4_route_event_executor_complete_events(
                        &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                    events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY)
                    return ROUTE_EVENT_EXECUTION_INVALID;
                if (!rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state; *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_PROGRESSED;
            }
        } else if (action.kind == GAME_LAND_ACTION_CHARGE) {
            RouteEventExecutor next_queue = *executor;
            next_queue.state.waiting_for_presentation = 1;
            next_queue.state.presentation_land = ref.id;
            next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
            if (!rich4_game_event_emit_typed_domain(
                    events,
                    rich4_game_event_property_charge_presentation_requested_from_fields,
                    executor->state.execution_actor, ref,
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, action.owner),
                    action.amount, 0, executor->state.turn_token))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
        } else if (action.kind == GAME_LAND_ACTION_COLLECTION_BLOCKED) {
            RouteEventExecutor next_queue = *executor;
            GameEventQueue staged;
            rich4_game_event_queue_reset(&staged);
            /* sub_41D559 presents the block reason and the current player's
             * fixed +0x34 line, but does not enter the payment transaction. */
            if (!rich4_game_event_emit_typed_domain(
                &staged,
                rich4_game_event_property_charge_presentation_requested_from_fields,
                executor->state.execution_actor, ref,
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, action.owner),
                    action.amount, action.collection_block_reason,
                    executor->state.turn_token) ||
                !rich4_route_event_executor_complete_events(
                    &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                !rich4_game_event_queue_append(events, &staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_PROGRESSED;
        } else if (action.kind == GAME_LAND_ACTION_DEFERRED) {
            result = ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED;
        } else {
            result = ROUTE_EVENT_APPLIED;
            }
        }
    } else if (family == ROUTE_EVENT_FAMILY_FACILITY) {
        GameDomainRef ref;
        GameFacilityAction action;
        if (event->phase == ROUTE_EVENT_PHASE_PASS) {
            result = ROUTE_EVENT_APPLIED;
        } else if (!property_domain_ref(event, state, &ref) ||
                   !rich4_facility_inspect(state, executor->state.execution_actor,
                                           ref.id, &action)) {
            return ROUTE_EVENT_EXECUTION_INVALID;
        } else if (action.kind == GAME_FACILITY_ACTION_PURCHASE ||
                   action.kind == GAME_FACILITY_ACTION_UPGRADE) {
            GameCommandType accept_type =
                action.kind == GAME_FACILITY_ACTION_PURCHASE ?
                GAME_COMMAND_PURCHASE_PROPERTY : GAME_COMMAND_UPGRADE_PROPERTY;
            GameCommandType decline_type =
                action.kind == GAME_FACILITY_ACTION_PURCHASE ?
                GAME_COMMAND_DECLINE_PROPERTY_PURCHASE :
                GAME_COMMAND_DECLINE_PROPERTY_UPGRADE;
            if (!state->players[executor->state.execution_actor].is_ai)
                return request_property_decision(executor, event, state,
                    accept_type, decline_type, events) ?
                    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                    ROUTE_EVENT_EXECUTION_INVALID;
            else {
                if (rich4_god_blocks_trade(
                        &state->players[executor->state.execution_actor]))
                    return rich4_route_event_executor_begin_god_trade_rejection(
                        executor, state, event, events) ?
                        ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                        ROUTE_EVENT_EXECUTION_INVALID;
                GameState next_state = *state;
                RouteEventExecutor next_queue = *executor;
                GameEventQueue staged;
                GameCommand command = rich4_game_domain_command(
                    accept_type, GAME_COMMAND_SOURCE_AI,
                    executor->state.execution_actor, ref,
                    rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
                rich4_game_event_queue_reset(&staged);
                command.turn_token = executor->state.turn_token;
                if (!rich4_facility_apply_command(&next_state, &command, &staged) ||
                    !rich4_route_event_executor_complete_events(
                        &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                    events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY)
                    return ROUTE_EVENT_EXECUTION_INVALID;
                if (!rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state; *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_PROGRESSED;
            }
        } else if (action.kind == GAME_FACILITY_ACTION_BUILD) {
            if (!state->players[executor->state.execution_actor].is_ai) {
                /* 0x41982D -> 0x440AAC is a mandatory five-way selector.
                 * Reusing the accept command for both event choices keeps
                 * cancellation out of the command surface; the UI supplies
                 * the selected facility type in command.value. */
                return request_property_decision(
                    executor, event, state,
                    GAME_COMMAND_UPGRADE_PROPERTY,
                    GAME_COMMAND_UPGRADE_PROPERTY, events) ?
                    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                    ROUTE_EVENT_EXECUTION_INVALID;
            } else {
                if (rich4_god_blocks_trade(
                        &state->players[executor->state.execution_actor]))
                    return rich4_route_event_executor_begin_god_trade_rejection(
                        executor, state, event, events) ?
                        ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                        ROUTE_EVENT_EXECUTION_INVALID;
                GameState next_state = *state;
                RouteEventExecutor next_queue = *executor;
                GameEventQueue staged;
                GameCommand command;
                Rich4Rng rng;
                rng.state = next_state.ai_rng_state;
                command = rich4_game_domain_command(
                    GAME_COMMAND_UPGRADE_PROPERTY,
                    GAME_COMMAND_SOURCE_AI, executor->state.execution_actor, ref,
                    rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                    rich4_rng_state_range(&rng, 4) + 1);
                next_state.ai_rng_state = rng.state;
                rich4_game_event_queue_reset(&staged);
                command.turn_token = executor->state.turn_token;
                /* Original AI branch uses rand()%4+1: it never builds the
                 * human-only park option (type 0). */
                if (!rich4_facility_apply_command(
                        &next_state, &command, &staged) ||
                    !rich4_route_event_executor_complete_events(
                        &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                    events->count + staged.count >
                        GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_PROGRESSED;
            }
        } else if (action.kind == GAME_FACILITY_ACTION_RESEARCH) {
            if (!state->players[executor->state.execution_actor].is_ai)
                return request_property_decision(
                    executor, event, state,
                    GAME_COMMAND_UPGRADE_PROPERTY,
                    GAME_COMMAND_DECLINE_PROPERTY_UPGRADE, events) ?
                    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                    ROUTE_EVENT_EXECUTION_INVALID;
            else {
                if (rich4_god_blocks_trade(
                        &state->players[executor->state.execution_actor]))
                    return rich4_route_event_executor_begin_god_trade_rejection(
                        executor, state, event, events) ?
                        ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                        ROUTE_EVENT_EXECUTION_INVALID;
                GameState next_state = *state;
                RouteEventExecutor next_queue = *executor;
                GameEventQueue staged;
                int selection = next_state.facility_level[ref.id] - 1;
                rich4_game_event_queue_reset(&staged);
                if (!rich4_facility_start_research(
                        &next_state, executor->state.execution_actor,
                        ref.id, selection, &staged) ||
                    !rich4_route_event_executor_complete_events(
                        &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                    events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_PROGRESSED;
            }
        } else if (action.kind == GAME_FACILITY_ACTION_DEFERRED ||
                   action.kind == GAME_FACILITY_ACTION_TRAFFIC_CHARGE) {
            GameState next_state = *state;
            RouteEventExecutor next_queue = *executor;
            GameEventQueue staged;
            GameFacilityLandingResult landing;
            GamePaymentResult payment;
            int type = next_state.facility_type[ref.id];
            int outcome = 0;

            /*
             * Rich4.exe 0x41982D sends hotel/department-store landings
             * through the 0x44090E random selector.  Gas stations instead
             * charge from the just-finished dice total.
             */
            if (type == 1 || type == 2)
                return request_facility_roulette(
                    executor, ref, type, events);
            if (type == 3 &&
                (next_state.players[executor->state.execution_actor].traffic_method &
                 3) != 0) {
                int amount;
                RouteEventExecutor presentation_queue = *executor;
                if (!rich4_facility_traffic_charge_amount(
                        &next_state, executor->state.execution_actor, ref.id,
                        event->snapshot.dice_total, &amount))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                presentation_queue.state.waiting_for_presentation = 1;
                presentation_queue.state.presentation_land = ref.id;
                presentation_queue.state.presentation_mode = -1;
                presentation_queue.state.last_result =
                    ROUTE_EVENT_WAITING_FOR_COMMAND;
                if (!rich4_game_event_emit_typed_domain(
                        events,
                        rich4_game_event_property_charge_presentation_requested_from_fields,
                        executor->state.execution_actor, ref,
                        rich4_game_domain_ref(GAME_DOMAIN_PLAYER,
                                              next_state.facility_owner[ref.id]),
                        amount, 0, executor->state.turn_token))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *executor = presentation_queue;
                return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
            }
            rich4_game_event_queue_reset(&staged);
            payment = rich4_facility_resolve_landing(
                &next_state, executor->state.execution_actor, ref.id, outcome,
                event->snapshot.dice_total, 0, -1, &landing, &staged);
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
                (payment == GAME_PAYMENT_BANKRUPTCY ?
                    ROUTE_EVENT_EXECUTION_FINISHED :
                    ROUTE_EVENT_EXECUTION_PROGRESSED);
        } else {
            result = ROUTE_EVENT_APPLIED;
        }
    }
    return rich4_route_event_executor_complete_events(executor, result, events) ?
        ROUTE_EVENT_EXECUTION_PROGRESSED : ROUTE_EVENT_EXECUTION_INVALID;
}



