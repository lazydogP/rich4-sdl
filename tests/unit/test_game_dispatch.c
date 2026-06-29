#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_command_flow.h"
#include "core/game_dispatch.h"
#include "core/game_event_payload.h"

static GameEvent protocol_event(GameEventType type, int value)
{
    Rich4GameEventFields fields = {
        0, 0, value, 0, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1)
    };
    switch (type) {
    case GAME_EVENT_MONEY_TRANSFERRED:
        return rich4_game_event_money_transferred_from_fields(fields);
    case GAME_EVENT_PROPERTY_OWNER_CHANGED:
        return rich4_game_event_property_owner_changed_from_fields(fields);
    case GAME_EVENT_PROPERTY_LEVEL_CHANGED:
        return rich4_game_event_property_level_changed_from_fields(fields);
    case GAME_EVENT_INVENTORY_CHANGED:
        return rich4_game_event_inventory_changed_from_fields(fields);
    case GAME_EVENT_STATUS_CHANGED:
        return rich4_game_event_status_changed_from_fields(fields);
    case GAME_EVENT_MARKET_CHANGED:
        return rich4_game_event_market_changed_from_fields(fields);
    case GAME_EVENT_INTERACTION_REQUESTED:
        return rich4_game_event_interaction_requested_from_fields(fields);
    case GAME_EVENT_PLAYER_ELIMINATED:
        return rich4_game_event_player_eliminated_from_fields(fields);
    case GAME_EVENT_GAME_ENDED:
        return rich4_game_event_game_ended_from_fields(fields);
    case GAME_EVENT_SAVE_RESULT:
        return rich4_game_event_save_result_from_fields(fields);
    case GAME_EVENT_LOAD_RESULT:
        return rich4_game_event_load_result_from_fields(fields);
    case GAME_EVENT_CARD_USED:
        return rich4_game_event_card_used_from_fields(fields);
    case GAME_EVENT_PLAYER_CASH_CHANGED:
        return rich4_game_event_player_cash_changed_from_fields(fields);
    case GAME_EVENT_TOOL_USED:
        return rich4_game_event_tool_used_from_fields(fields);
    case GAME_EVENT_MAP_OBJECT_CHANGED:
        return rich4_game_event_map_object_changed_from_fields(fields);
    case GAME_EVENT_TRAFFIC_CHANGED:
        return rich4_game_event_traffic_changed_from_fields(fields);
    default:
        return rich4_game_event_command_rejected_from_fields(fields);
    }
}

static GameEvent pop(GameEventQueue *queue)
{
    GameEvent event;
    assert(rich4_game_event_queue_pop(queue, &event));
    return event;
}

int main(void)
{
    static const GameEventType protocol_events[] = {
        GAME_EVENT_MONEY_TRANSFERRED,
        GAME_EVENT_PROPERTY_OWNER_CHANGED,
        GAME_EVENT_PROPERTY_LEVEL_CHANGED,
        GAME_EVENT_INVENTORY_CHANGED,
        GAME_EVENT_STATUS_CHANGED,
        GAME_EVENT_MARKET_CHANGED,
        GAME_EVENT_INTERACTION_REQUESTED,
        GAME_EVENT_PLAYER_ELIMINATED,
        GAME_EVENT_GAME_ENDED,
        GAME_EVENT_SAVE_RESULT,
        GAME_EVENT_LOAD_RESULT,
        GAME_EVENT_CARD_USED,
        GAME_EVENT_PLAYER_CASH_CHANGED,
        GAME_EVENT_TOOL_USED,
        GAME_EVENT_MAP_OBJECT_CHANGED,
        GAME_EVENT_TRAFFIC_CHANGED
    };
    GameState state;
    GameFlowState flow;
    GameEventQueue events;
    GameCommand command;
    GameEvent event;
    int i;
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    state.active = 1;
    state.player_count = 2;
    state.current_player = 0;
    state.players[0].traffic_method = 2;
    state.players[0].play_state = 1;
    state.players[1].play_state = 1;
    rich4_game_event_queue_reset(&events);

    command = rich4_game_command(GAME_COMMAND_ROLL_DICE,
                                 GAME_COMMAND_SOURCE_HUMAN, 0, 0);
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    event = pop(&events);
    assert(event.type == GAME_EVENT_TURN_FLOW_REQUESTED);

    state.game_over = 1;
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).reject_reason == GAME_COMMAND_REJECT_STATE);
    state.game_over = 0;

    flow.turn_flow_active = 1;
    flow.turn_state = 2;
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).type == GAME_EVENT_DICE_ROLL_REQUESTED);

    flow.turn_flow_active = 0;
    command = rich4_game_command(GAME_COMMAND_SELECT_DICE_COUNT,
                                 GAME_COMMAND_SOURCE_HUMAN, 0, 3);
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    event = pop(&events);
    assert(event.type == GAME_EVENT_DICE_COUNT_CHANGED &&
           rich4_game_event_value(&event) == 3);
    assert(state.players[0].ndices == 3);

    /* 0x4221C0 adjusts automatic dice count after PREROLL is entered. */
    flow.turn_flow_active = 1;
    flow.turn_state = GAME_TURN_STATE_PREROLL;
    state.players[0].is_ai = 1;
    command = rich4_game_command(GAME_COMMAND_SELECT_DICE_COUNT,
                                 GAME_COMMAND_SOURCE_AI, 0, 1);
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).type == GAME_EVENT_DICE_COUNT_CHANGED);
    assert(state.players[0].ndices == 1);
    flow.turn_state = GAME_TURN_STATE_MOVE;
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).reject_reason == GAME_COMMAND_REJECT_STATE);
    flow.turn_state = GAME_TURN_STATE_PREROLL;
    state.players[0].is_ai = 0;
    command.source = GAME_COMMAND_SOURCE_HUMAN;
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).reject_reason == GAME_COMMAND_REJECT_STATE);
    flow.turn_flow_active = 0;
    flow.turn_state = GAME_TURN_STATE_IDLE;

    {
        int *locked_status[] = {
            &state.players[0].delayed_status_block,
            &state.players[0].days_disappearing,
            &state.players[0].days_in_prison,
            &state.players[0].days_in_hospital,
            &state.players[0].days_sleeping,
            &state.players[0].days_sleep_walking
        };
        unsigned int status_index;
        for (status_index = 0;
             status_index < sizeof(locked_status) / sizeof(locked_status[0]);
             status_index++) {
            *locked_status[status_index] = 2;
            command = rich4_game_command(
                GAME_COMMAND_ROLL_DICE,
                GAME_COMMAND_SOURCE_HUMAN, 0, 0);
            assert(!rich4_game_dispatch_command(
                &state, &flow, &command, &events));
            assert(pop(&events).reject_reason ==
                   GAME_COMMAND_REJECT_STATE);
            command = rich4_game_command(
                GAME_COMMAND_SELECT_DICE_COUNT,
                GAME_COMMAND_SOURCE_HUMAN, 0, 1);
            state.players[0].ndices = 3;
            assert(!rich4_game_dispatch_command(
                &state, &flow, &command, &events));
            assert(pop(&events).reject_reason ==
                   GAME_COMMAND_REJECT_STATE);
            assert(state.players[0].ndices == 3);
            *locked_status[status_index] = 0;
        }
        state.players[0].days_sleep_walking = 2;
        flow.turn_flow_active = 1;
        flow.turn_state = GAME_TURN_STATE_PREROLL;
        command = rich4_game_command(
            GAME_COMMAND_ROLL_DICE,
            GAME_COMMAND_SOURCE_HUMAN, 0, 0);
        assert(rich4_game_dispatch_command(
            &state, &flow, &command, &events));
        assert(pop(&events).type == GAME_EVENT_DICE_ROLL_REQUESTED);
        flow.turn_flow_active = 0;
        flow.turn_state = GAME_TURN_STATE_IDLE;
        state.players[0].days_sleep_walking = 0;
    }

    command = rich4_game_command(GAME_COMMAND_SELECT_DICE_COUNT,
                                 GAME_COMMAND_SOURCE_HUMAN, 0, 3);
    command.value = 4;
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).reject_reason == GAME_COMMAND_REJECT_INVALID);
    command = rich4_game_command(GAME_COMMAND_ROLL_DICE,
                                 GAME_COMMAND_SOURCE_AI, 0, 0);
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).reject_reason == GAME_COMMAND_REJECT_SOURCE);
    command = rich4_game_command(GAME_COMMAND_ROLL_DICE,
                                 GAME_COMMAND_SOURCE_HUMAN, 1, 0);
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).reject_reason == GAME_COMMAND_REJECT_PLAYER);

    state.current_player = 1;
    state.players[1].is_ai = 1;
    command = rich4_game_command(GAME_COMMAND_ROLL_DICE,
                                 GAME_COMMAND_SOURCE_AI, 1, 0);
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(pop(&events).type == GAME_EVENT_TURN_FLOW_REQUESTED);

    state.current_player = 0;
    state.players[0].is_ai = 0;
    command = rich4_game_domain_command(
        GAME_COMMAND_PURCHASE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    {
        GameState before_state = state;
        GameFlowState before_flow = flow;
        assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
        event = pop(&events);
        assert(event.type == GAME_EVENT_COMMAND_REJECTED &&
               event.reject_reason == GAME_COMMAND_REJECT_STATE);
        assert(memcmp(&state, &before_state, sizeof(state)) == 0);
        assert(memcmp(&flow, &before_flow, sizeof(flow)) == 0);
    }

    rich4_game_event_queue_reset(&events);
    state.players[0].tool_inventory[4] = 1;
    state.players[0].traffic_method = 0;
    command = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 4),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(state.players[0].traffic_method == 1 &&
           state.players[0].tool_inventory[4] == 0);
    rich4_game_event_queue_reset(&events);
    state.players[0].tool_inventory[0] = 1;
    state.players[0].node = 1;
    state.players[0].next_node = 2;
    state.tool_route_node_count = 3;
    state.tool_route_next[1][0] = 2;
    state.tool_route_next[2][0] = 1;
    command.subject.id = 0;
    assert(rich4_game_dispatch_command(
        &state, &flow, &command, &events));
    assert(state.tool_robot.active &&
           state.players[0].tool_inventory[0] == 0);

    rich4_game_event_queue_reset(&events);
    state.players[0].card_inventory[0] = 14;
    command = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 13),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), 0);
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(state.players[1].days_stopping == 1 &&
           state.players[0].card_inventory[0] == 0);
    rich4_game_event_queue_reset(&events);
    state.players[0].card_inventory[0] = 2;
    command.subject.id = 1;
    assert(rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(state.players[0].card_inventory[0] == 0);

    rich4_game_event_queue_reset(&events);
    for (i = 0; i < (int)(sizeof(protocol_events) / sizeof(protocol_events[0])); i++) {
        event = protocol_event(protocol_events[i], i);
        assert(rich4_game_event_queue_push(&events, &event));
    }
    for (i = 0; i < (int)(sizeof(protocol_events) / sizeof(protocol_events[0])); i++) {
        event = pop(&events);
        assert(event.type == protocol_events[i] &&
               rich4_game_event_value(&event) == i);
        assert(rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_LAND &&
               rich4_game_event_target_ref(&event).kind == GAME_DOMAIN_PLAYER);
    }

    rich4_game_event_queue_reset(&events);
    event = rich4_game_event_dice_roll_requested_from_fields(
        (Rich4GameEventFields){
            0, 0, 0, 0, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
        });
    for (i = 0; i < GAME_EVENT_QUEUE_CAPACITY; i++)
        assert(rich4_game_event_queue_push(&events, &event));
    assert(!rich4_game_event_queue_push(&events, &event));
    assert(events.dropped == 1);
    command = rich4_game_command(GAME_COMMAND_SELECT_DICE_COUNT,
                                 GAME_COMMAND_SOURCE_HUMAN, 0, 2);
    state.players[0].ndices = 1;
    assert(!rich4_game_dispatch_command(&state, &flow, &command, &events));
    assert(state.players[0].ndices == 1);

    {
        GameSession session;
        Rich4AppCommand app_command;
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.players[0].play_state = 1;
        session.state.current_player = 0;
        command = rich4_game_command_roll_dice(
            GAME_COMMAND_SOURCE_HUMAN, 0);
        app_command = rich4_app_game_command(&command);
        rich4_game_event_queue_reset(&events);
        assert(rich4_app_dispatch_game_command(
            &session, &app_command, &events));
        assert(pop(&events).type == GAME_EVENT_TURN_FLOW_REQUESTED);
        rich4_game_event_queue_reset(&events);
        assert(rich4_app_apply_command(&session, &app_command, &events));
        assert(pop(&events).type == GAME_EVENT_TURN_FLOW_REQUESTED);
        app_command.type = RICH4_APP_COMMAND_POINTER_MOVE;
        assert(!rich4_app_dispatch_game_command(
            &session, &app_command, &events));
    }
    {
        GameSession session;
        GameCommand animation_command;
        memset(&session, 0, sizeof(session));
        session.state.player_count = 2;
        session.state.land_count = 1;
        session.state.land_owner[0] = 1;
        animation_command = rich4_game_domain_command(
            GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_AI, 1,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, 10),
            rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), 0);
        rich4_app_sync_ai_card_animation(
            &session, &animation_command, 1, 557, 3, 1000);
        assert(session.presentation.monster_card.active == 1 &&
               session.presentation.monster_card.resource == 557 &&
               session.presentation.monster_card.sound_resource == 80 &&
               session.presentation.monster_card.frame == 0 &&
               session.presentation.monster_card.speech_player == 1 &&
               session.presentation.monster_card.started == 1000 &&
               session.presentation.monster_card.until == 1740);
        rich4_app_sync_ai_card_animation(
            &session, &animation_command, 1, 529, 2, 2000);
        assert(session.presentation.monster_card.resource == 557 &&
               session.presentation.monster_card.until == 1740);
        session.presentation.monster_card.active = 0;
        animation_command.subject.id = 11;
        rich4_app_sync_ai_card_animation(
            &session, &animation_command, 1, 529, 2, 2000);
        assert(session.presentation.monster_card.active == 1 &&
               session.presentation.monster_card.resource == 529 &&
               session.presentation.monster_card.sound_resource == 97 &&
               session.presentation.monster_card.speech_player == -1 &&
               session.presentation.monster_card.until == 2660);
    }
    puts("GameCommand -> GameEvent dispatch: ok");
    return 0;
}
