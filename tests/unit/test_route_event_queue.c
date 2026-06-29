#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/route_event_queue.h"

static RouteEvent event(RouteEventPhase phase, int node, int kind)
{
    RouteEvent value;
    memset(&value, 0, sizeof(value));
    value.phase = phase;
    value.node = node;
    value.snapshot.event_kind = kind;
    return value;
}

int main(void)
{
    RouteEventQueue queue;
    RouteEvent popped;
    RouteEvent pass1 = event(ROUTE_EVENT_PHASE_PASS, 2,
                             GAME_ROUTE_EVENT_ROAD_NODE);
    RouteEvent pass2 = event(ROUTE_EVENT_PHASE_PASS, 3,
                             GAME_ROUTE_EVENT_LAND);
    RouteEvent stop = event(ROUTE_EVENT_PHASE_STOP, 4,
                            GAME_ROUTE_EVENT_FACILITY);

    assert(rich4_route_event_object_type_from_raw_id(0x7d0) ==
           GAME_ROUTE_OBJECT_NONE);
    assert(rich4_route_event_object_type_from_raw_id(0x7d2) ==
           GAME_ROUTE_OBJECT_LAND);
    assert(rich4_route_event_object_index_from_raw_id(0x7d2) == 1);
    assert(rich4_route_event_object_type_from_raw_id(0x1f41) ==
           GAME_ROUTE_OBJECT_SECOND_SPECIAL);

    rich4_route_event_queue_reset(&queue);
    assert(rich4_route_event_queue_peek(&queue) == NULL);
    assert(!rich4_route_event_queue_pop(&queue, &popped));
    assert(rich4_route_event_queue_push(&queue, &pass1));
    assert(rich4_route_event_queue_push(&queue, &pass2));
    assert(rich4_route_event_queue_push(&queue, &stop));
    assert(queue.count == 3);
    assert(rich4_route_event_queue_peek(&queue)->node == 2);
    assert(rich4_route_event_queue_pop(&queue, &popped));
    assert(popped.node == 2 && queue.count == 2);
    assert(rich4_route_event_queue_pop(&queue, &popped));
    assert(popped.node == 3 && queue.count == 1);
    assert(rich4_route_event_queue_push(&queue, &pass1));
    assert(rich4_route_event_queue_pop(&queue, &popped));
    assert(popped.node == 4);
    assert(rich4_route_event_queue_pop(&queue, &popped));
    assert(popped.node == 2 && queue.count == 0);
    assert(rich4_route_event_queue_peek(&queue) == NULL);

    rich4_route_event_queue_reset(&queue);
    memset(&popped, 0, sizeof(popped));
    for (popped.node = 0; popped.node < ROUTE_EVENT_QUEUE_CAPACITY; popped.node++)
        assert(rich4_route_event_queue_push(&queue, &popped));
    assert(!rich4_route_event_queue_push(&queue, &pass1));
    puts("route event queue: ok");
    return 0;
}
