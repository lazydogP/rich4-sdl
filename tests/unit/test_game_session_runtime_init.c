#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_session_runtime_init.h"
#include "app/game_session.h"
#include "core/rng.h"

static void check_shared_bootstrap_order(void)
{
    static GameSession session;
    MapData map = {0};
    MapRouteNode routes[21] = {{0}};
    const int nodes[] = {1};
    const int next[] = {2};
    const int slots[] = {0,2,4,6,8,10,12,13};
    const int chosen[] = {19,2,17,7,14,4,12,15};
    int i;
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.ai_rng_state = 1;
    session.state.stock_last_update_day = -2;
    for (i = 0; i < GAME_STOCK_COUNT; i++) session.state.stocks[i].f8 = 10000;
    map.routes = routes; map.route_count = 21;
    for (i = 1; i < 21; i++) {
        routes[i].x = (int16_t)(i * 32);
        routes[i].next[0] = (uint16_t)(i == 20 ? 1 : i + 1);
    }
    rich4_app_initialize_session_runtime(&session, &map, 21, nodes, next);
    /* 0x407AD2: eight ground placements consume draws 1..8, followed
     * by 0x42915A's twelve volume draws. Explicit actor nodes add no draws. */
    for (i = 0; i < 8; i++)
        assert(session.presentation.dynamic_objects[slots[i]].node == chosen[i]);
    assert(session.state.stocks[0].f10 == 1212);
    assert(session.state.ai_rng_state == UINT32_C(3823029677));
    assert(session.state.stock_last_update_day == -1);
}

int main(void)
{
    GameSession session;
    MapData map;
    MapRouteNode routes[3];
    int start_nodes[1] = { 1 };
    int next_nodes[1] = { 2 };
    check_shared_bootstrap_order();

    memset(&session, 0, sizeof(session));
    memset(&map, 0, sizeof(map));
    memset(routes, 0, sizeof(routes));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].play_state = 1;
    map.routes = routes;
    map.route_count = 3;
    routes[1].x = 100;
    routes[1].y = 200;
    routes[1].next[0] = 2;
    routes[2].x = 300;
    routes[2].y = 200;
    routes[2].next[0] = 1;

    rich4_app_initialize_session_runtime(
        &session, &map, 3, start_nodes, next_nodes);
    assert(session.flow.route_count == 3);
    assert(session.flow.landing_pending_player == 1);
    assert(session.state.players[0].node == 1);
    assert(session.state.players[0].next_node == 2);
    assert(session.presentation.actors[0].visual_x == 100);
    assert(session.presentation.actors[0].visual_y == 200);
    assert(session.presentation.actors[0].first_turn_landing_pending == 1);
    assert(session.presentation.dynamic_objects[0].type == 1);
    assert(session.presentation.dice.faces[0] == 1);
    assert(session.presentation.property.land == -1);

    {
        GameSession random_session;
        MapData random_map;
        MapRouteNode random_routes[21];
        int dynamic_count = 0;
        int i;
        int j;

        memset(&random_session, 0, sizeof(random_session));
        memset(&random_map, 0, sizeof(random_map));
        memset(random_routes, 0, sizeof(random_routes));
        random_session.state.active = 1;
        random_session.state.player_count = 4;
        random_session.state.current_player = 0;
        for (i = 0; i < 4; i++)
            random_session.state.players[i].play_state = 1;
        random_map.routes = random_routes;
        random_map.route_count = 21;
        for (i = 1; i < 21; i++) {
            random_routes[i].x = (int16_t)(i * 32);
            random_routes[i].y = (int16_t)((i & 1) * 32);
            random_routes[i].next[0] = (uint16_t)(i == 20 ? 1 : i + 1);
        }

        /* The original places dynamic objects before automatic player starts.
         * No player may receive one of those occupied route nodes. */
        rich4_rng_seed(1);
        rich4_app_initialize_session_runtime(
            &random_session, &random_map, 21, NULL, NULL);
        for (j = 0; j < GAME_MAP_DYNAMIC_OBJECT_COUNT; j++)
            if (random_session.presentation.dynamic_objects[j].node != 0)
                dynamic_count++;
        assert(dynamic_count == 8);
        for (i = 0; i < 4; i++) {
            assert(random_session.state.players[i].node > 0);
            for (j = 0; j < i; j++)
                assert(random_session.state.players[i].node !=
                       random_session.state.players[j].node);
            for (j = 0; j < GAME_MAP_DYNAMIC_OBJECT_COUNT; j++)
                if (random_session.presentation.dynamic_objects[j].node != 0)
                    assert(random_session.state.players[i].node !=
                           random_session.presentation.dynamic_objects[j].node);
        }
    }

    session.state.active = 1;
    session.time_machine_valid = 1;
    rich4_app_reset_session(&session);
    assert(session.state.active == 0);
    assert(session.time_machine_valid == 0);
    puts("app game session runtime init: ok");
    return 0;
}
