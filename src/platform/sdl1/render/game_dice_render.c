#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>

#include <SDL/SDL.h>

#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_render_cache.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/flc_animation.h"
#include "view/game_view_model.h"
#include "view/view_constants.h"

/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c. Only
 * reset_game_dice_cache and the two draw entrypoints are public; the surface
 * bookkeeping / graph compositing helpers stay file-static. */

void reset_game_dice_cache(GameRenderCache *cache)
{
    if (cache == NULL) {
        return;
    }
    if (cache->dice_graph_surface != NULL) {
        SDL_FreeSurface(cache->dice_graph_surface);
        cache->dice_graph_surface = NULL;
    }
    if (cache->dice_source_surface != NULL) {
        SDL_FreeSurface(cache->dice_source_surface);
        cache->dice_source_surface = NULL;
    }
    cache->dice_rendered_frame = -1;
    cache->dice_surface_resource = -1;
    cache->dice_surface_x = 0;
    cache->dice_surface_y = 0;
    cache->dice_result_composited = 0;
}

static void game_dice_popup_position(const GameViewModel *view, int *out_x, int *out_y)
{
    static const int x_offsets[8] = { 4, 12, 8, -4, -12, -24, -20, -12 };
    static const int y_offsets[8] = { 12, 12, 6, -6, -12, -12, -6, 6 };
    int direction = 0;

    if (view != NULL) direction = view->dice.visual_direction & 7;
    if (out_x != NULL) {
        *out_x = GAME_DICE_POPUP_BASE_X + x_offsets[direction];
    }
    if (out_y != NULL) {
        *out_y = GAME_DICE_POPUP_BASE_Y + y_offsets[direction];
    }
}

static int ensure_game_dice_surfaces(SDL_Surface *screen,
                                     GameRenderCache *cache,
                                     int resource_index,
                                     int x,
                                     int y)
{
    if (screen == NULL || cache == NULL) {
        return 0;
    }
    if (cache->dice_graph_surface != NULL &&
        cache->dice_source_surface != NULL &&
        cache->dice_surface_resource == resource_index &&
        cache->dice_surface_x == x && cache->dice_surface_y == y) {
        return 1;
    }

    reset_game_dice_cache(cache);
    cache->dice_graph_surface = create_game_work_surface(screen);
    cache->dice_source_surface = create_game_work_surface(screen);
    if (cache->dice_graph_surface == NULL || cache->dice_source_surface == NULL) {
        reset_game_dice_cache(cache);
        return 0;
    }
    SDL_BlitSurface(screen, NULL, cache->dice_source_surface, NULL);
    SDL_BlitSurface(screen, NULL, cache->dice_graph_surface, NULL);
    cache->dice_surface_resource = resource_index;
    cache->dice_surface_x = x;
    cache->dice_surface_y = y;
    cache->dice_rendered_frame = -1;
    return 1;
}

static int game_panel_graph_size(const GamePreviewAssets *assets,
                                 int resource_index,
                                 int *out_width,
                                 int *out_height)
{
    const unsigned char *data;
    Rich4FlcHeader header;

    if (assets == NULL || resource_index < 0 || resource_index >= 9 ||
        !game_asset_ready(assets, &assets->ui.has_panel_graph[resource_index])) {
        return 0;
    }
    data = assets->ui.panel_data[resource_index];
    if (rich4_flc_parse_header(data, assets->ui.panel_size[resource_index],
                               &header) != RICH4_FLC_OK) {
        return 0;
    }
    if (out_width != NULL) {
        *out_width = header.width;
    }
    if (out_height != NULL) {
        *out_height = header.height;
    }
    return 1;
}

static int render_game_dice_graph_to_frame(SDL_Surface *screen,
                                           const GamePreviewAssets *assets,
                                           GameRenderCache *cache,
                                           int resource_index,
                                           int frame_index,
                                           int x,
                                           int y)
{
    int frame;

    if (!ensure_game_dice_surfaces(screen, cache, resource_index, x, y)) {
        return 0;
    }
    if (frame_index < 0) {
        frame_index = 0;
    }
    if (frame_index < cache->dice_rendered_frame) {
        reset_game_dice_cache(cache);
        if (!ensure_game_dice_surfaces(screen, cache, resource_index, x, y)) {
            return 0;
        }
    }

    if (SDL_MUSTLOCK(cache->dice_graph_surface) &&
        SDL_LockSurface(cache->dice_graph_surface) != 0) {
        return 0;
    }
    if (SDL_MUSTLOCK(cache->dice_source_surface) &&
        SDL_LockSurface(cache->dice_source_surface) != 0) {
        if (SDL_MUSTLOCK(cache->dice_graph_surface)) {
            SDL_UnlockSurface(cache->dice_graph_surface);
        }
        return 0;
    }
    for (frame = cache->dice_rendered_frame + 1; frame <= frame_index; frame++) {
        if (!draw_game_panel_graph_frame_step(cache->dice_graph_surface,
                                              assets,
                                              resource_index,
                                              frame,
                                              x,
                                              y,
                                              (const unsigned char *)cache->dice_source_surface->pixels,
                                              cache->dice_source_surface->pitch)) {
            break;
        }
        cache->dice_rendered_frame = frame;
    }
    if (SDL_MUSTLOCK(cache->dice_source_surface)) {
        SDL_UnlockSurface(cache->dice_source_surface);
    }
    if (SDL_MUSTLOCK(cache->dice_graph_surface)) {
        SDL_UnlockSurface(cache->dice_graph_surface);
    }
    return cache->dice_rendered_frame >= frame_index;
}

void draw_game_dice_popup(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const GameViewModel *view,
                          GameRenderCache *cache)
{
    int left;
    int top;
    int graph_frames;
    int graph_frame;
    int graph_resource;
    int graph_width;
    int graph_height;
    int graph_ready = 0;
    int i;

    if (screen == NULL || assets == NULL || view == NULL || cache == NULL ||
        !view->active || !view->dice.popup_visible || view->actor_count <= 0) {
        return;
    }
    game_dice_popup_position(view, &left, &top);

    graph_frames = game_dice_graph_frame_count(assets, view->dice.count);
    graph_frame = view->dice.animation_frame;
    if (graph_frames > 0 && graph_frame >= graph_frames) {
        graph_frame = graph_frames - 1;
    }
    if (view->dice.count >= 1 && view->dice.count <= 3) {
        graph_resource = 3 + view->dice.count;
        if (game_panel_graph_size(assets, graph_resource, &graph_width, &graph_height) &&
            render_game_dice_graph_to_frame(screen,
                                            assets,
                                            cache,
                                            graph_resource,
                                            graph_frame,
                                            left,
                                            top)) {
            SDL_Rect rect;
            graph_ready = 1;
            rect.x = (Sint16)left;
            rect.y = (Sint16)top;
            rect.w = (Uint16)graph_width;
            rect.h = (Uint16)graph_height;
            if (view->dice.result_visible && !cache->dice_result_composited) {
                SDL_BlitSurface(cache->dice_source_surface, &rect, cache->dice_graph_surface, &rect);
                for (i = 0; i < view->dice.count && i < 3; i++) {
                    int face = view->dice.faces[i];
                    int chunk = i * 6 + face - 1;
                    draw_game_panel_chunk_at(cache->dice_graph_surface,
                                             assets,
                                             3,
                                             (size_t)chunk,
                                             left + GAME_DICE_FACE_OFFSET_X,
                                             top + GAME_DICE_FACE_OFFSET_Y,
                                             1);
                }
                cache->dice_result_composited = 1;
            }
            SDL_BlitSurface(cache->dice_graph_surface, &rect, screen, &rect);
        } else if (!view->dice.result_visible) {
            draw_game_panel_graph_frame(screen,
                                        assets,
                                        graph_resource,
                                        graph_frame,
                                        left,
                                        top);
        }
    }
    if (!view->dice.result_visible) {
        return;
    }
    if (graph_ready) {
        return;
    }
    for (i = 0; i < view->dice.count && i < 3; i++) {
        int face = view->dice.faces[i];
        int chunk = i * 6 + face - 1;
        draw_game_panel_chunk_at(screen,
                                 assets,
                                 3,
                                 (size_t)chunk,
                                 left + GAME_DICE_FACE_OFFSET_X,
                                 top + GAME_DICE_FACE_OFFSET_Y,
                                 1);
    }
}

void draw_game_dice_status_overlay(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   const GameViewModel *view)
{
    int x;
    int y;
    int traffic;
    int i;

    if (screen == NULL || assets == NULL || view == NULL ||
        !view->active || !view->dice.status_visible ||
        !game_asset_ready(assets, &assets->ui.has_panel[7])) {
        return;
    }
    traffic = view->dice.traffic_method;
    x = clamp_int(view->dice.status_x, 0, SCREEN_W - GAME_DICE_STATUS_W);
    y = clamp_int(view->dice.status_y, 0, SCREEN_H - GAME_DICE_STATUS_H);

    draw_game_panel_chunk(screen, assets, 7, (size_t)(view->dice.status_variant & 1), x, y, 1);
    if (traffic == 0) {
        draw_game_panel_chunk(screen, assets, 7, 7, x + 7, y + 26, 1);
    } else if (traffic == 1) {
        for (i = 0; i < 2; i++) {
            size_t chunk = (size_t)((i <= view->dice.available_dice - 1) ? (i * 2 + 7) : (i * 2 + 6));
            int icon_x = (i <= view->dice.available_dice - 1) ? x + 7 : x + 8;
            draw_game_panel_chunk(screen, assets, 7, chunk, icon_x, y + 16 + i * 19, 1);
        }
    } else if (traffic == 2) {
        for (i = 0; i < 3; i++) {
            size_t chunk = (size_t)((i <= view->dice.available_dice - 1) ? (i * 2 + 7) : (i * 2 + 6));
            int icon_x = (i <= view->dice.available_dice - 1) ? x + 7 : x + 8;
            draw_game_panel_chunk(screen, assets, 7, chunk, icon_x, y + 9 + i * 16, 1);
        }
    }
}
