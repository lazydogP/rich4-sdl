#include <assert.h>
#include "core/god.h"
#include <stdio.h>
#include <string.h>

#include "app/dynamic_object_flow.h"
#include "app/turn_movement_flow.h"
#include "app/property_flow.h"
#include "app/route_event_flow.h"
#include "app/route_flow.h"
#include "app/property_flow.h"
#include "app/tool_followup_flow.h"
#include "core/route_movement.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"

static void setup_short_route(GameSession *session, MapData *map,
                                MapRouteNode nodes[5], int steps)
{
    int i;
    memset(session, 0, sizeof(*session));
    memset(map, 0, sizeof(*map));
    memset(nodes, 0, 5 * sizeof(*nodes));
    map->routes = nodes;
    map->route_count = 5;
    for (i = 1; i < 5; ++i) {
        nodes[i].x = i * 8;
        nodes[i].next[0] = i == 4 ? 1 : i + 1;
        nodes[i].object.raw_object_id = 0;
    }
    session->state.active = 1;
    session->state.price_index = 1;
    session->state.player_count = 2;
    session->state.hospital_node = 4;
    session->state.tool_route_node_count = 5;
    for (i = 0; i < 2; ++i) {
        session->state.players[i].play_state = 1;
        session->state.players[i].attached_timed_bomb_slot = -1;
        session->state.players[i].cash = 10000;
    }
    session->state.players[0].node = 1;
    session->state.players[0].next_node = 2;
    session->state.players[1].node = 3;
    session->flow.turn_flow_active = 1;
    session->flow.turn_state = GAME_TURN_STATE_MOVE;
    session->flow.turn_state_tick = 100;
    session->flow.steps_remaining = steps;
    rich4_app_init_dynamic_object_pool(&session->presentation);
    assert(rich4_route_move_begin_with_next(
        &session->flow.route_move, map, 1, 0, 2, steps) == ROUTE_MOVE_READY);
}

static void assert_loop_state(GameSession *session, const MapData *map,
                              int moving, int bomb)
{
    Rich4AudioEventQueue queue;
    Rich4AudioEvent event;
    int seen = 0;
    rich4_audio_event_queue_init(&queue);
    rich4_app_collect_session_audio_events(session, map, &queue);
    while (rich4_audio_event_pop(&queue, &event)) {
        if (event.type == RICH4_AUDIO_EVENT_MOVEMENT) {
            assert(event.active == moving);
            seen |= 1;
        } else if (event.type == RICH4_AUDIO_EVENT_ATTACHED_BOMB) {
            assert(event.active == bomb);
            seen |= 2;
        }
    }
    assert(seen == 3);
}

static void movement_bomb_audio_lifecycle(void)
{
    GameSession session;
    MapData map;
    MapRouteNode nodes[5];
    Rich4AppTurnMovementResult result;
    int scenario;
    for (scenario = 0; scenario < 4; ++scenario) {
        setup_short_route(&session, &map, nodes, scenario == 3 ? 1 : 3);
        session.state.players[0].attached_timed_bomb_slot = 20;
        session.state.tool_placed_objects[20].kind = 18;
        session.state.tool_placed_objects[20].attached_player = 1;
        session.state.tool_placed_objects[20].countdown = scenario == 2 ? 1 : 10;
        if (scenario == 0) session.state.players[1].node = 2; /* transfer */
        if (scenario == 1) {
            session.state.tool_placed_objects[0].kind = 16;
            session.state.tool_placed_objects[0].node = 2;
        }
        assert(rich4_app_update_turn_movement(&session, &map, 20, 120, &result));
        assert(!session.presentation.actors[0].move_active);
        assert_loop_state(&session, &map, 1, 1); /* edge boundary, no restart */
        assert(rich4_app_update_turn_movement(&session, &map, 20, 140, &result));
        assert_loop_state(&session, &map, scenario == 0, scenario == 1);
        if (scenario == 1) {
            assert(rich4_app_update_turn_movement(&session, &map, 20, 160, &result));
            assert_loop_state(&session, &map, 0, 0);
        }
    }
}

static void road_chest_arrival(void)
{
    GameSession s;
    MapData map;
    MapRouteNode nodes[5];
    Rich4AppTurnMovementResult result;
    int steps;
    for (steps = 1; steps <= 2; ++steps) {
        setup_short_route(&s, &map, nodes, steps);
        rich4_app_init_dynamic_object_pool(&s.presentation);
        s.presentation.dynamic_objects[13].node = 2;
        assert(rich4_app_update_turn_movement(&s, &map, 20, 120, &result));
        assert(s.state.players[0].node == 2);
        assert(s.presentation.dynamic_objects[13].node == 2);
        assert(rich4_app_update_turn_movement(&s, &map, 20, 140, &result));
        assert(s.presentation.special_reward.active == (steps == 1));
        assert(s.presentation.dynamic_objects[13].node == (steps == 1 ? 0 : 2));
        assert(s.state.players[0].points == 0);
    }
}

static void road_gift_arrival(void)
{
    GameSession s;
    MapData map;
    MapRouteNode nodes[5];
    Rich4AppTurnMovementResult result;
    int steps;
    for (steps = 1; steps <= 2; ++steps) {
        setup_short_route(&s, &map, nodes, steps);
        rich4_app_init_dynamic_object_pool(&s.presentation);
        s.presentation.dynamic_objects[12].node = 2;
        s.state.tool_amount[0] = 1;
        assert(rich4_app_update_turn_movement(&s, &map, 20, 120, &result));
        assert(s.state.players[0].node == 2);
        assert(s.presentation.dynamic_objects[12].node == 2);
        assert(rich4_app_update_turn_movement(&s, &map, 20, 140, &result));
        assert(s.presentation.special_reward.active == (steps == 1));
        assert(s.presentation.dynamic_objects[12].node == (steps == 1 ? 0 : 2));
        assert(s.state.players[0].tool_inventory[0] == (steps == 1));
        assert(s.state.tool_amount[0] == (steps == 1 ? 0 : 1));
    }
}

static void obstacle_order(void)
{
    GameSession session;
    MapData map;
    MapRouteNode nodes[5];
    Rich4AppTurnMovementResult result;
    Rich4AppRouteEventServices services = { 0 };
    Rich4AppTurnRotationResult rotation = { 0 };
    int final_step;
    uint32_t now;
    services.property_end_delay_ms = 20;
    for (final_step = 0; final_step <= 1; ++final_step) {
        setup_short_route(&session, &map, nodes, final_step ? 1 : 3);
        session.state.tool_placed_objects[0].kind = 16;
        session.state.tool_placed_objects[0].node = 2;
        assert(rich4_app_update_turn_movement(&session, &map, 20, 120, &result));
        assert(session.state.players[0].node == 2);
        assert(session.state.tool_placed_objects[0].kind == 16);
        assert(session.flow.turn_wait == RICH4_MOVE_ARRIVAL);
        assert(rich4_app_update_turn_movement(&session, &map, 20, 140, &result));
        assert(session.state.tool_placed_objects[0].kind == 0);
        assert(session.state.tool_amount[1] == 1);
        assert(session.flow.steps_remaining == 0);
        assert(session.flow.turn_wait == (final_step ? 5 : RICH4_MOVE_FORCED_STOP));
        session.presentation.speech.active = 1;
        assert(!rich4_tool_followup_finish_trigger_turn_and_advance(
            &session, &map, 1000, &rotation));
        session.presentation.speech.active = 0;
        assert(rich4_tool_followup_finish_trigger_turn_and_advance(
            &session, &map, 1000, &rotation));
        assert(session.flow.turn_state == GAME_TURN_STATE_MOVE);
        now = 1000;
        if (!final_step) {
            assert(rich4_app_update_turn_movement(&session, &map, 20, now, &result));
            assert(session.flow.turn_wait == 5);
            now += 20;
        }
        for (int remaining = 4; remaining >= 0; --remaining, now += 20) {
            assert(rich4_app_update_turn_movement(&session, &map, 20, now, &result));
            assert(session.flow.turn_wait == remaining);
            if (remaining) assert(session.flow.turn_state == GAME_TURN_STATE_MOVE);
        }
        assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
        assert(session.flow.route_facts.count == 0);
        assert(session.state.players[0].node == 2);
        now -= 20; /* dispatch follows the final stop pulse */
        assert(rich4_app_route_event_update(&session, &map, now, 1, &services));
        assert(rich4_app_route_event_update(&session, &map, now + 1, 1, &services));
        assert(session.flow.property_turn_end_ticks == 1);
        assert(rich4_app_route_event_update(&session, &map, now + 21, 1, &services));
        assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
    }
    /* Existing bomb countdown is resolved before the roadblock on arrival. */
    setup_short_route(&session, &map, nodes, 3);
    session.state.players[0].attached_timed_bomb_slot = 20;
    session.state.tool_placed_objects[20].kind = 18;
    session.state.tool_placed_objects[20].attached_player = 1;
    session.state.tool_placed_objects[20].countdown = 1;
    session.state.tool_placed_objects[0].kind = 16;
    session.state.tool_placed_objects[0].node = 2;
    session.state.players[1].node = 2;
    assert(rich4_app_update_turn_movement(&session, &map, 20, 120, &result));
    assert(rich4_app_update_turn_movement(&session, &map, 20, 140, &result));
    /* The SDL continuation admits only after explosion/reaction return. */
    assert(session.state.players[0].days_in_hospital == 0);
    assert(session.state.players[0].node == 2);
    assert(session.state.players[1].days_in_hospital == 0);
    assert(session.state.tool_placed_objects[0].kind == 16);
    assert(session.state.tool_amount[3] == 1);
    assert(session.flow.steps_remaining == 0);

    /* A new bomb starts at 38 and is not advanced on its pickup square. */
    setup_short_route(&session, &map, nodes, 1);
    session.state.tool_placed_objects[20].kind = 18;
    session.state.tool_placed_objects[20].node = 2;
    assert(rich4_app_update_turn_movement(&session, &map, 20, 120, &result));
    assert(rich4_app_update_turn_movement(&session, &map, 20, 140, &result));
    assert(session.state.tool_placed_objects[20].countdown == 38);
    assert(session.state.players[0].attached_timed_bomb_slot == 20);
    assert(result.events.count == 2); /* attach, speech */

    /* A natural stop donates before removing a roadblock. An intermediate
     * forced stop skips the earlier beggar branch and does not replay it. */
    for (final_step = 0; final_step <= 1; ++final_step) {
        setup_short_route(&session, &map, nodes, final_step ? 1 : 3);
        session.state.players[1].node = 2;
        session.state.players[1].play_state = 0;
        session.state.tool_placed_objects[0].kind = 16;
        session.state.tool_placed_objects[0].node = 2;
        rich4_app_sync_tool_dynamic_objects(&session.presentation, &session.state);
        assert(rich4_app_update_turn_movement(&session, &map, 20, 120, &result));
        assert(session.presentation.dynamic_objects[16].node == 2);
        assert(rich4_app_update_turn_movement(&session, &map, 20, 140, &result));
        if (final_step) {
            assert(session.flow.turn_wait == RICH4_MOVE_BEGGAR);
            assert(session.state.tool_placed_objects[0].kind == 16);
            assert(session.presentation.dynamic_objects[16].node == 2);
            assert(session.state.players[0].cash == 10000);
            assert(rich4_property_skip_wait(&session, 200));
            rich4_property_update_charge_presentation(&session, 200);
            assert(session.state.players[0].cash == 9000);
            assert(rich4_app_route_event_update(&session, &map, 201, 1, &services));
            assert(session.flow.turn_wait == RICH4_MOVE_AFTER_BEGGAR);
            assert(rich4_app_update_turn_movement(&session, &map, 20, 220, &result));
        } else {
            assert(session.state.players[0].cash == 10000);
            assert(session.flow.turn_wait == RICH4_MOVE_FORCED_STOP);
        }
        assert(session.state.tool_placed_objects[0].kind == 0);
        /* Removal is rendered before the caller starts the roadblock speech. */
        assert(session.presentation.dynamic_objects[16].node == 0);
    }
}

static void landing_gates(void)
{
    GameSession session;
    MapData map;
    MapRouteNode nodes[5];
    Rich4AppTurnMovementResult result;
    Rich4AppRouteEventServices services = { 0 };
    int variant, tick;
    services.property_end_delay_ms = 20;
    for (variant = 0; variant < 3; ++variant) {
        setup_short_route(&session, &map, nodes, 3);
        nodes[2].flags = 14;
        if (variant == 1) session.state.players[0].days_sleep_walking = 1;
        if (variant == 2) {
            session.state.tool_placed_objects[0].kind = 16;
            session.state.tool_placed_objects[0].node = 2;
        }
        assert(rich4_app_update_turn_movement(&session, &map, 20, 120, &result));
        assert(rich4_app_update_turn_movement(&session, &map, 20, 140, &result));
        if (variant == 0) {
            assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
            assert(session.flow.turn_wait == RICH4_MOVE_AFTER_BANK);
            assert(session.flow.steps_remaining == 2);
        } else {
            assert(session.flow.turn_state == GAME_TURN_STATE_MOVE);
            assert(session.flow.route_facts.count == 0);
        }
    }
    /* 0x40C912(1) is evaluated only after the five stop ticks. */
    setup_short_route(&session, &map, nodes, 1);
    session.state.players[0].days_sleeping = 1;
    nodes[2].flags = 14;
    for (tick = 120; tick <= 240; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, &map, 20, (uint32_t)tick, &result));
    assert(session.flow.turn_wait == RICH4_ROUTE_END_COUNTDOWN);
    assert(session.flow.property_turn_end_ticks == 3);
    assert(session.flow.route_facts.count == 0);
    for (tick = 260; tick <= 300; tick += 20)
        assert(rich4_app_route_event_update(
            &session, &map, (uint32_t)tick, 1, &services));
    assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
}

static void property_tail_order(void)
{
    GameSession session;
    MapData map;
    MapRouteNode nodes[5];
    Rich4AppTurnMovementResult result;
    Rich4AppRouteEventServices services = { 0 };
    int tick;
    services.property_end_delay_ms = 20;
    setup_short_route(&session, &map, nodes, 1);
    nodes[2].object.raw_object_id = 0x7d1;
    session.state.route_object_raw[2] = 0x7d1;
    session.state.land_count = 1;
    session.state.land_owner[0] = -1;
    session.state.land_level[0] = 1;
    session.state.land_price[0] = 100;
    session.state.land_price_per_level[0] = 10;
    session.state.land_value[0] = 110;
    session.presentation.dynamic_objects[11].node = 2;
    for (tick = 120; tick <= 240; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, &map, 20, (uint32_t)tick, &result));
    assert(session.state.players[0].god_type == 12);
    assert(session.state.land_owner[0] == -1);
    assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(rich4_app_route_event_update(&session, &map, 240, 1, &services));
    assert(session.state.land_owner[0] == -1);
    assert(!session.presentation.route_interaction.active);
    assert(rich4_app_route_event_update(&session, &map, 241, 1, &services));
    assert(session.state.land_owner[0] == 0);
    assert(session.state.fixed_asset_value[0] == 110);
    assert(session.state.players[0].cash == 10000);
    assert(session.flow.turn_wait == RICH4_ROUTE_END_EFFECTS);
    assert(rich4_app_route_event_update(&session, &map, 242, 1, &services));
    assert(!session.presentation.property.sound_pending);
    assert(!session.presentation.speech.active);
    assert(session.presentation.property.phase == GAME_PROPERTY_PRESENT_GOD_BUILD_TEXT);
    assert(rich4_property_update_charge_presentation(&session, 1741));
    assert(session.presentation.speech.active);
    session.presentation.speech.active = 0;
    assert(!rich4_property_update_charge_presentation(&session, 1742));
    assert(rich4_app_route_event_update(&session, &map, 1742, 1, &services));
    assert(session.flow.property_turn_end_ticks == 8);
    for (tick = 1762; tick < 1902; tick += 20) {
        assert(rich4_app_route_event_update(
            &session, &map, (uint32_t)tick, 1, &services));
        assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    }
    assert(rich4_app_route_event_update(&session, &map, 1902, 1, &services));
    assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
}

int main(void)
{
    GameSession session;
    Rich4AppTurnMovementResult result;
    int tick;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[0].traffic_method = 0;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 80;
    session.flow.steps_remaining = 1;

    if (!rich4_app_update_turn_movement(&session, NULL, 20, 100, &result) ||
        !session.presentation.actors[0].move_active ||
        session.presentation.actors[0].direction != 2 ||
        session.presentation.movement_tick != 100 ||
        session.presentation.actors[0].move_progress != 1 ||
        session.state.players[0].node != 1 ||
        result.events.count != 0) {
        fprintf(stderr, "turn movement initial step mismatch\n");
        return 1;
    }

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[0].traffic_method = 0;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 80;
    session.flow.steps_remaining = 1;
    if (!rich4_app_update_turn_movement(&session, NULL, 20, 200, &result) ||
        !session.presentation.actors[0].move_active ||
        session.presentation.actors[0].move_progress != 1 ||
        session.presentation.movement_tick != 200 ||
        !rich4_app_update_turn_movement(&session, NULL, 20, 220, &result) ||
        session.presentation.actors[0].move_progress != 2 ||
        session.presentation.movement_tick != 220) {
        fprintf(stderr, "turn movement missed-pulse coalescing mismatch\n");
        return 1;
    }

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[0].play_state = 1;
    session.state.players[0].traffic_method = 0;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 100;
    session.flow.steps_remaining = 1;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    session.presentation.dynamic_objects[14].node = 2;
    if (!rich4_app_update_turn_movement(&session, NULL, 20, 100, &result) ||
        session.presentation.actors[0].move_active ||
        session.presentation.actors[0].move_progress != 0) {
        fprintf(stderr, "turn movement first pulse was not delayed\n");
        return 1;
    }
    for (tick = 120; tick <= 240; tick += 20)
        if (!rich4_app_update_turn_movement(
                &session, NULL, 20, (uint32_t)tick, &result))
            return 1;
    if (session.state.players[0].god_info != 15 ||
        session.state.players[0].god_type != 15 ||
        session.presentation.dynamic_objects[14].attached_player != 1 ||
        session.presentation.dynamic_objects[14].countdown != 13) {
        fprintf(stderr, "turn movement god landing mismatch\n");
        return 1;
    }

    /* Collecting Land God does not apply the property tail during arrival. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.price_index = 1;
    session.state.land_count = 1;
    session.state.route_object_raw[2] = 0x7d1;
    session.state.land_owner[0] = -1;
    session.state.land_level[0] = 1;
    session.state.land_type[0] = 0;
    session.state.land_price[0] = 100;
    session.state.land_price_per_level[0] = 10;
    session.state.land_value[0] = 110;
    session.state.players[0].cash = 1000;
    session.state.players[0].play_state = 1;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 100;
    session.flow.steps_remaining = 1;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    session.presentation.dynamic_objects[11].node = 2;
    for (tick = 100; tick <= 240; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.state.players[0].god_type == 12 &&
           session.state.land_owner[0] == -1 &&
           session.state.players[0].cash == 1000 &&
           session.state.fixed_asset_value[0] == 0);

    /* An empty level-zero square is not claimed by the original Land God,
     * even though the ordinary purchase dialog is suppressed. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.price_index = 1;
    session.state.land_count = 1;
    session.state.route_object_raw[2] = 0x7d1;
    session.state.land_owner[0] = -1;
    session.state.land_level[0] = 0;
    session.state.land_price[0] = 100;
    session.state.land_price_per_level[0] = 10;
    session.state.land_value[0] = 100;
    session.state.players[0].cash = 1000;
    session.state.players[0].play_state = 1;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 100;
    session.flow.steps_remaining = 1;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    session.presentation.dynamic_objects[11].node = 2;
    for (tick = 100; tick <= 240; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.state.players[0].god_type == 12 &&
           session.state.land_owner[0] == -1 &&
           session.state.players[0].cash == 1000);

    /* Passing over a god square is not a landing. The final square is
     * checked separately to make sure the stop path still collects gods. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[0].play_state = 1;
    session.state.players[0].traffic_method = 0;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 100;
    session.flow.steps_remaining = 2;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    session.presentation.dynamic_objects[14].node = 2;
    session.presentation.dynamic_objects[0].node = 3;
    assert(rich4_app_update_turn_movement(&session, NULL, 20, 100, &result));
    for (tick = 120; tick <= 240; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.state.players[0].node == 2 &&
           session.state.players[0].god_info == 0 &&
           session.presentation.dynamic_objects[14].attached_player == 0);
    for (tick = 260; tick <= 360; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.state.players[0].node == 3 &&
           session.state.players[0].god_info == 1 &&
           session.presentation.dynamic_objects[0].attached_player == 1);
    /* The number wheel owns this landing before the turn can rotate. */
    assert(session.flow.turn_state == GAME_TURN_STATE_MOVE &&
           session.flow.steps_remaining == 0);
    assert(rich4_god_resolve_money(&session.state, 0, 1, 1, 0,
                                   &result.events) == GAME_GOD_APPLIED);
    assert(rich4_app_update_turn_movement(&session, NULL, 20, 370, &result));
    assert(session.flow.turn_state == GAME_TURN_STATE_MOVE);
    for (tick = 380; tick <= 460; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);

    /* A type-11 dog is a landing encounter, not a road god.  It must be
     * consumed by the stop path and surface the resource presentation event. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.hospital_node = 4;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[0].play_state = 1;
    session.state.players[0].traffic_method = 0;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 100;
    session.flow.steps_remaining = 1;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    session.presentation.dynamic_objects[10].node = 2;
    for (tick = 100; tick <= 240; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.presentation.dynamic_objects[10].node == 0);
    /* 0x41B8EF calls 0x43EC3F only after the bite and reaction. */
    assert(session.state.players[0].node == 2 &&
           session.state.players[0].days_in_hospital == 0 &&
           session.state.players[0].confinement_actor_type ==
               GAME_CONFINEMENT_NONE);
    assert(result.events.count == 1);
    {
        GameEvent dog_event;
        assert(rich4_game_event_queue_pop(&result.events, &dog_event));
        assert(dog_event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED);
        assert(rich4_game_event_tool_resource(&dog_event) == 532);
        assert(rich4_game_event_tool_effect(&dog_event) == 93);
    }

    /* The original route-object dispatcher is reached at every edge
     * boundary, but type 11 itself is guarded by the remaining-step counter.
     * Passing a dog square therefore leaves the dog in place and emits no
     * encounter presentation; only the final stop consumes it. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[0].play_state = 1;
    session.state.players[0].traffic_method = 0;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_state_tick = 100;
    session.flow.steps_remaining = 2;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    session.presentation.dynamic_objects[10].node = 2;
    for (tick = 100; tick <= 240; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.state.players[0].node == 2 &&
           session.presentation.dynamic_objects[10].node == 2 &&
           result.events.count == 0);
    for (tick = 260; tick <= 360; tick += 20)
        assert(rich4_app_update_turn_movement(
            &session, NULL, 20, (uint32_t)tick, &result));
    assert(session.state.players[0].node == 3 &&
           session.presentation.dynamic_objects[10].node == 2 &&
           result.events.count == 0);
    road_chest_arrival();
    road_gift_arrival();
    obstacle_order();
    movement_bomb_audio_lifecycle();
    property_tail_order();
    landing_gates();
    puts("app turn movement flow: ok");
    return 0;
}
