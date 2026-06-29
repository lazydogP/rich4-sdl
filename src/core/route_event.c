#include "core/route_event.h"

#include <string.h>

void rich4_route_event_collection_reset(RouteEventCollection *collection)
{
    if (collection == NULL) return;
    collection->count = 0;
    memset(collection->events, 0, sizeof(collection->events));
}

int rich4_route_event_collection_push(RouteEventCollection *collection,
                                      const RouteEvent *event)
{
    if (collection == NULL || event == NULL ||
        collection->count >= ROUTE_EVENT_COLLECTION_CAPACITY)
        return 0;
    collection->events[collection->count++] = *event;
    return 1;
}

const RouteEvent *rich4_route_event_collection_at(
    const RouteEventCollection *collection, int index)
{
    if (collection == NULL || index < 0 || index >= collection->count)
        return NULL;
    return &collection->events[index];
}

const RouteEvent *rich4_route_event_collection_last_phase(
    const RouteEventCollection *collection, RouteEventPhase phase)
{
    int i;
    if (collection == NULL) return NULL;
    for (i = collection->count - 1; i >= 0; i--) {
        if (collection->events[i].phase == phase &&
            collection->events[i].snapshot.event_kind != GAME_ROUTE_EVENT_NONE)
            return &collection->events[i];
    }
    return NULL;
}

int rich4_route_event_collection_count_phase(
    const RouteEventCollection *collection, RouteEventPhase phase)
{
    int i;
    int count = 0;
    if (collection == NULL) return 0;
    for (i = 0; i < collection->count; i++) {
        if (collection->events[i].phase == phase &&
            collection->events[i].snapshot.event_kind != GAME_ROUTE_EVENT_NONE)
            count++;
    }
    return count;
}

#include "core/game_constants.h"

RouteEventHandlerResult rich4_route_event_default_result(
    const RouteEvent *event)
{
    if (event == NULL) return ROUTE_EVENT_INVALID_STATE;
    if (event->phase == ROUTE_EVENT_PHASE_DONE) return ROUTE_EVENT_APPLIED;
    switch (event->snapshot.event_kind) {
    case GAME_ROUTE_EVENT_ROAD_NODE:
        return ROUTE_EVENT_APPLIED;
    case GAME_ROUTE_EVENT_LAND:
    case GAME_ROUTE_EVENT_FACILITY:
    case GAME_ROUTE_EVENT_STOCK_SPECIAL:
    case GAME_ROUTE_EVENT_SECOND_SPECIAL:
        return ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED;
    default:
        return ROUTE_EVENT_UNKNOWN_SKIPPED;
    }
}

RouteEventObjectFamily rich4_route_event_family_from_raw_id(int raw_object_id)
{
    if (raw_object_id > 0 && raw_object_id < 0x7d0)
        return ROUTE_EVENT_FAMILY_ROAD;
    if (raw_object_id >= 0x7d1 && raw_object_id < 0xfa0)
        return ROUTE_EVENT_FAMILY_LAND;
    if (raw_object_id >= 0xfa1 && raw_object_id < 0x1770)
        return ROUTE_EVENT_FAMILY_FACILITY;
    if (raw_object_id >= 0x1771 && raw_object_id < 0x1f40)
        return ROUTE_EVENT_FAMILY_SPECIAL_TABLE_1;
    if (raw_object_id >= 0x1f41 && raw_object_id < 0x2710)
        return ROUTE_EVENT_FAMILY_SPECIAL_TABLE_2;
    return ROUTE_EVENT_FAMILY_UNKNOWN;
}

int rich4_route_event_object_type_from_raw_id(int raw_object_id)
{
    if (raw_object_id == 0 || raw_object_id == 0x7d0)
        return GAME_ROUTE_OBJECT_NONE;
    if (raw_object_id > 0 && raw_object_id < 0x7d0)
        return GAME_ROUTE_OBJECT_NODE;
    if (raw_object_id < 0xfa0)
        return GAME_ROUTE_OBJECT_LAND;
    if (raw_object_id < 0x1770)
        return GAME_ROUTE_OBJECT_FACILITY;
    if (raw_object_id < 0x1f40)
        return GAME_ROUTE_OBJECT_STOCK_SPECIAL;
    if (raw_object_id < 0x2710)
        return GAME_ROUTE_OBJECT_SECOND_SPECIAL;
    return GAME_ROUTE_OBJECT_UNKNOWN;
}

int rich4_route_event_object_index_from_raw_id(int raw_object_id)
{
    switch (rich4_route_event_object_type_from_raw_id(raw_object_id)) {
    case GAME_ROUTE_OBJECT_NODE:
        return raw_object_id;
    case GAME_ROUTE_OBJECT_LAND:
        return raw_object_id - 0x7d1;
    case GAME_ROUTE_OBJECT_FACILITY:
        return raw_object_id - 0xfa1;
    case GAME_ROUTE_OBJECT_STOCK_SPECIAL:
        return raw_object_id - 0x1771;
    case GAME_ROUTE_OBJECT_SECOND_SPECIAL:
        return raw_object_id - 0x1f41;
    default:
        return -1;
    }
}
