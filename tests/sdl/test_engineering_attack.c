#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#undef main

#include "app/engineering_attack_flow.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_overlay_view.h"
#include "platform/sdl1/render/game_overlay_render.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/mkf.h"

static GamePreviewAssets assets;
static GameSession session;
static GameOverlayRenderView view;

int main(void)
{
    static const int original_frames[12] = {5,5,6,6,5,6,6,6,6,6,5,6};
    Rich4Mkf *mkf = NULL;
    Rich4Config config = {0};
    NewGameSettings settings = {0};
    SDL_Surface *actual, *expected;
    int character, t;
    actual = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
                                  0xf800, 0x07e0, 0x001f, 0);
    expected = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
                                    0xf800, 0x07e0, 0x001f, 0);
    assert(actual && expected);
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    assert(rich4_mkf_read_resource(mkf, 526,
        &assets.ui.point_minigame_effect_data,
        &assets.ui.point_minigame_effect_size, NULL) == RICH4_MKF_OK);
    assets.ui.has_point_minigame_effect = 1;
    config.game_speed = 2;
    config.animation = 0; /* EA2=3 is not the optional event FLC player. */
    for (character = 0; character < 12; ++character) {
        unsigned char **data = &assets.character.map_player_data[character][12];
        size_t *size = &assets.character.map_player_size[character][12];
        Rich4SprSmpInfo *spr = &assets.character.map_player_spr[character][12];
        assert(rich4_mkf_read_resource(mkf, 140 + 21 * character,
                                       data, size, NULL) == RICH4_MKF_OK);
        assert(rich4_spr_smp_probe(*data, *size, spr));
        assert((int)spr->num_chunks / 8 == original_frames[character]);
        assets.character.has_map_player[character][12] = 1;
        memset(&session, 0, sizeof(session));
        session.state.active = 1;
        session.state.player_count = 1;
        session.state.price_index = 1;
        session.state.players[0].character = character;
        session.state.players[0].play_state = 1;
        session.state.players[0].traffic_method = 31;
        session.state.players[0].node = 1;
        session.state.tool_route_node_count = 2;
        session.state.route_object_raw[1] = 2001;
        session.state.tool_route_x[1] = 200;
        session.state.tool_route_y[1] = 200;
        session.state.tool_land_x[0] = 232;
        session.state.tool_land_y[0] = 200;
        session.state.land_count = 1;
        session.state.land_owner[0] = -1;
        session.state.land_level[0] = 3;
        session.presentation.actors[0].intro_visible = 1;
        session.presentation.actors[0].visual_x = 200;
        session.presentation.actors[0].visual_y = 200;
        assert(rich4_engineering_attack_begin(&session, 0, 1, 1000));
        for (t = 0; t <= 15; ++t) {
            int frame, direction, x, y;
            assert(game_update_presentation_dispatch(
                &session, &assets, &settings, &config, NULL,
                1040 + t * 40, 0));
            assert(session.state.land_level[0] == (t < 9 ? 3 : 0));
            if (t == 15) {
                assert(!session.presentation.engineering_attack.active);
                break;
            }
            assert(build_game_overlay_render_view(
                &assets, &settings, &session, &view));
            assert(view.scene.game.actors[0].use_action_part);
            assert(view.scene.game.actors[0].action_part == 12);
            assert(view.scene.game.actors[0].overlay_part == -1);
            frame = t <= 6 ? 0 : t == 7 ? 2 : t - 6;
            if (frame >= original_frames[character])
                frame = original_frames[character] - 1;
            assert(view.scene.game.actors[0].action_frame == frame);
            direction = view.scene.game.actors[0].visual_direction;
            SDL_FillRect(actual, NULL, 0x4210);
            SDL_FillRect(expected, NULL, 0x4210);
            assert(draw_game_player_model(actual, &assets, &view.scene.game,
                                            0, 220, 260, 1));
            assert(draw_spr_smp_center(expected, *data, *size, spr,
                direction * original_frames[character] + frame, 220, 260));
            draw_game_tool_animation(actual, &assets, &view);
            if (t >= 8) {
                rich4_game_viewport_project(&view.scene.game.viewport,
                    232, 200, &x, &y);
                assert(draw_game_graph_frame_data(expected,
                    assets.ui.point_minigame_effect_data,
                    assets.ui.point_minigame_effect_size,
                    t - 8, x - 55, y - 55));
            }
            assert(actual->pitch == expected->pitch);
            assert(memcmp(actual->pixels, expected->pixels,
                           actual->pitch * actual->h) == 0);
        }
        free(*data);
        *data = NULL;
        assets.character.has_map_player[character][12] = 0;
    }
    free(assets.ui.point_minigame_effect_data);
    rich4_mkf_close(mkf);
    SDL_FreeSurface(actual);
    SDL_FreeSurface(expected);
    puts("engineering SDL: 12 original actors, tick dispatch and effect anchoring ok");
    return 0;
}
