#include "app/route_facts.h"

#include <string.h>

#include "core/game_constants.h"
#include "core/route_event.h"

static int rich4_app_route_event_kind_for_object(
    const GameSession *session, int object_type, int object_index)
{
    switch (object_type) {
    case GAME_ROUTE_OBJECT_NONE:
        return GAME_ROUTE_EVENT_NONE;
    case GAME_ROUTE_OBJECT_NODE:
        return GAME_ROUTE_EVENT_ROAD_NODE;
    case GAME_ROUTE_OBJECT_LAND:
        return session != NULL && object_index >= 0 &&
                object_index < session->state.land_count
            ? GAME_ROUTE_EVENT_LAND : GAME_ROUTE_EVENT_UNKNOWN;
    case GAME_ROUTE_OBJECT_FACILITY:
        return session != NULL && object_index >= 0 &&
                object_index < session->state.facility_count
            ? GAME_ROUTE_EVENT_FACILITY : GAME_ROUTE_EVENT_UNKNOWN;
    case GAME_ROUTE_OBJECT_STOCK_SPECIAL:
        return GAME_ROUTE_EVENT_STOCK_SPECIAL;
    case GAME_ROUTE_OBJECT_SECOND_SPECIAL:
        return GAME_ROUTE_EVENT_SECOND_SPECIAL;
    default:
        return GAME_ROUTE_EVENT_UNKNOWN;
    }
}

static int rich4_app_route_property_branch(
    const GameSession *session, int player_index, int event_kind,
    int object_index, int *out_owner, int *out_value)
{
    int owner = -1;
    int value = 0;
    int branch = GAME_PROPERTY_STOP_BRANCH_NONE;

    if (event_kind == GAME_ROUTE_EVENT_LAND) {
        if (session == NULL || object_index < 0 ||
            object_index >= session->state.land_count ||
            object_index >= GAME_MAX_MAP_PROPERTIES) {
            branch = GAME_PROPERTY_STOP_BRANCH_INVALID;
        } else {
            owner = session->state.land_owner[object_index];
            value = session->state.land_value[object_index];
        }
    } else if (event_kind == GAME_ROUTE_EVENT_FACILITY) {
        if (session == NULL || object_index < 0 ||
            object_index >= session->state.facility_count ||
            object_index >= GAME_MAX_MAP_PROPERTIES) {
            branch = GAME_PROPERTY_STOP_BRANCH_INVALID;
        } else {
            owner = session->state.facility_owner[object_index];
            value = session->state.facility_value[object_index];
        }
    } else if (event_kind != GAME_ROUTE_EVENT_NONE) {
        branch = GAME_PROPERTY_STOP_BRANCH_UNSUPPORTED;
    }

    if (branch == GAME_PROPERTY_STOP_BRANCH_NONE &&
        (event_kind == GAME_ROUTE_EVENT_LAND ||
         event_kind == GAME_ROUTE_EVENT_FACILITY)) {
        if (owner < 0) {
            branch = GAME_PROPERTY_STOP_BRANCH_UNOWNED;
        } else if (owner == player_index) {
            branch = GAME_PROPERTY_STOP_BRANCH_OWNED_SELF;
        } else if (owner >= 0 && owner < GAME_MAX_PLAYERS) {
            branch = GAME_PROPERTY_STOP_BRANCH_OWNED_OTHER;
        } else {
            branch = GAME_PROPERTY_STOP_BRANCH_INVALID;
        }
    }
    if (out_owner != NULL) *out_owner = owner;
    if (out_value != NULL) *out_value = value;
    return branch;
}

void rich4_app_route_object_at(const MapData *map, int node,
                               int *out_object_id, int *out_object_type,
                               int *out_object_index)
{
    int object_id = 0;
    int object_type = GAME_ROUTE_OBJECT_NONE;
    int object_index = -1;

    if (map != NULL && map->routes != NULL && node >= 0 &&
        (size_t)node < map->route_count) {
        object_id = map->routes[node].object.raw_object_id;
        object_type = rich4_route_event_object_type_from_raw_id(object_id);
        object_index = rich4_route_event_object_index_from_raw_id(object_id);
    }
    if (out_object_id != NULL) *out_object_id = object_id;
    if (out_object_type != NULL) *out_object_type = object_type;
    if (out_object_index != NULL) *out_object_index = object_index;
}

int rich4_app_route_action_for_node(const MapData *map, int node)
{
    if (map == NULL || map->routes == NULL || node < 0 ||
        (size_t)node >= map->route_count)
        return 0;
    return (int)(map->routes[node].flags & 0xffu);
}

static void rich4_app_route_record(GameSession *session, const MapData *map,
                                   int node, RouteEventPhase phase)
{
    int object_id;
    int object_type;
    int object_index;
    int event_kind;
    int property_branch;
    int property_owner;
    int property_value;
    int route_action;
    RouteEvent event;

    if (session == NULL) return;
    rich4_app_route_object_at(map, node, &object_id, &object_type,
                              &object_index);
    event_kind = rich4_app_route_event_kind_for_object(
        session, object_type, object_index);
    route_action = rich4_app_route_action_for_node(map, node);
    /* Rich4 dispatches the node action before consulting object tables. */
    if (route_action != 0)
        event_kind = GAME_ROUTE_EVENT_SECOND_SPECIAL;
    property_branch = rich4_app_route_property_branch(
        session, session->state.current_player, event_kind, object_index,
        &property_owner, &property_value);
    if (event_kind == GAME_ROUTE_EVENT_NONE) return;
    memset(&event, 0, sizeof(event));
    event.phase = phase;
    event.node = node;
    event.object_ref = rich4_map_object_ref((uint16_t)object_id);
    event.snapshot.player = session->state.current_player;
    event.snapshot.event_kind = event_kind;
    event.snapshot.route_action = route_action;
    event.snapshot.property_branch = property_branch;
    event.snapshot.property_owner = property_owner;
    event.snapshot.property_value = property_value;
    event.snapshot.dice_total = session->flow.dice_value;
    rich4_route_event_collection_push(&session->flow.route_facts, &event);
}

void rich4_app_route_record_pass(GameSession *session, const MapData *map,
                                 int node)
{
    rich4_app_route_record(session, map, node, ROUTE_EVENT_PHASE_PASS);
}

void rich4_app_route_record_stop(GameSession *session, const MapData *map,
                                 int node)
{
    rich4_app_route_record(session, map, node, ROUTE_EVENT_PHASE_STOP);
}
