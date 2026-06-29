#include "core/route_event_queue.h"

#include <string.h>

void rich4_route_event_queue_reset(RouteEventQueue *queue)
{
    if (queue != NULL) memset(queue, 0, sizeof(*queue));
}

int rich4_route_event_queue_push(RouteEventQueue *queue, const RouteEvent *event)
{
    if (queue == NULL || event == NULL ||
        queue->count >= ROUTE_EVENT_QUEUE_CAPACITY) return 0;
    queue->events[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % ROUTE_EVENT_QUEUE_CAPACITY;
    queue->count++;
    return 1;
}

const RouteEvent *rich4_route_event_queue_peek(const RouteEventQueue *queue)
{
    if (queue == NULL || queue->count == 0) return NULL;
    return &queue->events[queue->head];
}

int rich4_route_event_queue_pop(RouteEventQueue *queue, RouteEvent *out_event)
{
    if (queue == NULL || queue->count == 0) return 0;
    if (out_event != NULL) *out_event = queue->events[queue->head];
    memset(&queue->events[queue->head], 0, sizeof(queue->events[queue->head]));
    queue->head = (queue->head + 1) % ROUTE_EVENT_QUEUE_CAPACITY;
    queue->count--;
    return 1;
}
