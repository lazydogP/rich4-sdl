#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/special_actor_flow.h"
#include "core/game_constants.h"
#include "core/confinement.h"
#include "core/game_event_payload.h"

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
    session.presentation.has_view_override = 1;
    session.presentation.view_world_x = 900;
    session.presentation.view_world_y = 1000;
    setup_map(&map, routes);
    session.presentation.sleepwalking_frame[4] = 5;
    rich4_special_actor_begin(&session, &map, &action);
    assert(session.presentation.sleepwalking_frame[4] == 5);
    /* Camera reset belongs to shared turn rotation, not movement startup. */
    assert(session.presentation.has_view_override);
    assert(session.presentation.view_world_x == 900 &&
           session.presentation.view_world_y == 1000);
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
    assert(session.presentation.sleepwalking_frame[4] == 0);
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

static void test_endpoint_pause(void)
{
    static const int intervals[] = {120, 80, 40};
    int speed;
    for (speed = 0; speed < 3; speed++) {
        GameSession session = {0};
        MapData map;
        MapRouteNode routes[4];
        TurnSpecialAutoActionResult action = action_for_path();
        uint32_t now = 1000;
        int tick;
        int interval = intervals[speed];
        setup_map(&map, routes);
        action.path_count = 2;
        rich4_special_actor_begin(&session, &map, &action);
        session.presentation.special_actor.move_progress = 9;
        session.presentation.special_actor.last_step_tick = now - interval;
        assert(rich4_special_actor_update(&session, &map, interval, now) ==
               RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
        assert(session.presentation.special_actor.arrival_pending);
        assert(session.presentation.special_actor.steps_remaining == 0);
        for (tick = 1; tick <= 8; tick++) {
            now += interval;
            assert(rich4_special_actor_update(&session, &map, interval, now - 1) ==
                   RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
            assert(rich4_special_actor_update(&session, &map, interval, now) ==
                   RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
        }
        now += interval;
        assert(rich4_special_actor_update(&session, &map, interval, now) ==
               RICH4_SPECIAL_ACTOR_UPDATE_FINISHED);
        assert(!session.presentation.special_actor.active);
    }
}

static void test_blocked_actor_pause(void)
{
    GameSession session = {0};
    MapData map;
    MapRouteNode routes[4];
    TurnSpecialAutoActionResult action = action_for_path();
    setup_map(&map, routes);
    action.kind = TURN_SPECIAL_AUTO_BLOCKED;
    action.path_count = 1;
    rich4_special_actor_begin(&session, &map, &action);
    assert(session.presentation.special_actor.end_ticks_remaining == 3);
    assert(rich4_special_actor_update(&session, &map, 40, 1000) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(rich4_special_actor_update(&session, &map, 40, 1040) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(rich4_special_actor_update(&session, &map, 40, 1080) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(rich4_special_actor_update(&session, &map, 40, 1120) ==
           RICH4_SPECIAL_ACTOR_UPDATE_FINISHED);
}

static void test_pass_through_stops_at_origin(void)
{
    GameSession session = {0};
    MapData map;
    MapRouteNode routes[4];
    TurnSpecialAutoActionResult action = action_for_path();
    int tick;
    setup_map(&map, routes);
    routes[2].flags = 4;
    session.state.prison_node = 2;
    session.state.special_player_actor_type[0] = 0x81;
    rich4_special_actor_begin(&session, &map, &action);
    session.presentation.special_actor.move_progress = 9;
    session.presentation.special_actor.last_step_tick = 960;
    assert(rich4_special_actor_update(&session, &map, 40, 1000) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.state.special_player_f10[0] == 0);
    assert(session.presentation.special_actor.steps_remaining == 1);
    assert(rich4_special_actor_update(&session, &map, 40, 1040) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.state.special_player_f10[0] == GAME_CONFINEMENT_PRISON);
    assert(session.state.special_player_node[0] == 2);
    assert(session.presentation.special_actor.path_count == 2);
    assert(session.presentation.special_actor.steps_remaining == 0);
    /* A busy interval coalesces to one pulse, rather than skipping the pause. */
    assert(rich4_special_actor_update(&session, &map, 40, 2040) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(session.presentation.special_actor.end_ticks_remaining == 7);
    for (tick = 1; tick <= 7; tick++)
        assert(rich4_special_actor_update(&session, &map, 40, 2040 + tick * 40) ==
            (tick == 7 ? RICH4_SPECIAL_ACTOR_UPDATE_FINISHED :
                         RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE));
}

static void test_dog_endpoint_retains_pause(void)
{
    GameSession session = {0};
    MapData map;
    MapRouteNode routes[4];
    TurnSpecialAutoActionResult action = action_for_path();
    GameEvent event;
    GameEventQueue events;
    int tick;
    setup_map(&map, routes);
    action.path_count = 2;
    session.state.hospital_node = 3;
    session.presentation.dynamic_objects[10].type = 11;
    session.presentation.dynamic_objects[10].node = 2;
    rich4_special_actor_begin(&session, &map, &action);
    session.presentation.special_actor.move_progress = 9;
    session.presentation.special_actor.last_step_tick = 960;
    assert(rich4_special_actor_update(&session, &map, 40, 1000) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(rich4_special_actor_update(&session, &map, 40, 1040) ==
           RICH4_SPECIAL_ACTOR_UPDATE_DOG);
    assert(rich4_game_event_queue_pop(&session.deferred_events, &event));
    assert(rich4_game_event_tool_resource(&event) == 532);
    /* The animation completion invokes the special admission branch. */
    rich4_game_event_queue_reset(&events);
    assert(rich4_confinement_enter(&session.state, 4,
        GAME_CONFINEMENT_HOSPITAL, 3, &events) == GAME_CONFINEMENT_APPLIED);
    for (tick = 1; tick <= 8; tick++)
        assert(rich4_special_actor_update(&session, &map, 40, 1040 + tick * 40) ==
            (tick == 8 ? RICH4_SPECIAL_ACTOR_UPDATE_FINISHED :
                         RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE));
}

int main(void)
{
    test_begin_and_interpolation();
    test_action_hold_is_app_owned();
    test_endpoint_pause();
    test_blocked_actor_pause();
    test_pass_through_stops_at_origin();
    test_dog_endpoint_retains_pause();
    puts("app special-actor flow: ok");
    return 0;
}
