#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/route_geometry_flow.h"
#include "core/game_constants.h"

int main(void)
{
    MapData map;
    MapRouteNode routes[5];
    int x;
    int y;
    uint32_t tick;

    memset(&map, 0, sizeof(map));
    memset(routes, 0, sizeof(routes));
    routes[1].x = 100;
    routes[1].y = 200;
    routes[1].next[0] = 2;
    routes[2].x = 120;
    routes[2].y = 200;
    routes[2].next[0] = 3;
    routes[3].x = 140;
    routes[3].y = 200;
    routes[3].next[0] = 1;
    map.routes = routes;
    map.route_count = 5;

    assert(rich4_route_count_for_map(&map) == 5);
    assert(rich4_route_first_node_for_map(&map) == 1);
    assert(rich4_route_node_is_valid(&map, 1));
    assert(!rich4_route_node_is_valid(&map, 4));
    rich4_route_node_xy_for_map(&map, 2, &x, &y);
    assert(x == 120 && y == 200);
    rich4_route_node_xy_for_map(&map, 0, &x, &y);
    assert(x == 100 && y == 200);
    assert(rich4_route_next_for_node(&map, 1, 0) == 2);
    assert(rich4_route_direction8_for_nodes(&map, 1, 2) >= 0);
    assert(rich4_int_sqrt_floor(0) == 0);
    assert(rich4_int_sqrt_floor(80) == 8);
    assert(rich4_original_fixed_move_duration(80, 0) == 10.0f);
    assert(rich4_original_interpolated_axis(
               0, 80, 1, rich4_original_fixed_move_duration(80, 0)) == 8);
    tick = 100;
    assert(!rich4_consume_animation_pulse(&tick, 109, 10));
    assert(tick == 100);
    assert(rich4_consume_animation_pulse(&tick, 135, 10));
    assert(tick == 130);
    /* A late render must coalesce the extra timer intervals instead of
     * replaying them one by one on subsequent frames. */
    assert(!rich4_consume_animation_pulse(&tick, 139, 10));
    assert(rich4_route_count_for_map(NULL) == GAME_ROUTE_NODE_COUNT);
    assert(rich4_route_first_node_for_map(NULL) == 0);
    assert(rich4_route_next_for_node(NULL, 23, 0) == 0);

    puts("app route-geometry flow: ok");
    return 0;
}
