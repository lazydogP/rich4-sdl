#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/route_facts.h"

int main(void)
{
    MapRouteNode routes[3];
    MapData map;
    GameSession session;
    const RouteEvent *event;

    memset(routes, 0, sizeof(routes));
    memset(&map, 0, sizeof(map));
    memset(&session, 0, sizeof(session));
    map.routes = routes;
    map.route_count = 3;
    routes[1].object.raw_object_id = 2001;
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.land_count = 1;
    session.state.land_owner[0] = -1;
    session.state.land_value[0] = 123;
    session.flow.dice_value = 6;
    rich4_app_route_record_pass(&session, &map, 1);
    rich4_app_route_record_stop(&session, &map, 1);
    assert(session.flow.route_facts.count == 2);
    event = rich4_route_event_collection_at(&session.flow.route_facts, 0);
    assert(event != NULL && event->phase == ROUTE_EVENT_PHASE_PASS);
    assert(event->snapshot.event_kind == GAME_ROUTE_EVENT_LAND);
    assert(event->snapshot.property_branch == GAME_PROPERTY_STOP_BRANCH_UNOWNED);
    assert(event->snapshot.property_value == 123);
    assert(event->snapshot.dice_total == 6);
    event = rich4_route_event_collection_at(&session.flow.route_facts, 1);
    assert(event != NULL && event->phase == ROUTE_EVENT_PHASE_STOP);
    routes[2].flags = 14;
    assert(rich4_app_route_action_for_node(&map, 2) == 14);
    rich4_app_route_record_pass(&session, &map, 2);
    event = rich4_route_event_collection_at(&session.flow.route_facts, 2);
    assert(event != NULL &&
           event->snapshot.event_kind == GAME_ROUTE_EVENT_SECOND_SPECIAL);
    puts("app route facts: ok");
    return 0;
}
