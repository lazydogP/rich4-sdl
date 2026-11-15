#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/dynamic_object_flow.h"
#include "app/bank_flow.h"
#include "app/bankruptcy_flow.h"
#include "app/special_place_notice_flow.h"
#include "app/turn_rotation_flow.h"
#include "app/turn_movement_flow.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
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

static void test_disappearance_return_animation(void)
{
    GameSession session;
    Rich4AppTurnRotationResult result;
    const GameToolAnimationRequest *animation;

    setup_session(&session);
    session.state.players[0].days_disappearing =
        GAME_STATUS_PENDING_BIT | GAME_DISAPPEARING_ALIEN_BIT;
    session.presentation.actors[0].visual_x = 123;
    session.presentation.actors[0].visual_y = 456;
    assert(rich4_app_advance_turn_rotation(&session, NULL, 1000, &result));
    assert(session.state.players[0].days_disappearing == 0);
    assert(session.presentation.tool_animation_count == 1);
    animation = &session.presentation.tool_animation[
        session.presentation.tool_animation_head];
    assert(animation->resource == GAME_DISAPPEARING_ALIEN_RESOURCE);
    assert(animation->effect == GAME_DISAPPEARING_ALIEN_EFFECT);
    assert(animation->origin_play_state ==
           (GAME_STATUS_PENDING_BIT | GAME_DISAPPEARING_ALIEN_BIT));
    assert(animation->after_sync_player == 0);
    assert(animation->world_x == 123 && animation->world_y == 456);
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

    setup_session(&session);
    session.state.special_player_f10[0] = 0;
    session.state.special_player_node[0] = 1;
    session.state.special_player_status[0][0] = 2;
    assert(rich4_app_advance_turn_rotation(&session, &map, 2000, &result));
    assert(result.special_action_pending);
    assert(result.special_action.kind == TURN_SPECIAL_AUTO_BLOCKED);
    assert(result.special_action.path_count == 1);
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

static void test_confinement_exit_defers_arrival_until_next_pulse(void)
{
    const int kinds[] = {0x11, 0x12};
    size_t i;

    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        GameSession session;
        int slot = kinds[i] == 0x11 ? 10 : 20;

        setup_session(&session);
        session.state.players[0].node = 1;
        session.state.tool_placed_objects[slot].kind = (uint8_t)kinds[i];
        session.state.tool_placed_objects[slot].node = 1;
        session.presentation.confinement_exit.active = 1;
        session.presentation.confinement_exit.player = 0;
        session.presentation.confinement_exit.kind = GAME_CONFINEMENT_HOSPITAL;
        session.presentation.actors[0].move_active = 1;
        session.presentation.actors[0].move_to_x = 30;
        session.presentation.actors[0].move_to_y = 40;
        session.presentation.actors[0].move_distance = 1;

        /* 0x40C05C finishes first; 0x40D7C4 -> 0x41B42D runs next pulse. */
        assert(rich4_app_update_confinement_exit(&session, 100, 50));
        assert(!rich4_app_update_confinement_exit(&session, 150, 50));
        assert(session.flow.turn_flow_active);
        assert(session.flow.turn_state == GAME_TURN_STATE_MOVE);
        assert(session.flow.turn_wait == RICH4_MOVE_ARRIVAL);
        assert(session.flow.steps_remaining == 0);
        assert(session.presentation.movement_tick == 150);
        assert(session.state.tool_placed_objects[slot].kind == kinds[i]);
        assert(session.state.tool_placed_objects[slot].node == 1);
        assert(session.state.players[0].attached_timed_bomb_slot == -1);
        assert(session.state.players[0].days_in_hospital == 0);
    }
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
        assert(s.presentation.auto_save_pending == 1);
        assert(s.pending_turn_events.count > 0);
        assert(s.state.players[0].god_info == 5);
        assert(!s.presentation.god_detachment.active);
        assert(!rich4_app_resume_turn_events(&s, NULL, 2000));
        assert(s.presentation.dynamic_objects[4].countdown == 1);
        s.presentation.periodic_finance.active = 0;
        s.presentation.bank_settlement.active = 0;
        assert(rich4_app_resume_turn_events(&s, NULL, 9000));
        assert(s.presentation.auto_save_pending == 1);
        assert(s.pending_turn_events.count == 0);
        assert(s.state.players[0].god_info == 0);
        assert(s.presentation.god_detachment.active);
        assert(!rich4_app_resume_turn_events(&s, NULL, 9100));
    }
}

static void test_holiday_animation_precedes_card_notice(void)
{
    GameSession s;
    Rich4AppTurnRotationResult result;
    const GameToolAnimationRequest *animation;
    const GameEvent *notice;

    setup_session(&s);
    s.state.calendar_start_year = 1998;
    s.state.calendar_start_month = 12;
    s.state.calendar_start_day = 24;
    s.state.stage = 0;
    s.state.map = 0;
    s.state.players[0].node = 1;
    s.state.tool_route_node_count = 2;
    s.state.tool_route_x[1] = 321;
    s.state.tool_route_y[1] = 654;
    s.state.card_amount[4] = 1;
    s.state.land_count = 1;
    s.state.land_owner[0] = 0;
    s.state.land_lease_date[0] = (1998u << 16) | (12u << 8) | 25u;
    assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, &result));
    assert(s.presentation.tool_animation_count == 1);
    animation = &s.presentation.tool_animation[
        s.presentation.tool_animation_head];
    assert(animation->resource == 554 && animation->effect == 114 &&
           animation->started == 0);
    assert(s.pending_turn_events.count == 4); /* gift, property day-end, status, dirty */
    assert(s.state.land_owner[0] == 0);
    assert(s.presentation.special_place_notice.queue.count == 0);

    /* The platform fills the pending FLC timing, then resumes the deferred
     * 1500 ms card presentation after the queue drains. */
    s.presentation.tool_animation_count = 0;
    assert(rich4_app_resume_turn_events(&s, NULL, 2000));
    assert(s.pending_turn_events.count == 3);
    assert(s.state.land_owner[0] == 0);
    assert(s.presentation.special_place_notice.queue.count == 1);
    assert(s.presentation.has_view_override &&
           s.presentation.view_world_x == 321 &&
           s.presentation.view_world_y == 654);
    notice = &s.presentation.special_place_notice.queue.events[
        s.presentation.special_place_notice.queue.head];
    assert(rich4_game_event_special_place_notice_requested_fields(notice).subject ==
           GAME_SPECIAL_NOTICE_HOLIDAY_GIFT);
    /* The gift window also returns before 0x41D0FF expires the lease. */
    rich4_game_event_queue_reset(&s.presentation.special_place_notice.queue);
    assert(rich4_app_resume_turn_events(&s, NULL, 4000));
    assert(s.pending_turn_events.count == 0 && s.state.land_owner[0] == -1);
}

static void test_research_completion_during_turn_rotation(void)
{
    const int play_states[] = {1, 2, 5};
    static GameSession session;
    Rich4AppTurnRotationResult result;
    int control, i;

    for (control = 0; control < 3; ++control) {
        setup_session(&session);
        session.state.price_index = 1;
        session.state.players[0].play_state = play_states[control];
        session.state.players[0].is_ai = control == 1;
        session.state.facility_count = 2;
        for (i = 0; i < 2; ++i) {
            session.state.facility_owner[i] = 0;
            session.state.facility_type[i] = 4;
            session.state.facility_level[i] = 5;
            session.state.facility_research_selection[i] = 1 + i * 4;
            session.state.facility_research_turns[i] = 1;
        }
        /* No inventory event is emitted for the second, already capped tool. */
        session.state.players[0].tool_inventory[12] = 9;
        assert(rich4_app_advance_turn_rotation(&session, NULL, 1000, &result));
        assert(session.presentation.special_place_notice.queue.count == 2);
        for (i = 0; i < 2; ++i) {
            const GameEvent *event =
                rich4_special_place_notice_current(&session.presentation);
            Rich4GameEventFields fields;
            assert(event != NULL && event->player == 0);
            fields = rich4_game_event_special_place_notice_requested_fields(event);
            assert(fields.subject == GAME_SPECIAL_NOTICE_RESEARCH_COMPLETE);
            assert(fields.target_ref.kind == GAME_DOMAIN_TOOL &&
                   fields.target_ref.id == 8 + i * 4);
            assert(session.presentation.special_place_notice.until ==
                   (uint32_t)(2500 + i * 1500));
            assert(rich4_special_place_notice_update(&session, 2499 + i * 1500));
            assert(session.presentation.special_place_notice.queue.count == 2 - i);
            if (i == 1)
                assert(rich4_special_place_notice_skip(&session, 3500));
            assert(rich4_special_place_notice_update(
                &session, i == 0 ? 2500 : 3500));
        }
        assert(session.presentation.special_place_notice.queue.count == 0);
        assert(!session.presentation.speech.active);
        assert(!session.presentation.route_interaction.active);
        assert(session.state.players[0].tool_inventory[12] == 9);
        assert(rich4_app_advance_turn_rotation(&session, NULL, 5000, &result));
        assert(session.presentation.special_place_notice.queue.count == 0);
    }
}

static void setup_loan_enforcement(GameSession *s, int count)
{
    int i;
    setup_session(s);
    s->state.player_count = count;
    s->state.human_player_count = count;
    s->state.turn_rotation_index = count - 1;
    s->state.current_player = count - 1;
    for (i = 0; i < count; i++) {
        s->state.players[i].play_state = 1;
        s->state.players[i].node = i + 1;
        s->state.players[i].linked_player = -1;
        s->state.players[i].attached_timed_bomb_slot = -1;
    }
    s->state.players[0].cash = 200;
    s->state.players[0].bank = 100;
    s->state.players[0].loan = 1000;
    s->state.players[0].loan_due_date = 1;
    s->state.land_count = 5;
    for (i = 0; i < 5; i++) {
        s->state.land_owner[i] = 0;
        s->state.land_price[i] = 100;
        s->state.land_value[i] = 100;
    }
}

static void test_loan_enforcement_waits_for_notice_auctions_and_speech(void)
{
    static GameSession s;
    GameEvent event;
    unsigned int seen = 0;
    int auctions = 0;
    setup_loan_enforcement(&s, 3);
    /* A hosted human remains a human for 0x40D020's auction gate. */
    s.state.human_player_count = 2;
    s.state.players[1].play_state = 5;
    s.state.players[1].is_ai = 1;
    s.state.players[2].play_state = 2;
    s.state.players[2].is_ai = 1;
    assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, NULL));
    assert(s.state.players[0].play_state == 0);
    assert(s.state.current_player == 0);
    assert(s.presentation.bank_loan_due.active && s.presentation.bank_loan_due.forced);
    assert(!s.presentation.bankruptcy.active && s.bankruptcy_auction_events.count == 0);
    assert(s.pending_turn_events.count > 0);
    assert(!rich4_app_resume_turn_events(&s, NULL, 2499));
    assert(rich4_bank_flow_update(&s, 2499, 0, NULL));
    assert(!rich4_bank_flow_update(&s, 2500, 0, NULL));
    assert(rich4_app_resume_turn_events(&s, NULL, 2500));
    assert(s.presentation.bankruptcy.active);
    assert(s.bankruptcy_auction_events.count == 3);
    assert(s.pending_turn_events.count > 0);
    assert(!rich4_app_advance_turn_rotation(&s, NULL, 2600, NULL));
    assert(rich4_bankruptcy_update(&s, 0, 100, 2500));
    assert(rich4_bankruptcy_update(&s, 0, 100, 4500));
    assert(s.presentation.bankruptcy.awaiting_auction);
    while (rich4_game_event_queue_pop(&s.deferred_events, &event)) {
        if (event.type == GAME_EVENT_AUCTION_REQUESTED) {
            GameDomainRef property = rich4_game_event_auction_property(&event);
            assert(property.kind == GAME_DOMAIN_LAND && property.id >= 0 && property.id < 5);
            assert((seen & (1u << property.id)) == 0);
            seen |= 1u << property.id;
            assert(rich4_game_event_auction_seller(&event) == -1);
            auctions++;
        }
    }
    assert(auctions == 3);
    assert(!rich4_app_resume_turn_events(&s, NULL, 4600));
    rich4_bankruptcy_finish_auctions(&s, 5000);
    assert(s.presentation.bankruptcy.active && s.presentation.speech.active);
    s.presentation.speech.active = 0;
    rich4_bankruptcy_finish_speech(&s);
    assert(rich4_app_resume_turn_events(&s, NULL, 7000));
    assert(s.state.current_player == 1 && s.state.elapsed_days == 1);
    assert(!s.presentation.bankruptcy.active && s.pending_turn_events.count == 0);
}

static void test_loan_enforcement_terminal_and_monthly_order(void)
{
    static GameSession s;
    setup_loan_enforcement(&s, 2);
    assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, NULL));
    assert(s.state.game_over && !s.presentation.ending.active);
    assert(!rich4_bank_flow_update(&s, 2500, 0, NULL));
    assert(rich4_app_resume_turn_events(&s, NULL, 2500));
    assert(s.presentation.bankruptcy.active && s.bankruptcy_terminal_events.count == 1);
    assert(!s.presentation.ending.active);
    assert(rich4_bankruptcy_update(&s, 0, 100, 2500));
    assert(rich4_bankruptcy_update(&s, 0, 100, 4500));
    assert(s.presentation.bankruptcy.active && s.presentation.speech.active);
    s.presentation.speech.active = 0;
    rich4_bankruptcy_finish_speech(&s);
    assert(s.lifecycle.code == RICH4_APP_LIFECYCLE_OUTER_MENU_AFTER_JUMP);

    setup_loan_enforcement(&s, 3);
    s.state.calendar_start_year = 2000;
    s.state.calendar_start_month = 1;
    s.state.calendar_start_day = 31;
    assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, NULL));
    assert(s.presentation.bank_settlement.active && !s.presentation.bank_loan_due.active);
    assert(!s.presentation.bankruptcy.active && s.pending_turn_events.count > 0);
    s.presentation.bank_settlement.active = 0;
    assert(rich4_app_resume_turn_events(&s, NULL, 9000));
    assert(s.presentation.bank_loan_due.active && s.presentation.bank_loan_due.until == 10500);
    assert(!s.presentation.bankruptcy.active);
    assert(!rich4_bank_flow_update(&s, 10500, 0, NULL));
    assert(rich4_app_resume_turn_events(&s, NULL, 10500));
    assert(s.presentation.bankruptcy.active && s.bankruptcy_auction_events.count == 3);
}

static void test_loan_reminder_precedes_player_status(void)
{
    static GameSession s;
    setup_loan_enforcement(&s, 3);
    s.state.players[0].cash = 1000;
    s.state.players[0].days_in_prison = 2;
    assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, NULL));
    assert(s.state.players[0].play_state == 1 && s.state.players[0].cash == 100);
    assert(s.state.players[0].days_in_prison == 2);
    assert(!rich4_bank_flow_update(&s, 2500, 0, NULL));
    assert(rich4_app_resume_turn_events(&s, NULL, 2500));
    assert(s.state.players[0].days_in_prison == 1);
    assert(s.state.current_player == 0 && s.pending_turn_events.count == 0);

    setup_loan_enforcement(&s, 3);
    s.state.players[0].loan_due_date = 4;
    s.state.players[0].days_sleeping = 2;
    assert(rich4_app_advance_turn_rotation(&s, NULL, 1000, NULL));
    assert(rich4_bank_flow_loan_reminder_active(&s));
    assert(s.state.players[0].days_sleeping == 2);
    assert(rich4_bank_flow_loan_reminder_click(&s, 1, 1010));
    assert(!rich4_bank_flow_update(&s, 1050, 0, NULL));
    assert(rich4_app_resume_turn_events(&s, NULL, 1050));
    assert(s.state.players[0].days_sleeping == 1 && s.state.current_player == 0);
}

int main(void)
{
    test_loan_enforcement_waits_for_notice_auctions_and_speech();
    test_loan_enforcement_terminal_and_monthly_order();
    test_loan_reminder_precedes_player_status();
    test_research_completion_during_turn_rotation();
    test_holiday_animation_precedes_card_notice();
    test_calendar_defers_turn_status();
    test_god_expiry();
    test_normal_status_consumption();
    test_disappearance_return_animation();
    test_special_action_is_returned_to_platform_seam();
    test_control_locked_turn_cleanup();
    test_confinement_exit_tick();
    test_confinement_exit_defers_arrival_until_next_pulse();
    test_monthly_bank_statement_timer();
    test_day_limit_finalizes_game();
    test_monthly_dynamic_objects_are_replaced();
    test_camera_rotation_rules();
    puts("app turn rotation flow: ok");
    return 0;
}
