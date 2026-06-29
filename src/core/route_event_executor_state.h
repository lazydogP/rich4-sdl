#ifndef RICH4_CORE_ROUTE_EVENT_EXECUTOR_STATE_H
#define RICH4_CORE_ROUTE_EVENT_EXECUTOR_STATE_H

#include "core/game_command.h"
#include "core/route_event.h"

/* Dispatch bookkeeping for the current route-fact collection.  The FIFO owns
 * only storage; the app route flow owns the cursor/count fields below.  The
 * executor lifecycle only supplies reset/sentinel initialization, while the
 * turn-movement flow owns the route-resume handoff. */
typedef struct RouteEventDispatchState {
    int phase;
    int index;
    int pass_count;
    int total_count;
    int current_ordinal;
    int next_phase;
    int next_index;
    int handled_count;
    int resume_movement;
} RouteEventDispatchState;

/* Mutable execution protocol owned by the route executor, not by the FIFO
 * container.  Presentation fields are wait-token data emitted to the app
 * layer; they are not a second copy of GameSession presentation state. */
typedef struct RouteEventExecutorState {
    unsigned int turn_token;
    RouteEventDispatchState dispatch;
    RouteEventHandlerResult last_result;
    int execution_started;
    int waiting_for_command;
    int waiting_for_liquidation;
    int waiting_for_presentation;
    int presentation_land;
    int presentation_action;
    int presentation_company;
    int presentation_mode;
    int presentation_condition;
    unsigned int presentation_targets;
    int magic_fortune_sequence;
    int magic_fortune_target;
    int magic_fortunes_left;
    unsigned int magic_fortune_targets_left;
    int lottery_purchase_committed;
    int bank_stop_stage;
    int execution_actor;
    /* One-shot guard for the original stop-node beggar interaction.  It is
     * saved because a donation dialog can be active across a save/load. */
    int beggar_checked;
    GameCommand pending_accept;
    GameCommand pending_decline;
} RouteEventExecutorState;

#endif
