#ifndef RICH4_DEBUG_GAME_DEBUG_TRACE_H
#define RICH4_DEBUG_GAME_DEBUG_TRACE_H

#include "core/game_constants.h"

typedef struct GameDebugTrace {
    int original_selected_player_count;
    int original_num_players;
    int setup_character_normalized[12];
    int last_special_status_slot;
    int last_special_status_index;
    int last_special_status_handler;
    int last_special_status_original_turn_index;
    int last_special_status_actor_refreshed;
    int status_event_processed_count;
    int last_status_event_player;
    int last_status_event_offset;
    int last_status_event_handler;
    int status_event_players[GAME_MAX_STATUS_EVENT_RECORDS];
    int status_event_offsets[GAME_MAX_STATUS_EVENT_RECORDS];
    int status_event_handlers[GAME_MAX_STATUS_EVENT_RECORDS];
    int god_balance_tick_count;
    int last_god_balance_source;
    int last_god_balance_target;
    int last_god_balance_delta;
    int last_god_balance_applied;

    int route_event_last_handler_family;
    int route_event_last_handler_phase;
    int route_event_last_handler_node;
    int route_event_handled_families[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_handled_phases[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_handled_ordinals[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_handled_nodes[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_handled_event_kinds[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_handled_property_branches[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_count;
    int route_event_last_deferred_phase;
    int route_event_last_deferred_kind;
    int route_event_last_deferred_node;
    int route_event_deferred_phases[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_ordinals[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_nodes[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_object_ids[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_object_types[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_object_indices[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_event_kinds[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_property_branches[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_property_owners[GAME_MAX_TURN_ROUTE_EVENTS];
    int route_event_deferred_property_values[GAME_MAX_TURN_ROUTE_EVENTS];

    int last_first_dispatch_result;
    int last_first_dispatch_branch;
    int first_dispatch_return_only;
    int automatic_preaction_player;
    int automatic_preaction_dispatch_result;
    int automatic_preaction_sale_state;
    int automatic_preaction_card_state;
    int automatic_preaction_tool_state;
    int automatic_preaction_status_checked;
    int automatic_preaction_status_remark_dirty;
    int automatic_preaction_card_pool_total;
    int automatic_preaction_tool_pool_total;
    int automatic_preaction_player_card_total;
    int automatic_preaction_player_tool_total;
} GameDebugTrace;

#endif
