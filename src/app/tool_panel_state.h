#ifndef RICH4_APP_TOOL_PANEL_STATE_H
#define RICH4_APP_TOOL_PANEL_STATE_H

#include <stdint.h>

#include "core/game_domain.h"
#include "app/map_edge_scroll_state.h"

typedef enum ToolPanelPhase {
    TOOL_PANEL_CLOSED = 0,
    TOOL_PANEL_INVENTORY,
    TOOL_PANEL_MAP_TARGET,
    TOOL_PANEL_TRANSFER_DESTINATION,
    TOOL_PANEL_SPEECH,
    TOOL_PANEL_IMMEDIATE_COMMAND,
    TOOL_PANEL_REMOTE_DICE,
    TOOL_PANEL_WORKER_CHOICE
} ToolPanelPhase;

typedef struct ToolPanelState {
    ToolPanelPhase phase;
    int player;
    int selected_tool;
    /* Physical inventory clicks have separate press and release phases in
     * the original tools window procedure. */
    int pressed_tool;
    int pending_tool_release;
    int hover;
    ToolPanelPhase phase_after_speech;
    GameDomainRef primary_target;
    Rich4MapEdgeScrollState edge_scroll;
    int dice_status_hidden;
    int dice_status_was_visible;
} ToolPanelState;

#endif
