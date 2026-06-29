#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

static int same_ref(GameDomainRef left, GameDomainRef right)
{
    return left.kind == right.kind && left.id == right.id;
}

int rich4_route_event_executor_begin_god_trade_rejection(
    RouteEventExecutor *executor, const GameState *state,
    const RouteEvent *event, GameEventQueue *events)
{
    RouteEventObjectFamily family;
    RouteEventExecutor next_queue;
    GameEvent presentation;
    int actor;
    int god_type;

    if (executor == NULL || state == NULL || event == NULL || events == NULL ||
        !executor->state.execution_started ||
        event->phase != ROUTE_EVENT_PHASE_STOP)
        return 0;
    family = rich4_route_event_family_from_raw_id(
        event->object_ref.raw_object_id);
    if (family != ROUTE_EVENT_FAMILY_LAND &&
        family != ROUTE_EVENT_FAMILY_FACILITY)
        return 0;
    actor = executor->state.execution_actor;
    if (actor < 0 || actor >= state->player_count ||
        actor >= GAME_MAX_PLAYERS)
        return 0;
    if (!executor->state.waiting_for_command &&
        !state->players[actor].is_ai)
        return 0;
    /* 0x40FA88 indexes the refusal-name table with the same +63 slot. */
    god_type = state->players[actor].god_info;
    if (!rich4_god_blocks_trade(&state->players[actor])) return 0;

    next_queue = *executor;
    clear_pending(&next_queue);
    next_queue.state.waiting_for_presentation = 1;
    next_queue.state.presentation_action =
        GAME_ROUTE_EVENT_PRESENTATION_GOD_TRADE_REJECTION;
    next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    presentation = rich4_game_event_god_trade_rejection(
        actor, god_type, 1500, executor->state.turn_token);
    if (!rich4_game_event_queue_push(events, &presentation)) return 0;
    *executor = next_queue;
    return 1;
}

static RouteEventCommandResult rich4_route_event_executor_submit_impl(
    RouteEventExecutor *executor,
    GameState *state,
    const GameCommand *command,
    GameEventQueue *events, int defer_confinement)
{
    if (executor == NULL || state == NULL || command == NULL || events == NULL ||
        !executor->state.execution_started ||
        (!executor->state.waiting_for_command &&
         !executor->state.waiting_for_liquidation))
        return ROUTE_EVENT_COMMAND_INVALID;
    if (command->turn_token != executor->state.turn_token)
        return ROUTE_EVENT_COMMAND_STALE;
    {
        const RouteEvent *bank_event = rich4_route_event_executor_current(executor);
        if (bank_event != NULL && bank_event->snapshot.route_action == 14 &&
            executor->state.waiting_for_command) {
            GameState next_state = *state;
            RouteEventExecutor next_queue = *executor;
            GameEventQueue staged;
            int amount = command->value;
            if (command->player != executor->state.execution_actor ||
                command->source != executor->state.pending_accept.source || amount < 0 ||
                (command->type != GAME_COMMAND_END_INTERACTION &&
                 command->type != GAME_COMMAND_BANK_WITHDRAW &&
                 command->type != GAME_COMMAND_BANK_DEPOSIT &&
                 command->type != GAME_COMMAND_BANK_APPLY_LOAN &&
                 command->type != GAME_COMMAND_BANK_REPAY_LOAN &&
                 command->type != GAME_COMMAND_BANK_SPECIAL_BORROW &&
                 command->type != GAME_COMMAND_BANK_SPECIAL_REPAY))
                return ROUTE_EVENT_COMMAND_UNEXPECTED;
            if ((bank_event->phase == ROUTE_EVENT_PHASE_PASS ||
                 (bank_event->phase == ROUTE_EVENT_PHASE_STOP &&
                  next_queue.state.bank_stop_stage == 0)) &&
                command->type != GAME_COMMAND_END_INTERACTION &&
                command->type != GAME_COMMAND_BANK_WITHDRAW &&
                command->type != GAME_COMMAND_BANK_DEPOSIT)
                return ROUTE_EVENT_COMMAND_UNEXPECTED;
            if (bank_event->phase == ROUTE_EVENT_PHASE_STOP &&
                next_queue.state.bank_stop_stage != 0 &&
                (command->type == GAME_COMMAND_BANK_WITHDRAW ||
                 command->type == GAME_COMMAND_BANK_DEPOSIT))
                return ROUTE_EVENT_COMMAND_UNEXPECTED;
            if (command->type == GAME_COMMAND_BANK_WITHDRAW &&
                !rich4_bank_withdraw(&next_state, executor->state.execution_actor, amount))
                return ROUTE_EVENT_COMMAND_INVALID;
            if (command->type == GAME_COMMAND_BANK_DEPOSIT &&
                !rich4_bank_deposit(&next_state, executor->state.execution_actor, amount))
                return ROUTE_EVENT_COMMAND_INVALID;
            if (command->type == GAME_COMMAND_BANK_APPLY_LOAN &&
                !rich4_bank_apply_loan(&next_state, executor->state.execution_actor, amount))
                return ROUTE_EVENT_COMMAND_INVALID;
            if (command->type == GAME_COMMAND_BANK_REPAY_LOAN &&
                !rich4_bank_repay_loan(&next_state, executor->state.execution_actor, amount))
                return ROUTE_EVENT_COMMAND_INVALID;
            if (command->type == GAME_COMMAND_BANK_SPECIAL_BORROW &&
                !rich4_bank_special_borrow(&next_state, executor->state.execution_actor, amount))
                return ROUTE_EVENT_COMMAND_INVALID;
            if (command->type == GAME_COMMAND_BANK_SPECIAL_REPAY &&
                !rich4_bank_special_repay(&next_state, executor->state.execution_actor, amount))
                return ROUTE_EVENT_COMMAND_INVALID;
            rich4_game_event_queue_reset(&staged);
            if ((command->type == GAME_COMMAND_BANK_WITHDRAW ||
                 command->type == GAME_COMMAND_BANK_DEPOSIT) && amount > 0 &&
                !rich4_bank_settle_special_finance(
                    &next_state, 1, executor->state.execution_actor,
                    &staged))
                return ROUTE_EVENT_COMMAND_INVALID;
            clear_pending(&next_queue);
            if (bank_event->phase == ROUTE_EVENT_PHASE_STOP &&
                next_queue.state.bank_stop_stage == 0) {
                next_queue.state.bank_stop_stage = 1;
                if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_COMMAND_INVALID;
                *state = next_state;
                *executor = next_queue;
                return ROUTE_EVENT_COMMAND_APPLIED;
            }
            if (!rich4_route_event_executor_complete_events(
                    &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                !rich4_game_event_queue_append(events, &staged))
                return ROUTE_EVENT_COMMAND_INVALID;
            *state = next_state;
            *executor = next_queue;
            return ROUTE_EVENT_COMMAND_APPLIED;
        }
    }
    if (executor->state.waiting_for_liquidation) {
        GameState next_state;
        RouteEventExecutor next_queue;
        GameEventQueue staged;
        if (command->type != GAME_COMMAND_SELL_ASSET ||
            command->player != executor->state.execution_actor ||
            (command->source == GAME_COMMAND_SOURCE_AI) !=
                (state->players[executor->state.execution_actor].is_ai != 0))
            return ROUTE_EVENT_COMMAND_UNEXPECTED;
        next_state = *state; next_queue = *executor;
        rich4_game_event_queue_reset(&staged);
        if (!rich4_ledger_sell_asset(&next_state, command, &staged))
            return ROUTE_EVENT_COMMAND_INVALID;
        if (!next_state.liquidation_active) {
            next_queue.state.waiting_for_liquidation = 0;
            if (!rich4_route_event_executor_complete_events(
                    &next_queue, ROUTE_EVENT_APPLIED, &staged))
                return ROUTE_EVENT_COMMAND_INVALID;
        }
        if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY)
            return ROUTE_EVENT_COMMAND_INVALID;
        if (!rich4_game_event_queue_append(events, &staged))
            return ROUTE_EVENT_COMMAND_INVALID;
        *state = next_state; *executor = next_queue;
        return ROUTE_EVENT_COMMAND_APPLIED;
    }
    if (state->special_event_phase == GAME_SPECIAL_PHASE_PREPARED &&
        executor->state.pending_accept.type == GAME_COMMAND_CONFIRM) {
        GameState next_state=*state;
        RouteEventExecutor next_queue=*executor;
        GameEventQueue staged;
        GameSpecialEventResult special;
        if (command->type!=GAME_COMMAND_CONFIRM ||
            command->player!=executor->state.execution_actor ||
            command->source!=executor->state.pending_accept.source)
            return ROUTE_EVENT_COMMAND_UNEXPECTED;
        rich4_game_event_queue_reset(&staged);
        {
            GameSpecialDefenseOutcome defense;
            if (command->flags & RICH4_SPECIAL_COMMAND_DEFENSE_RESOLVED) {
                if (command->value2 < GAME_SPECIAL_DEFENSE_NONE ||
                    command->value2 > GAME_SPECIAL_DEFENSE_DOUBLED)
                    return ROUTE_EVENT_COMMAND_INVALID;
                defense = (GameSpecialDefenseOutcome)command->value2;
            } else defense = rich4_special_event_default_defense(&next_state);
            special=(command->flags & RICH4_SPECIAL_COMMAND_TARGET_RESOLVED ?
                rich4_special_event_commit_resolved_target :
                rich4_special_event_commit_with_target)(&next_state, defense,
                command->target.kind == GAME_DOMAIN_PLAYER ? command->target.id : -1,
                &staged, defer_confinement);
        }
        if (special==GAME_SPECIAL_EVENT_LIQUIDATION) {
            clear_pending(&next_queue);
            next_queue.state.waiting_for_liquidation=1;
            next_queue.state.last_result=ROUTE_EVENT_WAITING_FOR_COMMAND;
        } else if (special==GAME_SPECIAL_EVENT_BANKRUPTCY &&
                   !next_queue.state.magic_fortune_sequence) {
            const RouteEvent *event=rich4_route_event_executor_current(&next_queue);
            clear_pending(&next_queue);
            if (!emit_actor_eliminated(&next_queue,event,&staged))
                return ROUTE_EVENT_COMMAND_INVALID;
        } else if (special==GAME_SPECIAL_EVENT_APPLIED ||
                   (special==GAME_SPECIAL_EVENT_BANKRUPTCY &&
                    next_queue.state.magic_fortune_sequence)) {
            /* 0x431DBC returns from bankruptcy to the same three-call loop;
             * the eliminated fortune target need not be execution_actor. */
            clear_pending(&next_queue);
            if (next_queue.state.magic_fortune_sequence) {
                int started;
                /* 0x431CAA waits for the complete 0x44DB81 callback,
                 * including speech and its 800ms tail, before phase 0. */
                if (defer_confinement) {
                    next_queue.state.waiting_for_presentation=1;
                    started=1;
                } else started=begin_next_magic_fortune(
                    &next_queue,&next_state,&staged);
                if (started<0)
                    return ROUTE_EVENT_COMMAND_INVALID;
                if (started==0 &&
                    !rich4_route_event_executor_complete_events(
                        &next_queue,ROUTE_EVENT_APPLIED,&staged))
                    return ROUTE_EVENT_COMMAND_INVALID;
            } else if (!rich4_route_event_executor_complete_events(
                           &next_queue,ROUTE_EVENT_APPLIED,&staged))
                return ROUTE_EVENT_COMMAND_INVALID;
        } else return ROUTE_EVENT_COMMAND_INVALID;
        if (events->count+staged.count>GAME_EVENT_QUEUE_CAPACITY ||
            !rich4_game_event_queue_append(events,&staged))
            return ROUTE_EVENT_COMMAND_INVALID;
        *state=next_state; *executor=next_queue;
        return ROUTE_EVENT_COMMAND_APPLIED;
    }
    if (command->player != executor->state.pending_decline.player ||
        command->source != executor->state.pending_decline.source ||
        !same_ref(command->subject, executor->state.pending_decline.subject))
        return ROUTE_EVENT_COMMAND_UNEXPECTED;
    if (command->type == executor->state.pending_accept.type) {
        if ((command->subject.kind == GAME_DOMAIN_LAND ||
             command->subject.kind == GAME_DOMAIN_FACILITY) &&
            rich4_god_blocks_trade(
                &state->players[executor->state.execution_actor])) {
            if (!rich4_route_event_executor_begin_god_trade_rejection(
                    executor, state,
                    rich4_route_event_executor_current(executor), events))
                return ROUTE_EVENT_COMMAND_INVALID;
            return ROUTE_EVENT_COMMAND_APPLIED;
        }
        GameState next_state = *state;
        RouteEventExecutor next_queue = *executor;
        GameEventQueue staged;
        GameFacilityAction facility_action;
        int facility_research =
            command->type == GAME_COMMAND_UPGRADE_PROPERTY &&
            command->subject.kind == GAME_DOMAIN_FACILITY &&
            rich4_facility_inspect(
                state, executor->state.execution_actor,
                command->subject.id, &facility_action) &&
            facility_action.kind == GAME_FACILITY_ACTION_RESEARCH;
        rich4_game_event_queue_reset(&staged);
        if (!(facility_research ?
              rich4_facility_start_research(
                  &next_state, executor->state.execution_actor,
                  command->subject.id, command->value, &staged) :
              ((command->subject.kind == GAME_DOMAIN_LAND &&
                rich4_land_apply_command(&next_state, command, &staged)) ||
               (command->subject.kind == GAME_DOMAIN_FACILITY &&
                rich4_facility_apply_command(&next_state, command, &staged)))) ||
            !rich4_route_event_executor_complete_events(
                &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
            events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY)
            return ROUTE_EVENT_COMMAND_INVALID;
        clear_pending(&next_queue);
        if (!rich4_game_event_queue_append(events, &staged))
            return ROUTE_EVENT_COMMAND_INVALID;
        *state = next_state; *executor = next_queue;
        return ROUTE_EVENT_COMMAND_APPLIED;
    }
    if (command->type != executor->state.pending_decline.type)
        return ROUTE_EVENT_COMMAND_UNEXPECTED;
    if (!rich4_route_event_executor_complete_events(
            executor, ROUTE_EVENT_DECISION_DECLINED, events))
        return ROUTE_EVENT_COMMAND_INVALID;
    clear_pending(executor);
    return ROUTE_EVENT_COMMAND_APPLIED;
}





RouteEventCommandResult rich4_route_event_executor_submit(RouteEventExecutor *executor, GameState *state, const GameCommand *command, GameEventQueue *events)
{
    return rich4_route_event_executor_submit_impl(executor, state, command, events, 0);
}

RouteEventCommandResult rich4_route_event_executor_submit_deferred_confinement(RouteEventExecutor *executor, GameState *state, const GameCommand *command, GameEventQueue *events)
{
    return rich4_route_event_executor_submit_impl(executor, state, command, events, 1);
}
