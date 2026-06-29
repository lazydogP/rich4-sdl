#ifndef RICH4_APP_ROUTE_FACTS_H
#define RICH4_APP_ROUTE_FACTS_H

#include "app/game_session.h"
#include "data/map_data.h"

/* App-owned route facts. MapData is borrowed immutable input; GameSession
 * owns the ordered route-fact collection. Evidence/tests: A8.3/A5. */
void rich4_app_route_object_at(const MapData *map, int node,
                               int *out_object_id, int *out_object_type,
                               int *out_object_index);
int rich4_app_route_action_for_node(const MapData *map, int node);
void rich4_app_route_record_pass(GameSession *session, const MapData *map,
                                 int node);
void rich4_app_route_record_stop(GameSession *session, const MapData *map,
                                 int node);

#endif
