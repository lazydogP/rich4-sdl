#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/render/game_intro_render.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/render/game_map_render.h"
#include "platform/sdl1/render/game_minimap_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/render/game_session_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"

/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c.
 * game_minimap_node_xy_for_map is a single-caller helper (small-map route
 * dot projection) that moved with its only caller, draw_game_session. */

static void game_minimap_node_xy_for_map(const GamePreviewAssets *assets,
                                         const NewGameSettings *settings,
                                         int node,
                                         int *out_x,
                                         int *out_y)
{
    int background_index;
    int world_x;
    int world_y;
    game_route_node_xy_for_map(assets, settings, node, &world_x, &world_y);
    if (assets == NULL || settings == NULL) {
        if (out_x != NULL) *out_x = world_x;
        if (out_y != NULL) *out_y = world_y;
        return;
    }
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        assets->map.maps[background_index].route_count == 0) {
        if (out_x != NULL) *out_x = world_x;
        if (out_y != NULL) *out_y = world_y;
        return;
    }
    if (out_x != NULL) {
        *out_x = 20 + ((world_x * 11392) >> 16);
    }
    if (out_y != NULL) {
        *out_y = 60 + ((world_y * 11392) >> 16);
    }
}

static void draw_game_god_detachment(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     const GameViewModel *view,
                                     const GameGodDetachmentView *detachment)
{
    Rich4SprSmpChunk sprite;
    const Rich4SprSmpInfo *spr;
    const unsigned char *data;
    size_t size;
    int source_y, height;
    int status_index;
    int chunks;
    int x;
    int y;
    int chunk;

    if (screen == NULL || assets == NULL || view == NULL ||
        detachment == NULL || !detachment->active ||
        detachment->god_type <= 0 ||
        detachment->god_type > GAME_DATA_MAP_STATUS_RESOURCE_COUNT)
        return;
    status_index = detachment->god_type - 1;
    if (!assets->map.has_data_map_status[status_index]) return;
    chunks = (int)assets->map.data_map_status_spr[status_index].num_chunks;
    if (chunks <= 0) return;
    rich4_map_god_detachment_anchor(&view->viewport,
        detachment->world_x, detachment->world_y, detachment->direction, &x, &y);
    /* 0x40E32C subtracts ten screen pixels before each of its 24 draws and
     * advances the eight-way chunk at the same boundary. */
    y -= (detachment->frame + 1) * 10;
    chunk = (detachment->direction + detachment->frame + 1) & 7;
    data = assets->map.data_map_status_data[status_index];
    size = assets->map.data_map_status_size[status_index];
    spr = &assets->map.data_map_status_spr[status_index];
    if (rich4_spr_smp_get_chunk(data, size, spr,
            (size_t)wrap_index(chunk, chunks), &sprite) != RICH4_SPR_SMP_OK) return;
    x -= sprite.x;
    y -= sprite.y;
    source_y = y < 40 ? 40 - y : 0;
    height = sprite.height - source_y;
    if (height <= 0) return;
    /* These software pixel blitters do not honor SDL_SetClipRect. */
    if (memcmp(spr->signature, "SPR", 4) == 0)
        draw_spr_chunk_crop_topleft(screen, data, size, spr, &sprite,
            0, source_y, sprite.width, height, x, y + source_y, 1);
    else
        draw_chunk_crop_topleft(screen, data, &sprite,
            0, source_y, sprite.width, height, x, y + source_y, 1);
}

void draw_game_steps_remaining(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                const GameViewModel *view)
{
    if (screen == NULL || assets == NULL || view == NULL) return;
    /* 0x40829D renders this with Data.mkf[0x205], not the UI text font:
     * digit chunk = ASCII digit - 40, start x = 245 - 25 * digit_count,
     * y = 400, with a 50 px advance. */
    if (view->show_steps) {
        char steps[16];
        int x;
        int i;
        snprintf(steps, sizeof(steps), "%d", view->steps_remaining);
        x = 245 - 25 * (int)strlen(steps);
        if (assets->map.has_map_control) {
            for (i = 0; steps[i] != '\0'; i++, x += 50) {
                draw_spr_smp_center(screen,
                                    assets->map.map_control_data,
                                    assets->map.map_control_size,
                                    &assets->map.map_control_spr,
                                    (size_t)((unsigned char)steps[i] - 40),
                                    x,
                                    400);
            }
        }
    }
}

void draw_game_session(SDL_Surface *screen,
                       const GamePreviewAssets *assets,
                       const NewGameSettings *settings,
                       GameSceneView *scene,
                       GameRenderCache *cache)
{
    const GameViewModel *view;
    int i;
    int route_count;
    int use_main_view;

    if (scene == NULL || !scene->game.active) return;
    view = &scene->game;
    use_main_view = scene->use_main_view;
    if (!use_main_view) reset_game_property_flash_cache(cache);
    route_count = view->route_count > 0 ? view->route_count :
        game_route_count_for_map(assets, settings);

    if (use_main_view) {
        trace_game_start("draw session: map objects begin");
        if (!draw_game_cached_property_flash(screen, assets, settings, view,
                scene->dynamic_objects, cache))
            draw_game_map_objects(screen, assets, settings, view,
                                  scene->dynamic_objects);
        draw_game_map_info(screen, assets, settings, scene);
        trace_game_start("draw session: map objects end");
    }
    trace_game_start("draw session: minimap begin");
    draw_game_minimap_panel(screen, assets, settings, view, scene->view_mode);
    trace_game_start("draw session: minimap end");

    trace_game_start("draw session: route begin");
    SDL_LockSurface(screen);
    if (!use_main_view) {
        for (i = game_route_first_node_for_map(assets, settings); i < route_count; i++) {
            int x;
            int y;
            uint16_t color = (i % 2) == 0 ? 0x7bef : 0x5acb;
            game_minimap_node_xy_for_map(assets, settings, i, &x, &y);
            if (x < -8 || x >= 448 || y < 32 || y >= 488) {
                continue;
            }
            fill_rect(screen, x - 5, y - 4, 10, 8, color);
            stroke_rect(screen, x - 5, y - 4, 10, 8, 0xffff);
        }
    }
    SDL_UnlockSurface(screen);
    trace_game_start("draw session: route end");

    if (use_main_view) {
    }

    trace_game_start("draw session: players begin");
    for (i = 0; i < view->actor_count; i++) {
        const GameActorView *actor = &view->actors[i];
        int x;
        int y;
        int offset_x = (i % 2) * 12 - 6;
        int offset_y = (i / 2) * 12 - 6;
        int airborne = 0;
        int airborne_frame = 0;
        int airborne_frames = 1;
        int show_aircraft = 0;
        int intro_player = scene->intro_player;
        int landing_player = scene->landing_player;

        if (intro_player >= 0 && i == intro_player) {
            airborne = 1;
            airborne_frame = scene->intro_frame;
            airborne_frames = scene->intro_total_frames;
            show_aircraft = 1;
        } else if (i == landing_player) {
            airborne = 1;
            airborne_frame = scene->landing_frame;
            airborne_frames = scene->landing_frames;
            show_aircraft = 0;
        } else if (!actor->visible) {
            continue;
        }
        if (use_main_view && !airborne) {
            /* Normal actors were already drawn in 0x40829D order together
             * with map objects.  Landing FLCs remain a later composition
             * pass, matching their separate original playback path. */
            continue;
        }
        if (use_main_view) {
            int world_x;
            int world_y;
            world_x = actor->world_x;
            world_y = actor->world_y;
            rich4_game_viewport_project(&view->viewport, world_x, world_y, &x, &y);
        } else {
            x = game_world_to_minimap_x(actor->world_x);
            y = game_world_to_minimap_y_at(actor->world_y,
                                           scene->view_mode == 2 ? 80 : 280);
        }
        if (x < -12 || x >= 452 || y < 28 || y >= 492) {
            continue;
        }
        if (airborne) {
            trace_game_start("draw session: airborne player begin");
            draw_game_airborne_player(screen,
                                      assets,
                                      view,
                                      i,
                                      x,
                                      y,
                                      airborne_frame,
                                      airborne_frames,
                                      show_aircraft,
                                      use_main_view);
            trace_game_start("draw session: airborne player end");
            continue;
        }
        if (use_main_view &&
            draw_game_player_model(screen,
                                   assets,
                                   view,
                                   i,
                                   x + offset_x,
                                   y + offset_y,
                                   1)) {
            continue;
        }
        SDL_LockSurface(screen);
        fill_rect(screen, x - 7 + offset_x, y - 7 + offset_y, 14, 14, game_player_color(i));
        stroke_rect(screen, x - 7 + offset_x, y - 7 + offset_y, 14, 14, 0x0000);
        SDL_UnlockSurface(screen);
    }
    trace_game_start("draw session: players end");
    if (use_main_view)
        draw_game_god_detachment(screen, assets, view,
                                 &scene->god_detachment);
    trace_game_start("draw session: dice popup begin");
    draw_game_dice_popup(screen, assets, view, cache);
    trace_game_start("draw session: dice popup end");
    draw_game_steps_remaining(screen, assets, view);
    draw_game_route_interaction(screen, assets, scene);
}

void draw_game_ending_presentation(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   const GameSceneView *scene)
{
    int character_resource;
    int drew_resource = 0;

    if (screen == NULL || scene == NULL || !scene->ending_active ||
        !scene->ending_jump_active)
        return;

    /* The original ending replaces the active game surface. Its first
     * operation is the stage/map-specific JUMP.MKF background; the later
     * ENDxx.AVI/THANKS.AVI branch remains an explicit platform boundary. */
    SDL_LockSurface(screen);
    fill_rect(screen, 0, 0, SCREEN_W, SCREEN_H, 0x0000);
    SDL_UnlockSurface(screen);

    if (assets != NULL && scene->ending_background_index >= 0 &&
        scene->ending_background_index < GAME_BACKGROUND_COUNT &&
        assets->intro.has_jump_background[
            scene->ending_background_index])
        (void)draw_raw_rgb565_fullscreen(
            screen,
            assets->intro.jump_data[scene->ending_background_index],
            assets->intro.jump_size[scene->ending_background_index]);

    if (assets != NULL && game_asset_ready(assets, &assets->ui.has_panel[93]) &&
        assets->ui.panel_spr[93].num_chunks > 0)
        drew_resource |= draw_game_panel_chunk_at(
            screen, assets, 93, 0, SCREEN_W / 2, 150, 1);

    character_resource = 100 + scene->ending_winner_character;
    if (assets != NULL && scene->ending_winner_character >= 0 &&
        scene->ending_winner_character < GAME_JUMP_CHARACTER_COUNT &&
        character_resource < GAME_PANEL_RESOURCE_COUNT &&
        game_asset_ready(assets, &assets->ui.has_panel[character_resource]) &&
        assets->ui.panel_spr[character_resource].num_chunks > 0)
        drew_resource |= draw_game_panel_chunk_at(
            screen, assets, character_resource, 0,
            SCREEN_W / 2, 300, 1);

    if (!drew_resource) {
        SDL_LockSurface(screen);
        draw_text(screen, 250, 220, "GAME OVER", 2, 0xffff);
        SDL_UnlockSurface(screen);
    }
}

void draw_game_continuation_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSceneView *scene)
{
    int drew_resource = 0;

    if (screen == NULL || scene == NULL || !scene->ending_active ||
        !scene->ending_continuation_active)
        return;

    /* fcn_00407842 captures the already-rendered game surface, then draws
     * Panel[112] over it.  The preview composition has the same ordering, so
     * do not clear the map here.  Chunk 0 is the 0x407842 prompt and chunks
     * 1..10 are the visible countdown digits (chunk 10 is the initial 10). */
    if (assets != NULL && game_asset_ready(assets, &assets->ui.has_panel[112]) &&
        assets->ui.panel_spr[112].num_chunks > 0)
        drew_resource = draw_game_panel_chunk_at(
            screen, assets, 112, 0, SCREEN_W / 2, 120, 1);
    if (assets != NULL && game_asset_ready(assets, &assets->ui.has_panel[112]) &&
        scene->ending_countdown > 0 &&
        scene->ending_countdown < (int)assets->ui.panel_spr[112].num_chunks)
        drew_resource |= draw_game_panel_chunk_at(
            screen, assets, 112,
            (size_t)scene->ending_countdown, SCREEN_W / 2, 350, 1);
    if (!drew_resource) {
        SDL_LockSurface(screen);
        draw_text(screen, 220, 220, "LOAD GAME", 2, 0xffff);
        SDL_UnlockSurface(screen);
    }
}

void draw_game_ending_confirmation_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameSceneView *scene)
{
    if (screen == NULL || assets == NULL || scene == NULL ||
        !scene->ending_confirmation_active ||
        !game_asset_ready(assets, &assets->ui.has_ending_success_animation))
        return;
    draw_game_graph_frame_data(
        screen, assets->ui.ending_success_data,
        assets->ui.ending_success_size,
        scene->ending_confirmation_frame, 0, 40);
}
