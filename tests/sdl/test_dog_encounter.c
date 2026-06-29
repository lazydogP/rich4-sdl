#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL/SDL.h>
#undef main
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/dynamic_object_flow.h"
#include "app/game_speech_flow.h"
#include "app/game_save.h"
#include "app/insurance_flow.h"
#include "app/tool_animation_flow.h"
#include "data/confinement_presentation.h"
#include "app/special_actor_flow.h"
#include "app/turn_movement_flow.h"
#include "core/game_event_payload.h"
#include "core/insurance.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_overlay_view.h"
#include "platform/sdl1/render/game_overlay_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "resource/mkf.h"

static GameSession session;
static GameSession restored;
static GamePreviewAssets assets;
static GameOverlayRenderView view;
static NewGameSettings settings;
static MapRouteNode routes[8];
static MapSpecialRecord specials[3];

static void setup(int traffic)
{
    int i;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.hospital_node = 6;
    session.state.prison_node = 5;
    session.state.price_index = 2;
    session.state.players[0].play_state = 1;
    session.state.players[0].node = 3;
    session.state.players[0].traffic_method = traffic;
    session.state.players[0].cash = 1000;
    session.state.players[0].days_assurance = 10;
    session.state.company_count = 1;
    session.state.company_type[0] = 4;
    session.state.company_stock_index[0] = 0;
    session.state.tool_route_node_count = 8;
    for (i = 1; i < 8; i++) {
        session.state.tool_route_x[i] = routes[i].x;
        session.state.tool_route_y[i] = routes[i].y;
    }
    session.presentation.actors[0].intro_visible = 1;
    session.presentation.actors[0].visual_x = routes[3].x;
    session.presentation.actors[0].visual_y = routes[3].y;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    session.presentation.dynamic_objects[10].node = 3;
}

static void start(int actor, Uint32 now)
{
    GameEventQueue events;
    GameEvent event;
    rich4_game_event_queue_reset(&events);
    assert(rich4_app_resolve_dynamic_dog(&session.presentation,
        &session.state, &assets.map.maps[0], actor, &events) == 11);
    assert(events.count == 1);
    assert(rich4_game_event_queue_pop(&events, &event));
    game_tool_animation_enqueue_from_platform(&session, &assets, &event, now);
}

static void update(Uint32 now, int animation)
{
    (void)game_update_tool_animation_queue(
        &session, &assets, &settings, NULL, now, animation);
    assert(build_game_overlay_render_view(&assets, &settings, &session, &view));
}

static void roundtrip(void)
{
    /* Make creates build/ on both Windows and POSIX hosts. */
    const char *path = "build/rich4-dog-encounter-test.R4S";
    assert(rich4_game_save_write(path, 1, &session));
    assert(rich4_game_save_read(path, 1, &restored));
    session = restored;
    assert(rich4_presentation_rebind_speech(&session.presentation));
    assert(remove(path) == 0);
}

static void check_camera(int hospital)
{
    assert(build_game_overlay_render_view(&assets, &settings, &session, &view));
    assert(view.scene.game.viewport.center_world_x ==
        (hospital ? specials[1].x : routes[3].x));
    assert(view.scene.game.viewport.center_world_y ==
        (hospital ? specials[1].y : routes[3].y));
}

static void explosion(int bomb, int traffic, int insured, int animation)
{
    Uint32 end, now;
    int reaction;
    int frames = game_graph_frame_count_data(assets.tool.tool_effect_data[3],
        assets.tool.tool_effect_size[3]);
    int delay = game_graph_frame_delay_ms_data(assets.tool.tool_effect_data[3],
        assets.tool.tool_effect_size[3]);
    setup(traffic);
    session.state.players[0].days_assurance = insured ? 10 : 0;
    session.state.players[0].attached_timed_bomb_slot = bomb ? 20 : -1;
    session.state.tool_placed_objects[bomb ? 20 : 10].kind = bomb ? 18 : 17;
    session.state.tool_placed_objects[bomb ? 20 : 10].node = bomb ? 0 : 3;
    session.state.tool_placed_objects[20].countdown = bomb ? 1 : 0;
    session.state.tool_placed_objects[20].attached_player = bomb ? 1 : 0;
    session.presentation.dynamic_objects[10].node = 0;
    session.presentation.actors[0].move_to_node = 3;
    session.flow.turn_flow_active = 1;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    session.flow.turn_wait = RICH4_MOVE_ARRIVAL;
    session.flow.turn_state_tick = 1;
    session.flow.steps_remaining = bomb ? 2 : 0;
    game_update_turn_movement(&session, &assets, &settings, NULL, NULL, 1000);
    assert(session.state.players[0].days_in_hospital == 0);
    assert(session.state.players[0].node == 3);
    assert(session.state.players[0].traffic_method == 0);
    assert(session.state.players[0].cash == 1000);
    assert(session.presentation.tool_animation_count == 1);
    assert(session.presentation.tool_animation[0].resource == 525);
    update(1000 + delay, animation);
    check_camera(0);
    assert(view.scene.game.actors[0].visible);
    assert(view.scene.game.actors[0].primary_part == traffic * 3);
    update(1000 + 2 * delay, animation);
    assert(view.scene.game.actors[0].visible);
    assert(view.scene.game.actors[0].primary_part == 18);
    assert(view.scene.game.actors[0].world_x == routes[3].x);
    assert(view.scene.game.actors[0].world_y == routes[3].y);
    end = 1000 + frames * delay;
    update(end, animation);
    assert(session.presentation.speech.active);
    check_camera(0);
    /* Mine: explosion reaction then admission reaction; bomb: admission
     * reaction only. Both must retain the injured sprite at the blast site. */
    now = end;
    for (reaction = 0; reaction < (bomb ? 1 : 2); reaction++) {
        assert(session.presentation.speech.active);
        assert(view.scene.game.actors[0].visible);
        assert(view.scene.game.actors[0].primary_part == 18);
        assert(session.state.players[0].days_in_hospital == 0);
        assert(session.state.players[0].cash == 1000);
        roundtrip();
        now += 10000;
        rich4_speech_request_skip(&session);
        assert(!rich4_speech_finish_update(&session, now, 0));
        update(now, animation);
    }
    assert(session.presentation.tool_animation_count == 1);
    assert(session.presentation.tool_animation[1].resource == 524);
    assert(session.state.players[0].days_in_hospital == (bomb ? 5 : 3));
    assert(session.state.players[0].node == 6);
    assert(session.state.players[0].total_confinement_days == 0);
    assert(session.presentation.insurance.count == 0);
    if (animation) {
        update(now, animation); /* Start ambulance on the retained explosion camera. */
        update(now + 2800, animation);
        assert(view.scene.game.actors[0].visible);
        assert(view.scene.game.actors[0].primary_part == 18);
        assert(view.scene.game.actors[0].world_x == routes[3].x);
        assert(view.scene.game.actors[0].world_y == routes[3].y);
        check_camera(0);
        update(now + 2900, animation); /* Decoded frame 30 refreshes the hospital state. */
        assert(!view.scene.game.actors[0].visible);
        assert(view.scene.game.actors[0].minimap_world_x == routes[3].x);
        assert(view.scene.game.actors[0].minimap_world_y == routes[3].y);
        check_camera(0);
        roundtrip();
        update(now + 6199, animation);
        assert(view.scene.game.actors[0].minimap_world_x == routes[3].x);
        now += 6200;
    } else now++;
    update(now, animation);
    check_camera(1);
    assert(view.scene.game.actors[0].minimap_world_x == specials[1].x);
    assert(!view.scene.game.viewport.has_override);
    update(++now, animation);
    assert(session.presentation.speech.active);
    assert(rich4_speech_finish_update(&session, now + 999, 0));
    assert(session.presentation.speech.current->big5[0] == '#');
    assert(rich4_speech_finish_update(&session, now + 1000, 1));
    assert(!rich4_speech_finish_update(&session, now + 1000, 0));
    now += 1000;
    update(now, animation);
    assert(session.presentation.tool_animation_count == 0);
    if (insured) {
        int amount = 2000 * session.state.price_index * (bomb ? 5 : 3);
        assert(session.presentation.insurance.count == 1);
        assert(session.presentation.insurance.queue[0].amount == amount);
        assert(session.state.players[0].cash == 1000);
        roundtrip();
        assert(rich4_insurance_flow_update(&session, now));
        assert(rich4_insurance_flow_update(&session, now + 1999));
        assert(!rich4_insurance_flow_update(&session, now + 2000));
        assert(session.state.players[0].cash == 1000 + amount);
    }
    assert(session.state.players[0].total_confinement_days == (bomb ? 5 : 3));
}

static void confinement(int kind, int animation, int repeat, int other_actor)
{
    const Rich4ConfinementPresentation *spec = rich4_confinement_presentation_for_kind(kind);
    int index = kind == GAME_CONFINEMENT_PRISON ? 4 : 2;
    int facility = kind == GAME_CONFINEMENT_PRISON ? 2 : 1;
    int frames = game_graph_frame_count_data(assets.tool.tool_effect_data[index], assets.tool.tool_effect_size[index]);
    int delay = game_graph_frame_delay_ms_data(assets.tool.tool_effect_data[index], assets.tool.tool_effect_size[index]);
    int node = kind == GAME_CONFINEMENT_PRISON ? 5 : 6;
    Uint32 now = 1000;
    GameToolAnimationRequest *request;
    setup(1);
    if (other_actor) {
        session.state.player_count = 2;
        session.state.current_player = session.state.turn_rotation_index = 1;
        session.state.players[1].play_state = 1;
        session.state.players[1].node = 2;
    }
    if (repeat) {
        session.state.players[0].node = node;
        session.presentation.actors[0].visual_x = specials[facility].x;
        session.presentation.actors[0].visual_y = specials[facility].y;
        session.state.players[0].confinement_actor_type = kind;
        if (kind == GAME_CONFINEMENT_PRISON) session.state.players[0].days_in_prison = 2;
        else session.state.players[0].days_in_hospital = 2;
    } else {
        session.state.players[0].delayed_status_block = 2;
        session.state.players[0].days_disappearing = 1;
    }
    assert(rich4_confinement_presentation_enqueue(&session, 0, kind, 5, now));
    roundtrip(); /* pending entry is serialized before any domain mutation */
    update(now, animation);
    request = &session.presentation.tool_animation[session.presentation.tool_animation_head];
    assert(request->resource == spec->resource);
    if (!repeat) {
        assert(session.presentation.speech.active);
        assert(session.state.players[0].node == 3);
        assert(session.state.players[0].days_in_prison == 0);
        assert(session.state.players[0].days_in_hospital == 0);
        assert(session.state.players[0].delayed_status_block == 0);
        assert(session.state.players[0].days_disappearing == 0);
        assert(!view.tool_animation[view.tool_animation_head].active);
        roundtrip(); /* pre-entry voice and its continuation survive together */
        assert(rich4_speech_finish_update(&session, now + 999, 0));
        assert(rich4_speech_finish_update(&session, now + 1000, 1));
        now += 1000;
        assert(!rich4_speech_finish_update(&session, now, 0));
        update(now, animation);
    } else assert(!session.presentation.speech.active);
    assert(session.state.players[0].node == node);
    assert((kind == GAME_CONFINEMENT_PRISON ? session.state.players[0].days_in_prison :
        session.state.players[0].days_in_hospital) == 5 + (repeat ? 2 : 0));
    assert(session.state.players[0].cash == 1000);
    assert(session.state.players[0].total_confinement_days == 0);
    if (animation && !repeat) {
        update(now + (spec->refresh_frame - 1) * delay - 1, animation);
        assert(view.scene.game.actors[0].visible);
        assert(view.scene.game.actors[0].world_x == routes[3].x);
        assert(view.scene.game.actors[0].primary_part == 3); /* preserve the old vehicle */
        update(now + (spec->refresh_frame - 1) * delay, animation);
        assert(!view.scene.game.actors[0].visible);
        assert(view.scene.game.actors[0].minimap_world_x == routes[3].x);
        assert(view.scene.game.viewport.center_world_x == routes[3].x);
        roundtrip();
        update(now + frames * delay - 1, animation);
        assert(view.scene.game.actors[0].minimap_world_x == routes[3].x);
        now += frames * delay;
    } else now++;
    update(now, animation);
    assert(view.scene.game.viewport.center_world_x == specials[facility].x);
    assert(view.scene.game.actors[0].minimap_world_x == specials[facility].x);
    assert(view.scene.game.viewport.has_override == other_actor);
    roundtrip();
    update(++now, animation);
    assert(session.presentation.speech.active);
    roundtrip();
    assert(rich4_speech_finish_update(&session, now + 999, 0));
    now += 1000;
    assert(!rich4_speech_finish_update(&session, now, 0));
    update(now, animation);
    assert(session.presentation.tool_animation_count == 0);
    assert(session.presentation.insurance.count == 1);
    assert(session.presentation.insurance.queue[0].cause ==
        (kind == GAME_CONFINEMENT_PRISON ? GAME_INSURANCE_CLAIM_PRISON : GAME_INSURANCE_CLAIM_HOSPITAL));
    assert(session.state.players[0].cash == 1000);
    assert(session.state.players[0].total_confinement_days == 0);
    roundtrip();
    assert(rich4_insurance_flow_update(&session, now));
    assert(rich4_insurance_flow_update(&session, now + 1999));
    assert(!rich4_insurance_flow_update(&session, now + 2000));
    assert(session.state.players[0].cash == 21000);
    assert(session.state.players[0].total_confinement_days == 5);
}

static void confinement_pair(void)
{
    Uint32 now = 1000;
    int player;
    setup(0);
    session.state.player_count = 2;
    session.state.players[1].play_state = 1;
    session.state.players[1].node = 2;
    session.state.players[1].character = 1;
    session.state.players[1].cash = 1000;
    session.state.players[1].days_assurance = 10;
    assert(rich4_confinement_presentation_enqueue(&session, 0, GAME_CONFINEMENT_PRISON, 5, now));
    assert(rich4_confinement_presentation_enqueue(&session, 1, GAME_CONFINEMENT_HOSPITAL, 3, now));
    roundtrip();
    for (player = 0; player < 2; player++) {
        int days = player == 0 ? 5 : 3;
        update(now, 0);
        assert(session.presentation.speech.active);
        assert(session.presentation.speech.player == player);
        if (player == 0) assert(session.state.players[1].node == 2);
        now += 1000;
        assert(!rich4_speech_finish_update(&session, now, 0));
        update(now, 0);
        update(++now, 0); /* synchronize this actor */
        update(++now, 0); /* final speech is bound to the head, not the tail */
        assert(session.presentation.speech.active);
        assert(session.presentation.speech.player == player);
        now += 1000;
        assert(!rich4_speech_finish_update(&session, now, 0));
        update(now, 0);
        assert(session.presentation.insurance.count == 1);
        assert(session.state.players[player].cash == 1000);
        {
            Rich4PresentationActivity activity = rich4_presentation_activity_from_state(
                &session.presentation, &session.state);
            assert(rich4_presentation_mode_resolve(&activity) == RICH4_PRESENTATION_INSURANCE);
        }
        assert(rich4_presentation_update_step(&session.presentation, &session.state) ==
            RICH4_PRESENTATION_UPDATE_FEATURES);
        if (player == 0) {
            assert(session.state.players[1].node == 2);
            assert(session.presentation.tool_animation_count == 1);
            assert(session.presentation.tool_animation[session.presentation.tool_animation_head].admission_stage == 1);
        }
        roundtrip();
        assert(rich4_insurance_flow_update(&session, now));
        now += 2000;
        assert(!rich4_insurance_flow_update(&session, now));
        assert(session.state.players[player].cash == 1000 + days * 4000);
        assert(session.state.players[player].total_confinement_days == days);
    }
    assert(session.presentation.tool_animation_count == 0);
}

static void walking(int animation, int insured)
{
    Uint32 hospital_start;
    setup(0);
    session.state.players[0].days_assurance = insured ? 10 : 0;
    start(0, 1000);
    assert(session.state.players[0].days_in_hospital == 0);
    update(1227, animation);
    assert(view.scene.game.actors[0].primary_part == 0);
    assert(view.scene.dynamic_objects[10].node == 3);
    update(1228, animation); /* decoded frame 3 */
    assert(view.scene.game.actors[0].primary_part == 18);
    assert(view.scene.dynamic_objects[10].node == 0);
    rich4_speech_request_skip(&session); /* FLC cannot be skipped */
    update(5331, animation);
    assert(session.state.players[0].days_in_hospital == 0);
    assert(session.presentation.tool_animation[0].frame == 37);
    update(5332, animation);
    assert(session.presentation.speech.active);
    assert(session.presentation.speech.type == 2);
    assert(session.presentation.speech.current->speaking_resource == 1055);
    assert(!view.tool_animation[0].active); /* no final FLC over portrait */
    assert(rich4_speech_finish_update(&session, 6331, 0));
    assert(rich4_speech_finish_update(&session, 6400, 1));
    assert(!rich4_speech_finish_update(&session, 6400, 0));
    update(6400, animation);
    assert(session.state.players[0].days_in_hospital == 3);
    assert(session.state.players[0].node == 6);
    assert(session.state.players[0].cash == 1000);
    assert(session.state.players[0].total_confinement_days == 0);
    assert(session.presentation.insurance.count == 0);
    check_camera(0);
    if (animation) {
        update(9299, animation);
        assert(view.scene.game.actors[0].visible);
        assert(view.scene.game.actors[0].primary_part == 18);
        assert(view.scene.game.actors[0].world_x == routes[3].x);
        update(9300, animation); /* decoded ambulance frame 30 */
        assert(!view.scene.game.actors[0].visible);
        check_camera(0);
        update(12599, animation);
        assert(!session.presentation.speech.active);
        check_camera(0);
        hospital_start = 12600;
    } else {
        assert(!view.tool_animation[view.tool_animation_head].active);
        hospital_start = 6401;
    }
    update(hospital_start, animation);
    check_camera(1);
    assert(!session.presentation.speech.active);
    roundtrip(); /* ambulance sync marker survives before the hospital voice */
    update(hospital_start + 1, animation);
    assert(session.presentation.speech.active);
    assert(session.presentation.speech.current->speaking_resource == 1070);
    assert(session.presentation.speech.type == 2);
    rich4_speech_request_skip(&session);
    assert(!rich4_speech_finish_update(&session, hospital_start + 2, 1));
    update(hospital_start + 2, animation);
    assert(session.presentation.tool_animation_count == 0);
    if (!insured) {
        assert(session.presentation.insurance.count == 0);
        assert(session.state.players[0].cash == 1000);
        assert(session.state.players[0].total_confinement_days == 3);
        return;
    }
    assert(session.presentation.insurance.count == 1);
    assert(session.presentation.insurance.queue[0].amount == 12000);
    assert(session.state.players[0].cash == 1000);
    assert(session.state.players[0].total_confinement_days == 0);
    roundtrip(); /* pending payout remains pending across a save */
    assert(rich4_insurance_flow_update(&session, hospital_start + 3));
    assert(rich4_insurance_flow_update(&session, hospital_start + 2002));
    assert(!rich4_insurance_flow_update(&session, hospital_start + 2003));
    assert(session.state.players[0].cash == 13000);
    assert(session.state.players[0].total_confinement_days == 3);
}

static void vehicle(void)
{
    setup(1);
    start(0, 1000);
    update(2277, 0);
    assert(session.presentation.tool_animation_count == 1);
    assert(session.presentation.tool_animation[0].frame == 17);
    update(2278, 0);
    assert(session.presentation.tool_animation_count == 0);
    assert(!session.presentation.speech.active);
    assert(session.state.players[0].node == 3);
    assert(session.state.players[0].days_in_hospital == 0);
    assert(session.presentation.insurance.count == 0);
    check_camera(0);
}

static void special_actor(void)
{
    setup(0);
    session.state.special_player_node[0] = 3;
    {
        TurnSpecialAutoActionResult action;
        GameEvent event;
        memset(&action, 0, sizeof(action));
        action.kind = TURN_SPECIAL_AUTO_MOVED;
        action.special_slot = 0;
        action.path_count = 2;
        action.path_nodes[0] = 2;
        action.path_nodes[1] = 3;
        /* The turn core already commits the endpoint before presentation. */
        session.state.special_player_node[0] = 3;
        rich4_special_actor_begin(&session, &assets.map.maps[0], &action);
        session.presentation.special_actor.move_progress =
            session.presentation.special_actor.move_distance - 1;
        session.presentation.special_actor.last_step_tick = 900;
        assert(rich4_special_actor_update(&session, &assets.map.maps[0],
            10, 1000) == RICH4_SPECIAL_ACTOR_UPDATE_DOG);
        assert(session.presentation.special_actor.active);
        assert(rich4_game_event_queue_pop(&session.deferred_events, &event));
        game_tool_animation_enqueue_from_platform(&session, &assets, &event, 1000);
    }
    update(5331, 1);
    assert(session.state.special_player_f10[0] == 0);
    update(5332, 1);
    assert(session.state.special_player_f10[0] == 2);
    assert(rich4_special_actor_update(&session, &assets.map.maps[0],
        10, 5333) == RICH4_SPECIAL_ACTOR_UPDATE_FINISHED);
    assert(session.presentation.tool_animation_count == 0);
    assert(!session.presentation.speech.active);
    assert(session.presentation.insurance.count == 0);
    check_camera(0);
}

static void placement(int index, int y)
{
    SDL_Surface *actual = SDL_CreateRGBSurface(SDL_SWSURFACE,
        640, 480, 16, 0x7c00, 0x3e0, 0x1f, 0);
    SDL_Surface *expected = SDL_CreateRGBSurface(SDL_SWSURFACE,
        640, 480, 16, 0x7c00, 0x3e0, 0x1f, 0);
    assert(actual && expected);
    SDL_FillRect(actual, NULL, 0x1234);
    SDL_FillRect(expected, NULL, 0x1234);
    memset(&view, 0, sizeof(view));
    view.tool_animation_count = 1;
    view.tool_animation[0].active = 1;
    view.tool_animation[0].has_world_position = 1;
    view.tool_animation[0].resource = assets.tool.tool_effect_resource[index];
    view.tool_animation[0].frame = 5;
    draw_game_tool_animation(actual, &assets, &view);
    assert(draw_game_graph_frame_data(expected, assets.tool.tool_effect_data[index],
        assets.tool.tool_effect_size[index], 5, 0, y));
    assert(memcmp(actual->pixels, expected->pixels, actual->pitch * 480) == 0);
    SDL_FreeSurface(actual);
    SDL_FreeSurface(expected);
}

int main(void)
{
    Rich4Mkf *mkf = NULL;
    int i;
    const int resources[] = {532, 552, 524, 525, 538};
    const int frames[] = {38, 18, 62};
    const int delays[] = {114, 71, 100};
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    for (i = 0; i < 5; i++) {
        assert(rich4_mkf_read_resource(mkf, resources[i],
            &assets.tool.tool_effect_data[i], &assets.tool.tool_effect_size[i],
            NULL) == RICH4_MKF_OK);
        assets.tool.tool_effect_resource[i] = resources[i];
        assets.tool.has_tool_effect[i] = 1;
        if (i < 3) {
            assert(game_graph_frame_count_data(assets.tool.tool_effect_data[i],
                assets.tool.tool_effect_size[i]) == frames[i]);
            assert(game_graph_frame_delay_ms_data(assets.tool.tool_effect_data[i],
                assets.tool.tool_effect_size[i]) == delays[i]);
        }
    }
    for (i = 1; i < 8; i++) {
        routes[i].x = (int16_t)(i * 320);
        routes[i].y = 320;
        routes[i].next[0] = (uint16_t)(i == 7 ? 1 : i + 1);
    }
    specials[1].x = 160;
    specials[1].y = 240;
    assets.map.maps[0].routes = routes;
    assets.map.maps[0].route_count = 8;
    assets.map.maps[0].specials = specials;
    assets.map.maps[0].special_count = 3;
    specials[2].x = 480;
    specials[2].y = 240;
    for (i = GAME_CONFINEMENT_PRISON; i <= GAME_CONFINEMENT_HOSPITAL; i++) {
        confinement(i, 1, 0, 0);
        confinement(i, 0, 0, 0);
        confinement(i, 1, 1, 0);
        confinement(i, 1, 0, 1);
    }
    confinement_pair();
    placement(4, 40);
    free(assets.tool.tool_effect_data[4]);
    walking(1, 1);
    walking(0, 1);
    walking(1, 0);
    vehicle();
    special_actor();
    for (i = 0; i < 3; i++) {
        explosion(0, i, 0, 1);
        explosion(1, i, 0, 1);
    }
    explosion(0, 1, 1, 1);
    explosion(1, 2, 1, 1);
    explosion(0, 1, 1, 0);
    explosion(1, 2, 1, 0);
    free(assets.tool.tool_effect_data[3]);
    for (i = 0; i < 3; i++) {
        placement(i, i == 2 ? 210 : 40);
        free(assets.tool.tool_effect_data[i]);
    }
    rich4_mkf_close(mkf);
    puts("dog encounter: original FLC timing, camera, portraits, insurance and placement ok");
    return 0;
}
