#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/special_actor_action.h"

static void setup(GameState *state, MapData *map, MapRouteNode routes[5])
{
    int i;
    memset(state, 0, sizeof(*state));
    memset(map, 0, sizeof(*map));
    memset(routes, 0, sizeof(MapRouteNode) * 5);
    state->active = 1;
    state->player_count = 3;
    state->price_index = 2;
    state->ai_rng_state = 7;
    for (i = 0; i < state->player_count; i++) {
        state->players[i].play_state = 1;
        state->players[i].cash = 2000;
        state->players[i].bank = 1000;
    }
    for (i = 0; i < 4; i++)
        state->special_player_play_state[i] = 0;
    map->routes = routes;
    map->route_count = 5;
}

int main(void)
{
    GameState state;
    MapData map;
    MapRouteNode routes[5];
    GameSpecialActorActionBatch batch;
    GameEventQueue events;

    setup(&state, &map, routes);
    state.players[0].node = 1;
    state.players[1].node = 2;
    state.players[1].points = 100;
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 0, 2, 0, &batch, &events));
    assert(batch.count == 1 &&
           batch.action[0].kind == GAME_SPECIAL_ACTOR_ACTION_THIEF_POINTS &&
           batch.action[0].target_player == 1 &&
           batch.action[0].amount == 50 &&
           state.players[0].points == 50 &&
           state.players[1].points == 50);

    setup(&state, &map, routes);
    state.players[1].node = 2;
    state.players[1].points = 100;
    state.special_player_status[0][1] = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 0, 2, 1, &batch, &events));
    assert(batch.count == 0 && state.players[1].points == 100);

    setup(&state, &map, routes);
    state.players[1].node = 2;
    state.players[1].card_inventory[0] = 4;
    state.players[1].bank = 1000;
    state.players[2].bank = 500;
    routes[2].flags = 14;
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 1, 2, 0, &batch, &events));
    assert(batch.count == 2 &&
           batch.action[0].kind == GAME_SPECIAL_ACTOR_ACTION_ROBBER_CARD &&
           batch.action[0].card == 3 &&
           batch.action[1].kind == GAME_SPECIAL_ACTOR_ACTION_ROBBER_BANK &&
           batch.action[1].amount == 300 &&
           state.players[0].card_inventory[0] == 4 &&
           state.players[1].card_inventory[0] == 0 &&
           state.players[0].cash == 2300 &&
           state.players[1].bank == 800 &&
           state.players[2].bank == 400);

    setup(&state, &map, routes);
    state.land_count = 2;
    state.land_owner[0] = state.land_owner[1] = 1;
    state.land_group[0] = state.land_group[1] = 9;
    state.land_price[0] = 100;
    state.land_price[1] = 200;
    routes[3].object = rich4_map_object_ref(0x7d1);
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 2, 3, 1, &batch, &events));
    assert(batch.count == 1 &&
           batch.action[0].kind == GAME_SPECIAL_ACTOR_ACTION_HOOLIGAN_FEE &&
           batch.action[0].amount == 600 &&
           state.players[1].cash == 1400 &&
           state.players[0].bank == 1600);

    setup(&state, &map, routes);
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_price[0] = 250;
    routes[3].object = rich4_map_object_ref(0xfa1);
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 2, 3, 1, &batch, &events));
    assert(batch.count == 1 &&
           batch.action[0].kind == GAME_SPECIAL_ACTOR_ACTION_HOOLIGAN_FEE &&
           batch.action[0].amount == 500);

    setup(&state, &map, routes);
    state.land_count = 1;
    state.land_owner[0] = 1;
    state.land_runtime_value[0] = 275;
    routes[3].object = rich4_map_object_ref(0x7d1);
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 3, 3, 1, &batch, &events));
    assert(batch.count == 1 &&
           batch.action[0].kind == GAME_SPECIAL_ACTOR_ACTION_SPY_TOLL &&
           batch.action[0].amount == 275);

    setup(&state, &map, routes);
    state.facility_count = 1;
    state.facility_owner[0] = 1;
    state.facility_runtime_value[0] = 425;
    routes[3].object = rich4_map_object_ref(0xfa1);
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 3, 3, 1, &batch, &events));
    assert(batch.count == 1 &&
           batch.action[0].kind == GAME_SPECIAL_ACTOR_ACTION_SPY_TOLL &&
           batch.action[0].amount == 425);

    setup(&state, &map, routes);
    state.company_count = 1;
    state.company_owner[0] = 1;
    state.company_period_business[0] = 345;
    routes[4].object = rich4_map_object_ref(0x1771);
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_actor_apply_step(
        &state, &map, 3, 4, 1, &batch, &events));
    assert(batch.count == 1 &&
           batch.action[0].kind == GAME_SPECIAL_ACTOR_ACTION_SPY_DIVIDEND &&
           batch.action[0].amount == 345 &&
           state.players[1].cash == 1655 &&
           state.players[0].bank == 1345);

    puts("special actor 0x41B42D actions: ok");
    return 0;
}
