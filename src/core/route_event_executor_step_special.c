#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

RouteEventExecutionResult rich4_route_event_executor_step_special(
    RouteEventExecutor *executor,
    GameState *state,
    GameEventQueue *events,
    const RouteEvent *event,
    RouteEventObjectFamily family)
{
    RouteEventHandlerResult result = ROUTE_EVENT_UNKNOWN_SKIPPED;
    if (family == ROUTE_EVENT_FAMILY_SPECIAL_TABLE_1 ||
               family == ROUTE_EVENT_FAMILY_SPECIAL_TABLE_2) {
        int raw = event->object_ref.raw_object_id;
        int action = event->snapshot.route_action;
        if (event->phase == ROUTE_EVENT_PHASE_PASS && action != 14) {
            result = ROUTE_EVENT_APPLIED;
        } else if (family == ROUTE_EVENT_FAMILY_SPECIAL_TABLE_1 &&
            action == 0 && raw - 0x1771 >= 0 &&
            raw - 0x1771 < state->company_count) {
            int company = raw - 0x1771;
            int type;
            GameState next_state;
            RouteEventExecutor next_queue;
            GameEventQueue staged;
            GameCompanyLandingResult landing;
            GamePaymentResult payment;
            if (event->phase == ROUTE_EVENT_PHASE_PASS) {
                result = ROUTE_EVENT_APPLIED;
            } else {
                type = state->company_type[company];
                if (type == 1 || type == 4) {
                    int stock = state->company_stock_index[company];
                    if (stock >= 0 && stock < GAME_STOCK_COUNT &&
                        state->stock_company_owner[stock] >= 0 &&
                        (type == 4 || state->stock_company_owner[stock] !=
                                      executor->state.execution_actor))
                        return request_facility_roulette(
                            executor,
                            rich4_game_domain_ref(
                                GAME_DOMAIN_COMPANY, company),
                            type == 1 ? 0 : 3, events);
                }
                {
                    int notice = request_god_company_charge(executor, state, company, 0,
                        event->snapshot.dice_total, events);
                    if (notice) return notice > 0 ? ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                                                  ROUTE_EVENT_EXECUTION_INVALID;
                }
                next_state = *state;
                next_queue = *executor;
                rich4_game_event_queue_reset(&staged);
                if (type == 11) {
                    int facility_type=0;
                    int stock = next_state.company_stock_index[company];
                    GameDomainRef target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
                    if (stock >= 0 && stock < GAME_STOCK_COUNT &&
                        next_state.stock_company_owner[stock] >= 0)
                        target = choose_construction_target(
                            &next_state,executor->state.execution_actor,&facility_type);
                    payment = rich4_company_resolve_construction(
                        &next_state, executor->state.execution_actor, company,
                        target, facility_type, 0, -1, &landing, &staged);
                } else {
                    payment = rich4_company_resolve_landing(
                        &next_state, executor->state.execution_actor, company, 0,
                        event->snapshot.dice_total, 0, -1,
                        &landing, &staged);
                }
                if (payment == GAME_PAYMENT_REJECTED)
                    return ROUTE_EVENT_EXECUTION_INVALID;
                {
                    RouteEventExecutionResult company_result =
                        finish_company_landing(
                            &next_queue, &next_state, company, payment,
                            &staged, events);
                    if (company_result ==
                        ROUTE_EVENT_EXECUTION_INVALID)
                        return company_result;
                    *state = next_state;
                    *executor = next_queue;
                    return company_result;
                }
            }
        } else if (event->phase == ROUTE_EVENT_PHASE_PASS && action == 14) {
            if (state->players[executor->state.execution_actor].days_rejected_by_bank != 0)
                return request_bank_rejection(executor, state, events);
            else if ((state->players[executor->state.execution_actor].play_state & 0xff) != 1) {
                GameState next_state = *state;
                GameEventQueue staged;
                rich4_game_event_queue_reset(&staged);
                if (!rich4_bank_run_ai_transfer_with_events(
                        &next_state, executor->state.execution_actor,
                        &staged) ||
                    events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                result = ROUTE_EVENT_APPLIED;
            }
            else
                return request_bank_transfer(executor, state, events) ?
                    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                    ROUTE_EVENT_EXECUTION_INVALID;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP && action == 14) {
            /*
             * 0x41982D calls sub_4379C9 first and, after it returns, calls
             * sub_436668.  A STOP event therefore owns both pages.
             */
            if (state->players[executor->state.execution_actor].days_rejected_by_bank != 0)
                return request_bank_rejection(executor, state, events);
            else if ((state->players[executor->state.execution_actor].play_state & 0xff) != 1) {
                GameState next_state = *state;
                GameEventQueue staged;
                int actor = executor->state.execution_actor;
                int loan_before = state->players[actor].loan;
                rich4_game_event_queue_reset(&staged);
                if (executor->state.bank_stop_stage == 0) {
                    int i;
                    if (!rich4_bank_run_ai_transfer_with_events(&next_state, actor, &staged))
                        return ROUTE_EVENT_EXECUTION_INVALID;
                    for (i = 0; i < staged.count; i++) {
                        const GameEvent *notice = &staged.events[
                            (staged.head + i) % GAME_EVENT_QUEUE_CAPACITY];
                        if (notice->type != GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED)
                            continue;
                        /* 0x41B396 waits for 0x4379C9 (including the bank
                         * owner's 0x436C1F notice) before 0x436668. */
                        if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                            !rich4_game_event_queue_append(events, &staged))
                            return ROUTE_EVENT_EXECUTION_INVALID;
                        *state = next_state;
                        executor->state.bank_stop_stage = 1;
                        return ROUTE_EVENT_EXECUTION_PROGRESSED;
                    }
                }
                if (!rich4_bank_run_ai(&next_state, actor))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                /* 0x436877/0x43694B: a real loan change owns a 1500 ms
                 * result notice; ordinary cash/bank rebalancing is silent. */
                if (next_state.players[actor].loan != loan_before) {
                    int repaid = next_state.players[actor].loan < loan_before;
                    GameEvent notice = rich4_game_event_special_place_notice(
                        actor, repaid ? GAME_SPECIAL_NOTICE_BANK_REPAY
                                      : GAME_SPECIAL_NOTICE_BANK_BORROW,
                        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                        repaid ? loan_before : next_state.players[actor].loan,
                        1500);
                    if (!rich4_game_event_queue_push(&staged, &notice))
                        return ROUTE_EVENT_EXECUTION_INVALID;
                }
                if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                result = ROUTE_EVENT_APPLIED;
            }
            else if (executor->state.bank_stop_stage == 0)
                return request_bank_transfer(executor, state, events) ?
                    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                    ROUTE_EVENT_EXECUTION_INVALID;
            else
                return request_bank_loan_page(executor, state, events) ?
                    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                    ROUTE_EVENT_EXECUTION_INVALID;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
                   (action == 4 || action == 5)) {
            int kind = action == 4 ? GAME_CONFINEMENT_PRISON :
                                     GAME_CONFINEMENT_HOSPITAL;
            if (!confinement_has_candidate(state, kind)) {
                result = ROUTE_EVENT_APPLIED;
            } else if ((state->players[executor->state.execution_actor].play_state & 0xff) != 1) {
                RouteEventExecutionResult ai_result =
                    run_ai_confinement_visit(executor, state, kind, events);
                return ai_result;
            } else {
                RouteEventExecutor next_queue = *executor;
                GameEvent request;
                next_queue.state.waiting_for_presentation = 1;
                next_queue.state.presentation_action = action;
                next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
                request = rich4_game_event_confinement_visit_request(
                    executor->state.execution_actor, kind,
                    action == 4 ? 63 : 65, 64, executor->state.turn_token);
                if (!rich4_game_event_queue_push(events, &request))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
            }
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
                   (action == 10 || action == 11 || action == 12 ||
                    action == 13)) {
            RouteEventExecutor next_queue = *executor;
            GameEvent request;
            next_queue.state.waiting_for_presentation = 1;
            next_queue.state.presentation_action = action;
            next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
            request = rich4_game_event_special_place_request(
                executor->state.execution_actor, action,
                action == 13 ? 536 : 537,
                action == 13 ? 0 : 1000, executor->state.turn_token);
            if (!rich4_game_event_queue_push(events, &request))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
                   action >= 6 && action <= 8 &&
                   (state->players[executor->state.execution_actor].play_state & 0xff)
                       != 1) {
            GameState next_state = *state;
            RouteEventExecutor next_queue = *executor;
            GameEventQueue staged;
            rich4_game_event_queue_reset(&staged);
            if (!apply_automatic_point_minigame(
                    &next_state, executor->state.execution_actor, action, &staged) ||
                !rich4_route_event_executor_complete_events(
                    &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                !rich4_game_event_queue_append(events, &staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *state = next_state;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_PROGRESSED;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
                   action >= 6 && action <= 8) {
            RouteEventExecutor next_queue = *executor;
            GameEvent request;
            next_queue.state.waiting_for_presentation = 1;
            next_queue.state.presentation_action = action;
            next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
            request = rich4_game_event_point_minigame_request(
                executor->state.execution_actor, action, 78, 79,
                executor->state.turn_token);
            if (!rich4_game_event_queue_push(events, &request))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP && action == 9 &&
                   (state->players[executor->state.execution_actor].play_state & 0xff)
                       != 1) {
            GameState next_state = *state;
            RouteEventExecutor next_queue = *executor;
            GameEventQueue staged;
            rich4_game_event_queue_reset(&staged);
            if (!run_ai_lottery_purchase(
                    &next_state, executor->state.execution_actor, &staged) ||
                !rich4_route_event_executor_complete_events(
                    &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                !rich4_game_event_queue_append(events, &staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *state = next_state;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_PROGRESSED;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP && action == 9) {
            RouteEventExecutor next_queue = *executor;
            GameEvent request;
            next_queue.state.waiting_for_presentation = 1;
            next_queue.state.presentation_action = action;
            next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
            request = rich4_game_event_lottery_visit_request(
                executor->state.execution_actor, action, 12, 13, 14);
            if (!rich4_game_event_queue_push(events, &request))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
            (action == 2 || action == 3 ||
             (action == 0 && (raw == 0x1772 || raw == 0x1773)))) {
            GameState next_state=*state;
            RouteEventExecutor next_queue=*executor;
            GameEventQueue staged;
            GameSpecialEventResult special;
            rich4_game_event_queue_reset(&staged);
            special = (action == 2 || (action == 0 && raw == 0x1772)) ?
                rich4_special_event_begin_news(&next_state,executor->state.execution_actor,&staged) :
                rich4_special_event_begin_fortune(&next_state,executor->state.execution_actor,&staged);
            if (special==GAME_SPECIAL_EVENT_PREPARED) {
                /*
                 * 0x44B6DF/0x44DB81 show the same fixed-time Panel[66]
                 * presentation for every actor.  This command is an
                 * internal phase-1 continuation, not a user confirmation;
                 * SDL submits it automatically after the original delay.
                 */
                if (!request_special_confirmation(&next_queue,&next_state,&staged) ||
                    events->count+staged.count>GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events,&staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state=next_state; *executor=next_queue;
                return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
            }
            if (special==GAME_SPECIAL_EVENT_DEFERRED ||
                special==GAME_SPECIAL_EVENT_NO_CANDIDATE) {
                RouteEventExecutor completed=*executor;
                GameEventQueue resolved;
                RouteEventHandlerResult completion =
                    special==GAME_SPECIAL_EVENT_NO_CANDIDATE ?
                        ROUTE_EVENT_APPLIED : ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED;
                rich4_game_event_queue_reset(&resolved);
                if (!rich4_route_event_executor_complete_events(
                        &completed,completion,&resolved) ||
                    events->count+resolved.count>GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events,&resolved))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state=next_state; *executor=completed;
                return ROUTE_EVENT_EXECUTION_PROGRESSED;
            } else return ROUTE_EVENT_EXECUTION_INVALID;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
                   action == 15) {
            GameState next_state = *state;
            RouteEventExecutor next_queue = *executor;
            GameEventQueue staged;
            GameDomainRef gift =
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
            int actor = executor->state.execution_actor;
            rich4_game_event_queue_reset(&staged);
            if (event->snapshot.property_owner == actor &&
                !grant_shop_owner_gift(
                    &next_state, actor, &gift, &staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
            if ((next_state.players[actor].play_state & 0xff) == 1) {
                GameEvent request;
                request = rich4_game_event_shop_visit_request(
                    actor, action, 10, 11, gift, executor->state.turn_token);
                next_queue.state.waiting_for_presentation = 1;
                next_queue.state.presentation_action = action;
                next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
                if (!rich4_game_event_queue_push(&staged, &request) ||
                    events->count + staged.count >
                        GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
            }
            if (!rich4_game_domain_ref_is_none(gift)) {
                const GameCardInfo *card = gift.kind == GAME_DOMAIN_CARD
                    ? rich4_card_info(gift.id) : NULL;
                const GameToolInfo *tool = gift.kind == GAME_DOMAIN_TOOL
                    ? rich4_tool_info(gift.id) : NULL;
                GameEvent notice = rich4_game_event_special_place_notice(
                    actor, GAME_SPECIAL_NOTICE_SHOP_GIFT, gift,
                    card != NULL ? card->shop_price :
                        tool != NULL ? tool->shop_price : 0, 1500);
                /* 0x42EA14/0x42EA23 precede the human/AI shop split. */
                next_queue.state.waiting_for_presentation = 1;
                next_queue.state.presentation_action = 15;
                next_queue.state.last_result = ROUTE_EVENT_WAITING_FOR_COMMAND;
                if (!rich4_game_event_queue_push(&staged, &notice) ||
                    events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
            }
            if (!run_ai_shop(
                    &next_state, actor,
                    shop_company_index(&next_queue, &next_state),
                    &staged) ||
                !rich4_route_event_executor_complete_events(
                    &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
                !rich4_game_event_queue_append(events, &staged))
                return ROUTE_EVENT_EXECUTION_INVALID;
            *state = next_state;
            *executor = next_queue;
            return ROUTE_EVENT_EXECUTION_PROGRESSED;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
                   action == 16) {
            GameState next_state = *state;
            RouteEventExecutor next_queue = *executor;
            GameEventQueue staged;
            GameSpecialEventResult special;
            unsigned int targets;
            int condition;
            int magic_action;
            rich4_game_event_queue_reset(&staged);
            special = rich4_magic_house_choose_condition(
                &next_state, &condition, &targets);
            if (special == GAME_SPECIAL_EVENT_NO_CANDIDATE) {
                if (!rich4_route_event_executor_complete_events(
                        &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
                    events->count + staged.count >
                        GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_PROGRESSED;
            }
            if (special != GAME_SPECIAL_EVENT_APPLIED)
                return ROUTE_EVENT_EXECUTION_INVALID;
            if ((next_state.players[executor->state.execution_actor].play_state & 0xff) != 1) {
                if (targets & (1u << executor->state.execution_actor))
                    magic_action = 6;
                else {
                    Rich4Rng rng;
                    rng.state = next_state.ai_rng_state;

                    magic_action = rich4_rng_original_state_range(&rng, 11);
                    if (magic_action == 6) magic_action = 7;
                    next_state.ai_rng_state = rng.state;
                }
            } else magic_action = -1;
            {
                GameEvent request;
                /* 0x43380A fixes Panel.MKF 18/19/20 inside the magic-house
                 * presentation; no resource index is part of this event. */
                request = magic_action >= 0
                    ? rich4_game_event_magic_house_auto_request(
                        executor->state.execution_actor, condition, targets, magic_action)
                    : rich4_game_event_magic_house_request(
                        executor->state.execution_actor, condition, targets);
                next_queue.state.waiting_for_presentation = 1;
                next_queue.state.presentation_action = action;
                next_queue.state.presentation_condition = condition;
                next_queue.state.presentation_targets = targets;
                next_queue.state.last_result =
                    ROUTE_EVENT_WAITING_FOR_COMMAND;
                if (!rich4_game_event_queue_push(&staged, &request) ||
                    events->count + staged.count >
                        GAME_EVENT_QUEUE_CAPACITY ||
                    !rich4_game_event_queue_append(events, &staged))
                    return ROUTE_EVENT_EXECUTION_INVALID;
                *state = next_state;
                *executor = next_queue;
                return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
            }
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP && action == 1) {
            /* PARK is intentionally presentation-only in the map tile. */
            result = ROUTE_EVENT_APPLIED;
        } else if (event->phase == ROUTE_EVENT_PHASE_STOP &&
                   family == ROUTE_EVENT_FAMILY_SPECIAL_TABLE_2 &&
                   action == 0) {
            /*
             * 0x41982D's action-0 object dispatcher only covers the
             * preceding land/facility/company ranges.  A raw
             * 0x1F41..0x270F object therefore reaches its normal tail with
             * no state change; keep that original no-op applied rather than
             * reporting an unsupported event.
             */
            result = ROUTE_EVENT_APPLIED;
        } else result = ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED;
    }
    return rich4_route_event_executor_complete_events(executor, result, events) ?
        ROUTE_EVENT_EXECUTION_PROGRESSED : ROUTE_EVENT_EXECUTION_INVALID;
}

