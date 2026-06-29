#include <stdio.h>
#include <SDL/SDL.h>

#include "app/game_presentation_feature_states.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "core/game_state.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_stock_panel_names.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_stock_panel_render_internal.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "view/view_constants.h"

static int stock_price_decimals(float price)
{
    if (price < 15.0f) return 2;
    if (price < 150.0f) return 1;
    return 0;
}

void game_stock_format_price(char *out, size_t out_size, float value,
                               float band_price)
{
    snprintf(out, out_size, "%.*f", stock_price_decimals(band_price), value);
}

uint16_t game_stock_price_color(GameStockPriceState state)
{
    if (state == GAME_STOCK_PRICE_RISE ||
        state == GAME_STOCK_PRICE_LIMIT_UP) return 0xf800;
    if (state == GAME_STOCK_PRICE_FALL ||
        state == GAME_STOCK_PRICE_LIMIT_DOWN) return 0x07e0;
    return RICH4_FONT_LIGHT;
}

void game_stock_draw_common_header(SDL_Surface *screen,
                                     const GameState *state,
                                     Rich4StockPanelPage page)
{
    char bank[48];
    int player = wrap_index(state->current_player, state->player_count);
    draw_big5_text(
        screen, 70, 24,
        page == RICH4_STOCK_PANEL_HOLDINGS
            ? "\xaa\xd1\x20\xbb\xf9\x20\xaa\xed"
            : "\xab\xf9\xa6\xb3\xaa\xd1\xbc\xc6\xaa\xed",
        16, RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 163, 24, "\xb6\x52\xb6\x69", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 237, 24, "\xbd\xe6\xa5\x58", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(
        screen, 343, 24,
        "\xa4\x57\xa5\xab\xa4\xbd\xa5\x71\xb8\xea\xb0\x54", 16,
        RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    format_hud_number(bank, sizeof(bank),
                      state->players[player].bank, 1);
    draw_big5_text(screen, 540, 24, bank, 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
}

void game_stock_draw_quote_page(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  const NewGameSettings *settings,
                                  const GameState *state,
                                  const GameStockInteractionPresentationState *interaction)
{
    int player = wrap_index(state->current_player, state->player_count);
    int i;
    draw_game_panel_chunk(screen, assets, 75, 0, 0, 0, 0);
    game_stock_draw_common_header(screen, state, RICH4_STOCK_PANEL_QUOTES);
    draw_big5_text(screen, 76, 64, "\xaa\xd1\xb2\xbc\xa6\x57\xba\xd9", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 188, 64, "\xa6\xa8\xa5\xe6\xbb\xf9", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 280, 64, "\xba\xa6\xb6\x5e", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 368, 64, "\xa5\xe6\xa9\xf6\xb6\x71", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 468, 64, "\xab\xf9\xa6\xb3\xaa\xd1\xbc\xc6", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 572, 64, "\xa5\xad\xa7\xa1\xa6\xa8\xa5\xbb", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        const GameStockInfo *stock = &state->stocks[i];
        GameStockPriceState price_state =
            rich4_stock_price_state(stock->f16, stock->f20);
        uint16_t color = game_stock_price_color(price_state);
        uint16_t name_color = stock->f4 != 0 ? 0x07ff : RICH4_FONT_LIGHT;
        int y = 96 + 32 * i;
        int holding = state->player_stock_amount[player][i];
        char text[48];
        if (interaction->selected_stock == i)
            stroke_rect_thin(screen, 15, 80 + 32 * i, 610, 32, 0xffff);
        if (price_state == GAME_STOCK_PRICE_LIMIT_UP)
            fill_rect(screen, 144, y - 10, 89, 20, 0x6000);
        else if (price_state == GAME_STOCK_PRICE_LIMIT_DOWN)
            fill_rect(screen, 144, y - 10, 89, 20, 0x0320);
        draw_big5_text(screen, 76, y, game_stock_panel_name(settings, i), 16,
                       name_color, RICH4_FONT_DARK, 2, 2);
        game_stock_format_price(text, sizeof(text), stock->f20, stock->f20);
        draw_big5_text(screen, 225, y, text, 16, color,
                       RICH4_FONT_DARK, 2, 6);
        game_stock_format_price(text, sizeof(text), stock->f20 - stock->f16,
                           stock->f20);
        draw_big5_text(screen, 305, y, text, 16, color,
                       RICH4_FONT_DARK, 2, 6);
        if (stock->f6 != 0) {
            draw_big5_text(screen, 401, y,
                           "\xbc\xc8\xb0\xb1\xa5\xe6\xa9\xf6", 16,
                           RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
        } else {
            snprintf(text, sizeof(text), "%u", (unsigned int)stock->f10);
            draw_big5_text(screen, 401, y, text, 16,
                           RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
        }
        if (holding > 0) {
            format_hud_number(text, sizeof(text), holding, 0);
            draw_big5_text(screen, 504, y, text, 16,
                           RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
            snprintf(text, sizeof(text), "%.2f",
                     state->player_stock_average_cost[player][i]);
            draw_big5_text(screen, 609, y, text, 16,
                           RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
        }
    }
}

void game_stock_draw_holdings_page(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     const NewGameSettings *settings,
                                     const GameState *state,
                                     const GameStockInteractionPresentationState *interaction)
{
    int i, player;
    draw_game_panel_chunk(screen, assets, 75, 1, 0, 0, 0);
    game_stock_draw_common_header(screen, state, RICH4_STOCK_PANEL_HOLDINGS);
    draw_big5_text(screen, 71, 64, "\xaa\xd1\xb2\xbc\xa6\x57\xba\xd9", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    for (player = 0; player < state->player_count; player++)
        draw_big5_text(
            screen, 168 + 80 * player, 64,
            new_game_character_name(state->players[player].character), 16,
            player == state->current_player
                ? game_player_color(player) : RICH4_FONT_LIGHT,
            RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 492, 64, "\xab\x4f\xaf\x64\xaa\xd1\xa5\xf7", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, 580, 64, "\xb2\xd6\xbf\x6e\xac\xd5\xbe\x6c", 16,
                   RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 2);
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        int y = 96 + 32 * i;
        char text[48];
        if (interaction->selected_stock == i)
            stroke_rect_thin(screen, 15, 80 + 32 * i, 610, 32, 0xffff);
        draw_big5_text(screen, 71, y, game_stock_panel_name(settings, i), 16,
                       state->stocks[i].f4 != 0 ? 0x07ff : RICH4_FONT_LIGHT,
                       RICH4_FONT_DARK, 2, 2);
        for (player = 0; player < state->player_count; player++) {
            int amount = state->player_stock_amount[player][i];
            if (amount <= 0) continue;
            format_hud_number(text, sizeof(text), amount, 0);
            draw_big5_text(
                screen, 192 + 80 * player, y, text, 16,
                state->stock_company_owner[i] == player
                    ? game_player_color(player) : RICH4_FONT_LIGHT,
                RICH4_FONT_DARK, 2, 6);
        }
        if (state->stocks[i].f4 != 0 &&
            state->stock_company_auxiliary[i] >= 0) {
            int link = state->stock_company_auxiliary[i];
            format_hud_number(text, sizeof(text),
                              (int)state->stock_company_available[i], 0);
            draw_big5_text(screen, 520, y, text, 16,
                           RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
            format_hud_number(
                text, sizeof(text),
                link < state->company_count
                    ? (int)state->company_period_business[link] : 0,
                0);
            draw_big5_text(screen, 609, y, text, 16,
                           RICH4_FONT_LIGHT, RICH4_FONT_DARK, 2, 6);
        }
    }
}
