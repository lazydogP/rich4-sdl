#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "app/point_minigame_flow.h"

#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_point_minigame_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"

static int point_minigame_catcher_draw_x(
    const GamePointMinigameObjectView *object)
{
    float progress;
    if (object == NULL) return 0;
    progress = (float)((double)(object->y > 130 ? object->y - 130 : 0) / 250.0);
    return object->x + (int)((double)object->velocity_x * progress);
}

static void draw_point_minigame_background(
    SDL_Surface *screen, const GamePreviewAssets *assets, int action)
{
    if (action == 6) {
        draw_game_panel_chunk(screen, assets, 80, 0, 0, 0, 0);
    } else if (action == 7) {
        draw_game_panel_chunk(screen, assets, 91, 0, 0, 0, 0);
    } else if (action == 8) {
        draw_raw_rgb565_fullscreen(
            screen, assets->ui.panel_data[92], assets->ui.panel_size[92]);
    }
}

static void draw_point_minigame_digit(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int digit, int x, int y, int large)
{
    if (digit < 0 || digit > 9) return;
    draw_game_panel_chunk_at(
        screen, assets, 79, (size_t)(digit + (large ? 10 : 0)),
        x, y, large ? 1 : 0);
}

static void draw_point_minigame_digits(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int value, int digits, int x, int y, int advance, int large)
{
    char text[16];
    int i;
    if (digits < 1) digits = 1;
    if (digits > 9) digits = 9;
    snprintf(text, sizeof(text), "%0*d", digits, value);
    for (i = 0; i < digits && text[i] != '\0'; i++)
        draw_point_minigame_digit(
            screen, assets, text[i] - '0', x + i * advance, y, large);
}

static void draw_point_minigame_timer(
    SDL_Surface *screen, const GamePreviewAssets *assets, int time)
{
    static const int digit_x[] = {49, 69, 94};
    char text[16];
    int i;
    if (time < 0) time = 0;
    snprintf(text, sizeof(text), "%03d", time);
    for (i = 0; i < 3; i++)
        draw_point_minigame_digit(
            screen, assets, text[i] - '0', digit_x[i], 421, 0);
    draw_point_minigame_digit(screen, assets, 0, 114, 421, 0);
}

static void draw_point_minigame_hud(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GamePointMinigameRenderState *game)
{
    int time;
    if (screen == NULL || assets == NULL || game == NULL) return;
    time = game->action == 8
        ? game->ticks_remaining / 2
        : game->ticks_remaining;
    draw_point_minigame_timer(screen, assets, time);
    if (game->action == 6) {
        draw_point_minigame_digits(
            screen, assets, game->counters[1], 2, 185, 421, 20, 0);
        draw_point_minigame_digits(
            screen, assets, game->counters[2], 2, 276, 421, 20, 0);
        draw_point_minigame_digits(
            screen, assets, game->counters[3], 2, 367, 421, 20, 0);
        draw_point_minigame_digits(
            screen, assets, game->counters[4], 2, 458, 421, 20, 0);
    } else if (game->action == 8) {
        draw_point_minigame_digits(
            screen, assets, game->counters[0], 2, 185, 421, 20, 0);
        draw_point_minigame_digits(
            screen, assets, game->counters[1], 2, 276, 421, 20, 0);
        draw_point_minigame_digits(
            screen, assets, game->counters[2], 2, 367, 421, 20, 0);
        draw_point_minigame_digits(
            screen, assets, game->counters[3], 2, 458, 421, 20, 0);
    }
    if (game->action == 7) {
        draw_point_minigame_digits(
            screen, assets, game->score, 4, 529, 421, 20, 0);
    } else {
        draw_point_minigame_digits(
            screen, assets, game->score, 3, 549, 421, 20, 0);
    }
}

static void draw_point_minigame_result(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int score)
{
    char text[16];
    int length;
    int x;
    int i;
    snprintf(text, sizeof(text), "%d", score);
    length = (int)strlen(text);
    x = 353 - 33 * length;
    for (i = 0; i < length; i++, x += 66)
        draw_game_panel_chunk_at(
            screen, assets, 79,
            (size_t)(10 + text[i] - '0'), x, 150, 1);
}

static void draw_penguin_minigame(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GamePointMinigameRenderState *game)
{
    int cell;
    int player_x;
    int player_y;
    draw_point_minigame_background(screen, assets, game->action);
    for (cell = 0; cell < 81; cell++) {
        int type = game->penguin_treasure[cell];
        int x;
        int y;
        if (type <= 0) continue;
        rich4_point_minigame_cell_xy(cell, &x, &y);
        draw_game_panel_chunk_at(
            screen, assets, 80, (size_t)(type + 3), x, y, 1);
    }
    rich4_point_minigame_cell_xy(
        game->penguin_player_cell, &player_x, &player_y);
    if (game->penguin_target_cell >= 0 &&
        game->penguin_player_cell != game->penguin_target_cell) {
        int target_x;
        int target_y;
        int frame = game->penguin_move_frame;
        int current_column = game->penguin_player_cell % 9;
        int current_row = game->penguin_player_cell / 9;
        int next_column = game->penguin_next_cell % 9;
        int next_row = game->penguin_next_cell / 9;
        int column_delta = next_column - current_column;
        int row_delta = next_row - current_row;
        int direction;
        int chunk;
        rich4_point_minigame_cell_xy(
            game->penguin_next_cell, &target_x, &target_y);
        player_x += (target_x - player_x) * frame / 4;
        player_y += (target_y - player_y) * frame / 4;
        if (column_delta <= 0) {
            if (column_delta == 0)
                direction = row_delta <= 0 ? 5 : 1;
            else
                direction = (row_delta + 7) & 7;
        } else {
            direction = 3 - row_delta;
        }
        chunk = 4 * direction + frame;
        if (chunk >= (int)assets->ui.panel_spr[82].num_chunks)
            chunk = frame;
        draw_game_panel_chunk_at(
            screen, assets, 82, (size_t)chunk,
            player_x, player_y, 1);
    } else if (game->penguin_target_cell >= 0 &&
               game->penguin_dig_frame < 4) {
        rich4_point_minigame_cell_xy(
            game->penguin_target_cell, &player_x, &player_y);
        draw_game_panel_chunk_at(
            screen, assets, 83,
            (size_t)(game->penguin_dig_frame %
                (int)assets->ui.panel_spr[83].num_chunks),
            player_x, player_y, 1);
    } else if (game->penguin_pickup_type > 0) {
        int resource = 85 + game->penguin_pickup_type;
        rich4_point_minigame_cell_xy(
            game->penguin_target_cell, &player_x, &player_y);
        draw_game_panel_chunk_at(
            screen, assets, resource,
            (size_t)(game->penguin_pickup_frame %
                (int)assets->ui.panel_spr[resource].num_chunks),
            player_x, player_y, 1);
    } else if (game->phase == RICH4_POINT_MINIGAME_RESULT) {
        int resource = 80;
        size_t chunk = 1;
        if (game->result_tier == 4) {
            resource = 84;
            chunk = (size_t)(game->animation_frame %
                (int)assets->ui.panel_spr[resource].num_chunks);
        } else if (game->result_tier == 5) {
            resource = 85;
            chunk = (size_t)(game->animation_frame %
                (int)assets->ui.panel_spr[resource].num_chunks);
        }
        draw_game_panel_chunk_at(
            screen, assets, resource,
            chunk,
            player_x, player_y, 1);
    } else {
        draw_game_panel_chunk_at(screen, assets, 80, 1,
                                 player_x, player_y, 1);
    }
}

static void draw_balloon_minigame(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GamePointMinigameRenderState *game)
{
    int i;
    draw_point_minigame_background(screen, assets, game->action);
    for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++) {
        const GamePointMinigameObjectView *object = &game->object[i];
        if (!object->active) continue;
        draw_game_panel_chunk_at(
            screen, assets, 91, (size_t)(object->type + 1),
            object->x, object->y, 1);
    }
}

static void draw_catcher_minigame(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GamePointMinigameRenderView *view)
{
    const GamePointMinigameRenderState *game = &view->game;
    int i;
    int catcher_resource = 100 + view->player_character;
    int catcher_chunk = game->result_tier;
    draw_point_minigame_background(screen, assets, game->action);
    /* 0x413248 draws objects, dragon, deity, then the player. */
    for (i = 0; i < GAME_POINT_MINIGAME_OBJECT_COUNT; i++) {
        const GamePointMinigameObjectView *object = &game->object[i];
        Rich4SprSmpChunk chunk;
        int resource = 95 + object->type;
        float progress;
        unsigned int scale;
        if (!object->active || resource < 95 || resource > 99 ||
            !game_asset_ready(assets, &assets->ui.has_panel[resource])) continue;
        if (rich4_spr_smp_get_chunk(assets->ui.panel_data[resource],
                assets->ui.panel_size[resource], &assets->ui.panel_spr[resource],
                (size_t)object->frame, &chunk) != RICH4_SPR_SMP_OK) continue;
        progress = (float)((double)(object->y > 130 ? object->y - 130 : 0) / 250.0);
        scale = (unsigned int)((double)progress * 32768.0 + 32768.0);
        draw_spr_chunk_scaled(screen, assets->ui.panel_data[resource],
            assets->ui.panel_size[resource], &assets->ui.panel_spr[resource],
            &chunk, point_minigame_catcher_draw_x(object), object->y, scale);
    }
    if (game->catcher_secondary_active && game_asset_ready(assets, &assets->ui.has_panel[94]))
        draw_game_panel_chunk_at(screen, assets, 94,
            (size_t)game->catcher_secondary_frame, game->catcher_secondary_x, 125, 1);
    if (game_asset_ready(assets, &assets->ui.has_panel[93]))
        draw_game_panel_chunk_at(screen, assets, 93,
            (size_t)game->catcher_source_chunk, game->catcher_source_x, 126, 1);
    if (catcher_resource >= 100 && catcher_resource < GAME_PANEL_RESOURCE_COUNT &&
        game_asset_ready(assets, &assets->ui.has_panel[catcher_resource])) {
        int chunks = (int)assets->ui.panel_spr[catcher_resource].num_chunks;
        int move_frames = chunks > 5 ? (chunks - 5) / 2 : 0;
        if (game->catcher_direction > 0 && move_frames > 0)
            catcher_chunk = 5 + (game->catcher_direction - 1) * move_frames +
                game->catcher_frame % move_frames;
        if (catcher_chunk < 0 || catcher_chunk >= chunks) catcher_chunk = 0;
        draw_game_panel_chunk_at(screen, assets, catcher_resource,
            (size_t)catcher_chunk, game->catcher_x, 380, 1);
    }
    if (game->catcher_bomb_active && game->catcher_explosion_frame >= 0 &&
        game_asset_ready(assets, &assets->ui.has_point_minigame_effect))
        draw_game_graph_frame_data(screen, assets->ui.point_minigame_effect_data,
            assets->ui.point_minigame_effect_size, game->catcher_explosion_frame,
            game->catcher_x - 55, 295);
}

void draw_point_minigame(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GamePointMinigameRenderView *view)
{
    const GamePointMinigameRenderState *game;
    if (screen == NULL || assets == NULL || view == NULL ||
        !view->game.active)
        return;
    game = &view->game;
    if (game->automatic) {
        static const char result_format[] =
            "\xb1\x6f\xc2\x49\xa8\xe9%d\xc2\x49";
        char message[32];
        Rich4SprSmpChunk background;
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(
                assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5,
                &background) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->ui.common_dialog_data,
                          &background, 220, 140, 1);
        snprintf(message, sizeof(message), result_format, game->score);
        draw_big5_text(
            screen, 220, 140, message, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
        return;
    }
    if (game->phase == RICH4_POINT_MINIGAME_INTRO ||
        game->phase == RICH4_POINT_MINIGAME_READY) {
        if (game->action == 8) draw_catcher_minigame(screen, assets, view);
        else draw_point_minigame_background(screen, assets, game->action);
        if (game->phase == RICH4_POINT_MINIGAME_INTRO &&
            game_asset_ready(assets, &assets->ui.has_panel_graph[78]))
            draw_game_graph_frame_data(
                screen, assets->ui.panel_data[78], assets->ui.panel_size[78],
                game->animation_frame, 0, 0);
        draw_point_minigame_hud(screen, assets, game);
        return;
    }
    if (game->action == 6)
        draw_penguin_minigame(screen, assets, game);
    else if (game->action == 7)
        draw_balloon_minigame(screen, assets, game);
    else
        draw_catcher_minigame(screen, assets, view);
    if (game->phase == RICH4_POINT_MINIGAME_RESULT)
        draw_point_minigame_result(screen, assets, game->score);
    draw_point_minigame_hud(screen, assets, game);
}
