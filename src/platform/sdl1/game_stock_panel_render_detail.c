#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include <SDL/SDL.h>

#include "app/game_presentation_feature_states.h"
#include "app/new_game_setup_flow.h"
#include "app/stock_interaction_flow.h"
#include "core/game_state.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "core/stock.h"
#include "platform/sdl1/game_stock_panel_names.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_stock_panel_render_internal.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

static float stock_history_at(const GameState *state, int stock, int ago)
{
    int index = state->stock_history_cursor - 1 - ago;
    while (index < 0) index += GAME_STOCK_HISTORY_COUNT;
    index %= GAME_STOCK_HISTORY_COUNT;
    return state->stock_history[stock][index];
}

static float stock_history_average(const GameState *state, int stock, int count)
{
    float sum = 0.0f;
    int i;
    for (i = 0; i < count; i++) sum += stock_history_at(state, stock, i);
    return count > 0 ? sum / (float)count : 0.0f;
}

static void stock_put_pixel(SDL_Surface *screen, int x, int y, uint16_t color)
{
    uint16_t *row;
    if (x < 0 || y < 0 || x >= screen->w || y >= screen->h) return;
    row = (uint16_t *)((unsigned char *)screen->pixels + y * screen->pitch);
    row[x] = color;
}

static void stock_draw_line(SDL_Surface *screen, int x0, int y0,
                            int x1, int y1, uint16_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        int doubled_error;
        stock_put_pixel(screen, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        doubled_error = 2 * error;
        if (doubled_error >= dy) { error += dy; x0 += sx; }
        if (doubled_error <= dx) { error += dx; y0 += sy; }
    }
}

static int stock_edge(int ax, int ay, int bx, int by, int px, int py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void stock_fill_triangle(SDL_Surface *screen,
                                int ax, int ay, int bx, int by,
                                int cx, int cy, uint16_t color)
{
    int min_x = ax < bx ? ax : bx;
    int max_x = ax > bx ? ax : bx;
    int min_y = ay < by ? ay : by;
    int max_y = ay > by ? ay : by;
    int x, y;
    if (cx < min_x) min_x = cx;
    if (cx > max_x) max_x = cx;
    if (cy < min_y) min_y = cy;
    if (cy > max_y) max_y = cy;
    for (y = min_y; y <= max_y; y++)
        for (x = min_x; x <= max_x; x++) {
            int e0 = stock_edge(ax, ay, bx, by, x, y);
            int e1 = stock_edge(bx, by, cx, cy, x, y);
            int e2 = stock_edge(cx, cy, ax, ay, x, y);
            if ((e0 >= 0 && e1 >= 0 && e2 >= 0) ||
                (e0 <= 0 && e1 <= 0 && e2 <= 0))
                stock_put_pixel(screen, x, y, color);
        }
}

static void stock_fill_ellipse(SDL_Surface *screen, int cx, int cy,
                               int rx, int ry, uint16_t color)
{
    int x, y;
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int radius2 = rx2 * ry2;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if (x * x * ry2 + y * y * rx2 <= radius2)
                stock_put_pixel(screen, cx + x, cy + y, color);
}

static void draw_stock_holding_pie(SDL_Surface *screen, int holding)
{
    static const signed char perimeter[33][2] = {
        {0,-29},{17,-28},{34,-27},{49,-24},{62,-21},{73,-16},{81,-11},{86,-6},
        {88,0},{86,6},{81,11},{73,16},{62,21},{49,24},{34,27},{17,28},
        {0,29},{-17,28},{-34,27},{-49,24},{-62,21},{-73,16},{-81,11},{-86,6},
        {-88,0},{-86,-6},{-81,-11},{-73,-16},{-62,-21},{-49,-24},{-34,-27},
        {-17,-28},{0,-29}
    };
    const uint16_t free_color = 0x001a;
    const uint16_t held_color = 0xd000;
    const uint16_t outline_color = 0x0000;
    int cx = 502, top_cy = 365, bottom_cy = 372;
    int scaled, full, remainder, i;
    if (holding < 0) holding = 0;
    if (holding > 10000) holding = 10000;
    scaled = holding * 32;
    full = scaled / 10000;
    remainder = scaled % 10000;

    /* Original 0x429D65 draws a 7-pixel-deep lower ellipse first. */
    stock_fill_ellipse(screen, cx, bottom_cy, 88, 29,
                       holding == 10000 ? held_color : free_color);

    /* Colour the visible front side for the held arc (right to left). */
    for (i = 8; i < 24 && i < full; i++) {
        stock_fill_triangle(
            screen,
            cx + perimeter[i][0], top_cy + perimeter[i][1],
            cx + perimeter[i + 1][0], top_cy + perimeter[i + 1][1],
            cx + perimeter[i + 1][0], bottom_cy + perimeter[i + 1][1],
            held_color);
        stock_fill_triangle(
            screen,
            cx + perimeter[i][0], top_cy + perimeter[i][1],
            cx + perimeter[i + 1][0], bottom_cy + perimeter[i + 1][1],
            cx + perimeter[i][0], bottom_cy + perimeter[i][1],
            held_color);
    }
    if (full >= 8 && full < 24 && remainder > 0) {
        int px = perimeter[full][0] +
            (perimeter[full + 1][0] - perimeter[full][0]) * remainder /
                10000;
        int py = perimeter[full][1] +
            (perimeter[full + 1][1] - perimeter[full][1]) * remainder /
                10000;
        stock_fill_triangle(
            screen,
            cx + perimeter[full][0], top_cy + perimeter[full][1],
            cx + px, top_cy + py,
            cx + px, bottom_cy + py, held_color);
        stock_fill_triangle(
            screen,
            cx + perimeter[full][0], top_cy + perimeter[full][1],
            cx + px, bottom_cy + py,
            cx + perimeter[full][0], bottom_cy + perimeter[full][1],
            held_color);
    }

    stock_fill_ellipse(screen, cx, top_cy, 88, 29, free_color);
    for (i = 0; i < full && i < 32; i++)
        stock_fill_triangle(
            screen, cx, top_cy,
            cx + perimeter[i][0], top_cy + perimeter[i][1],
            cx + perimeter[i + 1][0], top_cy + perimeter[i + 1][1],
            held_color);
    if (full < 32 && remainder > 0) {
        int px = perimeter[full][0] +
            (perimeter[full + 1][0] - perimeter[full][0]) * remainder / 10000;
        int py = perimeter[full][1] +
            (perimeter[full + 1][1] - perimeter[full][1]) * remainder / 10000;
        stock_fill_triangle(
            screen, cx, top_cy,
            cx + perimeter[full][0], top_cy + perimeter[full][1],
            cx + px, top_cy + py, held_color);
    }

    for (i = 0; i < 32; i++)
        stock_draw_line(
            screen,
            cx + perimeter[i][0], top_cy + perimeter[i][1],
            cx + perimeter[i + 1][0], top_cy + perimeter[i + 1][1],
            outline_color);
    for (i = 8; i < 24; i++)
        stock_draw_line(
            screen,
            cx + perimeter[i][0], bottom_cy + perimeter[i][1],
            cx + perimeter[i + 1][0], bottom_cy + perimeter[i + 1][1],
            outline_color);
    stock_draw_line(screen, cx + perimeter[8][0], top_cy,
                    cx + perimeter[8][0], bottom_cy, outline_color);
    stock_draw_line(screen, cx + perimeter[24][0], top_cy,
                    cx + perimeter[24][0], bottom_cy, outline_color);
    if (holding > 0 && holding < 10000) {
        int px = perimeter[full][0];
        int py = perimeter[full][1];
        if (remainder > 0) {
            px += (perimeter[full + 1][0] - perimeter[full][0]) *
                remainder / 10000;
            py += (perimeter[full + 1][1] - perimeter[full][1]) *
                remainder / 10000;
        }
        stock_draw_line(screen, cx, top_cy, cx, top_cy - 29,
                        outline_color);
        stock_draw_line(screen, cx, top_cy, cx + px, top_cy + py,
                        outline_color);
        if (py >= 0)
            stock_draw_line(screen, cx + px, top_cy + py,
                            cx + px, bottom_cy + py, outline_color);
    }
}

static void draw_stock_detail_page(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   const NewGameSettings *settings,
                                   const GameState *state,
                                   const GameStockInteractionPresentationState *interaction)
{
    static const unsigned char company_icon[96] = {
        11,0,7,5,6,9,3,0,0,4,10,0, 11,0,7,5,6,9,3,0,0,4,10,0,
        11,0,8,5,6,9,3,0,0,4,10,0, 11,0,7,5,6,9,3,0,0,4,10,0,
        11,0,7,12,6,9,3,0,0,4,10,0, 11,23,7,14,6,9,3,0,0,4,10,15,
        11,0,7,13,6,9,3,0,0,4,10,0, 11,24,7,5,6,9,3,0,0,4,10,0
    };
    int stock_id = interaction->selected_stock;
    const GameStockInfo *stock = &state->stocks[stock_id];
    int current_player = wrap_index(state->current_player, state->player_count);
    int map_index = game_stock_panel_map_index(settings);
    int holding = state->player_stock_amount[current_player][stock_id];
    float delta = stock->f20 - stock->f16;
    float week = stock_history_average(state, stock_id, 6);
    float month = stock_history_average(state, stock_id, 24);
    float high = 0.0f, low = 0.0f;
    int i, previous_x = 0, previous_y = 0;
    char text[64];
    game_stock_draw_quote_page(screen, assets, settings, state, interaction);
    draw_game_panel_chunk(screen, assets, 75, 2, 26, 52, 0);
    draw_big5_text(screen, 320, 82, game_stock_panel_name(settings, stock_id), 20,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    for (i = 0; i < GAME_STOCK_HISTORY_COUNT; i++) {
        float value = stock_history_at(state, stock_id, i);
        if (value <= 0.0f) continue;
        if (high == 0.0f || value > high) high = value;
        if (low == 0.0f || value < low) low = value;
    }
    if (high == 0.0f) high = stock->f20;
    if (low == 0.0f) low = stock->f20;
    draw_big5_text(screen, 395, 123, "\xa6\xa8\xa5\xe6\xbb\xf9", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 395, 162, "\xba\xa6\x20\x20\xb6\x5e", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 531, 123, "\xa5\xe6\xa9\xf6\xb6\x71", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 531, 162, "\xba\xa6\xb6\x5e\xb4\x54", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    game_stock_format_price(text, sizeof(text), stock->f20, stock->f20);
    draw_big5_text(screen, 453, 123, text, 16,
                   game_stock_price_color(rich4_stock_price_state(stock->f16, stock->f20)),
                   RICH4_FONT_DARK, 2, 6);
    game_stock_format_price(text, sizeof(text), delta, stock->f20);
    draw_big5_text(screen, 453, 162, text, 16,
                   game_stock_price_color(rich4_stock_price_state(stock->f16, stock->f20)),
                   RICH4_FONT_DARK, 2, 6);
    format_hud_number(text, sizeof(text), (int)stock->f10, 0);
    draw_big5_text(screen, 589, 123, text, 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    snprintf(text, sizeof(text), "%.2f%%",
             stock->f16 != 0.0f ? delta * 100.0f / stock->f16 : 0.0f);
    draw_big5_text(screen, 589, 162, text, 16,
                   game_stock_price_color(rich4_stock_price_state(stock->f16, stock->f20)),
                   RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 395, 203, "\xb6\x67\xa7\xa1\xbb\xf9", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 531, 203, "\xa4\xeb\xa7\xa1\xbb\xf9", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 531, 243, "\xbe\xfa\xa5\x76\xb0\xaa\xbb\xf9", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 531, 283, "\xbe\xfa\xa5\x76\xa7\x43\xbb\xf9", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    game_stock_format_price(text, sizeof(text), week, stock->f20);
    draw_big5_text(screen, 453, 203, text, 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    game_stock_format_price(text, sizeof(text), month, stock->f20);
    draw_big5_text(screen, 589, 203, text, 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    game_stock_format_price(text, sizeof(text), high, high);
    draw_big5_text(screen, 589, 243, text, 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    game_stock_format_price(text, sizeof(text), low, low);
    draw_big5_text(screen, 589, 283, text, 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);

    draw_big5_text(screen, 212, 123, "\xa5\xbb\xa4\xeb\xac\xd5\xbe\x6c", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 212, 162, "\xa5\xad\xa7\xa1\xac\xd5\xbe\x6c", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_big5_text(screen, 212, 203, "\xb8\x67\x20\xc0\xe7\x20\xaa\xcc", 14,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    if (stock->f4 != 0 && state->stock_company_auxiliary[stock_id] >= 0) {
        int link = state->stock_company_auxiliary[stock_id];
        int type = link < state->company_count ? state->company_type[link] : 0;
        int icon = type >= 1 && type <= 12
            ? company_icon[map_index * 12 + type - 1] : 0;
        int months = rich4_stock_elapsed_months(state);
        if (icon == 15 || icon == 24) {
            const MapData *map = &assets->map.maps[map_index];
            if ((size_t)(link + 1) < map->auxiliary_count) {
                int variant = map->auxiliary[link + 1].variant;
                icon += icon == 15 ? variant - 3 : variant - 2;
            }
        }
        if (icon >= 3 && icon <= 28)
            draw_game_panel_chunk(screen, assets, 75, (size_t)icon,
                                  50, 107, 1);
        format_hud_number(
            text, sizeof(text),
            link < state->company_count
                ? (int)state->company_period_business[link] : 0,
            0);
        draw_big5_text(screen, 309, 123, text, 16,
                       RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
        format_hud_number(
            text, sizeof(text),
            link < state->company_count
                ? (int)(months > 0
                    ? state->company_total_business[link] /
                        (unsigned int)months
                    : state->company_total_business[link])
                : 0,
            0);
        draw_big5_text(screen, 309, 162, text, 16,
                       RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
        if (state->stock_company_owner[stock_id] >= 0) {
            int owner = state->stock_company_owner[stock_id];
            draw_big5_text(
                screen, 269, 203,
                new_game_character_name(state->players[owner].character), 16,
                game_player_color(owner), RICH4_FONT_DARK, 2, 6);
        }
    }

    draw_big5_text(screen, 236, 248,
                   "\xa5\x62\xa6\x7e\xa4\xba\xaa\xd1\xbb\xf9\xa8\xab\xb6\xd5\xbd\x75\xb9\xcf",
                   14, RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    if (high > low) {
        float scale = 120.0f / (high - low);
        float center = (high + low) * 0.5f;
        for (i = 0; i < GAME_STOCK_HISTORY_COUNT; i++) {
            int history_index = GAME_STOCK_HISTORY_COUNT - 1 - i;
            float value = stock_history_at(state, stock_id, history_index);
            int x = 92 + i * 2;
            int y = 324 - (int)((value - center) * scale);
            if (y < 264) y = 264;
            if (y > 384) y = 384;
            if (i > 0) stock_draw_line(
                screen, previous_x, previous_y, x, y, 0xffff);
            previous_x = x;
            previous_y = y;
        }
    }
    game_stock_format_price(text, sizeof(text), high, high);
    draw_big5_text(screen, 88, 264, text, 12,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    game_stock_format_price(text, sizeof(text), low, low);
    draw_big5_text(screen, 88, 384, text, 12,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
    draw_stock_holding_pie(screen, holding);
}

static void draw_stock_quantity_panel(SDL_Surface *screen,
                                      const GamePreviewAssets *assets,
                                      const GameStockInteractionPresentationState *state)
{
    char text[48];
    Rich4SprSmpChunk bar;
    Rich4SprSmpChunk fill;
    int bar_width;
    if (!state->quantity_active) return;
    draw_game_panel_chunk(screen, assets, 21, 0, 256, 144, 1);
    snprintf(text, sizeof(text), "%d", state->quantity);
    draw_big5_text(screen, 376, 160, text, 18,
                   RICH4_FONT_DARK, RICH4_FONT_DARK, 2, 6);
    if (assets == NULL || !game_asset_ready(assets, &assets->ui.has_panel[21]) ||
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[21], assets->ui.panel_size[21],
            &assets->ui.panel_spr[21], 1, &bar) != RICH4_SPR_SMP_OK)
        return;
    /* Panel21 chunk 1 is the complete track.  The original overlays the
     * selected prefix from chunk 0, whose 128x192 image contains the filled
     * bar artwork at local (10,42). */
    draw_game_panel_chunk(screen, assets, 21, 1, 266, 186, 1);
    bar_width = rich4_app_stock_quantity_bar_width(
        state->quantity, state->maximum);
    if (bar_width > 0 &&
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[21], assets->ui.panel_size[21],
            &assets->ui.panel_spr[21], 0, &fill) == RICH4_SPR_SMP_OK)
        draw_spr_chunk_crop_topleft(
            screen, assets->ui.panel_data[21], assets->ui.panel_size[21],
            &assets->ui.panel_spr[21], &fill, 10, 42, bar_width, bar.height,
            266, 186, 1);
}

static int stock_pressed_control_rect(int control, int *left, int *top,
                                      int *right, int *bottom)
{
    static const int rects[5][4] = {
        {16, 9, 124, 39},
        {128, 9, 198, 39},
        {202, 9, 272, 39},
        {276, 9, 410, 39},
        {552, 8, 623, 40}
    };
    int index = control - RICH4_STOCK_HIT_TOGGLE;
    if (index < 0 || index >= 5 || left == NULL || top == NULL ||
        right == NULL || bottom == NULL)
        return 0;
    *left = rects[index][0];
    *top = rects[index][1];
    *right = rects[index][2];
    *bottom = rects[index][3];
    return 1;
}

static void draw_stock_pressed_control(SDL_Surface *screen, int control)
{
    int left, top, right, bottom;
    int x, y;
    if (screen == NULL ||
        !stock_pressed_control_rect(
            control, &left, &top, &right, &bottom))
        return;
    /* sub_451B9E shifts the button interior down/right by one pixel and
     * replaces its top and left edge with the inset border. */
    for (y = bottom - 2; y >= top; y--) {
        uint16_t *source = (uint16_t *)
            ((unsigned char *)screen->pixels + y * screen->pitch);
        uint16_t *destination = (uint16_t *)
            ((unsigned char *)screen->pixels + (y + 1) * screen->pitch);
        for (x = right - 2; x >= left; x--)
            destination[x + 1] = source[x];
    }
    for (x = left; x < right; x++) stock_put_pixel(screen, x, top, 0x0000);
    for (y = top; y < bottom; y++) stock_put_pixel(screen, left, y, 0x0000);
}

static void draw_stock_message(SDL_Surface *screen,
                               const GameStockInteractionPresentationState *state)
{
    const char *message;
    if (state->message == RICH4_STOCK_MESSAGE_LIMIT_UP)
        message = "\xba\xa6\xb0\xb1\xb5\x4c\xaa\x6b\xb6\x52\xb6\x69\xa1\x49";
    else if (state->message == RICH4_STOCK_MESSAGE_LIMIT_DOWN)
        message = "\xb6\x5e\xb0\xb1\xb5\x4c\xaa\x6b\xbd\xe6\xa5\x58\xa1\x49";
    else
        return;
    fill_rect(screen, 176, 202, 288, 76, 0x2104);
    stroke_rect(screen, 176, 202, 288, 76, 0xffff);
    draw_big5_text(screen, 320, 240, message, 20,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
}

static void draw_stock_closed_message(SDL_Surface *screen)
{
    static const char message[] = "\xa5\xbb\xa4\xe9\xa5\xf0\xa5\xab";

    /* sub_42B58F draws the large message twice: a one-pixel dark offset,
     * followed by the light foreground. */
    draw_big5_text(screen, 324, 244, message, 72,
                   rich4_character_rgb565_color(0x00101010u), 0, 2, 2);
    draw_big5_text(screen, 320, 240, message, 72,
                   rich4_character_rgb565_color(0x00f0f0f0u), 0, 2, 2);
}

static void render_stock_panel_contents(SDL_Surface *screen,
                                        const GamePreviewAssets *assets,
                                        const NewGameSettings *settings,
                                        const GameState *state,
                                        const GameStockInteractionPresentationState *interaction)
{
    if (state == NULL || interaction == NULL || !interaction->active) return;
    if (rich4_stock_market_is_closed(state)) {
        draw_game_panel_chunk(screen, assets, 75, 0, 0, 0, 0);
        game_stock_draw_common_header(screen, state, RICH4_STOCK_PANEL_QUOTES);
        draw_stock_closed_message(screen);
        return;
    }
    if (interaction->page == RICH4_STOCK_PANEL_HOLDINGS)
        game_stock_draw_holdings_page(screen, assets, settings, state, interaction);
    else if (interaction->page == RICH4_STOCK_PANEL_DETAIL)
        draw_stock_detail_page(screen, assets, settings, state, interaction);
    else
        game_stock_draw_quote_page(screen, assets, settings, state, interaction);
    draw_stock_pressed_control(screen, interaction->pressed_control);
    draw_stock_quantity_panel(screen, assets, interaction);
    draw_stock_message(screen, interaction);
}

static int stock_render_cache_matches(SDL_Surface *screen,
                                      SDL_Surface *cache)
{
    if (screen == NULL || cache == NULL || screen->format == NULL ||
        cache->format == NULL)
        return 0;
    return cache->w == screen->w && cache->h == screen->h &&
           cache->format->BitsPerPixel == screen->format->BitsPerPixel &&
           cache->format->Rmask == screen->format->Rmask &&
           cache->format->Gmask == screen->format->Gmask &&
           cache->format->Bmask == screen->format->Bmask &&
           cache->format->Amask == screen->format->Amask;
}

void game_draw_stock_panel(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const NewGameSettings *settings,
                           const GameState *game_state,
                           const GameStockInteractionPresentationState *state,
                           StockPanelState *panel)
{
    if (panel == NULL || game_state == NULL || state == NULL ||
        !state->active) return;
    if (!stock_render_cache_matches(screen, panel->render_cache)) {
        if (panel->render_cache != NULL)
            SDL_FreeSurface(panel->render_cache);
        panel->render_cache = create_game_work_surface(screen);
        panel->render_dirty = 1;
    }
    if (panel->render_cache == NULL) {
        render_stock_panel_contents(
            screen, assets, settings, game_state, state);
        return;
    }
    if (panel->render_dirty) {
        render_stock_panel_contents(
            panel->render_cache, assets, settings, game_state, state);
        panel->render_dirty = 0;
    }
    SDL_BlitSurface(panel->render_cache, NULL, screen, NULL);
}
