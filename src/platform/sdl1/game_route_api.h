#ifndef RICH4_PLATFORM_SDL1_GAME_ROUTE_API_H
#define RICH4_PLATFORM_SDL1_GAME_ROUTE_API_H

#include "app/config_types.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* SDL-facing route geometry and route-fact recording seams. */
int game_route_count_for_map(const GamePreviewAssets *assets,
                             const NewGameSettings *settings);
int game_route_first_node_for_map(const GamePreviewAssets *assets,
                                  const NewGameSettings *settings);
void game_route_node_xy_for_map(const GamePreviewAssets *assets,
                                const NewGameSettings *settings, int node,
                                int *out_x, int *out_y);
int game_route_node_is_valid(const GamePreviewAssets *assets,
                             const NewGameSettings *settings, int node);
int game_route_direction8_for_nodes(const GamePreviewAssets *assets,
                                    const NewGameSettings *settings,
                                    int from_node, int to_node);
int game_route_next_for_node(const GamePreviewAssets *assets,
                             const NewGameSettings *settings, int node,
                             int previous_node);
int game_move_frame_ms_from_config(const Rich4Config *config);
#endif
