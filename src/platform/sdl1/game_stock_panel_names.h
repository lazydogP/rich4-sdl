#ifndef RICH4_PLATFORM_SDL1_GAME_STOCK_PANEL_NAMES_H
#define RICH4_PLATFORM_SDL1_GAME_STOCK_PANEL_NAMES_H

#include "app/new_game_setup_flow.h"
#include "core/game_state.h"

const char *game_stock_name_for_state(const GameState *state, int stock);
int game_stock_panel_map_index(const NewGameSettings *settings);
const char *game_stock_panel_name(const NewGameSettings *settings, int stock);

#endif
