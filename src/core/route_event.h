#ifndef RICH4_CORE_ROUTE_EVENT_H
#define RICH4_CORE_ROUTE_EVENT_H

#include "core/game_constants.h"
#include "data/map_object_ref.h"

#define ROUTE_EVENT_COLLECTION_CAPACITY (GAME_MAX_TURN_ROUTE_EVENTS + 1)

/* RouteEvent is the immutable movement fact passed from route construction to
 * the executor. It owns no queue cursor, pending command, or UI state. */
typedef enum RouteEventPhase {
    ROUTE_EVENT_PHASE_PASS = 1,
    ROUTE_EVENT_PHASE_STOP,
    ROUTE_EVENT_PHASE_DONE
} RouteEventPhase;

typedef enum RouteEventHandlerResult {
    ROUTE_EVENT_APPLIED = 1,
    ROUTE_EVENT_WAITING_FOR_COMMAND,
    ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED,
    ROUTE_EVENT_INVALID_STATE,
    ROUTE_EVENT_UNKNOWN_SKIPPED,
    ROUTE_EVENT_DECISION_DECLINED,
    ROUTE_EVENT_ACTOR_ELIMINATED
} RouteEventHandlerResult;

typedef enum RouteEventObjectFamily {
    ROUTE_EVENT_FAMILY_UNKNOWN = 0,
    ROUTE_EVENT_FAMILY_ROAD,
    ROUTE_EVENT_FAMILY_LAND,
    ROUTE_EVENT_FAMILY_FACILITY,
    ROUTE_EVENT_FAMILY_SPECIAL_TABLE_1,
    ROUTE_EVENT_FAMILY_SPECIAL_TABLE_2
} RouteEventObjectFamily;

typedef enum RouteEventExecutionResult {
    ROUTE_EVENT_EXECUTION_INVALID = 0,
    ROUTE_EVENT_EXECUTION_PROGRESSED,
    ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND,
    ROUTE_EVENT_EXECUTION_FINISHED
} RouteEventExecutionResult;

typedef enum RouteEventCommandResult {
    ROUTE_EVENT_COMMAND_INVALID = 0,
    ROUTE_EVENT_COMMAND_APPLIED,
    ROUTE_EVENT_COMMAND_STALE,
    ROUTE_EVENT_COMMAND_UNEXPECTED,
    ROUTE_EVENT_COMMAND_DEFERRED
} RouteEventCommandResult;

typedef struct RouteEventSnapshot {
    int player;
    int event_kind;
    int route_action;
    int property_branch;
    int property_owner;
    int property_value;
    int dice_total;
} RouteEventSnapshot;

typedef struct RouteEvent {
    RouteEventPhase phase;
    int node;
    MapObjectRef object_ref;
    RouteEventSnapshot snapshot;
} RouteEvent;

/* Ordered route facts are separate from the executor work queue.  The
 * collection is the single source for pass/stop history; the queue is only a
 * consumable execution copy. */
typedef struct RouteEventCollection {
    RouteEvent events[ROUTE_EVENT_COLLECTION_CAPACITY];
    int count;
} RouteEventCollection;

void rich4_route_event_collection_reset(RouteEventCollection *collection);
int rich4_route_event_collection_push(RouteEventCollection *collection,
                                      const RouteEvent *event);
const RouteEvent *rich4_route_event_collection_at(
    const RouteEventCollection *collection, int index);
const RouteEvent *rich4_route_event_collection_last_phase(
    const RouteEventCollection *collection, RouteEventPhase phase);
int rich4_route_event_collection_count_phase(
    const RouteEventCollection *collection, RouteEventPhase phase);

RouteEventHandlerResult rich4_route_event_default_result(
    const RouteEvent *event);
RouteEventObjectFamily rich4_route_event_family_from_raw_id(int raw_object_id);
int rich4_route_event_object_type_from_raw_id(int raw_object_id);
int rich4_route_event_object_index_from_raw_id(int raw_object_id);

#endif
