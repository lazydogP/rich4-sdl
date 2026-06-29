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
#include "app/tool_followup_flow.h"
#include "core/tool.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/render/game_session_render.h"
#include "resource/mkf.h"

static GameSession session;
static GamePreviewAssets assets;
static NewGameSettings settings;
static MapRouteNode routes[4];
static GameSceneView scene;

static void setup(int slot, int steps)
{
    int16_t x[4] = {0, 256, 288, 1024};
    int16_t y[4] = {0, 256, 256, 1024};
    uint16_t next[4][4] = {{0}, {2}, {1}, {1}};
    memset(&session, 0, sizeof(session));
    memset(routes, 0, sizeof(routes));
    for (int i = 1; i < 4; ++i) {
        routes[i].x = x[i]; routes[i].y = y[i];
        routes[i].next[0] = next[i][0];
    }
    assets.map.maps[0].routes = routes;
    assets.map.maps[0].route_count = 4;
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].node = 1;
    session.state.players[0].previous_node = 2;
    assert(rich4_tool_set_map_geometry(&session.state, 4, x, y, next,
                                     0, NULL, NULL, 0, NULL, NULL));
    session.state.tool_robot.active = 1;
    session.state.tool_robot.node = 1;
    session.state.tool_robot.previous_node = 2;
    session.state.tool_robot.next_node = 2;
    session.state.tool_robot.steps_remaining = steps;
    rich4_app_init_dynamic_object_pool(&session.presentation);
    if (slot >= 0) {
        session.presentation.dynamic_objects[slot].node = 2;
        session.presentation.dynamic_objects[slot].node_direction = 5;
        if (slot >= 16) {
            session.state.tool_placed_objects[slot - 16].kind =
                session.presentation.dynamic_objects[slot].type;
            session.state.tool_placed_objects[slot - 16].node = 2;
            session.state.tool_placed_objects[slot - 16].attached_player = 0xff;
        }
    }
    assert(rich4_tool_followup_mark_robot_trigger(&session, 1000));
}

static void lifecycle(int slot, int steps)
{
    int frame_ms = game_move_frame_ms_from_config(NULL);
    int stopped = 0, hit = 0, endpoints = 0, last_arrived = 0;
    setup(slot, steps);
    for (int tick = 0; tick < 350; ++tick) {
        assert(game_tool_followup_update_robot(&session, &assets, &settings,
            NULL, NULL, 1000 + tick * frame_ms));
        assert(build_game_session_render_view(&assets, &settings, &session, &scene));
        if (session.presentation.tool_robot_motion.arrived && !last_arrived)
            endpoints++;
        last_arrived = session.presentation.tool_robot_motion.arrived;
        if (slot >= 0 && !hit && session.presentation.dynamic_objects[slot].flight_active) {
            GameMapDynamicObjectState *object = &session.presentation.dynamic_objects[slot];
            hit = 1;
            assert(object->node == 0 && object->world_x == 304);
            assert(object->velocity_x == 16 && object->velocity_y == 0);
            assert(object->flight_direction == 5);
            if (slot >= 16) {
                assert(session.state.tool_placed_objects[slot - 16].kind == 0);
                assert(session.state.tool_amount[(slot - 16) / 10 + 1] == 1);
                rich4_app_sync_tool_dynamic_objects(&session.presentation, &session.state);
                assert(object->flight_active && object->world_x == 304);
            }
            if (slot < 12) assert(session.presentation.dynamic_objects[slot ^ 1].node == 3);
        }
        if (!session.state.tool_robot.active) {
            assert(!scene.game.tool_robot.visible && !scene.show_steps);
            assert(!session.presentation.has_view_override);
            assert(scene.game.viewport.center_world_x == 256);
            assert(session.state.current_player == 0 && session.state.players[0].node == 1);
            assert(session.presentation.actors[0].dirty_flags & GAME_PLAYER_DIRTY_TURN);
            stopped = 1;
            break;
        }
        assert(scene.game.tool_robot.visible);
        if (session.presentation.tool_robot_motion.end_wait) {
            assert(scene.game.tool_robot.resource_part == 0);
            assert(scene.game.tool_robot.animation_frame == 0 && !scene.show_steps);
        } else {
            assert(scene.game.tool_robot.resource_part == 1);
            assert(scene.game.steps_remaining == steps - endpoints);
            assert(scene.show_steps == (steps > endpoints));
        }
    }
    assert(stopped && endpoints == steps && (slot < 0 || hit));
}

static void pixels(void)
{
    SDL_Surface *actual = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
        0xf800, 0x07e0, 0x001f, 0);
    SDL_Surface *expected = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
        0xf800, 0x07e0, 0x001f, 0);
    assert(actual && expected);
    setup(-1, 9);
    assert(build_game_session_render_view(&assets, &settings, &session, &scene));
    scene.game.actor_count = scene.game.special_actor_count = 0;
    scene.game.tool_robot.world_x = 256;
    scene.game.tool_robot.world_y = 256;
    for (int part = 0; part < 2; ++part) {
        for (int direction = 0; direction < 8; ++direction) {
            int frames = assets.character.tool_robot_spr[part].num_chunks / 8;
            int frame = part ? frames - 1 : 0;
            SDL_FillRect(actual, NULL, 0x1234);
            SDL_FillRect(expected, NULL, 0x1234);
            scene.game.tool_robot.resource_part = part;
            scene.game.tool_robot.visual_direction = direction;
            scene.game.tool_robot.animation_frame = frame;
            draw_game_map_objects(actual, &assets, &settings, &scene.game, NULL);
            assert(draw_spr_smp_center(expected, assets.character.tool_robot[part].data,
                assets.character.tool_robot[part].size, &assets.character.tool_robot_spr[part],
                direction * frames + frame, 220, 260));
            assert(memcmp(actual->pixels, expected->pixels, actual->pitch * 480) == 0);
        }
    }
    for (int count = 1; count <= 18; ++count) {
        SDL_FillRect(actual, NULL, 0x1234);
        SDL_FillRect(expected, NULL, 0x1234);
        scene.game.show_steps = 1;
        scene.game.steps_remaining = count;
        draw_game_steps_remaining(actual, &assets, &scene.game);
        if (count < 10) {
            assert(draw_spr_smp_center(expected, assets.map.map_control_data,
                assets.map.map_control_size, &assets.map.map_control_spr,
                8 + count, 220, 400));
        } else {
            assert(draw_spr_smp_center(expected, assets.map.map_control_data,
                assets.map.map_control_size, &assets.map.map_control_spr, 9, 195, 400));
            assert(draw_spr_smp_center(expected, assets.map.map_control_data,
                assets.map.map_control_size, &assets.map.map_control_spr,
                8 + count % 10, 245, 400));
        }
        assert(memcmp(actual->pixels, expected->pixels, actual->pitch * 480) == 0);
    }
    SDL_FillRect(actual, NULL, 0x1234);
    SDL_FillRect(expected, NULL, 0x1234);
    scene.game.show_steps = 0;
    draw_game_steps_remaining(actual, &assets, &scene.game);
    assert(memcmp(actual->pixels, expected->pixels, actual->pitch * 480) == 0);
    SDL_FreeSurface(actual); SDL_FreeSurface(expected);
}

int main(void)
{
    Rich4Mkf *mkf = NULL;
    unsigned char *robot_data[2];
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    for (int part = 0; part < 2; ++part) {
        assert(rich4_mkf_read_resource(mkf, 521 + part,
            &robot_data[part],
            &assets.character.tool_robot[part].size, NULL) == RICH4_MKF_OK);
        assets.character.tool_robot[part].data = robot_data[part];
        assert(rich4_spr_smp_probe(assets.character.tool_robot[part].data,
            assets.character.tool_robot[part].size, &assets.character.tool_robot_spr[part]));
        assets.character.has_tool_robot[part] = 1;
    }
    assert(rich4_mkf_read_resource(mkf, 517, &assets.map.map_control_data,
        &assets.map.map_control_size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(assets.map.map_control_data,
        assets.map.map_control_size, &assets.map.map_control_spr));
    assets.map.has_map_control = 1;
    lifecycle(-1, 9);
    for (int slot = 0; slot < 46; ++slot) lifecycle(slot, 1);
    pixels();
    free(assets.map.map_control_data);
    for (int part = 0; part < 2; ++part) free(robot_data[part]);
    rich4_mkf_close(mkf);
    puts("machine doll: 46 object slots, flight/wait/camera, Data[521/522] and Data[517] pixels ok");
    return 0;
}
