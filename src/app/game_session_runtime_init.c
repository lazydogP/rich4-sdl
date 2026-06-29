/* Purpose: initialize the app-owned runtime and initial player presentation.
 * Owns: flow defaults, debug defaults, initial actor state and tool geometry.
 * Borrows: immutable map facts and optional caller-provided start nodes.
 * Allowed deps: app state, core initialization helpers, and map data only.
 * Invariant: initial player placement is selected in player order so occupied
 * route nodes are excluded exactly as in the original setup flow.
 */

#include "app/game_session_runtime_init.h"

#include <string.h>

#include "app/confinement_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/game_session_map_init.h"
#include "app/game_presentation.h"
#include "app/input_flow.h"
#include "app/intro_flow.h"
#include "app/route_flow.h"
#include "app/route_geometry_flow.h"
#include "core/game_constants.h"
#include "core/rng.h"
#include "core/tool.h"
#include "core/stock.h"

static int dynamic_object_occupies_route_node(
    const GamePresentationState *presentation, int node)
{
    int object_id;

    if (presentation == NULL || node <= 0) return 0;
    for (object_id = 0;
         object_id < GAME_MAP_DYNAMIC_OBJECT_COUNT; object_id++) {
        if (presentation->dynamic_objects[object_id].node == node)
            return 1;
    }
    return 0;
}

static int random_start_node(GameSession *session,
                             const MapData *map, int player_index)
{
    int candidates[GAME_MAX_MAP_ROUTE_NODES];
    int candidate_count = 0;
    int node;

    if (map == NULL || map->routes == NULL || map->route_count <= 1)
        return player_index % GAME_ROUTE_NODE_COUNT;
    for (node = 1; (size_t)node < map->route_count &&
                   node < GAME_MAX_MAP_ROUTE_NODES; node++) {
        int occupied = 0;
        int i;
        if ((map->routes[node].flags & 0x80ffff00u) != 0 ||
            !rich4_route_node_is_valid(map, node))
            continue;
        /* 0x407AD2 places the initial dynamic objects before 0x40829D
         * chooses the first player node.  The original uses the same
         * route-node occupancy word for both; MapData is immutable here, so
         * observe the equivalent presentation occupancy explicitly. */
        if (session != NULL && dynamic_object_occupies_route_node(
                &session->presentation, node))
            continue;
        if (session != NULL) {
            for (i = 0; i < player_index; i++) {
                if (session->state.players[i].node == node) {
                    occupied = 1;
                    break;
                }
            }
        }
        if (!occupied) candidates[candidate_count++] = node;
    }
    if (candidate_count <= 0) return rich4_route_first_node_for_map(map);
    {
        int selected = candidates[rich4_rng_range_at(
            &session->state.ai_rng_state, candidate_count)];
        return selected;
    }
}

static int random_outgoing_node(GameSession *session, const MapData *map, int node)
{
    int options[4];
    int count = 0;
    int i;

    if (map == NULL || map->routes == NULL || map->route_count <= 1)
        return rich4_route_next_for_node(NULL, node, 0);
    if (!rich4_route_node_is_valid(map, node))
        return rich4_route_first_node_for_map(map);
    for (i = 0; i < 4; i++) {
        int next = map->routes[node].next[i];
        if (next > 0 && (size_t)next < map->route_count)
            options[count++] = next;
    }
    if (count > 0) {
        int selected = options[rich4_rng_range_at(
            &session->state.ai_rng_state, count)];
        return selected;
    }
    return rich4_route_first_node_for_map(map);
}

void rich4_app_initialize_session_runtime(
    GameSession *session, const MapData *map, int route_count,
    const int *start_nodes, const int *next_nodes)
{
    int i;
    int count;

    if (session == NULL) return;
    rich4_presentation_initialize(&session->presentation);
    rich4_app_init_dynamic_object_pool(&session->presentation);
    /* 0x407AD2 places these before the later player/view setup. */
    rich4_app_initialize_dynamic_map_objects_with_rng(&session->presentation, map, &session->state.ai_rng_state);
    if (session->state.stock_last_update_day == -2)
        rich4_stock_state_init(&session->state, NULL);
    rich4_app_route_reset_facts(session);
    rich4_app_route_reset_dispatch(session);
    session->flow.dice_value = 0;
    session->flow.steps_remaining = 0;
    session->flow.route_count = route_count > 0
        ? route_count : GAME_ROUTE_NODE_COUNT;
    rich4_app_input_set_top_button_hover(session, -1);
    rich4_intro_begin(session);

    session->flow.landing_pending_player = 0;
    session->flow.turn_flow_active = 0;
    session->flow.turn_state = GAME_TURN_STATE_IDLE;
    session->flow.turn_state_frame = 0;
    session->flow.turn_wait = 0;
    session->flow.turn_state_tick = 0;
    session->debug.last_first_dispatch_result = -1;
    session->debug.last_first_dispatch_branch =
        GAME_FIRST_DISPATCH_BRANCH_NONE;
    session->debug.first_dispatch_return_only = 0;
    session->debug.automatic_preaction_player = -1;
    session->debug.automatic_preaction_dispatch_result = -1;
    session->debug.automatic_preaction_sale_state =
        GAME_AUTOMATIC_PREACTION_NONE;
    session->debug.automatic_preaction_card_state =
        GAME_AUTOMATIC_PREACTION_NONE;
    session->debug.automatic_preaction_tool_state =
        GAME_AUTOMATIC_PREACTION_NONE;
    session->debug.automatic_preaction_status_checked = 0;
    session->debug.automatic_preaction_status_remark_dirty = 0;
    session->debug.automatic_preaction_card_pool_total = 0;
    session->debug.automatic_preaction_tool_pool_total = 0;
    session->debug.automatic_preaction_player_card_total = 0;
    session->debug.automatic_preaction_player_tool_total = 0;
    session->debug.last_special_status_slot = -1;
    session->debug.last_special_status_index = -1;
    session->debug.last_special_status_handler =
        GAME_STATUS_EVENT_HANDLER_NONE;
    session->debug.last_special_status_original_turn_index = -1;
    session->debug.last_special_status_actor_refreshed = 0;
    session->debug.status_event_processed_count = 0;
    session->debug.last_status_event_player = -1;
    session->debug.last_status_event_offset = 0;
    session->debug.last_status_event_handler =
        GAME_STATUS_EVENT_HANDLER_NONE;
    memset(session->debug.status_event_players, 0xff,
           sizeof(session->debug.status_event_players));
    memset(session->debug.status_event_offsets, 0,
           sizeof(session->debug.status_event_offsets));
    memset(session->debug.status_event_handlers, 0,
           sizeof(session->debug.status_event_handlers));
    session->debug.god_balance_tick_count = 0;
    session->debug.last_god_balance_source = -1;
    session->debug.last_god_balance_target = -1;
    session->debug.last_god_balance_delta = 0;
    session->debug.last_god_balance_applied = 0;

    count = session->state.player_count;
    if (count > GAME_MAX_PLAYERS) count = GAME_MAX_PLAYERS;
    for (i = 0; i < count; i++) {
        if (start_nodes != NULL)
            session->state.players[i].node = start_nodes[i];
        else
            session->state.players[i].node =
                random_start_node(session, map, i);
        if (next_nodes != NULL)
            session->state.players[i].next_node = next_nodes[i];
        else
            session->state.players[i].next_node =
                random_outgoing_node(session, map, session->state.players[i].node);
        session->state.players[i].previous_node = 0;
        (void)rich4_confinement_sync_player_actor_to_node(
            session, map, i);
        session->presentation.actors[i].walk_frame = 0;
        session->presentation.actors[i].intro_visible = 0;
        session->presentation.actors[i].first_turn_landing_pending = 1;
        session->presentation.actors[i].dirty_flags = 0;
    }
    if (count > 0)
        session->flow.landing_pending_player =
            session->state.current_player + 1;

    if (map != NULL && map->routes != NULL && map->route_count > 1) {
        uint8_t placeable[GAME_MAX_MAP_ROUTE_NODES];
        int16_t route_x[GAME_MAX_MAP_ROUTE_NODES];
        int16_t route_y[GAME_MAX_MAP_ROUTE_NODES];
        uint16_t route_next[GAME_MAX_MAP_ROUTE_NODES][4];
        int16_t land_x[GAME_MAX_MAP_PROPERTIES];
        int16_t land_y[GAME_MAX_MAP_PROPERTIES];
        int16_t facility_x[GAME_MAX_MAP_PROPERTIES];
        int16_t facility_y[GAME_MAX_MAP_PROPERTIES];
        int route_nodes = (int)map->route_count;

        memset(placeable, 0, sizeof(placeable));
        memset(route_x, 0, sizeof(route_x));
        memset(route_y, 0, sizeof(route_y));
        memset(route_next, 0, sizeof(route_next));
        memset(land_x, 0, sizeof(land_x));
        memset(land_y, 0, sizeof(land_y));
        memset(facility_x, 0, sizeof(facility_x));
        memset(facility_y, 0, sizeof(facility_y));
        if (route_nodes > GAME_MAX_MAP_ROUTE_NODES)
            route_nodes = GAME_MAX_MAP_ROUTE_NODES;
        for (i = 0; i < route_nodes; i++) {
            int edge;
            route_x[i] = map->routes[i].x;
            route_y[i] = map->routes[i].y;
            for (edge = 0; edge < 4; edge++)
                route_next[i][edge] = map->routes[i].next[edge];
            placeable[i] = (uint8_t)(
                map->routes[i].next[0] != 0 ||
                map->routes[i].next[1] != 0 ||
                map->routes[i].next[2] != 0 ||
                map->routes[i].next[3] != 0);
        }
        placeable[0] = 0;
        rich4_game_session_load_map_state(session, map);
        rich4_tool_set_route_targets(&session->state, route_nodes,
                                     placeable, (size_t)route_nodes);
        for (i = 0; i < session->state.land_count; i++) {
            land_x[i] = map->lands[i + 1].x;
            land_y[i] = map->lands[i + 1].y;
        }
        for (i = 0; i < session->state.facility_count; i++) {
            facility_x[i] = map->facilities[i + 1].x;
            facility_y[i] = map->facilities[i + 1].y;
        }
        rich4_tool_set_map_geometry(
            &session->state, route_nodes, route_x, route_y, route_next,
            session->state.land_count, land_x, land_y,
            session->state.facility_count, facility_x, facility_y);
    }
    if (count > 0) {
        const GameActorPresentationState *actor =
            &session->presentation.actors[0];
        rich4_presentation_set_view_position(
            &session->presentation, actor->visual_x, actor->visual_y);
    }
}
