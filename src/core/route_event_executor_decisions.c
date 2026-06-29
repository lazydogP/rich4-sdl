#include "core/route_event_executor_internal.h"
#include "core/facility_internal.h"
#include "core/game_event_payload.h"
#include "core/game_event_payload.h"

int property_domain_ref(const RouteEvent *event,
                               const GameState *state,
                               GameDomainRef *out_ref)
{
    RouteEventObjectFamily family = rich4_route_event_family_from_raw_id(
        event->object_ref.raw_object_id);
    int id;
    if (family == ROUTE_EVENT_FAMILY_LAND) {
        id = event->object_ref.raw_object_id - 0x7d1;
        if (id < 0 || id >= state->land_count) return 0;
        out_ref->kind = GAME_DOMAIN_LAND;
    } else if (family == ROUTE_EVENT_FAMILY_FACILITY) {
        /* The original table is one-based: 0xFA0 is the base sentinel and
         * the first facility record is addressed by raw id 0xFA1. */
        id = event->object_ref.raw_object_id - 0xfa1;
        if (id < 0 || id >= state->facility_count) return 0;
        out_ref->kind = GAME_DOMAIN_FACILITY;
    } else {
        return 0;
    }
    out_ref->id = id;
    return 1;
}

int request_property_decision(RouteEventExecutor *executor,
                                     const RouteEvent *event,
                                     const GameState *state,
                                     GameCommandType accept_type,
                                     GameCommandType decline_type,
                                     GameEventQueue *events)
{
    GameDomainRef ref;
    GameDomainRef none;
    GameCommandSource source;
    if (!property_domain_ref(event, state, &ref)) return 0;
    none.kind = GAME_DOMAIN_NONE;
    none.id = -1;
    source = state->players[executor->state.execution_actor].is_ai ?
        GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN;
    memset(&executor->state.pending_accept, 0,
           sizeof(executor->state.pending_accept));
    executor->state.pending_accept.type = accept_type;
    executor->state.pending_accept.source = source;
    executor->state.pending_accept.player = executor->state.execution_actor;
    executor->state.pending_accept.subject = ref;
    executor->state.pending_accept.target = none;
    executor->state.pending_accept.turn_token = executor->state.turn_token;
    executor->state.pending_decline = executor->state.pending_accept;
    executor->state.pending_decline.type = decline_type;
    if (!rich4_game_event_emit_interaction_requested(
            events, executor->state.execution_actor, 0, ref, none,
            accept_type, decline_type, executor->state.turn_token)) {
        clear_pending(executor);
        return 0;
    }
    executor->state.waiting_for_command = 1;
    executor->state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    return 1;
}

int request_special_confirmation(RouteEventExecutor *executor,
                                        const GameState *state,
                                        GameEventQueue *events)
{
    GameCommandSource source = state->players[executor->state.execution_actor].is_ai ?
        GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN;
    executor->state.pending_accept = rich4_game_command_confirm(
        source, executor->state.execution_actor);
    executor->state.pending_accept.turn_token = executor->state.turn_token;
    executor->state.pending_decline = executor->state.pending_accept;
    if (!rich4_game_event_emit_interaction_requested(
            events, executor->state.execution_actor, state->special_event_resolved_id,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            GAME_COMMAND_CONFIRM, state->special_event_resource_id,
            executor->state.turn_token)) {
        clear_pending(executor);
        return 0;
    }
    executor->state.waiting_for_command = 1;
    executor->state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    return 1;
}

int first_player_in_mask(const GameState *state, unsigned int mask)
{
    int player;
    for (player=0;player<state->player_count;player++)
        if (mask & (1u<<player)) return player;
    return -1;
}

GameDomainRef choose_construction_target(GameState *state, int actor,
                                                int *facility_type)
{
    GameDomainRef choices[GAME_MAX_MAP_PROPERTIES*2];
    int count=0, i, pick;
    Rich4Rng rng;
    if (state->players[actor].is_ai) {
        GameDomainRef best = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        int best_value = 0;
        /* 0x40B455: scan owned residential land, then facilities; strict
         * comparisons retain the first tie and never fall back to rivals. */
        for (i = 0; i < state->land_count; i++) {
            int level = state->land_level[i];
            if (state->land_owner[i] == actor && state->land_type[i] == 0 &&
                level >= 0 && level < 5 &&
                state->land_toll_by_level[i][level] > best_value) {
                best_value = state->land_toll_by_level[i][level];
                best = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
            }
        }
        for (i = 0; i < state->facility_count; i++) {
            int type = state->facility_type[i];
            if (state->facility_owner[i] == actor && type >= 0 && type <= 4 &&
                state->facility_level[i] < rich4_facility_max_level[type] &&
                state->facility_price[i] > best_value) {
                best_value = state->facility_price[i];
                best = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
            }
        }
        if (facility_type != NULL) {
            *facility_type = 0;
            if (best.kind == GAME_DOMAIN_FACILITY &&
                state->facility_level[best.id] == 0) {
                rng.state = state->ai_rng_state;
                *facility_type = rich4_rng_original_state_range(&rng, 4) + 1;
                state->ai_rng_state = rng.state;
            }
        }
        return best;
    }
    for (i=0;i<state->land_count;i++)
        if (state->land_type[i]==0 && state->land_level[i]<5 &&
            state->land_owner[i]==actor)
            choices[count++]=rich4_game_domain_ref(GAME_DOMAIN_LAND,i);
    for (i=0;i<state->facility_count;i++)
        if (state->facility_owner[i]==actor &&
            ((state->facility_level[i]==0) ||
             (state->facility_level[i]<5 &&
              (state->facility_type[i]==1 ||
               state->facility_type[i]==2 ||
               state->facility_type[i]==4))))
            choices[count++]=rich4_game_domain_ref(GAME_DOMAIN_FACILITY,i);
    if (count==0) {
        for (i=0;i<state->land_count;i++)
            if (state->land_type[i]==0 && state->land_level[i]<5)
                choices[count++]=
                    rich4_game_domain_ref(GAME_DOMAIN_LAND,i);
        for (i=0;i<state->facility_count;i++)
            if (state->facility_level[i]==0 ||
                (state->facility_level[i]<5 &&
                 (state->facility_type[i]==1 ||
                  state->facility_type[i]==2 ||
                  state->facility_type[i]==4)))
                choices[count++]=
                    rich4_game_domain_ref(GAME_DOMAIN_FACILITY,i);
    }
    if (count==0) return rich4_game_domain_ref(GAME_DOMAIN_NONE,-1);
    rng.state=state->ai_rng_state;
    pick=rich4_rng_original_state_range(&rng,count);
    state->ai_rng_state=rng.state;
    if (facility_type!=NULL) {
        *facility_type=0;
        if (choices[pick].kind==GAME_DOMAIN_FACILITY &&
            state->facility_level[choices[pick].id]==0 &&
            state->facility_owner[choices[pick].id]==actor)
            *facility_type=rich4_rng_original_state_range(&rng,4)+1;
        state->ai_rng_state=rng.state;
    }
    return choices[pick];
}

int begin_next_magic_fortune(RouteEventExecutor *executor,
                                    GameState *state,
                                    GameEventQueue *events)
{
    GameSpecialEventResult special;
    int guard=GAME_FORTUNE_EVENT_COUNT*GAME_MAX_PLAYERS;
    while (guard-- > 0) {
        if (executor->state.magic_fortune_target < 0 ||
            executor->state.magic_fortunes_left == 0) {
            if (executor->state.magic_fortune_target >= 0)
                executor->state.magic_fortune_targets_left &=
                    ~(1u<<executor->state.magic_fortune_target);
            executor->state.magic_fortune_target=first_player_in_mask(
                state,executor->state.magic_fortune_targets_left);
            if (executor->state.magic_fortune_target < 0) {
                executor->state.magic_fortune_sequence=0;
                return 0;
            }
            executor->state.magic_fortunes_left=3;
        }
        special=rich4_special_event_begin_fortune(
            state,executor->state.magic_fortune_target,events);
        if (special==GAME_SPECIAL_EVENT_PREPARED) {
            executor->state.magic_fortunes_left--;
            return request_special_confirmation(executor,state,events);
        }
        if (special!=GAME_SPECIAL_EVENT_NO_CANDIDATE)
            return -1;
        executor->state.magic_fortunes_left=0;
    }
    return -1;
}

RouteEventExecutionResult rich4_route_event_executor_finish_fortune_callback(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    RouteEventExecutor next;
    GameState next_state;
    GameEventQueue staged;
    int started;
    if (executor==NULL || state==NULL || events==NULL ||
        !executor->state.magic_fortune_sequence ||
        !executor->state.waiting_for_presentation ||
        executor->state.waiting_for_command ||
        state->special_event_phase!=GAME_SPECIAL_PHASE_IDLE)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next=*executor; next_state=*state;
    rich4_game_event_queue_reset(&staged);
    next.state.waiting_for_presentation=0;
    started=begin_next_magic_fortune(&next,&next_state,&staged);
    if (started<0 || (started==0 &&
        !rich4_route_event_executor_complete_events(&next,ROUTE_EVENT_APPLIED,&staged)) ||
        events->count+staged.count>GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events,&staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor=next; *state=next_state;
    return started ? ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
        ROUTE_EVENT_EXECUTION_PROGRESSED;
}

int request_bank_transfer(RouteEventExecutor *executor,
                                 const GameState *state,
                                 GameEventQueue *events)
{
    GameCommandSource source = state->players[executor->state.execution_actor].is_ai ?
        GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN;
    executor->state.pending_accept = rich4_game_command_bank_withdraw(
        source, executor->state.execution_actor, 0);
    executor->state.pending_accept.turn_token = executor->state.turn_token;
    executor->state.pending_decline = rich4_game_command_bank_deposit(
        source, executor->state.execution_actor, 0);
    executor->state.pending_decline.turn_token = executor->state.turn_token;
    if (!rich4_game_event_emit_interaction_requested(
            events, executor->state.execution_actor, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            GAME_COMMAND_BANK_WITHDRAW, GAME_COMMAND_BANK_DEPOSIT,
            executor->state.turn_token)) {
        clear_pending(executor);
        return 0;
    }
    executor->state.waiting_for_command = 1;
    executor->state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    return 1;
}

int request_bank_loan_page(RouteEventExecutor *executor,
                                  const GameState *state,
                                  GameEventQueue *events)
{
    GameCommandSource source = state->players[executor->state.execution_actor].is_ai ?
        GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN;
    executor->state.pending_accept = rich4_game_command_bank_apply_loan(
        source, executor->state.execution_actor, 0);
    executor->state.pending_accept.turn_token = executor->state.turn_token;
    executor->state.pending_decline = rich4_game_command_bank_repay_loan(
        source, executor->state.execution_actor, 0);
    executor->state.pending_decline.turn_token = executor->state.turn_token;
    if (!rich4_game_event_emit_interaction_requested(
            events, executor->state.execution_actor, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            GAME_COMMAND_BANK_APPLY_LOAN, GAME_COMMAND_BANK_REPAY_LOAN,
            executor->state.turn_token)) {
        clear_pending(executor);
        return 0;
    }
    executor->state.waiting_for_command = 1;
    executor->state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    return 1;
}

RouteEventExecutionResult request_bank_rejection(
    RouteEventExecutor *executor, const GameState *state, GameEventQueue *events)
{
    RouteEventExecutor next;
    int days;
    GameEvent request;
    if (executor == NULL || state == NULL || events == NULL ||
        executor->state.execution_actor < 0 ||
        executor->state.execution_actor >= state->player_count)
        return ROUTE_EVENT_EXECUTION_INVALID;
    days = (state->players[executor->state.execution_actor].days_rejected_by_bank &
            0x7f) + 1;
    next = *executor;
    next.state.waiting_for_presentation = 1;
    next.state.presentation_action = 14;
    next.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
    request = rich4_game_event_bank_rejection(
        executor->state.execution_actor, days, 1000, executor->state.turn_token);
    if (!rich4_game_event_queue_push(events, &request))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next;
    return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
}


