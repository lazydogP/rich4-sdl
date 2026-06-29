#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "app/dynamic_object_flow.h"
#include "core/game_event_payload.h"
#include "core/god.h"
#include "core/rng.h"

int main(void)
{
    GameState state;
    GamePresentationState presentation;
    GameViewportModel viewport;
    GameMapDynamicObjectState *object;

    assert(sizeof(GameMapDynamicObjectState) == 24);
    assert(offsetof(GameMapDynamicObjectState, type) == 0);
    assert(offsetof(GameMapDynamicObjectState, node_direction) == 1);
    assert(offsetof(GameMapDynamicObjectState, node) == 2);
    assert(offsetof(GameMapDynamicObjectState, countdown) == 4);
    assert(offsetof(GameMapDynamicObjectState, attached_player) == 5);
    assert(offsetof(GameMapDynamicObjectState, flight_active) == 6);
    assert(offsetof(GameMapDynamicObjectState, flight_direction) == 7);
    assert(offsetof(GameMapDynamicObjectState, world_x) == 8);
    assert(offsetof(GameMapDynamicObjectState, world_y) == 12);
    assert(offsetof(GameMapDynamicObjectState, velocity_x) == 16);
    assert(offsetof(GameMapDynamicObjectState, velocity_y) == 20);

    memset(&presentation, 0, sizeof(presentation));
    memset(&viewport, 0, sizeof(viewport));
    rich4_app_init_dynamic_object_pool(&presentation);
    assert(presentation.dynamic_objects[0].type == 1);
    assert(presentation.dynamic_objects[14].type == 15);
    assert(presentation.dynamic_objects[26].type == 17);
    assert(presentation.dynamic_objects[36].type == 18);

    memset(&state, 0, sizeof(state));
    state.player_count = 1;
    state.players[0].node = 1;
    state.tool_route_node_count = 5;
    state.tool_route_x[1] = 0;
    state.tool_route_y[1] = 0;
    state.tool_route_x[2] = 300;
    state.tool_route_y[2] = 0;
    state.tool_route_x[3] = 100;
    state.tool_route_y[3] = 0;
    state.tool_route_x[4] = 50;
    state.tool_route_y[4] = 50;
    presentation.dynamic_objects[0].type = 1;
    presentation.dynamic_objects[0].node = 2;
    presentation.dynamic_objects[1].type = 11;
    presentation.dynamic_objects[1].node = 3;
    presentation.dynamic_objects[2].type = 15;
    presentation.dynamic_objects[2].node = 4;
    presentation.dynamic_objects[3].type = 3;
    presentation.dynamic_objects[3].node = 3;
    presentation.dynamic_objects[3].attached_player = 1;
    {
        int god_type = 0;
        assert(rich4_app_find_nearest_dynamic_god(
                   &presentation, &state, 0, &god_type) == 2);
        assert(god_type == 15);
    }
    viewport.center_world_x = 448;
    viewport.center_world_y = 448;
    object = &presentation.dynamic_objects[0];
    object->flight_active = 2;
    object->world_x = 448.0f;
    object->world_y = 448.0f;
    object->velocity_x = 1.5f;
    object->velocity_y = -2.0f;
    rich4_app_update_dynamic_objects(&presentation, &viewport, -1);
    assert(object->flight_active == 1);
    assert(object->world_x == 449.5f && object->world_y == 446.0f);

    object->flight_active = 2;
    object->world_x = -32.0f;
    object->world_y = -32.0f;
    rich4_app_update_dynamic_objects(&presentation, &viewport, -1);
    assert(object->flight_active == 0);
    object->type = 3;
    object->flight_active = 4;
    rich4_app_clear_dynamic_object(&presentation, 0);
    assert(object->type == 0 && object->flight_active == 0);

    memset(&presentation, 0, sizeof(presentation));
    rich4_app_init_dynamic_object_pool(&presentation);
    {
        GameState state;
        memset(&state, 0, sizeof(state));
        state.tool_route_node_count = 3;
        state.tool_route_x[1] = 100;
        state.tool_route_y[1] = 20;
        state.tool_route_x[2] = 200;
        state.tool_route_y[2] = 40;
        state.tool_route_next[2][0] = 1;
        state.tool_placed_objects[0].kind = 16;
        state.tool_placed_objects[0].attached_player = 0xff;
        state.tool_placed_objects[0].node = 2;
        state.tool_placed_objects[0].countdown = 0x26;
        rich4_app_sync_tool_dynamic_objects(&presentation, &state);
        object = &presentation.dynamic_objects[16];
        assert(object->type == 16 && object->node == 2);
        assert(object->attached_player == 0);
        assert(object->countdown == 0x26);
        assert(object->world_x == 200.0f && object->world_y == 40.0f);
        state.tool_placed_objects[0].kind = 0;
        rich4_app_sync_tool_dynamic_objects(&presentation, &state);
        assert(object->type == 16 && object->node == 0);
    }

    {
        MapData map;
        MapRouteNode routes[12];
        const int initial_types[] = {1, 3, 5, 7, 9, 11, 13, 14};
        int used[12] = {0};
        int i;

        memset(&map, 0, sizeof(map));
        memset(routes, 0, sizeof(routes));
        map.routes = routes;
        map.route_count = 12;
        for (i = 1; i < 12; i++) {
            routes[i].x = (int16_t)(i * 100);
            routes[i].y = (int16_t)(i * 20);
            routes[i].next[0] = (uint16_t)(i == 11 ? 1 : i + 1);
        }
        /* +0x20 is not part of 0x40AA6C's candidate filter. */
        routes[10].object.raw_object_id = 0x1f42;
        /* A nonzero +0x24 masked bit excludes this node. */
        routes[11].flags = 0x00010000u;
        /* 0x40AA6C checks only route fields +0x18 and +0x1c, i.e. next[0]
         * and next[2]; a node with only the other two exits is not eligible. */
        routes[9].next[0] = 0;
        routes[9].next[1] = 8;
        routes[9].next[2] = 0;
        routes[9].next[3] = 8;

        rich4_rng_seed(1);
        memset(&presentation, 0, sizeof(presentation));
        rich4_app_init_dynamic_object_pool(&presentation);
        rich4_app_initialize_dynamic_map_objects(&presentation, &map);
        for (i = 0; i < (int)(sizeof(initial_types) /
                             sizeof(initial_types[0])); i++) {
            int type = initial_types[i];
            GameMapDynamicObjectState *initial =
                &presentation.dynamic_objects[type - 1];
            int node = initial->node;
            assert(initial->type == type);
            assert(node > 0 && node < 12 && node != 11 && node != 9);
            assert(!used[node]);
            used[node] = 1;
            assert(initial->world_x == (float)routes[node].x);
            assert(initial->world_y == (float)routes[node].y);
            assert(initial->node_direction ==
                   rich4_game_actor_direction_from_delta(
                       routes[node].x - routes[routes[node].next[0]].x,
                       routes[node].y - routes[routes[node].next[0]].y));
        }
    }

    {
        MapData map;
        MapRouteNode routes[20];
        int old_node;
        int replacement_node;
        int old_gift_node;
        int old_chest_node;
        int i;

        memset(&map, 0, sizeof(map));
        memset(routes, 0, sizeof(routes));
        map.routes = routes;
        map.route_count = 20;
        for (i = 1; i < 20; i++) {
            routes[i].x = (int16_t)(i * 500);
            routes[i].next[0] = (uint16_t)(i == 19 ? 1 : i + 1);
        }
        rich4_rng_seed(2);
        memset(&presentation, 0, sizeof(presentation));
        rich4_app_init_dynamic_object_pool(&presentation);
        rich4_app_initialize_dynamic_map_objects(&presentation, &map);
        old_node = presentation.dynamic_objects[0].node;
        assert(old_node > 0);
        assert(rich4_app_remove_dynamic_object(
                   &presentation, &map, 0));
        assert(presentation.dynamic_objects[0].type == 1);
        assert(presentation.dynamic_objects[0].node == 0);
        replacement_node = presentation.dynamic_objects[1].node;
        assert(presentation.dynamic_objects[1].type == 2);
        assert(replacement_node > 0 && replacement_node != old_node);
        assert(presentation.dynamic_objects[1].world_x ==
               (float)routes[replacement_node].x);
        assert(rich4_app_remove_dynamic_object(
                   &presentation, &map, 1));
        assert(presentation.dynamic_objects[1].type == 2);
        assert(presentation.dynamic_objects[1].node == 0);
        assert(presentation.dynamic_objects[0].type == 1);
        assert(presentation.dynamic_objects[0].node > 0);

        old_gift_node = presentation.dynamic_objects[12].node;
        old_chest_node = presentation.dynamic_objects[13].node;
        assert(old_gift_node > 0 && old_chest_node > 0);
        rich4_rng_seed(3);
        assert(rich4_app_reset_monthly_dynamic_objects(
                   &presentation, &map));
        assert(presentation.dynamic_objects[12].type == 13);
        assert(presentation.dynamic_objects[13].type == 14);
        assert(presentation.dynamic_objects[12].node > 0);
        assert(presentation.dynamic_objects[13].node > 0);
        assert(presentation.dynamic_objects[12].node != old_gift_node);
        assert(presentation.dynamic_objects[13].node != old_chest_node);
        assert(presentation.dynamic_objects[12].node !=
               presentation.dynamic_objects[13].node);
    }

    {
        GameState state;
        GameEventQueue events;

        memset(&state, 0, sizeof(state));
        state.active = 1;
        state.player_count = 2;
        state.players[0].play_state = 1;
        state.players[0].node = 7;
        memset(&presentation, 0, sizeof(presentation));
        rich4_app_init_dynamic_object_pool(&presentation);
        assert(rich4_god_is_road_type(1));
        assert(rich4_god_is_road_type(12));
        assert(rich4_god_is_road_type(15));
        assert(!rich4_god_is_road_type(11));
        assert(!rich4_god_is_road_type(16));

        /* Type 15 occupies one of the two shared Death slots and follows the
         * same landing attach path as an ordinary god. */
        presentation.dynamic_objects[14].node = 7;
        rich4_game_event_queue_reset(&events);
        assert(rich4_app_collect_dynamic_god(
                   &presentation, &state, 0, &events) == 15);
        assert(state.players[0].god_info == 15 &&
               state.players[0].god_type == 15);
        assert(presentation.dynamic_objects[14].attached_player == 1 &&
               presentation.dynamic_objects[14].countdown == 13 &&
               presentation.dynamic_objects[14].flight_active == 0);

        /* Replacing a current god clears its visual object before attaching
         * the new one, just as 0x40EAD7 does. */
        state.players[0].node = 8;
        presentation.dynamic_objects[0].node = 8;
        rich4_game_event_queue_reset(&events);
        assert(rich4_app_collect_dynamic_god(
                   &presentation, &state, 0, &events) == 1);
        assert(state.players[0].god_info == 1 &&
               state.players[0].god_type == 1);
        assert(presentation.dynamic_objects[14].type == 15 &&
               presentation.dynamic_objects[14].node == 0 &&
               presentation.dynamic_objects[14].attached_player == 0);
        assert(presentation.dynamic_objects[0].attached_player == 1 &&
               presentation.dynamic_objects[0].countdown == 7 &&
               presentation.dynamic_objects[0].flight_active == 0);

        /* Detaching a god must not clear an attached tool in slots 16..45. */
        presentation.dynamic_objects[16].attached_player = 1;
        presentation.dynamic_objects[16].node = 9;
        assert(rich4_app_detach_dynamic_god(&presentation, 0));
        assert(presentation.dynamic_objects[0].node == 0 &&
               presentation.dynamic_objects[16].attached_player == 1);
    }

    {
        GameState detach_state;

        memset(&detach_state, 0, sizeof(detach_state));
        detach_state.player_count = 1;
        detach_state.tool_route_node_count = 3;
        /* The god was acquired at node 1, but its owner is now at node 2.
         * Also leave stale interpolation coordinates to exercise idle mode. */
        detach_state.tool_route_x[1] = 1600;
        detach_state.tool_route_y[1] = 960;
        detach_state.tool_route_x[2] = 320;
        detach_state.tool_route_y[2] = 160;
        detach_state.players[0].node = 2;
        memset(&presentation, 0, sizeof(presentation));
        rich4_app_init_dynamic_object_pool(&presentation);
        presentation.map_direction = 1;
        presentation.actors[0].direction = 2;
        presentation.dynamic_objects[0].node = 1;
        presentation.dynamic_objects[0].attached_player = 1;
        assert(rich4_app_begin_dynamic_god_detach(
                   &presentation, &detach_state, 0, 1000));
        assert(presentation.god_detachment.active);
        assert(presentation.god_detachment.sound_pending == 1);
        assert(presentation.god_detachment.god_type == 1);
        assert(presentation.god_detachment.frame_count == 24);
        assert(presentation.god_detachment.frame_delay_ms == 60);
        assert(presentation.god_detachment.direction == 5);
        assert(presentation.god_detachment.world_x == 320 &&
               presentation.god_detachment.world_y == 160);
        assert(presentation.dynamic_objects[0].node == 0 &&
               presentation.dynamic_objects[0].attached_player == 0);
        assert(rich4_app_update_dynamic_god_detachment(
                   &presentation, 1059));
        assert(presentation.god_detachment.frame == 0);
        assert(rich4_app_update_dynamic_god_detachment(
                   &presentation, 1060));
        assert(presentation.god_detachment.frame == 1);
        assert(!rich4_app_update_dynamic_god_detachment(
                    &presentation, 1000 + 24 * 60));
        assert(!presentation.god_detachment.active);
        /* While moving, use the current interpolated actor coordinates,
         * not either the acquisition node or the committed route node. */
        presentation.dynamic_objects[0].node = 1;
        presentation.dynamic_objects[0].attached_player = 1;
        presentation.actors[0].move_active = 1;
        presentation.actors[0].visual_x = 336;
        presentation.actors[0].visual_y = 176;
        assert(rich4_app_begin_dynamic_god_detach(
            &presentation, &detach_state, 0, 2500));
        assert(presentation.god_detachment.world_x == 336 &&
               presentation.god_detachment.world_y == 176);
        assert(presentation.map_direction == 1);
        presentation.actors[0].move_active = 0;
        {
            int status;
            for (status = 0; status < 4; status++) {
                detach_state.players[0].delayed_status_block = status == 0;
                detach_state.players[0].days_disappearing = status == 1;
                detach_state.players[0].days_in_prison = status == 2;
                detach_state.players[0].days_in_hospital = status == 3;
                presentation.dynamic_objects[0].type = 5;
                presentation.dynamic_objects[0].node = 1;
                presentation.dynamic_objects[0].attached_player = 1;
                assert(rich4_app_begin_dynamic_god_detach(
                    &presentation, &detach_state, 0, 1000));
                assert(!presentation.god_detachment.active &&
                       !presentation.god_detachment.sound_pending &&
                       !presentation.god_detachment.speech_pending);
            }
        }
    }

    {
        MapData map;
        MapRouteNode routes[8];
        GameEvent event;
        GameEventQueue events;
        int i;

        memset(&map, 0, sizeof(map));
        memset(routes, 0, sizeof(routes));
        map.routes = routes;
        map.route_count = 8;
        for (i = 1; i < 8; i++) {
            routes[i].x = (int16_t)(i * 1000);
            routes[i].next[0] = (uint16_t)(i == 7 ? 1 : i + 1);
        }
        memset(&state, 0, sizeof(state));
        state.active = 1;
        state.player_count = 1;
        state.hospital_node = 6;
        state.players[0].play_state = 1;
        state.players[0].node = 3;
        memset(&presentation, 0, sizeof(presentation));
        rich4_app_init_dynamic_object_pool(&presentation);
        presentation.dynamic_objects[10].node = 3;
        rich4_game_event_queue_reset(&events);
        assert(rich4_app_resolve_dynamic_dog(
                   &presentation, &state, &map, 0, &events) == 11);
        assert(presentation.dynamic_objects[10].node == 0);
        assert(presentation.dynamic_objects[11].type == 12);
        assert(presentation.dynamic_objects[11].node > 0);
        assert(state.players[0].node == 3 &&
               state.players[0].days_in_hospital == 0 &&
               state.players[0].confinement_actor_type == GAME_CONFINEMENT_NONE);
        assert(state.players[0].play_state & GAME_PLAYER_PLAY_STATE_FROZEN);
        assert(events.count == 1);
        assert(rich4_game_event_queue_pop(&events, &event));
        assert(event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED);
        assert(rich4_game_event_tool_resource(&event) == 532);
        assert(rich4_game_event_tool_effect(&event) == 93);
        assert(rich4_game_event_target_ref(&event).kind ==
               GAME_DOMAIN_ROUTE_NODE);
        assert(rich4_game_event_target_ref(&event).id == 3);
        memset(&state, 0, sizeof(state));
        state.active = 1;
        state.player_count = 1;
        state.hospital_node = 6;
        state.players[0].play_state = 1;
        state.players[0].node = 4;
        state.players[0].traffic_method = 1;
        presentation.dynamic_objects[10].node = 4;
        rich4_game_event_queue_reset(&events);
        assert(rich4_app_resolve_dynamic_dog(
                   &presentation, &state, &map, 0, &events) == 11);
        assert(events.count == 1);
        assert(rich4_game_event_queue_pop(&events, &event));
        assert(rich4_game_event_tool_resource(&event) == 552);
        assert(rich4_game_event_tool_effect(&event) == 85);
        assert(state.players[0].node == 4 &&
               state.players[0].days_in_hospital == 0 &&
               state.players[0].confinement_actor_type ==
                   GAME_CONFINEMENT_NONE);
        assert(state.players[0].traffic_method == 1);
    }
    puts("app dynamic object flow: ok");
    return 0;
}
