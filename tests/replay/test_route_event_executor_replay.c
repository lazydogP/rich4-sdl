#include <stdio.h>
#include <string.h>

#include "core/game_dispatch.h"

typedef struct Replay {
    char text[2048];
    size_t used;
} Replay;

static int append_events(Replay *replay, GameEventQueue *events)
{
    GameEvent event;
    while (rich4_game_event_queue_pop(events, &event)) {
        int written = snprintf(replay->text + replay->used,
                               sizeof(replay->text) - replay->used,
                               "%d,%d,%d,%d,%u,%d,%d\n",
                               event.type, event.player,
                               rich4_game_event_value(&event),
                               rich4_game_event_value2(&event),
                               rich4_game_event_flags(&event),
                               rich4_game_event_subject_ref(&event).kind,
                               rich4_game_event_subject_ref(&event).id);
        if (written <= 0 || (size_t)written >=
            sizeof(replay->text) - replay->used) return 0;
        replay->used += (size_t)written;
    }
    return 1;
}

static RouteEvent event(RouteEventPhase phase, int node, int raw, int branch)
{
    RouteEvent value;
    memset(&value, 0, sizeof(value));
    value.phase = phase;
    value.node = node;
    value.object_ref = rich4_map_object_ref((unsigned short)raw);
    value.snapshot.player = 0;
    value.snapshot.property_branch = branch;
    value.snapshot.property_owner = -1;
    return value;
}

static int generate(Replay *replay)
{
    GameState state;
    GameFlowState flow;
    GameEventQueue events;
    GameCommand commands[1];
    int command_index = 0;
    int guard;
    RouteEvent pass1 = event(ROUTE_EVENT_PHASE_PASS, 2, 1,
                             GAME_PROPERTY_STOP_BRANCH_NONE);
    RouteEvent pass2 = event(ROUTE_EVENT_PHASE_PASS, 3, 0x1771,
                             GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED);
    RouteEvent stop = event(ROUTE_EVENT_PHASE_STOP, 4, 0x7d1,
                            GAME_PROPERTY_STOP_BRANCH_UNOWNED);
    memset(replay, 0, sizeof(*replay));
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    state.active = 1;
    state.price_index = 1;
    state.player_count = 1;
    state.players[0].play_state = 1;
    state.land_count = 1;
    state.land_owner[0] = -1;
    rich4_route_event_executor_reset(&flow.route_events);
    if (!rich4_route_event_queue_push(&flow.route_events.queue, &pass1) ||
        !rich4_route_event_queue_push(&flow.route_events.queue, &pass2) ||
        !rich4_route_event_queue_push(&flow.route_events.queue, &stop) ||
        !rich4_route_event_executor_begin(&flow.route_events, 0, 91)) return 0;
    flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    commands[0] = rich4_game_domain_command(
        GAME_COMMAND_DECLINE_PROPERTY_PURCHASE,
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
    commands[0].turn_token = 91;

    for (guard = 0; guard < 16; guard++) {
        RouteEventExecutionResult result;
        rich4_game_event_queue_reset(&events);
        result = rich4_route_event_executor_step(&flow.route_events,
                                                  &state, &events);
        if (!append_events(replay, &events)) return 0;
        if (result == ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND) {
            if (command_index >= 1) return 0;
            rich4_game_event_queue_reset(&events);
            if (!rich4_game_dispatch_command(&state, &flow,
                                             &commands[command_index++],
                                             &events) ||
                !append_events(replay, &events)) return 0;
        } else if (result == ROUTE_EVENT_EXECUTION_FINISHED) {
            return command_index == 1;
        } else if (result == ROUTE_EVENT_EXECUTION_INVALID) {
            return 0;
        }
    }
    return 0;
}

int main(void)
{
    Replay first;
    Replay second;
    if (!generate(&first) || !generate(&second) ||
        strcmp(first.text, second.text) != 0 ||
        strstr(first.text, "25,0,6,7,91,2,0\n") == NULL ||
        strstr(first.text, "18,0,6,4,2001,0,-1\n") == NULL) {
        fprintf(stderr, "route-event replay mismatch\n%s", first.text);
        return 1;
    }
    puts(first.text);
    puts("headless route-event decision replay: ok");
    return 0;
}
