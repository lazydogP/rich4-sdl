#ifndef RICH4_APP_GAME_SESSION_RUNTIME_INIT_H
#define RICH4_APP_GAME_SESSION_RUNTIME_INIT_H

#include "app/game_session.h"
#include "data/map_data.h"

/* Initialize app-owned flow/presentation defaults and initial player state.
 * MapData is borrowed; optional node arrays are caller-provided test seams. */
void rich4_app_initialize_session_runtime(
    GameSession *session, const MapData *map, int route_count,
    const int *start_nodes, const int *next_nodes);

#endif
