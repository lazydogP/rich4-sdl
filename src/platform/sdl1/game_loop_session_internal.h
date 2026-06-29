#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_SESSION_INTERNAL_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_SESSION_INTERNAL_H

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "core/stock.h"
#include "platform/sdl1/app_asset_types.h"

extern const GameStockInfo game_stock_table[];
void game_session_prepare_intro_avi(GameSession *session,
                                    const NewGameSettings *settings);
int game_route_random_outgoing_for_landing(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings,
    int node);
int game_route_random_start_node(
    const GamePreviewAssets *assets,
    const NewGameSettings *settings,
    GameSession *session,
    int player_index);
int initial_money_from_settings(const NewGameSettings *settings);
int game_day_limit_from_settings(const NewGameSettings *settings);
int game_win_target_from_settings(const NewGameSettings *settings,
                                  int initial_fund);

#endif
