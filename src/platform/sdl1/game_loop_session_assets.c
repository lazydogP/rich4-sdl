#include <stdio.h>
#include <string.h>

#include "app/game_session.h"
#include "app/intro_flow.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "core/rng.h"
#include "core/stock.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_loop_session_internal.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/runtime_render_api.h"

static const char *game_intro_original_avi_name(int stage, int map)
{
    static const char *map_avi_names[GAME_MAPS_PER_STAGE] = {
        "FLYTW.AVI",
        "FLYCHINA.AVI",
        "FLYJP.AVI",
        "FLYUS.AVI"
    };

    if (stage != 0) {
        return "AIRPLANE.AVI";
    }
    if (map < 0 || map >= GAME_MAPS_PER_STAGE) {
        return NULL;
    }
    return map_avi_names[map];
}

void game_session_prepare_intro_avi(GameSession *session,
                                           const NewGameSettings *settings)
{
    int stage = settings != NULL ? settings->stage : 0;
    int map = settings != NULL ? settings->map : 0;
    const char *avi_name = game_intro_original_avi_name(stage, map);

    if (session == NULL) {
        return;
    }
    rich4_intro_set_avi_selection(session, stage, map, avi_name);
    if (avi_name == NULL) {
        return;
    }
    printf("original intro AVI %s selected; AVI backend unavailable, using JUMP.MKF intro\n",
           avi_name);
}

int game_route_random_outgoing_for_landing(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings,
    int node)
{
    int background_index;
    const MapData *map;
    int options[4];
    int count = 0;
    int i;

    if (assets == NULL || settings == NULL) {
        return game_route_next_for_node(assets, settings, node, 0);
    }
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT) {
        return game_route_first_node_for_map(assets, settings);
    }
    map = &assets->map.maps[background_index];
    if (node <= 0 || (size_t)node >= map->route_count) {
        return game_route_first_node_for_map(assets, settings);
    }
    /* 0x408312 copies every nonzero route[node].next[0..3], and 0x408328
     * chooses rand() % count.  This selection is stored before the landing
     * animation and is also the first walking edge. */
    for (i = 0; i < 4; i++) {
        int next = map->routes[node].next[i];
        if (next > 0 && (size_t)next < map->route_count) {
            options[count++] = next;
        }
    }
    return count > 0 ? options[session != NULL ? rich4_rng_range_at(&session->state.ai_rng_state, count) : rich4_rng_range(count)]
                     : game_route_first_node_for_map(assets, settings);
}

int game_route_random_start_node(const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 GameSession *session,
                                 int player_index)
{
    int background_index;
    int candidates[GAME_MAX_MAP_ROUTE_NODES];
    int candidate_count = 0;
    int node;

    if (assets == NULL || settings == NULL) {
        return wrap_index(player_index, GAME_ROUTE_NODE_COUNT);
    }
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        assets->map.maps[background_index].route_count <= 1) {
        return game_route_first_node_for_map(assets, settings);
    }
    for (node = 1; (size_t)node < assets->map.maps[background_index].route_count; node++) {
        int occupied = 0;
        int i;
        if ((assets->map.maps[background_index].routes[node].flags & 0x80ffff00u) != 0 ||
            !game_route_node_is_valid(assets, settings, node)) {
            continue;
        }
        if (session != NULL) {
            for (i = 0; i < player_index; i++) {
                if (session->state.players[i].node == node) {
                    occupied = 1;
                    break;
                }
            }
        }
        if (!occupied) {
            candidates[candidate_count++] = node;
        }
    }
    if (candidate_count <= 0) {
        return game_route_first_node_for_map(assets, settings);
    }
    return candidates[session != NULL ? rich4_rng_range_at(&session->state.ai_rng_state, candidate_count) : rich4_rng_range(candidate_count)];
}

int initial_money_from_settings(const NewGameSettings *settings)
{
    static const int funds[] = { 300000, 200000, 100000, 50000, 30000, 10000 };
    int index = settings != NULL ? settings->fund_index : 1;
    if (index < 0 || index >= (int)(sizeof(funds) / sizeof(funds[0]))) {
        index = 1;
    }
    return funds[index];
}

int game_day_limit_from_settings(const NewGameSettings *settings)
{
    static const int days[] = { 0, 730, 365, 182, 91, 30 };
    int index = settings != NULL ? settings->days_index : 0;
    if (index < 0 || index >= (int)(sizeof(days) / sizeof(days[0]))) {
        index = 0;
    }
    return days[index];
}

int game_win_target_from_settings(const NewGameSettings *settings, int initial_fund)
{
    static const int winning_condition[] = { 0, 100, 50, 10, 5, 3 };
    int index = settings != NULL ? settings->goal_index : 0;
    if (index < 0 || index >= (int)(sizeof(winning_condition) / sizeof(winning_condition[0]))) {
        index = 0;
    }
    return winning_condition[index] * initial_fund;
}

const GameStockInfo game_stock_table[GAME_BACKGROUND_COUNT * GAME_STOCK_COUNT] = {
    { 1, 0, 0, 10000, 0, 100.0f, 100.0f, 100.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 40.0f, 40.0f, 40.0f, 0.600000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 25.0f, 25.0f, 25.0f, 1.500000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 180.0f, 180.0f, 180.0f, 1.600000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 80.0f, 80.0f, 80.0f, 1.200000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 60.0f, 60.0f, 60.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 60.0f, 60.0f, 60.0f, 1.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 27.0f, 27.0f, 27.0f, 0.900000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 310.0f, 310.0f, 310.0f, 0.700000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 66.0f, 66.0f, 66.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 171.0f, 171.0f, 171.0f, 1.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 280.0f, 280.0f, 280.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 70.0f, 70.0f, 70.0f, 1.200000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 36.0f, 36.0f, 36.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 20.0f, 20.0f, 20.0f, 1.600000f, 0.0f, 0 },
    { 1, 0, 0, 0, 0, 15.0f, 15.0f, 15.0f, 1.500000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 300.0f, 300.0f, 300.0f, 2.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 296.0f, 296.0f, 296.0f, 1.800000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 133.0f, 133.0f, 133.0f, 1.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 170.0f, 170.0f, 170.0f, 1.500000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 121.0f, 121.0f, 121.0f, 0.800000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 60.0f, 60.0f, 60.0f, 0.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 92.0f, 92.0f, 92.0f, 0.600000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 366.0f, 366.0f, 366.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 160.0f, 160.0f, 160.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 99.0f, 99.0f, 99.0f, 1.400000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 22.0f, 22.0f, 22.0f, 2.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 28.0f, 28.0f, 28.0f, 2.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 300.0f, 300.0f, 300.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 35.0f, 35.0f, 35.0f, 1.200000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 540.0f, 540.0f, 540.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 830.0f, 830.0f, 830.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 300.0f, 300.0f, 300.0f, 1.200000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 225.0f, 225.0f, 225.0f, 0.700000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 300.0f, 300.0f, 300.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 150.0f, 150.0f, 150.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 200.0f, 200.0f, 200.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 55.0f, 55.0f, 55.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 20.0f, 20.0f, 20.0f, 2.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 18.0f, 18.0f, 18.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 16.0f, 16.0f, 16.0f, 1.400000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 550.0f, 550.0f, 550.0f, 2.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 245.0f, 245.0f, 245.0f, 1.500000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 166.0f, 166.0f, 166.0f, 1.300000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 955.0f, 955.0f, 955.0f, 1.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 1030.0f, 1030.0f, 1030.0f, 0.800000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 1440.0f, 1440.0f, 1440.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 310.0f, 310.0f, 310.0f, 1.200000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 50.0f, 50.0f, 50.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 40.0f, 40.0f, 40.0f, 0.600000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 25.0f, 25.0f, 25.0f, 1.500000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 90.0f, 90.0f, 90.0f, 1.600000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 45.0f, 45.0f, 45.0f, 1.200000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 60.0f, 60.0f, 60.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 60.0f, 60.0f, 60.0f, 1.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 27.0f, 27.0f, 27.0f, 0.900000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 110.0f, 110.0f, 110.0f, 0.700000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 66.0f, 66.0f, 66.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 71.0f, 71.0f, 71.0f, 1.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 80.0f, 80.0f, 80.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 70.0f, 70.0f, 70.0f, 1.200000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 70.0f, 70.0f, 70.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 20.0f, 20.0f, 20.0f, 1.600000f, 0.0f, 0 },
    { 1, 0, 0, 3000, 0, 250.0f, 250.0f, 250.0f, 1.500000f, 0.0f, 0 },
    { 1, 0, 0, 3000, 0, 101.0f, 101.0f, 101.0f, 2.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 96.0f, 96.0f, 96.0f, 1.800000f, 0.0f, 0 },
    { 1, 0, 0, 6000, 0, 120.0f, 120.0f, 120.0f, 1.400000f, 0.0f, 0 },
    { 1, 0, 0, 4000, 0, 47.0f, 47.0f, 47.0f, 1.500000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 40.0f, 40.0f, 40.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 4000, 0, 30.0f, 30.0f, 30.0f, 0.400000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 90.0f, 90.0f, 90.0f, 0.600000f, 0.0f, 0 },
    { 1, 0, 0, 4000, 0, 300.0f, 300.0f, 300.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 60.0f, 60.0f, 60.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 33.0f, 33.0f, 33.0f, 1.400000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 22.0f, 22.0f, 22.0f, 2.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 18.0f, 18.0f, 18.0f, 2.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 100.0f, 100.0f, 100.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 15.0f, 15.0f, 15.0f, 1.200000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 40.0f, 40.0f, 40.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 30.0f, 30.0f, 30.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 100.0f, 100.0f, 100.0f, 1.200000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 25.0f, 25.0f, 25.0f, 0.700000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 100.0f, 100.0f, 100.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 50.0f, 50.0f, 50.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 100.0f, 100.0f, 100.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 10000, 0, 33.0f, 33.0f, 33.0f, 0.800000f, 0.0f, 0 },
    { 1, 0, 0, 3000, 0, 500.0f, 500.0f, 500.0f, 2.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 100.0f, 100.0f, 100.0f, 1.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 130.0f, 130.0f, 130.0f, 1.400000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 150.0f, 150.0f, 150.0f, 2.000000f, 0.0f, 0 },
    { 1, 0, 0, 5000, 0, 70.0f, 70.0f, 70.0f, 1.500000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 66.0f, 66.0f, 66.0f, 1.300000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 55.0f, 55.0f, 55.0f, 1.400000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 30.0f, 30.0f, 30.0f, 0.800000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 44.0f, 44.0f, 44.0f, 1.000000f, 0.0f, 0 },
    { 0, 0, 0, 10000, 0, 30.0f, 30.0f, 30.0f, 1.200000f, 0.0f, 0 }
};
