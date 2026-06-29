#ifndef RICH4_PLATFORM_SDL1_GAME_STOCK_PANEL_RENDER_INTERNAL_H
#define RICH4_PLATFORM_SDL1_GAME_STOCK_PANEL_RENDER_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <SDL/SDL.h>

#include "app/game_presentation_feature_states.h"
#include "app/new_game_setup_flow.h"
#include "core/game_state.h"
#include "platform/sdl1/app_asset_types.h"
#include "core/stock.h"

void game_stock_draw_common_header(SDL_Surface *screen,
                                   const GameState *state,
                                   Rich4StockPanelPage page);
void game_stock_draw_quote_page(SDL_Surface *screen,
                               const GamePreviewAssets *assets,
                               const NewGameSettings *settings,
                               const GameState *state,
                               const GameStockInteractionPresentationState *interaction);
void game_stock_draw_holdings_page(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  const NewGameSettings *settings,
                                  const GameState *state,
                                  const GameStockInteractionPresentationState *interaction);
void game_stock_format_price(char *out, size_t out_size, float value,
                             float band_price);
uint16_t game_stock_price_color(GameStockPriceState state);

#endif
