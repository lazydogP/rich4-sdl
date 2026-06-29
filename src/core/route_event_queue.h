#ifndef RICH4_CORE_ROUTE_EVENT_QUEUE_H
#define RICH4_CORE_ROUTE_EVENT_QUEUE_H

#include "core/route_event.h"

#include "core/game_constants.h"

#define ROUTE_EVENT_QUEUE_CAPACITY (GAME_MAX_TURN_ROUTE_EVENTS + 1)

typedef struct RouteEventQueue {
    RouteEvent events[ROUTE_EVENT_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
} RouteEventQueue;

void rich4_route_event_queue_reset(RouteEventQueue *queue);
int rich4_route_event_queue_push(RouteEventQueue *queue, const RouteEvent *event);
const RouteEvent *rich4_route_event_queue_peek(const RouteEventQueue *queue);
int rich4_route_event_queue_pop(RouteEventQueue *queue, RouteEvent *out_event);

#endif
