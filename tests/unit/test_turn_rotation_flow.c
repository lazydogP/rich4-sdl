#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/dynamic_object_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/game_constants.h"
#include "core/rng.h"
#include "core/god.h"
#include "view/game_view_model.h"

static void setup_session(GameSession *session)
{
    int i;

    memset(session, 0, sizeof(*session));
    session->state.active = 1;
    session->state.player_count = 1;
    session->state.players[0].play_state = 1;
    session->state.players[0].linked_player = -1;
    session->state.players[0].attached_timed_bomb_slot = -1;
    session->state.turn_rotation_index = 0;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        session->state.special_player_f10[i] = 1;
}

static void setup_ring_map(MapData *map, MapRouteNode routes[13])
{
    int i;

    memset(map, 0, sizeof(*map));
    memset(routes, 0, sizeof(MapRouteNode) * 13);
    map->routes = routes;
    map->route_count = 13;
    for (i = 1; i <= 12; i++) {
        routes[i].x = (int16_t)(i * 500);
        routes[i].next[0] = (uint16_t)(i == 1 ? 12 : i - 1);
        routes[i].next[1] = (uint16_t)(i == 12 ? 1 : i + 1);
    }
}

static void setup_camera_map(MapData *map,
                             MapRouteNode routes[6],
                             MapSpecialRecord specials[3])
{
    int i;

    memset(map, 0, sizeof(*map));
    memset(routes, 0, sizeof(MapRouteNode) * 6);
    memset(specials, 0, sizeof(MapSpecialRecord) * 3);
    map->routes = routes;
    map->route_count = 6;
    map->specials = specials;
    map->special_count = 3;
    for (i = 1; i < 6; ++i) {
        routes[i].x = (int16_t)(100 + i * 100);
        routes[i].y = (int16_t)(200 + i * 100);
        routes[i].next[0] = (uint16_t)(i == 1 ? 5 : i - 1);
        routes[i].next[1] = (uint16_t)(i == 5 ? 1 : i + 1);
    }
    specials[1].x = 900;
    specials[1].y = 1000;
    specials[2].x = 1100;
    specials[2].y = 1200;
}

static void setup_two_player_camera_session(GameSession *session)
{
    int i;

    setup_session(session);
    session->state.player_count = 2;
    for (i = 0; i < 2; ++i) {
        session->state.players[i].play_state = 1;
        session->state.players[i].linked_player = -1;
        session->state.players[i].attached_timed_bomb_slot = -1;
        session->state.special_player_f10[i] = 1;
    }
    session->state.players[0].node = 1;
    session->state.players[1].node = 2;
}

static void test_camera_rotation_rules(void)
{
    GameSession session;
    MapData map;
    MapRouteNode routes[6];
    MapSpecialRecord specials[3];
    GameViewModelInput input;
    GameViewportModel viewport;

    setup_camera_map(&map, routes, specials);
    setup_two_player_camera_session(&session);
    session.presentation.has_view_override = 1;
    session.presentation.view_world_x = 777;
    session.presentation.view_world_y = 888;
    {
        Rich4AppTurnRotationResult result;
        assert(rich4_app_advance_turn_rotation(
                   &session, &map, 1000, &result));
    }
    assert(session.state.current_player == 1);
    assert(session.presentation.has_view_override == 0);

    memset(&input, 0, sizeof(input));
    rich4_game_viewport_build(
        &session.state, &input, &map, &viewport);
    assert(viewport.center_world_x == routes[2].x);
    assert(viewport.center_world_y == routes[2].y);

    setup_two_player_camera_session(&session);
    session.state.players[1].days_in_hospital = 3;
    session.state.players[1].confinement_actor_type =
        GAME_CONFINEMENT_HOSPITAL;
    {
        Rich4AppTurnRotationResult result;
        assert(rich4_app_advance_turn_rotation(
                   &session, &map, 1000, &result));
    }
    assert(session.state.current_player == 1);
    assert(session.presentation.has_view_override == 0);
    memset(&input, 0, sizeof(input));
    rich4_game_viewport_build(
        &session.state, &input, &map, &viewport);
    assert(viewport.center_world_x == specials[1].x);
    assert(viewport.center_world_y == specials[1].y);

    setup_session(&session);
    session.state.players[0].node = 1;
    session.state.players[0].days_in_hospital = GAME_STATUS_PENDING_BIT;
    session.state.players[0].confinement_actor_type =
        GAME_CONFINEMENT_HOSPITAL;
    session.presentation.has_view_override = 1;
    session.presentation.view_world_x = 777;
    session.presentation.view_world_y = 888;
    {
        Rich4AppTurnRotationResult result;
        assert(rich4_app_advance_turn_rotation(
                   &session, &map, 1000, &result));
    }
    assert(session.presentation.confinement_exit.active == 1);
    assert(session.presentation.confinement_exit.player == 0);
    assert(session.presentation.has_view_override == 0);
    assert(session.presentation.actors[0].move_active == 1);
    assert(session.presentation.actors[0].visual_x == specials[1].x);
    assert(session.presentation.actors[0].visual_y == specials[1].y);

    setup_session(&session);
    session.state.players[0].node = 1;
    session.state.players[0].days_in_prison = GAME_STATUS_PENDING_BIT;
    session.state.players[0].confinement_actor_type = GAME_CONFINEMENT_PRISON;
    assert(rich4_app_advance_turn_rotation(
               &session, &map, 1000,
               &(Rich4AppTurnRotationResult){0}));
    assert(session.presentation.confinement_exit.active == 1);
    assert(session.presentation.actors[0].move_active == 1);
    assert(session.presentation.actors[0].visual_x == specials[2].x);
    assert(session.presentation.actors[0].visual_y == specials[2].y);
}

static void test_normal_status_consumption(void)
{
    GameSession session;
    Rich4AppTurnRotationResult result;

    setup_session(&session);
    session.state.players[0].days_in_prison = GAME_STATUS_PENDING_BIT;
    assert(rich4_app_advance_turn_rotation(&session, NULL, 1000, &result));
    assert(!result.special_action_pending);
    assert(session.state.current_player == 0);
    assert(session.state.players[0].days_in_prison == 0);
    assert(session.debug.status_event_processed_count == 1);
    assert((session.presentation.actors[0].dirty_flags &
            GAME_PLAYER_DIRTY_TURN) != 0);
}

static void test_special_action_is_returned_to_platform_seam(void)
{
    GameSession session;
    MapData map;
    MapRouteNode routes[13];
    Rich4AppTurnRotationResult result;

    setup_session(&session);
    setup_ring_map(&map, routes);
    session.state.special_player_f10[0] = 0;
    session.state.special_player_node[0] = 1;
    session.state.special_player_previous_node[0] = 12;
    session.state.ai_rng_state = 7;
    assert(rich4_app_advance_turn_rotation(&session, &map, 2000, &result));
    assert(result.special_action_pending);
    assert(result.special_action.kind == TURN_SPECIAL_AUTO_MOVED);
    assert(result.special_action.start_node == 1);
    assert(result.special_action.end_node != 1);
    assert(session.state.special_player_node[0] ==
           result.special_action.end_node);
}

static void test_control_locked_turn_cleanup(void)
{
    GameSession session;
    Rich4AppTurnRotationResult result;

    setup_session(&session);
    session.flow.turn_flow_active = 1;
    session.flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    session.flow.steps_remaining = 3;
    session.presentation.dice.popup_active = 1;
    session.presentation.dice.status_visible = 1;
    session.presentation.dice.status_dragging = 1;
    session.presentation.dice.action_hint_active = 1;
    session.presentation.dice.input_refresh_active = 1;
    session.presentation.confinement_status.active = 1;
    session.presentation.confinement_status.until = 200;
    assert(rich4_app_finish_control_locked_turn(
        &session, NULL, 1000, &result));
    assert(session.flow.turn_flow_active == 0);
    assert(session.flow.turn_state == GAME_TURN_STATE_IDLE);
    assert(session.flow.steps_remaining == 0);
    assert(session.presentation.dice.popup_active == 0);
    assert(session.presentation.dice.status_visible == 0);
    assert(session.presentation.dice.status_dragging == 0);
    assert(session.presentation.dice.action_hint_active == 0);
    assert(session.presentation.dice.input_refresh_active == 0);
    assert(!session.presentation.confinement_status.active);
    assert(session.presentation.actors[0].dirty_flags & GAME_PLAYER_DIRTY_TURN);
}

static void test_confinement_exit_tick(void)
{
    GameSession session;

    setup_session(&session);
    session.presentation.confinement_exit.active = 1;
    session.presentation.confinement_exit.player = 0;
    session.presentation.actors[0].move_from_x = 10;
    session.presentation.actors[0].move_from_y = 20;
    session.presentation.actors[0].move_to_x = 30;
    session.presentation.actors[0].move_to_y = 40;
    session.presentation.actors[0].move_distance = 2;
    assert(rich4_app_update_confinement_exit(&session, 100, 50));
    assert(session.presentation.confinement_exit.last_step_tick == 100);
    assert(rich4_app_update_confinement_exit(&session, 149, 50));
    assert(session.presentation.actors[0].move_progress == 0);
    assert(rich4_app_update_confinement_exit(&session, 150, 50));
    assert(session.presentation.actors[0].move_progress == 1);
    assert(!rich4_app_update_confinement_exit(&session, 200, 50));
    assert(!session.presentation.confinement_exit.active);
    assert(session.presentation.actors[0].move_active == 0);
    assert(session.presentation.actors[0].visual_x == 30);
    assert(session.presentation.actors[0].visual_y == 40);
}

static void test_monthly_bank_statement_timer(void)
{
    GameSession session;
    Rich4AppTurnRotationResult result;

    setup_session(&session);
    session.state.players[0].bank = 1000;
    session.state.calendar_start_year = 2000;
    session.state.calendar_start_month = 1;
    session.state.calendar_start_day = 31;
    assert(rich4_app_advance_turn_rotation(&session, NULL, 5000, &result));
    assert(session.presentation.bank_settlement.active);
    assert(session.presentation.bank_settlement.tick == 5000);
    assert(session.presentation.bank_settlement.until == 8200);
    assert(session.presentation.bank_settlement.interest[0] == 100);
    assert(session.presentation.bank_settlement.balance[0] == 1100);
}

static void test_day_limit_finalizes_game(void)
{
    GameSession session;
    Rich4AppTurnRotationResult result;

    setup_session(&session);
    session.state.players[0].cash = 100;
    session.state.day_limit = 1;
    assert(rich4_app_advance_turn_rotation(&session, NULL, 6000, &result));
    assert(session.state.game_over);
    assert(session.state.winner_player == 0);
    assert(session.state.winner_reason == GAME_WIN_CANDIDATE_DAY_LIMIT);
    assert(!rich4_app_advance_turn_rotation(
        &session, NULL, 7000, &result));
}

static void test_monthly_dynamic_objects_are_replaced(void)
{
    GameSession session;
    MapData map;
    MapRouteNode routes[13];
    Rich4AppTurnRotationResult result;
    int old_gift_node;
    int old_chest_node;

    setup_session(&session);
    setup_ring_map(&map, routes);
    session.state.calendar_start_year = 2000;
    session.state.calendar_start_month = 1;
    session.state.calendar_start_day = 31;
    rich4_rng_seed(11);
    rich4_app_init_dynamic_object_pool(&session.presentation);
    rich4_app_initialize_dynamic_map_objects(&session.presentation, &map);
    old_gift_node = session.presentation.dynamic_objects[12].node;
    old_chest_node = session.presentation.dynamic_objects[13].node;
    assert(old_gift_node > 0 && old_chest_node > 0);

    assert(rich4_app_advance_turn_rotation(
               &session, &map, 5000, &result));
    assert(session.state.elapsed_days == 1);
    assert(session.presentation.dynamic_objects[12].type == 13);
    assert(session.presentation.dynamic_objects[13].type == 14);
    assert(session.presentation.dynamic_objects[12].node > 0);
    assert(session.presentation.dynamic_objects[13].node > 0);
    assert(session.presentation.dynamic_objects[12].node != old_gift_node);
    assert(session.presentation.dynamic_objects[13].node != old_chest_node);
    assert(session.presentation.dynamic_objects[12].node !=
           session.presentation.dynamic_objects[13].node);
}

static void test_god_expiry(void)
{
    GameSession s;
    Rich4AppTurnRotationResult result;
    GameEventQueue events;
    int ai, confined;
    for (ai = 0; ai < 2; ai++) for (confined = 0; confined < 2; confined++) {
        setup_session(&s);
        s.state.players[0].is_ai = ai;
        s.state.players[0].play_state = ai ? 2 : 1;
        s.state.players[0].node = 1;
        rich4_game_event_queue_reset(&events);
        assert(rich4_god_attach(&s.state, 0, 5, 5, &events) == GAME_GOD_APPLIED);
        s.presentation.dynamic_objects[4].type = 5;
        s.presentation.dynamic_objects[4].attached_player = 1;
        s.presentation.dynamic_objects[4].countdown = 2;
        s.presentation.dynamic_objects[4].node = 1;
        if (confined) s.state.players[0].days_in_prison = 5;
        assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, &result));
        assert(s.state.players[0].god_info == 5);
        assert(s.presentation.dynamic_objects[4].countdown == 1);
        assert(rich4_app_advance_turn_rotation(&s, NULL, 2000, &result));
        assert(s.state.players[0].god_info == 0 && s.state.players[0].god_type == 0);
        assert(s.presentation.dynamic_objects[4].attached_player == 0);
        assert(s.presentation.god_detachment.active == !confined);
        if (!confined) assert(s.presentation.god_detachment.god_type == 5);
    }
}

static void test_calendar_defers_turn_status(void)
{
    GameSession s;
    GameEventQueue events;
    Rich4AppTurnRotationResult result;
    int day;
    for (day = 14; day <= 31; day += 17) {
        setup_session(&s);
        s.state.calendar_start_year = 2000;
        s.state.calendar_start_month = 1;
        s.state.calendar_start_day = day;
        s.state.players[0].node = 1;
        s.state.players[0].bank = 1000;
        rich4_game_event_queue_reset(&events);
        assert(rich4_god_attach(&s.state, 0, 5, 5, &events) == GAME_GOD_APPLIED);
        s.presentation.dynamic_objects[4].type = 5;
        s.presentation.dynamic_objects[4].attached_player = 1;
        s.presentation.dynamic_objects[4].countdown = 1;
        s.presentation.dynamic_objects[4].node = 1;
        assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, &result));
        assert(s.pending_turn_events.count > 0);
        assert(s.state.players[0].god_info == 5);
        assert(!s.presentation.god_detachment.active);
        assert(!rich4_app_resume_turn_events(&s, NULL, 2000));
        assert(s.presentation.dynamic_objects[4].countdown == 1);
        s.presentation.periodic_finance.active = 0;
        s.presentation.bank_settlement.active = 0;
        assert(rich4_app_resume_turn_events(&s, NULL, 9000));
        assert(s.pending_turn_events.count == 0);
        assert(s.state.players[0].god_info == 0);
        assert(s.presentation.god_detachment.active);
        assert(!rich4_app_resume_turn_events(&s, NULL, 9100));
    }
}

int main(void)
{
    test_calendar_defers_turn_status();
    test_god_expiry();
    test_normal_status_consumption();
    test_special_action_is_returned_to_platform_seam();
    test_control_locked_turn_cleanup();
    test_confinement_exit_tick();
    test_monthly_bank_statement_timer();
    test_day_limit_finalizes_game();
    test_monthly_dynamic_objects_are_replaced();
    test_camera_rotation_rules();
    puts("app turn rotation flow: ok");
    return 0;
}
