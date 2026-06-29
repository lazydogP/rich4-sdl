#ifndef RICH4_APP_GAME_SESSION_MAP_INIT_H
#define RICH4_APP_GAME_SESSION_MAP_INIT_H

#include "app/game_session.h"
#include "data/map_data.h"

/* Copy immutable map facts into the app-owned deterministic session state.
 * The map is borrowed for the duration of the call; no platform types or
 * resource handles cross this boundary. */
void rich4_game_session_load_map_state(GameSession *session,
                                       const MapData *map);

#endif
