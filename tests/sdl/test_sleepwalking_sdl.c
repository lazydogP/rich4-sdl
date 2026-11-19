#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "app/dice_flow.h"
#include "app/turn_start_flow.h"
#include "platform/sdl1/game_view_model_input.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "resource/mkf.h"

static GamePreviewAssets assets;
static GameSession session;
static GameViewModel view;
static GameViewModelInput input;
static unsigned char *marker_data;
static size_t marker_size;
static Rich4SprSmpInfo marker_spr;

static void equal_frames(SDL_Surface *a, SDL_Surface *b)
{
    assert(a->pitch == b->pitch);
    assert(memcmp(a->pixels, b->pixels, a->pitch * a->h) == 0);
}

static void check_actor(SDL_Surface *actual, SDL_Surface *expected,
    Rich4Mkf *data, int actor, const MapData *map)
{
    NewGameSettings settings = {0};
    unsigned char *body[2];
    size_t size[2];
    Rich4SprSmpInfo spr[2];
    int lane, direction, frame;
    for (lane = 0; lane < 2; lane++) {
        int resource = actor < 12 ? 128 + 21 * actor + 16 + lane
            : 380 + 4 * (actor - 12) + (lane ? 3 : 0);
        assert(rich4_mkf_read_resource(data, resource, &body[lane], &size[lane], NULL) == RICH4_MKF_OK);
        assert(rich4_spr_smp_probe(body[lane], size[lane], &spr[lane]));
        if (actor < 12) {
            int part = 16 + lane;
            assets.character.map_player_data[actor][part] = body[lane];
            assets.character.map_player_size[actor][part] = size[lane];
            assets.character.map_player_spr[actor][part] = spr[lane];
            assets.character.has_map_player[actor][part] = 1;
        } else {
            int slot = actor - 12, part = lane ? 3 : 0;
            assets.character.map_special_actor_data[slot][part] = body[lane];
            assets.character.map_special_actor_size[slot][part] = size[lane];
            assets.character.map_special_actor_spr[slot][part] = spr[lane];
            assets.character.has_map_special_actor[slot][part] = 1;
        }
    }
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    if (actor < 12) {
        session.state.players[0].character = actor;
        session.state.players[0].node = 1;
        session.state.players[0].days_sleep_walking = 5;
        session.presentation.actors[0].intro_visible = 1;
        session.presentation.actors[0].visual_x = 320;
        session.presentation.actors[0].visual_y = 320;
    } else {
        session.state.special_player_node[actor - 12] = 1;
        session.state.special_player_status[actor - 12][1] = 5;
    }
    for (lane = 0; lane < 2; lane++)
    for (direction = 0; direction < 8; direction++)
    for (frame = 0; frame < 6; frame++) {
        int body_frame, x, y;
        int index = actor < 12 ? 0 : actor - 12 + 4;
        session.presentation.sleepwalking_frame[index] = frame;
        session.presentation.actors[0].direction = direction;
        session.presentation.actors[0].move_active = lane;
        session.presentation.actors[0].walk_frame = 2;
        session.presentation.special_actor.active = actor >= 12;
        session.presentation.special_actor.slot = actor - 12;
        session.presentation.special_actor.visual_x = 320;
        session.presentation.special_actor.visual_y = 320;
        session.presentation.special_actor.direction = direction;
        session.presentation.special_actor.resource_part = lane ? 3 : 0;
        session.presentation.special_actor.animation_frame = 2;
        rich4_sdl1_game_view_model_input_build(
            &session.flow, &session.presentation, &input);
        assert(rich4_game_view_model_build(&session.state, &input, map, &view));
        SDL_FillRect(actual, NULL, 0x4210);
        SDL_FillRect(expected, NULL, 0x4210);
        draw_game_map_objects(actual, &assets, &settings, &view, NULL);
        rich4_game_viewport_project(&view.viewport, 320, 320, &x, &y);
        body_frame = direction * (spr[lane].num_chunks / 8);
        if (actor >= 12 || lane) body_frame += 2 % (spr[lane].num_chunks / 8);
        assert(draw_spr_smp_center(expected, body[lane], size[lane], &spr[lane],
            body_frame, x, y));
        /* 0x496978 = 0x496930 + 18*4: the independent expected image is
         * Data.mkf[396+18], not the Map.mkf[25] decoy loaded below. */
        assert(draw_spr_smp_center(expected, marker_data,
            marker_size, &marker_spr, frame, x, y));
        equal_frames(actual, expected);
    }
    for (lane = 0; lane < 2; lane++) {
        if (actor < 12) assets.character.has_map_player[actor][16 + lane] = 0;
        else assets.character.has_map_special_actor[actor - 12][lane ? 3 : 0] = 0;
        free(body[lane]);
    }
}

static void check_automatic_turn(const MapData *map)
{
    Rich4TurnStartRequest request;
    int ai;
    for (ai = 0; ai < 2; ai++) {
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.players[0].play_state = ai ? 2 : 1;
        session.state.players[0].is_ai = ai;
        session.state.players[0].node = 1;
        session.state.players[0].ndices = 1;
        session.state.players[0].days_sleep_walking = 4;
        session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
        assert(rich4_turn_start_prepare_request(&session, 100, &request));
        assert(request.action == RICH4_TURN_START_STARTED);
        assert(!session.presentation.confinement_status.active);
        assert(!session.presentation.dice.status_visible);
        assert(rich4_dice_update(&session, map, 0, 20, 1, 10, 120));
        assert(session.presentation.dice.popup_active);
        assert(session.flow.dice_value >= 1 && session.flow.dice_value <= 6);
    }
}

int main(void)
{
    Rich4Mkf *data, *map_archive;
    MapData map = {0};
    MapRouteNode routes[2] = {{0}};
    SDL_Surface *actual, *expected;
    int actor;
    actual = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    expected = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16, 0xf800, 0x07e0, 0x001f, 0);
    assert(actual && expected);
    assert(rich4_mkf_open("assets/original/Data.mkf", &data) == RICH4_MKF_OK);
    assert(rich4_mkf_open("assets/original/Map.mkf", &map_archive) == RICH4_MKF_OK);
    assert(rich4_mkf_read_resource(map_archive, 25, &assets.map.map_common_data[1],
        &assets.map.map_common_size[1], NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(assets.map.map_common_data[1],
        assets.map.map_common_size[1], &assets.map.map_common_spr[1]));
    assert(assets.map.map_common_spr[1].num_chunks >= 6);
    assets.map.has_map_common_graph[1] = 1;
    /* Keep the unrelated Map[25] sprite available: selecting it must fail
     * the image comparison rather than silently skipping the marker. */
    assert(rich4_mkf_read_resource(data, 414, &marker_data,
        &marker_size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(marker_data, marker_size, &marker_spr));
    assert(marker_spr.num_chunks == 6);
    assets.map.data_map_status_data[18] = marker_data;
    assets.map.data_map_status_size[18] = marker_size;
    assets.map.data_map_status_spr[18] = marker_spr;
    assets.map.has_data_map_status[18] = 1;
    routes[1].x = routes[1].y = 320;
    map.routes = routes; map.route_count = 2;
    for (actor = 0; actor < 16; actor++) check_actor(actual, expected, data, actor, &map);
    check_automatic_turn(&map);
    free(assets.map.map_common_data[1]);
    free(marker_data);
    rich4_mkf_close(map_archive); rich4_mkf_close(data);
    SDL_FreeSurface(actual); SDL_FreeSurface(expected);
    puts("sleepwalking SDL: 12 players + 4 special actors, 8 directions, 6 marker frames, automatic turn: ok");
    return 0;
}
