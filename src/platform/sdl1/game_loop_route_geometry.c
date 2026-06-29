#include "app/route_geometry_flow.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_route_api.h"

/* SDL adapter for route geometry. It selects the current borrowed MapData
 * from the platform asset bundle, then delegates all route facts and math to
 * the app-owned, SDL-free implementation. */
static const MapData *game_route_current_map(
    const GamePreviewAssets *assets, const NewGameSettings *settings)
{
    int background_index;
    if (assets == NULL || settings == NULL) return NULL;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT)
        return NULL;
    return &assets->map.maps[background_index];
}

int game_route_count_for_map(const GamePreviewAssets *assets,
                             const NewGameSettings *settings)
{
    return rich4_route_count_for_map(game_route_current_map(assets, settings));
}

int game_route_first_node_for_map(const GamePreviewAssets *assets,
                                  const NewGameSettings *settings)
{
    return rich4_route_first_node_for_map(
        game_route_current_map(assets, settings));
}

int game_route_node_is_valid(const GamePreviewAssets *assets,
                             const NewGameSettings *settings, int node)
{
    return rich4_route_node_is_valid(
        game_route_current_map(assets, settings), node);
}

void game_route_node_xy_for_map(const GamePreviewAssets *assets,
                                const NewGameSettings *settings,
                                int node, int *out_x, int *out_y)
{
    rich4_route_node_xy_for_map(
        game_route_current_map(assets, settings), node, out_x, out_y);
}

int game_route_direction8_for_nodes(const GamePreviewAssets *assets,
                                    const NewGameSettings *settings,
                                    int from_node, int to_node)
{
    return rich4_route_direction8_for_nodes(
        game_route_current_map(assets, settings), from_node, to_node);
}

int game_route_next_for_node(const GamePreviewAssets *assets,
                             const NewGameSettings *settings,
                             int node, int previous_node)
{
    return rich4_route_next_for_node(
        game_route_current_map(assets, settings), node, previous_node);
}
