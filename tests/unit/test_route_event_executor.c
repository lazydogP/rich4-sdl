#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/game_dispatch.h"
#include "core/game_event_payload.h"
#include "core/property.h"
#include "core/special_event.h"
#include "core/turn_rotation.h"

static RouteEvent route_event(RouteEventPhase phase, int node, int raw,
                              int branch)
{
    RouteEvent event;
    memset(&event, 0, sizeof(event));
    event.phase = phase;
    event.node = node;
    event.object_ref = rich4_map_object_ref((unsigned short)raw);
    event.snapshot.player = 0;
    event.snapshot.property_branch = branch;
    event.snapshot.property_owner = -1;
    return event;
}
static void insurance_automatic_issue_and_zero_quote(void)
{
    static GameState state;
    RouteEventExecutor executor;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP,20,6001,GAME_PROPERTY_STOP_BRANCH_NONE);
    const int controls[2] = {2,5};
    int c, budget;
    for (c=0;c<2;c++) for (budget=0;budget<2;budget++) {
        memset(&state,0,sizeof(state));
        state.active=1; state.player_count=2; state.price_index=1; state.initial_fund=100000;
        state.players[0].play_state=controls[c]; state.players[0].is_ai=controls[c]==2;
        state.players[1].play_state=1;
        state.players[0].cash=budget ? 100000 : 30099;
        state.company_count=1; state.company_type[0]=4; state.company_stock_index[0]=2;
        state.stock_company_owner[2]=-1; state.stock_company_auxiliary[2]=-1;
        state.stock_company_value[2]=1000000; state.stock_company_available[2]=1000;
        rich4_route_event_executor_reset(&executor);
        assert(rich4_route_event_queue_push(&executor.queue,&stop));
        assert(rich4_route_event_executor_begin(&executor,0,5));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&executor,&state,&events)==ROUTE_EVENT_EXECUTION_PROGRESSED);
        assert(state.player_stock_amount[0][2]==(budget ? 700 : 0));
        assert(state.players[0].cash==(budget ? 30000 : 30099));
        assert(state.players[0].days_assurance==0);
        while(rich4_game_event_queue_pop(&events,&event)) {
            assert(event.type!=GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED);
            assert(event.type!=GAME_EVENT_FACILITY_ROULETTE_REQUESTED);
        }
    }
    state.stock_company_owner[2]=1; state.stock_company_available[2]=0;
    rich4_route_event_executor_reset(&executor);
    assert(rich4_route_event_queue_push(&executor.queue,&stop));
    assert(rich4_route_event_executor_begin(&executor,0,6));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&executor,&state,&events)==ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_resume_company_roulette(&executor,&state,5,&events)==ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].days_assurance==5);
    while(rich4_game_event_queue_pop(&events,&event)) {
        assert(event.type!=GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED);
        assert(event.type!=GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED);
    }
}

static GameEvent pop(GameEventQueue *events)
{
    GameEvent event;
    assert(rich4_game_event_queue_pop(events, &event));
    return event;
}

static void force_first(uint8_t *order, int count, int id)
{
    int i;
    for (i=0;i<count;i++) if (order[i]==id) {
        uint8_t t=order[0]; order[0]=order[i]; order[i]=t; return;
    }
    assert(0);
}

static void interaction_sequence(void)
{
    GameState state;
    GameFlowState flow;
    GameEventQueue events;
    GameCommand command;
    GameEvent event;
    int frozen_count;
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    state.active = 1;
    state.player_count = 2;
    state.current_player = 0;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 100;
    state.players[0].linked_player = -1;
    state.players[1].play_state = 2;
    state.players[1].linked_player = -1;
    state.land_count = 1;
    state.land_owner[0] = -1;
    state.land_group[0] = 0;
    state.land_price[0] = 50;
    rich4_route_event_executor_reset(&flow.route_events);
    {
        RouteEvent pass1 = route_event(ROUTE_EVENT_PHASE_PASS, 2, 1,
                                       GAME_PROPERTY_STOP_BRANCH_NONE);
        RouteEvent pass2 = route_event(ROUTE_EVENT_PHASE_PASS, 3, 0x1771,
                                       GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
        RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 4, 0x7d1,
                                      GAME_PROPERTY_STOP_BRANCH_UNOWNED);
        assert(rich4_route_event_queue_push(&flow.route_events.queue, &pass1));
        assert(rich4_route_event_queue_push(&flow.route_events.queue, &pass2));
        assert(rich4_route_event_queue_push(&flow.route_events.queue, &stop));
    }
    assert(rich4_route_event_executor_begin(&flow.route_events, 0, 77));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    event = pop(&events);
    assert(rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED &&
           flow.route_events.queue.count == 2);
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    event = pop(&events);
    assert(rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED &&
           flow.route_events.queue.count == 1);
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_INTERACTION_REQUESTED &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_LAND &&
           rich4_game_event_subject_ref(&event).id == 0 &&
           rich4_game_event_flags(&event) == 77);
    assert(rich4_game_event_value(&event) == GAME_COMMAND_PURCHASE_PROPERTY &&
           rich4_game_event_value2(&event) == GAME_COMMAND_DECLINE_PROPERTY_PURCHASE);
    frozen_count = flow.route_events.queue.count;
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(flow.route_events.queue.count == frozen_count && events.count == 0);

    flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    command = rich4_game_domain_command(
        GAME_COMMAND_DECLINE_PROPERTY_PURCHASE,
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    command.turn_token = 76;
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    event = pop(&events);
    assert(event.type == GAME_EVENT_COMMAND_REJECTED &&
           event.reject_reason == GAME_COMMAND_REJECT_STATE);
    assert(flow.route_events.queue.count == frozen_count);
    command.type = GAME_COMMAND_PURCHASE_PROPERTY;
    command.turn_token = 77;
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    event = pop(&events);
    assert(event.type == GAME_EVENT_MONEY_TRANSFERRED &&
           rich4_game_event_value(&event) == 50);
    assert(pop(&events).type == GAME_EVENT_PROPERTY_OWNER_CHANGED);
    assert(pop(&events).type == GAME_EVENT_SCENE_REFRESH_REQUESTED);
    event = pop(&events);
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED);
    assert(state.land_owner[0] == 0 && state.players[0].cash == 50);
    assert(!rich4_route_event_executor_is_waiting(&flow.route_events));
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_FINISHED);
}

static void god_trade_yes_shows_refusal_and_finishes(int god_type)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    GameCommand command;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 4, 0x7d1,
                                  GAME_PROPERTY_STOP_BRANCH_UNOWNED);

    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 1000;
    state.players[0].linked_player = -1;
    state.players[0].god_info = god_type;
    state.players[0].god_type = 1; /* Refusal text indexes +63, not this type. */
    state.land_count = 1;
    state.land_owner[0] = -1;
    state.land_price[0] = 100;

    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 132));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(pop(&events).type == GAME_EVENT_INTERACTION_REQUESTED);

    command = rich4_game_domain_command(
        GAME_COMMAND_PURCHASE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    command.turn_token = 132;
    assert(rich4_route_event_executor_submit(
               &queue, &state, &command, &events) ==
           ROUTE_EVENT_COMMAND_APPLIED);
    assert(state.land_owner[0] == -1 && state.players[0].cash == 1000);
    assert(queue.state.waiting_for_presentation &&
           queue.state.presentation_action ==
               GAME_ROUTE_EVENT_PRESENTATION_GOD_TRADE_REJECTION);
    event = pop(&events);
    assert(event.type == GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED);
    assert(rich4_game_event_god_trade_type(&event) == god_type &&
           rich4_game_event_god_trade_duration(&event) == 1500 &&
           rich4_game_event_god_trade_turn_token(&event) == 132);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(events.count == 0);

    assert(rich4_route_event_executor_finish_god_trade_rejection(
               &queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    event = pop(&events);
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED);
    assert(queue.queue.count == 0 &&
           !rich4_route_event_executor_is_waiting(&queue));
}

static void god_trade_yes_is_refused_for_all_blocking_gods(void)
{
    static const int god_types[] = { 7, 8, 15 };
    size_t i;
    for (i = 0; i < sizeof(god_types) / sizeof(god_types[0]); i++)
        god_trade_yes_shows_refusal_and_finishes(god_types[i]);
}

static void eliminated_during_events(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent first = route_event(ROUTE_EVENT_PHASE_PASS, 1, 1,
                                   GAME_PROPERTY_STOP_BRANCH_NONE);
    RouteEvent second = route_event(ROUTE_EVENT_PHASE_STOP, 2, 2,
                                    GAME_PROPERTY_STOP_BRANCH_NONE);
    memset(&state, 0, sizeof(state));
    state.player_count = 1;
    state.players[0].play_state = 1;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &first));
    assert(rich4_route_event_queue_push(&queue.queue, &second));
    assert(rich4_route_event_executor_begin(&queue, 0, 1));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    (void)pop(&events);
    state.players[0].play_state = 0;
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_FINISHED);
    event = pop(&events);
    assert(rich4_game_event_value(&event) == ROUTE_EVENT_ACTOR_ELIMINATED);
    assert(rich4_route_event_executor_current(&queue) == NULL);
}

static void unpurchasable_stop_does_not_spend(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 2, 2,
                                  GAME_PROPERTY_STOP_BRANCH_NONE);
    memset(&state, 0, sizeof(state));
    state.player_count = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 500;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 79));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].cash == 500);
    assert(events.count == 1);
    assert(pop(&events).type == GAME_EVENT_ROUTE_EVENT_RESOLVED);
    assert(!rich4_route_event_executor_is_waiting(&queue));
}

static void first_facility_uses_zero_based_domain_id(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameCommand command;
    GameEvent event;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 4, 0xfa1,
                                  GAME_PROPERTY_STOP_BRANCH_UNOWNED);
    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 100;
    state.players[0].linked_player = -1;
    state.facility_count = 1;
    state.facility_owner[0] = -1;
    state.facility_price[0] = 50;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 78));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_INTERACTION_REQUESTED &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_FACILITY &&
           rich4_game_event_subject_ref(&event).id == 0);
    command = rich4_game_domain_command(
        GAME_COMMAND_PURCHASE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    command.turn_token = 78;
    assert(rich4_route_event_executor_submit(&queue, &state, &command, &events) ==
           ROUTE_EVENT_COMMAND_APPLIED);
    assert(state.facility_owner[0] == 0 && state.players[0].cash == 50);
}

static void owned_unbuilt_facility_selects_type(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    GameCommand command;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 4, 0xfa1,
                                  GAME_PROPERTY_STOP_BRANCH_OWNED_SELF);
    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 100;
    state.players[0].linked_player = -1;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_level[0] = 0;
    state.facility_price[0] = 30;
    state.facility_price_per_level[0] = 30;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 80));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_INTERACTION_REQUESTED &&
           rich4_game_event_value(&event) == GAME_COMMAND_UPGRADE_PROPERTY &&
           rich4_game_event_value2(&event) == GAME_COMMAND_UPGRADE_PROPERTY);
    command = rich4_game_domain_command(
        GAME_COMMAND_UPGRADE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 3);
    command.turn_token = 80;
    assert(rich4_route_event_executor_submit(
               &queue, &state, &command, &events) ==
           ROUTE_EVENT_COMMAND_APPLIED);
    assert(state.facility_level[0] == 1 &&
           state.facility_type[0] == 3 &&
           state.players[0].cash == 70);

    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.price_index = 1;
    state.ai_rng_state = 77;
    state.players[0].play_state = 2;
    state.players[0].is_ai = 1;
    state.players[0].cash = 100;
    state.players[0].linked_player = -1;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_price[0] = 30;
    state.facility_price_per_level[0] = 30;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 81));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.facility_level[0] == 1 &&
           state.facility_type[0] >= 1 &&
           state.facility_type[0] <= 4 &&
           state.players[0].cash == 70);
}

static void nonowner_facility_executes_landing(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 4, 0xfa1,
                                  GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER);
    int outcome = 3;

    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 2;
    state.price_index = 1;
    state.ai_rng_state = 77;
    state.players[0].play_state = 1;
    state.players[0].cash = 1000;
    state.players[0].linked_player = -1;
    state.players[1].play_state = 1;
    state.players[1].linked_player = -1;
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_level[0] = 2;
    state.facility_type[0] = 1;
    state.facility_charge_by_level[0][2] = 100;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 82));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].cash == 1000);
    assert(rich4_route_event_executor_resume_company_roulette(
               &queue, &state, outcome, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].cash == 1000);
    assert(queue.state.presentation_mode == 10 + outcome);
    assert(rich4_route_event_executor_resume_charge(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].cash == 1000 - 100 * outcome);
    assert(state.players[1].bank == 100 * outcome);
    assert(state.players[0].traffic_method == 0);
    assert(state.players[0].delayed_status_block ==
           (outcome == 1 ? 0x80 : outcome - 1));

    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 2;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 5000;
    state.players[0].linked_player = -1;
    state.players[0].traffic_method = 1;
    state.players[1].play_state = 1;
    state.players[1].linked_player = -1;
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_level[0] = 1;
    state.facility_type[0] = 3;
    stop.snapshot.dice_total = 5;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 83));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED &&
           rich4_game_event_property_ref(&event).kind == GAME_DOMAIN_FACILITY &&
           rich4_game_event_property_ref(&event).id == 0 &&
           rich4_game_event_value(&event) == 2500);
    assert(state.players[0].cash == 5000);
    assert(rich4_route_event_executor_resume_charge(
               &queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].cash == 2500);
    assert(state.players[1].bank == 2500);
}

static void magic_three_fortunes_sequence(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    RouteEvent stop=route_event(
        ROUTE_EVENT_PHASE_STOP,7,0,
        GAME_PROPERTY_STOP_BRANCH_INVALID);
    Rich4Rng rng;
    GameEvent request;
    static const int safe_fortunes[6]={20,21,22,25,27,28};
    int i, j, presentations=0, expected=0;
    memset(&state,0,sizeof(state));
    state.active=1; state.player_count=2; state.price_index=1;
    state.map=0; state.stage=0; state.prison_node=70;
    state.hospital_node=80;
    for (i=0;i<2;i++) {
        state.players[i].play_state=1;
        state.players[i].cash=state.players[i].bank=1000000;
        state.players[i].linked_player=-1;
        state.players[i].sex=i;
    }
    rich4_rng_state_seed(&rng,901);
    rich4_special_event_sequences_init(&state,&rng);
    for (i=0;i<6;i++)
        for (j=i;j<GAME_FORTUNE_EVENT_COUNT;j++)
            if (state.fortune_event_order[j]==safe_fortunes[i]) {
                uint8_t value=state.fortune_event_order[i];
                state.fortune_event_order[i]=
                    state.fortune_event_order[j];
                state.fortune_event_order[j]=value;
                break;
            }
    stop.snapshot.route_action=16;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,901));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    request=pop(&events);
    assert(request.type==GAME_EVENT_MAGIC_HOUSE_REQUESTED);
    assert(rich4_game_event_value(&request)==0 &&
           rich4_game_event_value2(&request)==0);
    assert(rich4_game_event_subject(&request)>=0 &&
           rich4_game_event_subject(&request)<12);
    assert(rich4_game_event_flags(&request)!=0);
    assert(rich4_game_domain_ref_is_none(rich4_game_event_subject_ref(&request)));
    assert(rich4_game_domain_ref_is_none(rich4_game_event_target_ref(&request)));
    assert(events.count==0);
    for (i=0;i<state.player_count;i++)
        if (queue.state.presentation_targets&(1u<<i)) expected+=3;
    assert(expected>=3 && expected<=6);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_resume_magic_house(
               &queue,&state,1,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    while (queue.state.magic_fortune_sequence) {
        GameCommand confirm=queue.state.pending_accept;
        assert(state.special_event_phase==GAME_SPECIAL_PHASE_PREPARED);
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_submit(
                   &queue,&state,&confirm,&events)==
               ROUTE_EVENT_COMMAND_APPLIED);
        presentations++;
        assert(presentations<=6);
    }
    assert(presentations==expected);
}

/* 0x431DBC does not stop its three calls when the target is eliminated.
 * Cover the triggering player and a different selected player, both with
 * immediate callbacks and the runtime's presentation continuation. */
static void magic_fortunes_continue_after_bankruptcy(void)
{
    static GameState state;
    RouteEventExecutor executor;
    GameEventQueue events;
    const int following[4][2]={{20,4},{6,33},{24,20},{12,7}};
    int victim, deferred, variant;
    for (victim=0;victim<2;victim++) for (deferred=0;deferred<2;deferred++)
    for (variant=0;variant<4;variant++) {
        RouteEvent stop=route_event(ROUTE_EVENT_PHASE_STOP,2,6001,
                                    GAME_PROPERTY_STOP_BRANCH_NONE);
        int ids[3]={23,following[variant][0],following[variant][1]};
        int i,j,callbacks=0,eliminations=0;
        memset(&state,0,sizeof(state));
        state.active=1; state.player_count=3; state.price_index=1;
        state.prison_node=5; state.hospital_node=6;
        for (i=0;i<3;i++) {
            state.players[i].play_state=1;
            state.players[i].linked_player=-1;
            state.players[i].attached_timed_bomb_slot=-1;
            state.players[i].bank=i==victim ? 0 : 10000;
        }
        for (i=0;i<GAME_FORTUNE_EVENT_COUNT;i++) state.fortune_event_order[i]=(uint8_t)i;
        /* Force the first target's three callbacks in a valid permutation. */
        for (i=0;i<3;i++) for (j=i;j<GAME_FORTUNE_EVENT_COUNT;j++)
            if (state.fortune_event_order[j]==ids[i]) {
                uint8_t t=state.fortune_event_order[i];
                state.fortune_event_order[i]=state.fortune_event_order[j];
                state.fortune_event_order[j]=t; break;
            }
        rich4_route_event_executor_reset(&executor);
        assert(rich4_route_event_queue_push(&executor.queue,&stop));
        assert(rich4_route_event_executor_begin(&executor,0,901));
        executor.state.magic_fortune_sequence=1;
        executor.state.magic_fortune_target=victim;
        executor.state.magic_fortunes_left=2;
        executor.state.magic_fortune_targets_left=(1u<<victim)|(1u<<(victim+1));
        executor.state.waiting_for_command=1;
        executor.state.pending_accept=rich4_game_command_confirm(GAME_COMMAND_SOURCE_HUMAN,0);
        executor.state.pending_accept.turn_token=901;
        rich4_game_event_queue_reset(&events);
        assert(rich4_special_event_begin_fortune(&state,victim,&events)==GAME_SPECIAL_EVENT_PREPARED);
        while (executor.state.magic_fortune_sequence) {
            GameCommand command=executor.state.pending_accept;
            GameEvent event;
            int target=state.special_event_actor;
            assert(target==(callbacks<3 ? victim : victim+1));
            if (callbacks<3) assert(state.special_event_logical_id==ids[callbacks]);
            rich4_game_event_queue_reset(&events);
            {
                RouteEventCommandResult result=(deferred ? rich4_route_event_executor_submit_deferred_confinement :
                    rich4_route_event_executor_submit)(
                    &executor,&state,&command,&events);
                if (result!=ROUTE_EVENT_COMMAND_APPLIED)
                    fprintf(stderr,"magic fortune failed: victim=%d deferred=%d variant=%d callback=%d id=%d result=%d\n",
                        victim,deferred,variant,callbacks,state.special_event_logical_id,result);
                assert(result==ROUTE_EVENT_COMMAND_APPLIED);
            }
            callbacks++;
            while (rich4_game_event_queue_pop(&events,&event)) {
                if (event.type==GAME_EVENT_PLAYER_ELIMINATED) {
                    assert(event.player==victim); eliminations++;
                }
                if (event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED)
                    assert(rich4_game_event_value(&event)!=ROUTE_EVENT_ACTOR_ELIMINATED);
            }
            assert(!state.game_over && state.players[victim].play_state==0);
            if (callbacks==3) {
                if (variant==0)
                    assert(state.players[victim].cash==1000 && state.players[victim].bank==2000);
                if (variant==1 && !deferred) assert(state.players[victim].days_in_prison==3);
                if (variant==2) assert(state.players[victim].cash==1000);
                if (variant==3) assert(state.players[victim].days_disappearing==0x43);
            }
            if (deferred) {
                assert(executor.queue.count==1 && executor.state.waiting_for_presentation);
                assert(state.special_event_phase==GAME_SPECIAL_PHASE_IDLE);
                assert(rich4_route_event_executor_finish_fortune_callback(
                    &executor,&state,&events)!=ROUTE_EVENT_EXECUTION_INVALID);
            }
            assert(callbacks<=6);
        }
        assert(callbacks==6 && eliminations==1 && executor.queue.count==0);
        assert(!executor.state.waiting_for_command && !executor.state.waiting_for_presentation);
    }
}

static void unknown_allows_next_turn(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    TurnAdvanceResult turn;
    RouteEvent unknown = route_event(ROUTE_EVENT_PHASE_STOP, 9, 10000,
                                     GAME_PROPERTY_STOP_BRANCH_INVALID);
    memset(&state, 0, sizeof(state));
    state.player_count = 2;
    state.players[0].play_state = 1;
    state.players[1].play_state = 2;
    state.turn_rotation_index = 0;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &unknown));
    assert(rich4_route_event_executor_begin(&queue, 0, 2));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    event = pop(&events);
    assert(rich4_game_event_value(&event) == ROUTE_EVENT_UNKNOWN_SKIPPED);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_FINISHED);
    assert(rich4_turn_advance_once(&state, &turn));
    assert(turn.selected_index == 1 && state.current_player == 1);
}

static void ai_upgrade_and_toll_bankruptcy(void)
{
    GameState state;
    GameFlowState flow;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop;
    memset(&state, 0, sizeof(state)); memset(&flow, 0, sizeof(flow));
    state.active = 1; state.player_count = 2; state.current_player = 0;
    state.price_index = 1; state.players[0].play_state = 2;
    state.players[0].is_ai = 1; state.players[0].cash = 100;
    state.players[0].linked_player = -1;
    state.players[1].play_state = 1; state.players[1].linked_player = -1;
    state.land_count = 1; state.land_owner[0] = 0;
    state.land_price[0] = 100; state.land_price_per_level[0] = 20;
    stop = route_event(ROUTE_EVENT_PHASE_STOP, 3, 0x7d1,
                       GAME_PROPERTY_STOP_BRANCH_OWNED_SELF);
    rich4_route_event_executor_reset(&flow.route_events);
    assert(rich4_route_event_queue_push(&flow.route_events.queue, &stop));
    assert(rich4_route_event_executor_begin(&flow.route_events, 0, 4));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.land_level[0] == 1 && state.players[0].cash == 80);
    assert(events.count == 4);

    memset(&state, 0, sizeof(state)); memset(&flow, 0, sizeof(flow));
    state.active = 1; state.player_count = 2; state.current_player = 0;
    state.price_index = 1; state.players[0].play_state = 1;
    state.players[0].cash = 100; state.players[0].linked_player = -1;
    state.players[1].play_state = 1; state.players[1].linked_player = -1;
    state.land_count = 2; state.land_owner[0] = 1; state.land_owner[1] = 0;
    state.land_group[0] = 0; state.land_group[1] = 1;
    state.land_toll_by_level[0][0] = 500;
    state.land_price[1] = 1000; state.land_value[1] = 1000;
    stop = route_event(ROUTE_EVENT_PHASE_STOP, 3, 0x7d1,
                       GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER);
    rich4_route_event_executor_reset(&flow.route_events);
    assert(rich4_route_event_queue_push(&flow.route_events.queue, &stop));
    assert(rich4_route_event_executor_begin(&flow.route_events, 0, 5));
    flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(pop(&events).type ==
           GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED);
    assert(state.players[0].cash == 100 &&
           flow.route_events.state.waiting_for_presentation);
    assert(rich4_route_event_executor_resume_charge(
               &flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_FINISHED);
    assert(!state.liquidation_active &&
           !flow.route_events.state.waiting_for_liquidation);
    assert(state.land_owner[1] == -1 && state.players[0].cash == 0 &&
           state.players[1].bank == 100);
    assert((state.players[0].play_state & 0xff) == 0);
    assert(events.count >= 2);

    memset(&state, 0, sizeof(state)); memset(&flow, 0, sizeof(flow));
    state.active = 1; state.player_count = 3; state.current_player = 0;
    state.price_index = 1; state.players[0].play_state = 1;
    state.players[0].cash = 200; state.players[0].linked_player = -1;
    state.players[1].play_state = 1; state.players[1].linked_player = 2;
    state.players[2].play_state = 1; state.players[2].linked_player = 1;
    state.land_count = 2; state.land_owner[0] = 1; state.land_owner[1] = 2;
    state.land_group[0] = state.land_group[1] = 4;
    state.land_toll_by_level[0][0] = 100;
    state.land_toll_by_level[1][0] = 50;
    stop = route_event(ROUTE_EVENT_PHASE_STOP, 3, 0x7d1,
                       GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER);
    rich4_route_event_executor_reset(&flow.route_events);
    assert(rich4_route_event_queue_push(&flow.route_events.queue, &stop));
    assert(rich4_route_event_executor_begin(&flow.route_events, 0, 8));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED &&
           rich4_game_event_value(&event) == 150 &&
           rich4_game_event_target_ref(&event).id == 1);
    assert(rich4_route_event_executor_resume_charge(
               &flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(rich4_route_event_executor_step(&flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_FINISHED);
    assert(state.players[0].cash == 50 && state.players[1].bank == 100 &&
           state.players[2].bank == 50 &&
           !flow.route_events.state.waiting_for_presentation);

    memset(&state, 0, sizeof(state)); memset(&flow, 0, sizeof(flow));
    state.active = 1; state.player_count = 2; state.current_player = 0;
    state.price_index = 1; state.players[0].play_state = 1;
    state.players[0].cash = 1000; state.players[0].linked_player = -1;
    state.players[1].play_state = 1; state.players[1].linked_player = -1;
    state.players[1].days_in_prison = 2;
    state.land_count = 1; state.land_owner[0] = 1;
    state.land_group[0] = 0; state.land_toll_by_level[0][0] = 500;
    stop = route_event(ROUTE_EVENT_PHASE_STOP, 3, 0x7d1,
                       GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER);
    rich4_route_event_executor_reset(&flow.route_events);
    assert(rich4_route_event_queue_push(&flow.route_events.queue, &stop));
    assert(rich4_route_event_executor_begin(&flow.route_events, 0, 6));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(
               &flow.route_events, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    event = pop(&events);
    assert(event.type ==
               GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED &&
           event.player == 0 &&
           rich4_game_event_target_ref(&event).kind == GAME_DOMAIN_PLAYER &&
           rich4_game_event_target_ref(&event).id == 1 &&
           rich4_game_event_value(&event) == 500 &&
           rich4_game_event_value2(&event) == GAME_PROPERTY_COLLECTION_BLOCK_PRISON);
    event = pop(&events);
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED &&
           events.count == 0);
    assert(state.players[0].cash == 1000 && state.players[1].bank == 0 &&
           !flow.route_events.state.waiting_for_presentation);
}

static void news_presentation_pause_resume(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameCommand confirm;
    Rich4Rng rng;
    RouteEvent stop=route_event(ROUTE_EVENT_PHASE_STOP,9,0,
                                GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    int i;
    memset(&state,0,sizeof(state)); state.active=1; state.player_count=1;
    stop.snapshot.route_action=2;
    state.players[0].play_state=1; state.players[0].linked_player=-1;
    rich4_rng_state_seed(&rng,77); rich4_special_event_sequences_init(&state,&rng);
    force_first(state.news_event_order,GAME_NEWS_EVENT_COUNT,24);
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,91));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(events.count==4 && queue.state.waiting_for_command &&
           queue.queue.count==1);
    assert(pop(&events).type==GAME_EVENT_MARKET_CHANGED);
    assert(pop(&events).type==GAME_EVENT_SPECIAL_EVENT_PREPARED);
    assert(pop(&events).type==GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED);
    assert(pop(&events).type==GAME_EVENT_INTERACTION_REQUESTED);
    confirm=rich4_game_command(GAME_COMMAND_CONFIRM,GAME_COMMAND_SOURCE_HUMAN,0,0);
    confirm.turn_token=91;
    assert(rich4_route_event_executor_submit(&queue,&state,&confirm,&events)==
           ROUTE_EVENT_COMMAND_APPLIED);
    assert(pop(&events).type==GAME_EVENT_SPECIAL_EVENT_COMMITTED);
    assert(pop(&events).type==GAME_EVENT_ROUTE_EVENT_RESOLVED);
    for (i=0;i<GAME_STOCK_COUNT;i++) assert(state.stocks[i].f7==1);
    assert(queue.queue.count==0 && !queue.state.waiting_for_command);
}

static void bank_stop_itself_uses_two_pages(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent resolved;
    GameCommand command;
    RouteEvent stop=route_event(ROUTE_EVENT_PHASE_STOP,19,0x1773,
                                GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state)); state.active=1; state.player_count=1;
    state.players[0].play_state=1; state.players[0].linked_player=-1;
    state.players[0].cash=100; state.players[0].bank=200;
    stop.snapshot.route_action=14;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,92));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    resolved=pop(&events);
    assert(resolved.type==GAME_EVENT_INTERACTION_REQUESTED);
    assert(rich4_game_event_value(&resolved)==GAME_COMMAND_BANK_WITHDRAW);
    assert(rich4_game_event_value2(&resolved)==GAME_COMMAND_BANK_DEPOSIT);
    assert(state.special_event_phase==GAME_SPECIAL_PHASE_IDLE);
    assert(queue.state.waiting_for_command && queue.queue.count==1);
    command=rich4_game_command(GAME_COMMAND_BANK_WITHDRAW,
                               GAME_COMMAND_SOURCE_HUMAN,0,50);
    command.turn_token=92;
    assert(rich4_route_event_executor_submit(&queue,&state,&command,&events)==
           ROUTE_EVENT_COMMAND_APPLIED);
    assert(state.players[0].cash==150 && state.players[0].bank==150);
    assert(events.count==0);
    assert(!queue.state.waiting_for_command && queue.queue.count==1 &&
           queue.state.bank_stop_stage==1);

    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    resolved=pop(&events);
    assert(resolved.type==GAME_EVENT_INTERACTION_REQUESTED);
    assert(rich4_game_event_value(&resolved)==GAME_COMMAND_BANK_APPLY_LOAN);
    assert(rich4_game_event_value2(&resolved)==GAME_COMMAND_BANK_REPAY_LOAN);
    command=rich4_game_command(GAME_COMMAND_BANK_APPLY_LOAN,
                               GAME_COMMAND_SOURCE_HUMAN,0,50);
    command.turn_token=92;
    assert(rich4_route_event_executor_submit(&queue,&state,&command,&events)==
           ROUTE_EVENT_COMMAND_APPLIED);
    assert(state.players[0].cash==150 && state.players[0].bank==200 &&
           state.players[0].loan==50);
    resolved=pop(&events);
    assert(resolved.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&resolved)==ROUTE_EVENT_APPLIED);
    assert(!queue.state.waiting_for_command && queue.queue.count==0);
}

static void passive_special_rewards_apply_original_values(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop=route_event(ROUTE_EVENT_PHASE_STOP,20,0,
                                GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state)); state.active=1; state.player_count=1;
    state.players[0].play_state=1; state.players[0].linked_player=-1;
    state.players[0].points=65530;
    stop.snapshot.route_action=12;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,93));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].points==65530);
    event=pop(&events);
    assert(event.type==GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED &&
           rich4_game_event_subject(&event)==12 &&
           rich4_game_event_value(&event)==537 &&
           rich4_game_event_value2(&event)==1000);
    assert(rich4_route_event_executor_resume_special_place(
               &queue,&state,&events)==ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].points==4);
    event=pop(&events);
    assert(event.type==GAME_EVENT_INVENTORY_CHANGED &&
           rich4_game_event_value(&event)==10 &&
           rich4_game_event_value2(&event)==4);
    assert(pop(&events).type==GAME_EVENT_ROUTE_EVENT_RESOLVED);

    memset(&state,0,sizeof(state)); state.active=1; state.player_count=1;
    state.players[0].play_state=1; state.players[0].linked_player=-1;
    state.ai_rng_state=7;
    state.card_amount[3]=2;
    stop.snapshot.route_action=13;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,94));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event=pop(&events);
    assert(event.type==GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED &&
           rich4_game_event_subject(&event)==13 &&
           rich4_game_event_value(&event)==536 &&
           rich4_game_event_value2(&event)==0);
    assert(rich4_route_event_executor_resume_special_place(
               &queue,&state,&events)==ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].card_inventory[0]==4 &&
           state.card_amount[3]==1);
    event=pop(&events);
    assert(event.type==GAME_EVENT_INVENTORY_CHANGED &&
           rich4_game_event_subject_ref(&event).kind==GAME_DOMAIN_CARD &&
           rich4_game_event_subject_ref(&event).id==3);
    assert(pop(&events).type==GAME_EVENT_ROUTE_EVENT_RESOLVED);
}

static void confinement_visit_pauses_and_resumes(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop=route_event(ROUTE_EVENT_PHASE_STOP,20,0,
                                GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state)); state.active=1; state.player_count=2;
    state.players[0].play_state=1; state.players[1].play_state=1;
    state.players[0].linked_player=-1; state.players[1].linked_player=-1;
    state.players[0].points=30;
    state.players[1].days_in_prison=2;
    state.players[1].confinement_actor_type=GAME_CONFINEMENT_PRISON;
    stop.snapshot.route_action=4;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,95));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event=pop(&events);
    assert(event.type==GAME_EVENT_CONFINEMENT_VISIT_REQUESTED &&
           rich4_game_event_subject(&event)==GAME_CONFINEMENT_PRISON &&
           rich4_game_event_value(&event)==63 &&
           rich4_game_event_value2(&event)==64 &&
           rich4_game_event_flags(&event)==95);
    assert(rich4_route_event_executor_resume_confinement(
               &queue,&state,
               rich4_game_domain_ref(GAME_DOMAIN_PLAYER,1),&events)==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].points==0 &&
           state.players[1].days_in_prison==GAME_STATUS_PENDING_BIT);
    assert(pop(&events).type==GAME_EVENT_INVENTORY_CHANGED);
    assert(pop(&events).type==GAME_EVENT_STATUS_CHANGED);
    assert(pop(&events).type==GAME_EVENT_ACTOR_DIRTY_REQUESTED);
    assert(pop(&events).type==GAME_EVENT_SCENE_REFRESH_REQUESTED);
    assert(pop(&events).type==GAME_EVENT_ROUTE_EVENT_RESOLVED);

    state.players[1].days_in_hospital=2;
    state.players[1].confinement_actor_type=GAME_CONFINEMENT_HOSPITAL;
    stop.snapshot.route_action=5;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,96));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(pop(&events).type==GAME_EVENT_CONFINEMENT_VISIT_REQUESTED);
    assert(rich4_route_event_executor_cancel_confinement(
               &queue,&state,&events)==ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[1].days_in_hospital==2);
    event=pop(&events);
    assert(event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event)==ROUTE_EVENT_DECISION_DECLINED);
}

static void managed_special_tiles_use_automatic_branch(void)
{
    int scenario;
    for (scenario = 0; scenario < 3; scenario++) {
        GameState state;
        RouteEventExecutor executor;
        GameEventQueue events;
        RouteEvent event = route_event(
            scenario == 0 ? ROUTE_EVENT_PHASE_PASS : ROUTE_EVENT_PHASE_STOP,
            20, 0, GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
        memset(&state, 0, sizeof(state));
        state.active = 1; state.player_count = 1; state.price_index = 1;
        state.players[0].play_state = 5; /* managed human, not is_ai */
        state.players[0].linked_player = -1;
        state.players[0].cash = 100000;
        state.ai_rng_state = 1;
        event.snapshot.route_action = scenario == 2 ? 16 : 14;
        rich4_route_event_executor_reset(&executor);
        assert(rich4_route_event_queue_push(&executor.queue, &event));
        assert(rich4_route_event_executor_begin(&executor, 0, 96));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&executor, &state, &events) ==
               (scenario == 2 ? ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND :
                                ROUTE_EVENT_EXECUTION_PROGRESSED));
        while (events.count > 0) {
            GameEvent emitted = pop(&events);
            assert(emitted.type != GAME_EVENT_INTERACTION_REQUESTED);
            if (scenario == 2) {
                assert(emitted.type == GAME_EVENT_MAGIC_HOUSE_REQUESTED);
                assert(rich4_game_event_magic_house_auto_action(&emitted) == 6);
            }
        }
    }
}

static void ai_confinement_visit_keeps_original_point_threshold(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop=route_event(ROUTE_EVENT_PHASE_STOP,20,0,
                                GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state)); state.active=1; state.player_count=2;
    state.players[0].play_state=2; state.players[0].is_ai=1;
    state.players[0].traffic_method=2; state.players[0].points=30;
    state.players[0].ai_personality=0; /* car still selects ordinary players */
    state.players[1].play_state=1; state.players[1].days_in_prison=2;
    state.players[1].confinement_actor_type=GAME_CONFINEMENT_PRISON;
    state.ai_rng_state=3;
    stop.snapshot.route_action=4;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,97));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].points==30 &&
           state.players[1].days_in_prison==2);
    event=pop(&events);
    assert(event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event)==ROUTE_EVENT_APPLIED);

    state.players[0].points=31; state.ai_rng_state=3;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,98));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].points==31 && state.players[1].days_in_prison==2);
    event=pop(&events);
    assert(event.type==GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED);
    assert(rich4_route_event_executor_resume_confinement(
        &queue,&state,rich4_game_domain_ref(GAME_DOMAIN_PLAYER,1),&events)==
        ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].points==1 &&
           state.players[1].days_in_prison==GAME_STATUS_PENDING_BIT);

    memset(&state,0,sizeof(state)); state.active=1; state.player_count=1;
    /* Managed human: raw +21 is not 1, even when is_ai remains false. */
    state.players[0].play_state=5; state.players[0].is_ai=0;
    state.players[0].traffic_method=0; state.players[0].points=699;
    state.players[0].ai_personality=2; /* walking still selects special actors */
    state.special_player_confinement[0]=GAME_CONFINEMENT_HOSPITAL;
    state.special_player_f10[0]=1;
    state.hospital_node=40; state.ai_rng_state=3;
    stop.snapshot.route_action=5;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,99));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].points==699 &&
           state.special_player_confinement[0]==
               GAME_CONFINEMENT_HOSPITAL);

    state.players[0].points=700; state.ai_rng_state=3;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,100));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].points==700);
    event=pop(&events);
    assert(event.type==GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED);
    assert(rich4_route_event_executor_resume_confinement(
        &queue,&state,rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER,0),&events)==
        ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].points==400 &&
           state.special_player_confinement[0]==GAME_CONFINEMENT_NONE &&
           state.special_player_node[0]==40 &&
           state.special_player_previous_node[0]==0 &&
           state.special_player_f10[0]==0 &&
           state.special_player_play_state[0]==0);
}

static void automatic_point_minigames_share_original_fallback(void)
{
    int action;
    for (action = 6; action <= 8; action++) {
        GameState state;
        RouteEventExecutor queue;
        GameEventQueue events;
        GameEvent event;
        Rich4Rng expected_rng;
        int expected_amount;
        int expected_variant;
        RouteEvent stop=route_event(
            ROUTE_EVENT_PHASE_STOP,20,0,
            GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
        memset(&state,0,sizeof(state));
        state.active=1; state.player_count=1;
        /* Original 0x415215 tests the low play-state byte against 1. */
        state.players[0].play_state=2;
        state.players[0].character=action-6;
        state.players[0].points=65520;
        state.ai_rng_state=7;
        stop.snapshot.route_action=action;
        rich4_rng_state_seed(&expected_rng,7);
        expected_amount=rich4_rng_original_state_range(&expected_rng,20)+50;
        expected_variant=(int)(rich4_rng_original_state_next(&expected_rng)&1u);

        rich4_route_event_executor_reset(&queue);
        assert(rich4_route_event_queue_push(&queue.queue,&stop));
        assert(rich4_route_event_executor_begin(
            &queue,0,(unsigned int)(100+action)));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&queue,&state,&events)==
               ROUTE_EVENT_EXECUTION_PROGRESSED);
        assert(state.players[0].points==
               (uint16_t)(65520+expected_amount));
        assert(state.ai_rng_state==expected_rng.state);
        event=pop(&events);
        assert(event.type==
                   GAME_EVENT_POINT_MINIGAME_AUTOMATIC_RESOLVED &&
               rich4_game_event_subject(&event)==action &&
               rich4_game_event_value(&event)==expected_amount &&
               rich4_game_event_value2(&event)==expected_variant);
        event=pop(&events);
        assert(event.type==GAME_EVENT_INVENTORY_CHANGED &&
               rich4_game_event_value(&event)==expected_amount &&
               rich4_game_event_value2(&event)==state.players[0].points);
        event=pop(&events);
        assert(event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
               rich4_game_event_value(&event)==ROUTE_EVENT_APPLIED);
    }
}

static void ai_lottery_uses_strict_cash_threshold_and_empty_numbers(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    Rich4Rng expected_rng;
    uint32_t unchanged_rng;
    int i;
    RouteEvent stop=route_event(
        ROUTE_EVENT_PHASE_STOP,20,0,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state));
    state.active=1; state.player_count=1;
    state.players[0].play_state=2;
    state.players[0].cash=1001;
    state.lottery_pool=5000;
    for (i=0;i<36;i++) state.lottery_owner[i]=1;
    state.lottery_owner[17]=0;
    state.ai_rng_state=9;
    stop.snapshot.route_action=9;
    rich4_rng_state_seed(&expected_rng,9);
    (void)rich4_rng_original_state_range(&expected_rng,1);

    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,119));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].cash==1 && state.lottery_pool==6000 &&
           state.lottery_owner[17]==1 &&
           state.ai_rng_state==expected_rng.state);
    event=pop(&events);
    assert(event.type==GAME_EVENT_PLAYER_CASH_CHANGED &&
           rich4_game_event_value(&event)==1 &&
           rich4_game_event_value2(&event)==-1000);
    event=pop(&events);
    assert(event.type==GAME_EVENT_INVENTORY_CHANGED &&
           rich4_game_event_subject_ref(&event).kind==GAME_DOMAIN_NONE &&
           rich4_game_event_value(&event)==17 &&
           rich4_game_event_value2(&event)==6000);
    assert(pop(&events).type==GAME_EVENT_ROUTE_EVENT_RESOLVED);

    state.players[0].cash=1000;
    state.lottery_owner[17]=0;
    unchanged_rng=state.ai_rng_state;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,120));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].cash==1000 && state.lottery_pool==6000 &&
           state.lottery_owner[17]==0 &&
           state.ai_rng_state==unchanged_rng);
    event=pop(&events);
    assert(event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event)==ROUTE_EVENT_APPLIED);
}

static void human_lottery_allows_exactly_one_number(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop=route_event(
        ROUTE_EVENT_PHASE_STOP,20,0,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state));
    state.active=1; state.player_count=1;
    state.players[0].play_state=1;
    state.players[0].cash=1000;
    stop.snapshot.route_action=9;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,121));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event=pop(&events);
    assert(event.type==GAME_EVENT_LOTTERY_VISIT_REQUESTED &&
           rich4_game_event_value(&event)==12 &&
           rich4_game_event_value2(&event)==13 &&
           rich4_game_event_flags(&event)==14);

    assert(rich4_route_event_executor_purchase_lottery(
               &queue,&state,2,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].cash==0 && state.lottery_pool==1000 &&
           state.lottery_owner[2]==1 && queue.state.waiting_for_presentation &&
           queue.state.lottery_purchase_committed);
    assert(pop(&events).type==GAME_EVENT_PLAYER_CASH_CHANGED);
    assert(pop(&events).type==GAME_EVENT_INVENTORY_CHANGED);
    /* 0x430022 posts message 0x406/wParam 3; its handler sets state 5,
     * and timer case 5 closes instead of returning to number selection. */
    assert(rich4_route_event_executor_purchase_lottery(
               &queue,&state,3,&events)==
           ROUTE_EVENT_EXECUTION_INVALID);
    assert(events.count==0 && state.players[0].cash==0 &&
           state.lottery_pool==1000 && state.lottery_owner[3]==0);
    assert(rich4_route_event_executor_finish_lottery(
               &queue,&state,&events)==ROUTE_EVENT_EXECUTION_PROGRESSED);
    event=pop(&events);
    assert(event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event)==ROUTE_EVENT_APPLIED);
}

static void human_point_minigame_pauses_and_commits_score(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop=route_event(
        ROUTE_EVENT_PHASE_STOP,20,0,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state));
    state.active=1; state.player_count=1;
    state.players[0].play_state=1;
    state.players[0].points=100;
    stop.snapshot.route_action=6;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,122));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event=pop(&events);
    assert(event.type==GAME_EVENT_POINT_MINIGAME_REQUESTED &&
           rich4_game_event_subject(&event)==6 &&
           rich4_game_event_value(&event)==78 &&
           rich4_game_event_value2(&event)==79 &&
           rich4_game_event_flags(&event)==122);
    assert(queue.state.waiting_for_presentation &&
           queue.state.presentation_action==6);
    assert(rich4_route_event_executor_finish_point_minigame(
               &queue,&state,37,&events)==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].points==137);
    event=pop(&events);
    assert(event.type==GAME_EVENT_INVENTORY_CHANGED &&
           rich4_game_event_value(&event)==37 &&
           rich4_game_event_value2(&event)==137);
    event=pop(&events);
    assert(event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event)==ROUTE_EVENT_APPLIED);
}

static void human_shop_supports_card_and_tool_round_trips(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop=route_event(
        ROUTE_EVENT_PHASE_STOP,20,0x1774,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state,0,sizeof(state));
    state.active=1; state.player_count=1; state.company_count=4;
    state.players[0].play_state=1;
    state.players[0].points=1000;
    state.card_amount[0]=2;
    state.tool_amount[0]=2;
    stop.snapshot.route_action=15;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,123));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event=pop(&events);
    assert(event.type==GAME_EVENT_SHOP_VISIT_REQUESTED &&
           rich4_game_event_subject(&event)==15 &&
           rich4_game_event_value(&event)==10 &&
           rich4_game_event_value2(&event)==11 &&
           rich4_game_event_flags(&event)==123);

    assert(rich4_route_event_executor_shop_buy_card(
               &queue,&state,0,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].points==800 &&
           state.players[0].card_inventory[0]==1 &&
           state.card_amount[0]==1 &&
           state.company_period_business[3]==2000u &&
           state.company_total_business[3]==2000u);
    assert(events.count==2);
    (void)pop(&events); (void)pop(&events);

    assert(rich4_route_event_executor_shop_buy_tool(
               &queue,&state,0,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].points==785 &&
           state.players[0].tool_inventory[0]==1 &&
           state.tool_amount[0]==1 &&
           state.company_period_business[3]==2150u &&
           state.company_total_business[3]==2150u);
    assert(events.count==2);
    (void)pop(&events); (void)pop(&events);

    assert(rich4_route_event_executor_shop_sell_card_slot(
               &queue,&state,0,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].points==965 &&
           state.players[0].card_inventory[0]==0 &&
           state.card_amount[0]==2 &&
           state.company_period_business[3]==2350u &&
           state.company_total_business[3]==2350u);
    assert(events.count==2);
    (void)pop(&events); (void)pop(&events);

    assert(rich4_route_event_executor_shop_sell_tool(
               &queue,&state,0,&events)==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].points==978 &&
           state.players[0].tool_inventory[0]==0 &&
           state.tool_amount[0]==2 &&
           state.company_period_business[3]==2365u &&
           state.company_total_business[3]==2365u);
    assert(events.count==2);
    (void)pop(&events); (void)pop(&events);

    assert(rich4_route_event_executor_finish_shop(
               &queue,&state,&events)==ROUTE_EVENT_EXECUTION_PROGRESSED);
    event=pop(&events);
    assert(event.type==GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event)==ROUTE_EVENT_APPLIED);
}

static void ai_shop_uses_original_split_budgets_and_price_order(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    RouteEvent stop=route_event(
        ROUTE_EVENT_PHASE_STOP,20,0x1771,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    int tool;
    memset(&state,0,sizeof(state));
    state.active=1; state.player_count=1; state.company_count=1;
    state.players[0].play_state=2;
    state.players[0].points=1000;
    state.players[0].ai_personality=0;
    state.card_amount[0]=1;  /* price 200, difficulty gap 2: rejected */
    state.card_amount[2]=1;  /* price 35 */
    state.card_amount[8]=1;  /* price 160: must be bought first */
    for (tool=0;tool<8;tool++) state.tool_amount[tool]=1;
    stop.snapshot.route_action=15;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue,&stop));
    assert(rich4_route_event_executor_begin(&queue,0,124));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue,&state,&events)==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].card_inventory[0]==9 &&
           state.players[0].card_inventory[1]==3 &&
           state.card_amount[0]==1);
    assert(state.players[0].points==450);
    assert(state.players[0].tool_inventory[4]==1 &&
           state.players[0].tool_inventory[5]==1);
    assert(state.company_period_business[0]==5500u &&
           state.company_total_business[0]==5500u);
}

static void ai_shop_preserves_slot_difficulty_quirk(void)
{
    int personality;
    for (personality = 0; personality <= 2; personality += 2) {
        GameState state;
        RouteEventExecutor executor;
        GameEventQueue events;
        RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 20, 0,
            GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
        memset(&state, 0, sizeof(state));
        state.active = 1; state.player_count = 1;
        state.players[0].play_state = 2;
        state.players[0].is_ai = 1;
        state.players[0].ai_personality = personality;
        state.players[0].points = 100;
        state.players[0].card_inventory[0] = 3; /* difficulty 1, price 35 */
        state.players[0].card_inventory[1] = 4; /* difficulty 0, price 25 */
        stop.snapshot.route_action = 15;
        rich4_route_event_executor_reset(&executor);
        assert(rich4_route_event_queue_push(&executor.queue, &stop));
        assert(rich4_route_event_executor_begin(&executor, 0, 550));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&executor, &state, &events) ==
            ROUTE_EVENT_EXECUTION_PROGRESSED);
        if (personality == 0) {
            /* Slot zero uses card 1's difficulty 2. Both held cards are
             * sold as that slot is compacted, then repurchased by price. */
            assert(state.players[0].points == 93);
            assert(state.players[0].card_inventory[0] == 3 &&
                   state.players[0].card_inventory[1] == 4);
        } else {
            assert(state.players[0].points == 100 &&
                   state.players[0].card_inventory[0] == 3 &&
                   state.players[0].card_inventory[1] == 4);
        }
    }
}

static void ai_company_construction_and_issue_policy(void)
{
    GameState state;
    RouteEventExecutor executor;
    GameEventQueue events;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 20, 0x1771,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    int scenario;
    for (scenario = 0; scenario < 4; scenario++) {
        memset(&state, 0, sizeof(state));
        state.active = 1; state.player_count = 2; state.price_index = 1;
        state.players[0].play_state = 2; state.players[0].is_ai = 1;
        state.players[1].play_state = 1;
        state.players[0].cash = 10000;
        state.players[0].linked_player = state.players[1].linked_player = -1;
        state.company_count = 1; state.company_stock_index[0] = 0;
        state.company_type[0] = 11;
        state.stock_company_owner[0] = scenario == 0 ? 0 : 1;
        state.ai_rng_state = 42;
        state.land_count = 3;
        state.land_owner[0] = state.land_owner[1] = 0;
        state.land_owner[2] = 1;
        state.land_level[0] = state.land_level[1] = 2;
        state.land_toll_by_level[0][2] = 100;
        state.land_toll_by_level[1][2] = 200;
        state.land_toll_by_level[2][0] = 900;
        state.land_price[1] = 300;
        if (scenario == 2) state.land_owner[0] = state.land_owner[1] = 1;
        if (scenario == 3) state.stock_company_owner[0] = -1;
        rich4_route_event_executor_reset(&executor);
        assert(rich4_route_event_queue_push(&executor.queue, &stop));
        assert(rich4_route_event_executor_begin(&executor, 0, 551));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&executor, &state, &events) ==
            ROUTE_EVENT_EXECUTION_PROGRESSED);
        assert(state.land_level[0] == 2 && state.land_level[2] == 0);
        assert(state.land_level[1] == (scenario == 0 ? 4 : scenario == 1 ? 3 : 2));
        assert(state.players[0].cash ==
            (scenario == 1 ? 9700 : scenario == 2 ? 9000 : 10000));
        assert(state.ai_rng_state == 42); /* selection is deterministic */
    }
    memset(&state, 0, sizeof(state));
    state.active = 1; state.player_count = 1; state.price_index = 1;
    state.players[0].play_state = 2; state.players[0].is_ai = 1;
    state.players[0].cash = 159; state.initial_fund = 199;
    state.company_count = 1; state.company_type[0] = 2;
    state.company_stock_index[0] = 0;
    state.stock_company_owner[0] = -1; state.stock_company_auxiliary[0] = 0;
    state.stock_company_value[0] = 1000000;
    state.stock_company_available[0] = 100;
    state.stocks[0].f4 = 1;
    rich4_route_event_executor_reset(&executor);
    assert(rich4_route_event_queue_push(&executor.queue, &stop));
    assert(rich4_route_event_executor_begin(&executor, 0, 552));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&executor, &state, &events) ==
        ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.player_stock_amount[0][0] == 1 && state.players[0].cash == 59);
}

static void ai_landing_charge_defenses(void)
{
    GameState state;
    GameEventQueue events;
    int scenario, fee;
    for (scenario = 0; scenario < 5; scenario++) {
        memset(&state, 0, sizeof(state));
        state.active = 1; state.player_count = 3; state.price_index = 1;
        state.players[0].play_state = 2; state.players[0].is_ai = 1;
        state.players[1].play_state = state.players[2].play_state = 1;
        state.players[0].cash = state.players[2].cash = 20000;
        state.players[0].linked_player = state.players[1].linked_player =
            state.players[2].linked_player = -1;
        state.ai_rng_state = 1;
        state.land_count = 1; state.land_owner[0] = 1;
        state.land_toll_by_level[0][0] = scenario == 0 ? 1999 :
            scenario == 1 ? 2000 : 8000;
        state.players[0].card_inventory[0] = scenario < 3 ? 20 : 19;
        if (scenario == 3) state.players[0].relation_balance[2] = 100;
        if (scenario == 4) state.players[2].god_info = state.players[2].god_type = 14;
        rich4_game_event_queue_reset(&events);
        assert(rich4_land_charge(&state, 0, 0, &fee, &events) == GAME_PAYMENT_COMMITTED);
        if (scenario == 0) {
            assert(state.players[0].cash == 18001 && state.ai_rng_state == 1);
            assert(state.players[0].card_inventory[0] == 20);
        } else if (scenario == 1) {
            assert(state.players[0].cash == 18000 && state.ai_rng_state != 1);
            assert(state.players[0].card_inventory[0] == 20);
        } else if (scenario == 2) {
            assert(fee == 0 && state.players[0].cash == 20000 &&
                   state.players[0].card_inventory[0] == 0);
        } else {
            assert(state.players[0].cash == 20000 && state.players[2].cash == 12000);
            assert(state.players[0].card_inventory[0] == 0);
        }
    }
}

static void insurance_company_requires_chairman_for_roulette(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop = route_event(
        ROUTE_EVENT_PHASE_STOP, 20, 0x1771,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[0].cash = 100000;
    state.company_count = 1;
    state.company_type[0] = 4;
    state.company_stock_index[0] = 2;
    state.company_base_fee[0] = 100;
    state.stock_company_owner[2] = -1;
    state.stock_company_auxiliary[2] = 0;
    state.stock_company_value[2] = 1000000;
    state.stock_company_available[2] = 500;
    state.stocks[2].f4 = 1;
    stop.snapshot.route_action = 0;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 125));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_COMPANY &&
           state.players[0].days_assurance == 0 &&
           queue.state.presentation_mode == 4);

    state.stock_company_owner[2] = 0;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 126));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_FACILITY_ROULETTE_REQUESTED &&
           rich4_game_event_subject(&event) == 3 &&
           queue.state.presentation_mode == 3);
}

/* Original 0x41AB6D -> 0x41D1A9, and 0x41AC3C -> 0x41AEB4. */
static void insurance_visit_order(void)
{
    GameState state, before;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 20, 6001,
                                  GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    int god;
    memset(&state, 0, sizeof(state));
    state.active = 1; state.player_count = 2; state.price_index = 1;
    state.players[0].play_state = state.players[1].play_state = 1;
    state.players[0].cash = state.players[1].cash = 100000;
    state.players[0].linked_player = state.players[1].linked_player = -1;
    state.company_count = 1; state.company_type[0] = 4;
    state.company_stock_index[0] = 2; state.company_base_fee[0] = 100;
    state.stock_company_owner[2] = -1; state.stock_company_auxiliary[2] = 0;
    state.stock_company_value[2] = 1000000; state.stock_company_available[2] = 500;
    state.stocks[2].f4 = 1;
    stop.snapshot.route_action = 0;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 125));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(pop(&events).type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_buy_company_issue(&queue, &state, 100, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.stock_company_owner[2] == 0);
    assert(state.players[0].cash == 90000 && state.players[0].days_assurance == 0);
    while (rich4_game_event_queue_pop(&events, &event))
        assert(event.type != GAME_EVENT_FACILITY_ROULETTE_REQUESTED);
    before = state;
    /* Both ordinary and god-adjusted premiums wait before paying/issuing. */
    for (god = 0; god <= 1; god++) {
        state = before;
        state.players[1].god_type = god;
        state.players[1].god_info = god;
        rich4_route_event_executor_reset(&queue);
        stop.snapshot.player = 1;
        assert(rich4_route_event_queue_push(&queue.queue, &stop));
        assert(rich4_route_event_executor_begin(&queue, 1, 126));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&queue, &state, &events) ==
               ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
        assert(pop(&events).type == GAME_EVENT_FACILITY_ROULETTE_REQUESTED);
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_resume_company_roulette(&queue, &state, 10, &events) ==
               ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
        assert(state.players[1].cash == 100000 && state.players[1].days_assurance == 10);
        assert(state.company_total_business[0] == 0 && queue.state.presentation_mode == 30);
        assert(pop(&events).type == GAME_EVENT_STATUS_CHANGED);
        assert(pop(&events).type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED);
        assert(events.count == 0);
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_resume_charge(&queue, &state, &events) ==
               ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
        assert(state.players[1].days_assurance == 10); /* never added twice */
        assert(state.players[1].cash < 100000);
        assert(queue.state.presentation_mode == 4);
        while (rich4_game_event_queue_pop(&events, &event))
            assert(event.type != GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED);
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_buy_company_issue(&queue, &state, 200, &events) ==
               ROUTE_EVENT_EXECUTION_PROGRESSED);
        assert(state.stock_company_owner[2] == 1 && state.players[1].cash < 80000);
    }
    /* Existing chairman: free coverage, then issue opportunity. */
    state = before;
    stop.snapshot.player = 0;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 127));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_resume_company_roulette(&queue, &state, 5, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    assert(state.players[0].cash == 90000 && state.players[0].days_assurance == 5);
    assert(pop(&events).type == GAME_EVENT_STATUS_CHANGED);
    assert(pop(&events).type == GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED);
    assert(events.count == 0);
}

static void second_special_action_zero_is_original_noop(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 20, 0x1f41,
                                  GAME_PROPERTY_STOP_BRANCH_NONE);

    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.players[0].play_state = 1;
    stop.snapshot.route_action = 0;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 127));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    event = pop(&events);
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED);
    assert(queue.queue.count == 0 &&
           !rich4_route_event_executor_is_waiting(&queue));
}

static void facility_research_route_uses_original_selection(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 20, 0xfa1,
                                  GAME_PROPERTY_STOP_BRANCH_NONE);

    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 1;
    state.price_index = 1;
    state.players[0].play_state = 1;
    state.players[0].is_ai = 1;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 4;
    state.facility_level[0] = 2;
    state.facility_price[0] = 100;
    state.facility_price_per_level[0] = 100;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 128));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.facility_research_selection[0] == 2 &&
           state.facility_research_turns[0] == 5);
    event = pop(&events);
    assert(event.type == GAME_EVENT_STATUS_CHANGED &&
           rich4_game_event_value(&event) == 5);
    event = pop(&events);
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED);

    state.players[0].is_ai = 0;
    state.facility_research_selection[0] = 0;
    state.facility_research_turns[0] = 0;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 129));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_INTERACTION_REQUESTED &&
           rich4_game_event_value(&event) == GAME_COMMAND_UPGRADE_PROPERTY &&
           rich4_game_event_value2(&event) ==
               GAME_COMMAND_DECLINE_PROPERTY_UPGRADE);
    {
        GameCommand command = rich4_game_domain_command(
            GAME_COMMAND_UPGRADE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 1);
        command.turn_token = 129;
        assert(rich4_route_event_executor_submit(
                   &queue, &state, &command, &events) ==
               ROUTE_EVENT_COMMAND_APPLIED);
    }
    assert(state.facility_research_selection[0] == 2 &&
           state.facility_research_turns[0] == 5);
    event = pop(&events);
    assert(event.type == GAME_EVENT_STATUS_CHANGED);
    event = pop(&events);
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED);
}

static void beggar_stop_precedes_route_event(void)
{
    GameState state;
    RouteEventExecutor queue;
    GameEventQueue events;
    GameEvent event;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 1, 1,
                                  GAME_PROPERTY_STOP_BRANCH_NONE);

    memset(&state, 0, sizeof(state));
    state.active = 1;
    state.player_count = 3;
    state.price_index = 2;
    state.players[0].play_state = 1;
    state.players[0].cash = 2500;
    state.players[0].node = 1;
    state.players[1].node = 1;
    state.players[2].play_state = 1;
    state.tool_route_node_count = 4;
    state.tool_route_x[1] = 100; state.tool_route_y[1] = 100;
    state.tool_route_x[2] = 500; state.tool_route_y[2] = 500;
    state.tool_route_x[3] = 900; state.tool_route_y[3] = 500;
    state.tool_route_next[2][0] = 3;
    state.tool_route_next[3][0] = 2;
    rich4_route_event_executor_reset(&queue);
    assert(rich4_route_event_queue_push(&queue.queue, &stop));
    assert(rich4_route_event_executor_begin(&queue, 0, 131));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    event = pop(&events);
    assert(event.type == GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED &&
           rich4_game_event_beggar_player(&event) == 1 &&
           rich4_game_event_beggar_node(&event) == 1 &&
           rich4_game_event_beggar_amount(&event) == 2000 &&
           rich4_game_event_beggar_turn_token(&event) == 131);
    assert(queue.state.beggar_checked &&
           queue.state.presentation_action == GAME_ROUTE_EVENT_ACTION_BEGGAR);
    assert(rich4_route_event_executor_resume_beggar(
               &queue, &state, &events) == ROUTE_EVENT_EXECUTION_PROGRESSED);
    assert(state.players[0].cash == 500 && state.players[1].node != 1 &&
           queue.queue.count == 1 && events.count == 1);
    assert(pop(&events).type == GAME_EVENT_MONEY_TRANSFERRED);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&queue, &state, &events) ==
           ROUTE_EVENT_EXECUTION_PROGRESSED);
    event = pop(&events);
    assert(event.type == GAME_EVENT_ROUTE_EVENT_RESOLVED &&
           rich4_game_event_value(&event) == ROUTE_EVENT_APPLIED &&
           queue.queue.count == 0 && !queue.state.beggar_checked);
}

static void god_company_fee_continuation(void)
{
    GameState state;
    RouteEventExecutor executor;
    GameEventQueue events;
    RouteEvent stop = route_event(ROUTE_EVENT_PHASE_STOP, 20, 0x1771,
        GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    int type, god;
    const int gods[] = {1, 2, 5, 6};
    const int fees[] = {50, 0, 151, 202};
    for (type = 0; type < 2; type++) for (god = 0; god < 4; god++) {
        memset(&state, 0, sizeof(state));
        state.active = 1; state.player_count = 2; state.price_index = 1;
        state.players[0].play_state = state.players[1].play_state = 1;
        state.players[0].cash = 10000;
        state.players[0].god_info = state.players[0].god_type = gods[god];
        state.players[0].linked_player = state.players[1].linked_player = -1;
        state.company_count = 1; state.company_type[0] = type ? 11 : 12;
        state.company_base_fee[0] = 101; state.stock_company_owner[0] = 1;
        state.company_stock_index[0] = 0;
        state.land_count = 1; state.land_owner[0] = 0;
        state.land_price[0] = state.land_value[0] = state.fixed_asset_value[0] = 101;
        state.land_price_per_level[0] = 10;
        stop.snapshot.route_action = 0; stop.snapshot.dice_total = 1;
        rich4_route_event_executor_reset(&executor);
        assert(rich4_route_event_queue_push(&executor.queue, &stop));
        assert(rich4_route_event_executor_begin(&executor, 0, 500));
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_step(&executor, &state, &events) ==
            ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
        assert(state.players[0].cash == 10000);
        assert(state.land_level[0] == type); /* construction precedes quote */
        assert(executor.state.presentation_land == 101);
        rich4_game_event_queue_reset(&events);
        assert(rich4_route_event_executor_resume_charge(&executor, &state, &events) !=
            ROUTE_EVENT_EXECUTION_INVALID);
        assert(state.players[0].cash == 10000 - fees[god]);
        assert(state.land_level[0] == type); /* continuation never builds twice */
    }
}

int main(void)
{
    ai_shop_preserves_slot_difficulty_quirk();
    ai_company_construction_and_issue_policy();
    ai_landing_charge_defenses();
    assert(rich4_route_event_family_from_raw_id(0x7d1) ==
           ROUTE_EVENT_FAMILY_LAND);
    assert(rich4_route_event_family_from_raw_id(0xfa0) ==
           ROUTE_EVENT_FAMILY_UNKNOWN);
    assert(rich4_route_event_family_from_raw_id(0xfa1) ==
           ROUTE_EVENT_FAMILY_FACILITY);
    assert(rich4_route_event_family_from_raw_id(0x1770) ==
           ROUTE_EVENT_FAMILY_UNKNOWN);
    assert(rich4_route_event_family_from_raw_id(0x1771) ==
           ROUTE_EVENT_FAMILY_SPECIAL_TABLE_1);
    assert(rich4_route_event_family_from_raw_id(0x1f40) ==
           ROUTE_EVENT_FAMILY_UNKNOWN);
    assert(rich4_route_event_family_from_raw_id(0x1f41) ==
           ROUTE_EVENT_FAMILY_SPECIAL_TABLE_2);
    assert(rich4_route_event_family_from_raw_id(0x2710) ==
           ROUTE_EVENT_FAMILY_UNKNOWN);
    interaction_sequence();
    god_trade_yes_is_refused_for_all_blocking_gods();
    first_facility_uses_zero_based_domain_id();
    owned_unbuilt_facility_selects_type();
    nonowner_facility_executes_landing();
    magic_three_fortunes_sequence();
    magic_fortunes_continue_after_bankruptcy();
    eliminated_during_events();
    unpurchasable_stop_does_not_spend();
    unknown_allows_next_turn();
    ai_upgrade_and_toll_bankruptcy();
    news_presentation_pause_resume();
    bank_stop_itself_uses_two_pages();
    passive_special_rewards_apply_original_values();
    confinement_visit_pauses_and_resumes();
    ai_confinement_visit_keeps_original_point_threshold();
    managed_special_tiles_use_automatic_branch();
    automatic_point_minigames_share_original_fallback();
    ai_lottery_uses_strict_cash_threshold_and_empty_numbers();
    human_lottery_allows_exactly_one_number();
    human_point_minigame_pauses_and_commits_score();
    human_shop_supports_card_and_tool_round_trips();
    ai_shop_uses_original_split_budgets_and_price_order();
    god_company_fee_continuation();
    insurance_company_requires_chairman_for_roulette();
    insurance_visit_order();
    insurance_automatic_issue_and_zero_quote();
    second_special_action_zero_is_original_noop();
    facility_research_route_uses_original_selection();
    beggar_stop_precedes_route_event();
    puts("route event executor pause/resume: ok");
    return 0;
}
