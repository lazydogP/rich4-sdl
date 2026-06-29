#ifndef RICH4_CORE_TURN_ROTATION_H
#define RICH4_CORE_TURN_ROTATION_H

#include "core/game_state.h"
#include "core/game_event.h"
#include "data/map_data.h"

typedef enum TurnSelectionKind {
    TURN_SELECTION_INVALID = 0,
    TURN_SELECTION_NORMAL,
    TURN_SELECTION_SPECIAL
} TurnSelectionKind;

typedef enum TurnDomainEvent {
    TURN_EVENT_NONE = 0,
    TURN_EVENT_STATUS_TICK = 1 << 0,
    TURN_EVENT_MARK_DIRTY = 1 << 1,
    TURN_EVENT_FULL_ROUND = 1 << 2,
    TURN_EVENT_DAY_ADVANCED = 1 << 3,
    TURN_EVENT_NORMAL_STATE_CLEARED = 1 << 4
} TurnDomainEvent;

typedef struct TurnAdvanceResult {
    int previous_index;
    int selected_index;
    TurnSelectionKind kind;
    unsigned int events;
    unsigned int skipped_normal_mask;
    unsigned int skipped_special_mask;
} TurnAdvanceResult;

typedef enum TurnSpecialAutoActionKind {
    TURN_SPECIAL_AUTO_INVALID = 0,
    TURN_SPECIAL_AUTO_BLOCKED,
    TURN_SPECIAL_AUTO_MOVED
} TurnSpecialAutoActionKind;

#define TURN_SPECIAL_AUTO_MAX_STEPS 10

typedef struct TurnSpecialAutoActionResult {
    TurnSpecialAutoActionKind kind;
    int special_slot;
    int requested_steps;
    int completed_steps;
    int start_node;
    int end_node;
    /* path_nodes[0] is start_node; every following entry is one visibly
     * traversed route node.  The original special actors move one edge at a
     * time, so presentation must not reconstruct this after the RNG state
     * has advanced. */
    int path_count;
    int path_nodes[TURN_SPECIAL_AUTO_MAX_STEPS + 1];
} TurnSpecialAutoActionResult;

int rich4_turn_advance_once(GameState *state, TurnAdvanceResult *out_result);
/* 0x40118F: called on configured cancel-key release in the main callback. */
int rich4_turn_request_ai_cancel(GameState *state);
int rich4_turn_advance_once_events(GameState *state,
                                   TurnAdvanceResult *out_result,
                                   GameEventQueue *events);
int rich4_turn_advance_once_events_at_tick(GameState *state,
    TurnAdvanceResult *out_result, GameEventQueue *events, uint32_t tick);
int rich4_turn_run_special_auto_action(
    GameState *state, const MapData *map, int special_slot,
    TurnSpecialAutoActionResult *out_result);

#endif
