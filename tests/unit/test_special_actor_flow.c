#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/special_actor_flow.h"
#include "core/game_constants.h"

static void setup_map(MapData *map, MapRouteNode routes[4])
{
    memset(map, 0, sizeof(*map));
    memset(routes, 0, sizeof(MapRouteNode) * 4);
    map->routes = routes;
    map->route_count = 4;
    routes[1].x = 0;
    routes[1].y = 0;
    routes[2].x = 80;
    routes[2].y = 0;
    routes[3].x = 80;
    routes[3].y = 80;
}

static TurnSpecialAutoActionResult action_for_path(void)
{
    TurnSpecialAutoActionResult action;
    memset(&action, 0, sizeof(action));
    action.kind = TURN_SPECIAL_AUTO_MOVED;
    action.special_slot = 0;
    action.path_count = 3;
    action.path_nodes[0] = 1;
    action.path_nodes[1] = 2;
    action.path_nodes[2] = 3;
    return action;
}

static void test_begin_and_interpolation(void)
{
    GameSession session;
    MapData map;
    MapRouteNode routes[4];
    TurnSpecialAutoActionResult action = action_for_path();

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.special_player_status[0][1] = 1;
    setup_map(&map, routes);
    rich4_special_actor_begin(&session, &map, &action);
    assert(session.presentation.special_actor.active == 1);
    assert(session.presentation.special_actor.path_count == 3);
    assert(session.presentation.special_actor.steps_remaining == 2);
    assert(session.presentation.special_actor.resource_part == 3);
    assert(session.presentation.special_actor.visual_x == 0);
    assert(session.presentation.special_actor.visual_y == 0);
    assert(session.presentation.special_actor.move_distance == 10);

    assert(rich4_special_actor_update(&session, &map, 10, 100) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.presentation.special_actor.last_step_tick == 100);
    assert(rich4_special_actor_update(&session, &map, 10, 110) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.presentation.special_actor.move_progress == 1);
    assert(session.presentation.special_actor.visual_x == 8);
    assert(session.presentation.special_actor.visual_y == 0);
}

static void test_action_hold_is_app_owned(void)
{
    GameSession session;
    MapData map;
    MapRouteNode routes[4];

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    setup_map(&map, routes);
    session.presentation.special_actor.active = 1;
    session.presentation.special_actor.path_count = 2;
    session.presentation.special_actor.path_nodes[0] = 1;
    session.presentation.special_actor.path_nodes[1] = 2;
    session.presentation.special_actor.move_distance = 10;
    session.presentation.special_actor.action_batch.count = 1;
    session.presentation.special_actor.action_batch.action[0].kind =
        GAME_SPECIAL_ACTOR_ACTION_ROBBER_BANK;
    assert(rich4_special_actor_update(&session, &map, 10, 1000) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.presentation.special_actor.action_until == 3000);
    assert(rich4_special_actor_update(&session, &map, 10, 2999) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.presentation.special_actor.action_batch.count == 1);
    assert(rich4_special_actor_update(&session, &map, 10, 3000) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.presentation.special_actor.action_batch.count == 0);
    assert(session.presentation.special_actor.last_step_tick == 3000);
}

int main(void)
{
    test_begin_and_interpolation();
    test_action_hold_is_app_owned();
    puts("app special-actor flow: ok");
    return 0;
}
