#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "view/game_view_model.h"

int main(void)
{
    GameState state;
    GameViewModelInput flow;
    GameViewModel model;
    MapRouteNode routes[3];
    MapSpecialRecord specials[3];
    MapData map;
    int x, y, overlay;
    int traffic;
    int direction;
    int rotation;
    static const int expected_x_axis[8][2] = {
        { 34, -11 }, { 34, 9 }, { 13, 25 }, { -16, 25 },
        { -37, 9 }, { -36, -11 }, { -16, -26 }, { 14, -26 }
    };
    static const int expected_y_axis[8][2] = {
        { 14, 25 }, { -16, 25 }, { -37, 9 }, { -36, -11 },
        { -16, -26 }, { 14, -26 }, { 35, -11 }, { 35, 9 }
    };
    assert(rich4_game_actor_direction_from_delta(1, 0) == 2);
    assert(rich4_game_actor_direction_from_delta(0, -1) == 4);
    assert(rich4_game_actor_direction_from_delta(-1, 0) == 6);
    assert(rich4_game_actor_direction_from_delta(0, 1) == 0);
    assert(rich4_game_actor_direction_from_delta(1, -1) == 3);
    assert(rich4_game_actor_direction_from_delta(-1, -1) == 5);
    assert(rich4_game_actor_direction_from_delta(-1, 1) == 7);
    assert(rich4_game_actor_direction_from_delta(1, 1) == 1);
    /* The original ratio-table index 848 remains in the axial bucket. */
    assert(rich4_game_actor_direction_from_delta(106, -256) == 4);
    assert(rich4_game_actor_direction_from_delta(105, -256) == 4);
    assert(rich4_game_actor_direction_from_delta(1000, -2414) == 4);
    assert(((rich4_game_actor_direction_from_delta(-1, 0) -
             rich4_game_actor_direction_from_delta(1, 0)) & 7) == 4);
    assert(((rich4_game_actor_direction_from_delta(0, -1) -
             rich4_game_actor_direction_from_delta(0, 1)) & 7) == 4);
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    memset(routes, 0, sizeof(routes));
    memset(specials, 0, sizeof(specials));
    memset(&map, 0, sizeof(map));
    state.active = 1;
    state.player_count = 1;
    state.players[0].play_state = 1;
    state.players[0].attached_timed_bomb_slot = -1;
    state.players[0].character = 3;
    flow.actors[0].visual_x = 320;
    flow.actors[0].visual_y = 640;
    flow.actors[0].direction = 2;
    state.players[0].traffic_method = 3;
    state.players[0].ndices = 2;
    flow.actors[0].intro_visible = 1;
    flow.route_count = 77;
    state.players[0].node = 1;
    state.players[0].cash = 10;
    state.players[0].bank = 20;
    state.players[0].loan = 5;
    state.price_index = 4;
    state.land_count = 1;
    state.land_owner[0] = 0;
    state.land_type[0] = 0;
    state.land_level[0] = 2;
    state.land_value[0] = 100;
    state.facility_count = 1;
    state.facility_owner[0] = 0;
    state.facility_type[0] = 3;
    state.facility_level[0] = 1;
    state.facility_value[0] = 200;
    state.company_count = 1;
    state.company_stock_index[0] = 2;
    state.stock_company_owner[2] = 0;
    state.player_stock_amount[0][0] = 3;
    state.player_stock_average_cost[0][0] = 7.25f;
    state.stocks[0].f20 = 10.5f;
    state.players[0].points = 12;
    state.players[0].days_assurance = 9;
    flow.map_direction = 1;
    flow.active_info_tab = 2;
    flow.top_button_hover = 4;
    flow.dice_status_visible = 1;
    flow.dice_status_x = 12;
    flow.dice_status_y = 34;
    flow.dice_status_variant = 1;
    flow.dice_popup_active = 1;
    flow.dice_count = 2;
    flow.dice_faces[0] = 3;
    flow.dice_faces[1] = 5;
    flow.dice_anim_frame = 7;
    flow.dice_result_visible = 1;
    map.routes = routes;
    map.route_count = 3;
    map.specials = specials;
    map.special_count = 3;
    specials[1].x = 420;
    specials[1].y = 450;
    specials[2].x = 365;
    specials[2].y = 450;
    routes[1].x = 320;
    routes[1].y = 640;
    routes[1].flags = 0x80000000u;
    routes[2].x = 512;
    routes[2].y = 768;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.active == 1 && model.route_count == 77);
    assert(model.current_actor == 0);
    assert(model.viewport.center_world_x == 320 && model.viewport.center_world_y == 640);
    assert(model.actors[0].primary_part == 13 && model.actors[0].overlay_part == -1);
    assert(model.actors[0].has_dynamic_attachment == 0);
    assert(model.actors[0].visual_direction == 1);
    assert(model.special_actor_count == GAME_SPECIAL_ACTOR_MODEL_COUNT);
    assert(!model.special_actors[0].visible);
    for (rotation = 0; rotation < 8; rotation++) {
        flow.map_direction = rotation;
        for (direction = 0; direction < 8; direction++) {
            flow.actors[0].direction = direction;
            assert(rich4_game_view_model_build(&state, &flow, &map, &model));
            assert(model.actors[0].visual_direction ==
                   ((direction + 8 - rotation) & 7));
        }
    }
    flow.map_direction = 1;
    flow.actors[0].direction = 2;
    assert(model.hud.active_tab == 2 && model.hud.cash_and_bank == 30 &&
           model.hud.price_index == 4 && model.hud.top_button_hover == 4);
    /* 0x423A0A: total assets truncate 25 + 3*10.5 before adding property. */
    assert(model.hud.total_assets == 356 &&
           model.hud.owned_property_count == 2 &&
           model.hud.owned_chain_count == 0 &&
           model.hud.built_facility_count == 1 &&
           model.hud.stock_market_value == 32 &&
           model.hud.stock_cost == 22 &&
           model.hud.stock_company_control_count == 1 &&
           model.hud.points == 12 && model.hud.loan == 5 &&
           model.hud.insurance_days == 9);
    assert(model.dice.popup_visible && model.dice.count == 2 &&
           model.dice.faces[0] == 3 && model.dice.faces[1] == 5 &&
           model.dice.animation_frame == 7 && model.dice.result_visible &&
           model.dice.status_visible && model.dice.status_x == 12 &&
           model.dice.status_y == 34 && model.dice.status_variant == 1 &&
           model.dice.traffic_method == 3 && model.dice.available_dice == 2);
    flow.turn_flow_active = 1;
    flow.turn_state = GAME_TURN_STATE_PREROLL;
    /* Before the dice popup, the actor still renders the EA2=2 action lane;
     * once the popup is active the original has already switched to EA2=1. */
    flow.dice_popup_active = 0;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].action_part == 15);
    routes[1].flags = 0;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].action_part == 11);
    flow.dice_popup_active = 1;
    flow.turn_state_frame = 4;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].use_action_part == 1 &&
           model.actors[0].action_part == 11 &&
           model.actors[0].action_holds_last_frame == 1);
    flow.dice_popup_active = 0;
    flow.turn_flow_active = 0;
    /* sub_40D7C4 keeps the movement resource selected between two route
     * edges.  The SDL presentation has move_active=0 during that one timer
     * interval, but it must not render the idle lane and visibly stutter. */
    state.players[0].play_state = 1;
    state.players[0].traffic_method = 0;
    state.players[0].node = 1;
    state.players[0].next_node = 2;
    flow.actors[0].move_active = 0;
    flow.actors[0].walk_frame = 5;
    flow.turn_flow_active = 1;
    flow.turn_state = GAME_TURN_STATE_MOVE;
    flow.steps_remaining = 2;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].moving == 1 &&
           model.actors[0].primary_part == 1 &&
           model.actors[0].fallback_part == 1 &&
           model.actors[0].walk_frame == 5);
    flow.turn_flow_active = 0;
    flow.turn_state = GAME_TURN_STATE_IDLE;
    flow.steps_remaining = 0;
    assert(model.land_count == 1 && model.lands[0].owner == 0 &&
           model.lands[0].type == 0 && model.lands[0].level == 2);
    assert(model.facility_count == 1 && model.facilities[0].owner == 0 &&
           model.facilities[0].type == 3 && model.facilities[0].level == 1);
    assert(model.company_count == 1 && model.companies[0].owner == 0);
    routes[1].flags = 0;
    state.players[0].traffic_method = 3;
    assert(rich4_game_actor_part(&state.players[0], 0, 1, &overlay) == 10);
    assert(overlay == -1); /* part 12 is EA2=3, not an idle overlay. */
    state.players[0].days_sleep_walking = 1;
    assert(rich4_game_actor_part(&state.players[0], 0, 0, &overlay) == 16);
    state.players[0].days_sleep_walking = 0;
    state.players[0].play_state = GAME_PLAYER_PLAY_STATE_FROZEN | 1;
    assert(rich4_game_actor_part(&state.players[0], 0, 0, &overlay) == 18);
    state.players[0].days_sleep_walking = 1;
    assert(rich4_game_actor_part(&state.players[0], 1, 1, &overlay) == 18);
    state.players[0].play_state = 0;
    assert(rich4_game_actor_part(&state.players[0], 0, 0, &overlay) == 18);
    state.players[0].play_state = 1;
    assert(rich4_game_actor_part(&state.players[0], 1, 0, &overlay) == 16);
    assert(rich4_game_actor_part(&state.players[0], 1, 1, &overlay) == 17);
    state.players[0].days_sleep_walking = 0;
    assert(rich4_game_actor_part(&state.players[0], 1, 0, &overlay) == 13);
    assert(rich4_game_actor_part(&state.players[0], 1, 1, &overlay) == 14);
    assert(overlay == -1);
    /* 0x40B93B selects part 18 from the full +21 byte/bit 0x40, not from
     * delayed, disappearance or confinement counters that gate other paths. */
    state.players[0].delayed_status_block = 1;
    assert(rich4_game_actor_part(&state.players[0], 0, 0, &overlay) == 9);
    state.players[0].delayed_status_block = 0;
    state.players[0].days_disappearing = 1;
    assert(rich4_game_actor_part(&state.players[0], 0, 1, &overlay) == 10);
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(!model.actors[0].visible);
    assert(model.actors[0].first_turn_landing_pending ==
           flow.actors[0].first_turn_landing_pending);
    state.players[0].days_disappearing = 0;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].visible);
    state.players[0].days_in_prison = 1;
    state.players[0].confinement_actor_type = GAME_CONFINEMENT_PRISON;
    assert(rich4_game_actor_part(&state.players[0], 0, 0, &overlay) == 9);
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].world_x == 365 &&
           model.actors[0].world_y == 450 &&
           model.viewport.center_world_x == 365 &&
           model.viewport.center_world_y == 450);
    state.players[0].days_in_prison = 0;
    state.players[0].confinement_actor_type = GAME_CONFINEMENT_NONE;
    state.players[0].days_in_hospital = 1;
    state.players[0].confinement_actor_type = GAME_CONFINEMENT_HOSPITAL;
    assert(rich4_game_actor_part(&state.players[0], 0, 1, &overlay) == 10);
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].world_x == 420 &&
           model.actors[0].world_y == 450);
    state.players[0].days_in_hospital = 0;
    state.players[0].confinement_actor_type = GAME_CONFINEMENT_NONE;
    state.players[0].days_sleeping = 1;
    assert(rich4_game_actor_part(&state.players[0], 0, 0, &overlay) == 9);
    state.players[0].days_sleeping = 0;
    state.players[0].god_info = 1;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].has_dynamic_attachment == 1);
    state.players[0].god_info = 0;
    state.players[0].attached_timed_bomb_slot = 0;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].has_dynamic_attachment == 0);
    assert(model.actors[0].attachments_hidden == 0);
    state.players[0].days_in_hospital = 1;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].attachments_hidden == 1);
    state.players[0].days_in_hospital = 0;
    state.players[0].attached_timed_bomb_slot = -1;
    state.special_player_f10[0] = 0;
    state.special_player_node[0] = 2;
    state.special_player_previous_node[0] = 1;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.special_actors[0].visible &&
           model.special_actors[0].world_x == 512 &&
           model.special_actors[0].world_y == 768 &&
           model.special_actors[0].resource_part == 0);
    assert(model.special_actors[0].overlay_chunk == -1);
    assert(model.special_actors[0].visual_direction == 0);
    state.turn_rotation_index = GAME_MAX_PLAYERS;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.current_actor == GAME_MAX_PLAYERS);
    state.special_player_status[0][1] = 5;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.special_actors[0].resource_part == 0 &&
           model.special_actors[0].overlay_chunk == 0);
    flow.special_actor.active = 1;
    flow.special_actor.slot = 0;
    flow.special_actor.visual_x = 450;
    flow.special_actor.visual_y = 700;
    flow.special_actor.direction = 5;
    flow.special_actor.resource_part = 1;
    flow.special_actor.animation_frame = 7;
    flow.special_actor.steps_remaining = 4;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.special_actors[0].world_x == 450 &&
           model.special_actors[0].world_y == 700 &&
           model.special_actors[0].resource_part == 1 &&
           model.special_actors[0].overlay_chunk == 1 &&
           model.special_actors[0].animation_frame == 7 &&
           model.special_actors[0].visual_direction == 4 &&
           model.viewport.center_world_x == 450 &&
           model.viewport.center_world_y == 700 &&
           model.steps_remaining == 4);
    /* 0x415E70 honors an observation target even during a special actor's
     * action, for example when an ordinary recipient speaks. */
    flow.has_view_override = 1;
    flow.view_world_x = 900;
    flow.view_world_y = 1000;
    rich4_game_viewport_build(&state, &flow, &map, &model.viewport);
    assert(model.viewport.center_world_x == 900 && model.viewport.center_world_y == 1000);
    flow.has_view_override = 0;
    flow.special_actor.active = 0;
    state.special_player_status[0][1] = 0;
    state.turn_rotation_index = 0;
    routes[2].flags = 0x80000000u;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.special_actors[0].resource_part == 2);
    state.special_player_f10[0] = GAME_CONFINEMENT_PRISON;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(!model.special_actors[0].visible);
    routes[2].flags = 0;
    for (traffic = 0; traffic < 4; traffic++) {
        state.players[0].traffic_method = traffic;
        assert(rich4_game_actor_part(&state.players[0], 0, 0, &overlay) == traffic * 3);
        assert(overlay == -1);
        assert(rich4_game_actor_part(&state.players[0], 0, 1, &overlay) == traffic * 3 + 1);
        assert(overlay == -1);
    }
    flow.has_view_override = 1;
    flow.view_world_x = 352;
    flow.view_world_y = 640;
    rich4_game_viewport_build(&state, &flow, &map, &model.viewport);
    assert(model.viewport.center_world_x == 352 && model.viewport.has_override);
    rich4_game_viewport_project(&model.viewport, 352, 640, &x, &y);
    assert(x == GAME_MAIN_MAP_ORIGIN_X && y == GAME_MAIN_MAP_ORIGIN_Y);
    /* 0x408484/0x408494 read {Y,X} from 0x46CCF0: for direction zero,
     * one world tile on X is (+34,-11), while one on Y is (+14,+25).
     * Keep this explicit so ground rasterization cannot silently swap axes
     * while map objects still use rich4_game_viewport_project(). */
    for (rotation = 0; rotation < 8; rotation++) {
        flow.map_direction = rotation;
        rich4_game_viewport_build(&state, &flow, &map, &model.viewport);
        rich4_game_viewport_project(&model.viewport, 352, 640, &x, &y);
        assert(x == GAME_MAIN_MAP_ORIGIN_X && y == GAME_MAIN_MAP_ORIGIN_Y);
        rich4_game_viewport_project(&model.viewport, 384, 640, &x, &y);
        assert(x == GAME_MAIN_MAP_ORIGIN_X + expected_x_axis[rotation][0] &&
               y == GAME_MAIN_MAP_ORIGIN_Y + expected_x_axis[rotation][1]);
        rich4_game_viewport_project(&model.viewport, 352, 672, &x, &y);
        assert(x == GAME_MAIN_MAP_ORIGIN_X + expected_y_axis[rotation][0] &&
               y == GAME_MAIN_MAP_ORIGIN_Y + expected_y_axis[rotation][1]);
    }
    flow.has_view_override = 0;
    flow.actors[0].move_active = 0;
    flow.actors[0].visual_x = 999;
    flow.actors[0].visual_y = 999;
    routes[1].x = 416;
    routes[1].y = 704;
    rich4_game_viewport_build(&state, &flow, &map, &model.viewport);
    assert(model.viewport.center_world_x == 416 && model.viewport.center_world_y == 704);
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].world_x == 416 &&
           model.actors[0].world_y == 704);
    state.players[0].node = 2;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].world_x == 512 &&
           model.actors[0].world_y == 768);
    flow.actors[0].move_active = 1;
    rich4_game_viewport_build(&state, &flow, &map, &model.viewport);
    assert(model.viewport.center_world_x == 999 && model.viewport.center_world_y == 999);
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.actors[0].world_x == 999 &&
           model.actors[0].world_y == 999);
    /* 0x409971: shared display eligibility and actor-8 endpoint decrement. */
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    state.active = 1;
    state.player_count = 1;
    state.players[0].play_state = 1;
    flow.turn_flow_active = 1;
    flow.steps_remaining = 12;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.show_steps && model.steps_remaining == 12);
    state.players[0].play_state |= 0x10;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(!model.show_steps);
    state.players[0].play_state = 1;
    state.players[0].days_in_hospital = 1;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(!model.show_steps);
    flow.special_actor.active = 1;
    flow.special_actor.steps_remaining = 4;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.show_steps && model.steps_remaining == 4);
    state.tool_robot.active = 1;
    state.tool_robot.steps_remaining = 9;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(!model.show_steps); /* still speaking */
    flow.tool_robot_started = 1;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.show_steps && model.steps_remaining == 9 && model.current_actor == 8);
    flow.tool_robot_arrived = 1;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.show_steps && model.steps_remaining == 8);
    state.tool_robot.steps_remaining = 1;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(!model.show_steps && model.steps_remaining == 0);
    memset(&flow, 0, sizeof(flow));
    state.land_count = 3;
    state.facility_count = 1;
    state.player_count = 3;
    state.players[1].linked_player = 2;
    state.land_owner[0] = 1;
    state.land_owner[1] = 2;
    state.land_owner[2] = 0;
    state.land_group[0] = state.land_group[1] = state.land_group[2] = 7;
    state.land_price_status[0] = 0x50;
    state.land_price_status[1] = 0x51;
    state.facility_price_status[0] = 0x31;
    flow.property_presentation_land = 0;
    flow.property_flash_adjust = 8;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.lands[0].price_status == 0x50 && model.lands[1].price_status == 0x51);
    assert(model.facilities[0].price_status == 0x31);
    assert(model.lands[0].flash_adjust == 8 && model.lands[1].flash_adjust == 8 &&
           model.lands[2].flash_adjust == 0);
    state.land_price_status[0] = state.land_price_status[1] = 0;
    state.facility_price_status[0] = 0;
    flow.property_flash_adjust = 0;
    assert(rich4_game_view_model_build(&state, &flow, &map, &model));
    assert(model.lands[0].price_status == 0 && model.lands[1].price_status == 0 &&
           model.facilities[0].price_status == 0 && model.lands[1].flash_adjust == 0);
    puts("game view model: ok");
    return 0;
}
