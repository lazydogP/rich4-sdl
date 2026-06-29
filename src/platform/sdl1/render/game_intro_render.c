#include "platform/sdl1/game_lazy_assets.h"
#include <SDL/SDL.h>

#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_intro_render.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/spr_smp.h"
#include "view/game_view_model.h"
#include "view/view_constants.h"

int draw_game_airborne_player(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const GameViewModel *view,
                              int player_index,
                              int target_x,
                              int target_y,
                              int frame,
                              int frames,
                              int show_aircraft,
                              int use_main_view)
{
    int character;
    int phase;
    int graph_frame;
    int draw_x;
    int draw_y;
    int offset_x = (player_index % 2) * 12 - 6;
    int offset_y = (player_index / 2) * 12 - 6;
    /* Data.mkf[559..570] final FLC bottom-row foot anchors. */
    static const int landing_anchor_x[GAME_JUMP_CHARACTER_COUNT] = {
        216, 214, 213, 214, 216, 215, 220, 211, 216, 216, 216, 216
    };
    static const int landing_anchor_y[GAME_JUMP_CHARACTER_COUNT] = {
        232, 232, 233, 233, 233, 235, 233, 232, 233, 233, 232, 232
    };

    if (frames <= 0) {
        frames = 1;
    }
    character = view != NULL && player_index >= 0 && player_index < view->actor_count
        ? view->actors[player_index].character : player_index;
    if (character < 0 || character >= GAME_JUMP_CHARACTER_COUNT) {
        character = wrap_index(character, GAME_JUMP_CHARACTER_COUNT);
    }
    phase = 0;
    graph_frame = frame;
    if (assets != NULL && show_aircraft &&
        phase >= 0 && phase < 2 &&
        game_asset_ready(assets, &assets->character.has_jump_intro_actor_graph[phase][character]) &&
        draw_game_graph_frame_data(screen,
                                   assets->character.jump_intro_actor_data[phase][character],
                                   assets->character.jump_intro_actor_size[phase][character],
                                   graph_frame,
                                   0,
                                   0)) {
        (void)target_x;
        (void)target_y;
        (void)frames;
        (void)use_main_view;
        return 1;
    }
    if (assets != NULL && !show_aircraft &&
        game_asset_ready(assets, &assets->character.has_first_landing_graph[character]) &&
        draw_game_graph_frame_data(screen,
                                   assets->character.first_landing_data[character],
                                   assets->character.first_landing_size[character],
                                   frame,
                                   target_x - landing_anchor_x[character],
                                   target_y - landing_anchor_y[character])) {
        (void)frames;
        (void)use_main_view;
        return 1;
    }
    if (show_aircraft) {
        draw_x = target_x - 80 + (80 * frame) / frames;
        draw_y = target_y - 150 + (150 * frame) / frames;
    } else {
        draw_x = target_x;
        draw_y = target_y - 54 + (54 * frame) / frames;
    }
    if (use_main_view) {
        if (view != NULL &&
            draw_game_player_model(screen, assets, view, player_index,
                                   draw_x + offset_x, draw_y + offset_y, 1)) {
            return 1;
        }
    }
    SDL_LockSurface(screen);
    fill_rect(screen,
              draw_x - 7 + offset_x,
              draw_y - 7 + offset_y,
              14,
              14,
              game_player_color(player_index));
    stroke_rect(screen,
                draw_x - 7 + offset_x,
                draw_y - 7 + offset_y,
                14,
                14,
                0x0000);
    SDL_UnlockSurface(screen);
    return 1;
}

int draw_game_intro_fullscreen(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               const GameIntroRenderView *view)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || assets == NULL || view == NULL || !view->active ||
        view->player_count <= 0) {
        return 0;
    }

    {
        Rich4IntroStage stage = view->stage;

        SDL_LockSurface(screen);
        fill_rect(screen, 0, 0, SCREEN_W, SCREEN_H, 0x0000);
        SDL_UnlockSurface(screen);

        if (stage.kind == RICH4_INTRO_STAGE_WINDOW) {
            if (game_asset_ready(assets, &assets->intro.has_jump_intro_scene) &&
                rich4_spr_smp_get_chunk(assets->intro.jump_intro_scene_data,
                                            assets->intro.jump_intro_scene_size,
                                            &assets->intro.jump_intro_scene_spr,
                                            0,
                                            &chunk) == RICH4_SPR_SMP_OK) {
                draw_chunk_at(screen, assets->intro.jump_intro_scene_data, &chunk, 0, 0, 0);
            }
            if (game_asset_ready(assets, &assets->intro.has_jump_intro_window_graph)) {
                draw_game_graph_frame_data(screen,
                                           assets->intro.jump_intro_window_data,
                                           assets->intro.jump_intro_window_size,
                                           stage.graph_frame,
                                           180,
                                           60);
            }
            return 1;
        }

        if (stage.kind == RICH4_INTRO_STAGE_TAKEOFF) {
            if (game_asset_ready(assets, &assets->intro.has_jump_intro_scene) &&
                rich4_spr_smp_get_chunk(assets->intro.jump_intro_scene_data,
                                            assets->intro.jump_intro_scene_size,
                                            &assets->intro.jump_intro_scene_spr,
                                            0,
                                            &chunk) == RICH4_SPR_SMP_OK) {
                draw_chunk_at(screen, assets->intro.jump_intro_scene_data, &chunk, 0, 0, 0);
            }
            if (stage.static_player_index >= 0 &&
                stage.static_player_index < view->player_count) {
                int static_character = view->characters[stage.static_player_index];
                if (game_asset_ready(assets, &assets->intro.has_jump_intro_scene) &&
                    rich4_spr_smp_get_chunk(assets->intro.jump_intro_scene_data,
                                                assets->intro.jump_intro_scene_size,
                                                &assets->intro.jump_intro_scene_spr,
                                                (size_t)(static_character + 3),
                                                &chunk) == RICH4_SPR_SMP_OK) {
                    draw_chunk_at_jump_darken_minus20(screen,
                                                      assets->intro.jump_intro_scene_data,
                                                      &chunk,
                                                      270,
                                                      220,
                                                      1);
                }
            }
            if (game_asset_ready(assets, &assets->intro.has_jump_intro_scene) &&
                rich4_spr_smp_get_chunk(assets->intro.jump_intro_scene_data,
                                            assets->intro.jump_intro_scene_size,
                                            &assets->intro.jump_intro_scene_spr,
                                            2,
                                            &chunk) == RICH4_SPR_SMP_OK) {
                draw_chunk_at(screen, assets->intro.jump_intro_scene_data, &chunk, 150, 60, 1);
            }
            if (stage.kind == RICH4_INTRO_STAGE_TAKEOFF &&
                stage.player_index >= 0 &&
                stage.player_index < view->player_count) {
                int jump_character = view->characters[stage.player_index];
                if (game_asset_ready(assets, &assets->character.has_jump_intro_actor_graph[0][jump_character])) {
                    draw_game_graph_frame_data(screen,
                                               assets->character.jump_intro_actor_data[0][jump_character],
                                               assets->character.jump_intro_actor_size[0][jump_character],
                                               stage.graph_frame,
                                               0,
                                               0);
                }
            }
            return 1;
        }

        if (stage.kind == RICH4_INTRO_STAGE_PARACHUTE) {
            if (game_asset_ready(assets, &assets->intro.has_jump_intro_scene) &&
                rich4_spr_smp_get_chunk(assets->intro.jump_intro_scene_data,
                                            assets->intro.jump_intro_scene_size,
                                            &assets->intro.jump_intro_scene_spr,
                                            1,
                                            &chunk) == RICH4_SPR_SMP_OK) {
                draw_chunk_at(screen, assets->intro.jump_intro_scene_data, &chunk, 0, 0, 0);
            }
            if (stage.player_index >= 0 &&
                stage.player_index < view->player_count) {
                int character = view->characters[stage.player_index];
                if (game_asset_ready(assets, &assets->character.has_jump_intro_actor_graph[1][character])) {
                    draw_game_graph_frame_data(screen,
                                               assets->character.jump_intro_actor_data[1][character],
                                               assets->character.jump_intro_actor_size[1][character],
                                               stage.graph_frame,
                                               0,
                                               0);
                }
            }
            return 1;
        }
    }

    return 1;
}
