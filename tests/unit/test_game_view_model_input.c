#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/game_constants.h"
#include "app/game_speech_flow.h"
#include "app/special_actor_flow.h"
#include "app/turn_rotation_flow.h"
#include "platform/sdl1/game_view_model_input.h"

static void test_special_actor_follows_after_property_speech(void)
{
    static GameSession session;
    MapData map = {0};
    MapRouteNode routes[4] = {0};
    GameViewModelInput input;
    GameViewportModel viewport;
    Rich4AppTurnRotationResult rotation;
    int i;

    /* A (player 1) pays B (player 0), the owner of special actor 4. */
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.current_player = session.state.turn_rotation_index = 1;
    session.state.tool_route_node_count = 4;
    session.state.players[0].node = 3;
    session.state.players[1].node = 2;
    session.state.players[1].character = 1;
    session.state.special_player_node[0] = 1;
    session.state.special_player_play_state[0] = 0;
    for (i = 0; i < 2; i++) {
        session.state.players[i].play_state = 1;
        session.state.players[i].linked_player = -1;
        session.state.players[i].attached_timed_bomb_slot = -1;
    }
    for (i = 1; i < 4; i++) {
        session.state.special_player_f10[i] = 1;
        routes[i].next[0] = i == 3 ? 1 : i + 1;
        routes[i].x = i == 1 ? 0 : 80;
        routes[i].y = i == 3 ? 80 : 0;
        session.state.tool_route_x[i] = routes[i].x;
        session.state.tool_route_y[i] = routes[i].y;
    }
    map.routes = routes;
    map.route_count = 4;
    assert(game_start_property_speech(&session, 1,
        RICH4_PROPERTY_SPEECH_PAYER_HIGH_1, 3, 0, 100));
    assert(!rich4_speech_finish_update(&session, 1100, 0));
    assert(game_start_property_speech(&session, 0,
        RICH4_PROPERTY_SPEECH_RECEIVER_HIGH_1, 3, 0, 1200));
    assert(!rich4_speech_finish_update(&session, 2200, 0));
    assert(session.presentation.has_view_override);
    assert(session.presentation.view_world_x == 80 &&
           session.presentation.view_world_y == 80);

    assert(rich4_app_advance_turn_rotation(&session, &map, 2300, &rotation));
    assert(rotation.special_action_pending);
    assert(!session.presentation.has_view_override);
    rich4_special_actor_begin(&session, &map, &rotation.special_action);
    assert(!session.presentation.has_view_override);
    rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
    rich4_game_viewport_build(&session.state, &input, &map, &viewport);
    assert(viewport.center_world_x == 0 && viewport.center_world_y == 0);
    assert(rich4_special_actor_update(&session, &map, 40, 2300) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    assert(rich4_special_actor_update(&session, &map, 40, 2340) ==
           RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE);
    rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
    rich4_game_viewport_build(&session.state, &input, &map, &viewport);
    assert(viewport.center_world_x == 8 && viewport.center_world_y == 0);

    /* A new observation target during this turn still takes precedence. */
    assert(game_start_property_speech(&session, 1,
        RICH4_PROPERTY_SPEECH_PAYER_LOW, 3, 0, 2400));
    rich4_sdl1_game_view_model_input_build(&session.flow, &session.presentation, &input);
    rich4_game_viewport_build(&session.state, &input, &map, &viewport);
    assert(viewport.center_world_x == 80 && viewport.center_world_y == 0);
}

int main(void)
{
    GameFlowState flow;
    GamePresentationState presentation;
    GameViewModelInput input;

    test_special_actor_follows_after_property_speech();

    memset(&flow, 0, sizeof(flow));
    memset(&presentation, 0, sizeof(presentation));
    flow.route_count = 77;
    presentation.map_direction = 3;
    presentation.has_view_override = 1;
    presentation.view_world_x = 352;
    presentation.view_world_y = 640;
    presentation.actors[0].visual_x = 123;
    presentation.actors[0].move_active = 1;
    presentation.dice.faces[0] = 4;
    presentation.special_actor.active = 1;
    presentation.special_actor.slot = 2;
    presentation.special_actor.visual_x = 456;
    presentation.special_actor.visual_y = 789;
    presentation.special_actor.direction = 5;
    presentation.special_actor.animation_frame = 11;
    presentation.special_actor.resource_part = 3;
    presentation.special_actor.steps_remaining = 5;
    presentation.property.land = 12;
    presentation.property.phase = GAME_PROPERTY_PRESENT_FLASH;
    presentation.property.flash_adjust = -8;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.route_count == 77 && input.map_direction == 3);
    assert(input.has_view_override && input.view_world_x == 352);
    assert(input.actors[0].visual_x == 123 && input.actors[0].move_active);
    assert(input.dice_faces[0] == 4);
    assert(input.property_presentation_land == 12 && input.property_flash_adjust == -8 &&
           input.property_flash_active);
    assert(input.special_actor.active && input.special_actor.slot == 2 &&
           input.special_actor.visual_x == 456 &&
           input.special_actor.visual_y == 789 &&
           input.special_actor.direction == 5 &&
           input.special_actor.animation_frame == 11 &&
           input.special_actor.resource_part == 3 &&
           input.special_actor.steps_remaining == 5);

    presentation.special_event.active = 1;
    presentation.special_event.phase = RICH4_SPECIAL_PRESENT_PROPERTY_FLASH;
    presentation.special_event.resolved_id = 18;
    presentation.special_event.pre_state.facility_count = 1;
    presentation.special_event.target_mask = 0x80000001u;
    presentation.special_event.flash_adjust = 12;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.property_flash_active && input.property_flash_adjust == 12 &&
           input.property_flash_same_name &&
           input.property_flash_target.kind == GAME_DOMAIN_FACILITY &&
           input.property_flash_target.id == 0);
    presentation.special_event.resolved_id = 19;
    presentation.special_event.pre_state.land_count = 1;
    presentation.special_event.target_mask = 1u;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(!input.property_flash_same_name &&
           input.property_flash_target.kind == GAME_DOMAIN_LAND);
    presentation.special_event.active = 0;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.property_flash_adjust == -8 &&
           input.property_flash_target.kind == GAME_DOMAIN_NONE);

    presentation.tool_animation_count = 1;
    presentation.tool_animation[0].active = 1;
    presentation.tool_animation[0].after_sync_player = 0;
    presentation.tool_animation[0].resource =
        GAME_DISAPPEARING_ALIEN_RESOURCE;
    presentation.tool_animation[0].frame =
        GAME_DISAPPEARING_ALIEN_REFRESH_FRAME - 2;
    presentation.tool_animation[0].origin_play_state =
        GAME_STATUS_PENDING_BIT | GAME_DISAPPEARING_ALIEN_BIT;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.tool_effect_actor == 1);
    assert(input.tool_effect_resource == GAME_DISAPPEARING_ALIEN_RESOURCE);
    assert(input.tool_effect_frame ==
           GAME_DISAPPEARING_ALIEN_REFRESH_FRAME - 2);
    assert(input.tool_effect_origin_play_state ==
           (GAME_STATUS_PENDING_BIT | GAME_DISAPPEARING_ALIEN_BIT));

    presentation.view_world_x = 999;
    presentation.actors[0].visual_x = 999;
    assert(input.view_world_x == 352 && input.actors[0].visual_x == 123);
    assert(input.presentation_player == 0);
    presentation.magic_house.active = 1;
    presentation.magic_house.phase = RICH4_MAGIC_HOUSE_TARGET;
    presentation.magic_house.target_player = 2;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.presentation_player == 3);
    presentation.magic_house.phase = RICH4_MAGIC_HOUSE_EFFECTS;
    presentation.tool_animation[0].after_sync_player = -3; /* synchronized player 1 */
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.presentation_player == 2);
    presentation.insurance.count = 1;
    presentation.insurance.queue[0].player = 0;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.presentation_player == 1);
    presentation.magic_house.active = 0;
    rich4_sdl1_game_view_model_input_build(&flow, &presentation, &input);
    assert(input.presentation_player == 0);
    puts("SDL1 view-model input adapter: ok");
    return 0;
}
