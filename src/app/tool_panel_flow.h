#ifndef RICH4_APP_TOOL_PANEL_FLOW_H
#define RICH4_APP_TOOL_PANEL_FLOW_H

#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/tool_panel_state.h"
#include "core/game_command.h"

typedef enum Rich4ToolPanelTargetMode {
    RICH4_TOOL_PANEL_TARGET_NONE = 0,
    RICH4_TOOL_PANEL_TARGET_REMOTE_DICE,
    RICH4_TOOL_PANEL_TARGET_WORKER_CHOICE,
    RICH4_TOOL_PANEL_TARGET_ROUTE,
    RICH4_TOOL_PANEL_TARGET_PROPERTY,
    RICH4_TOOL_PANEL_TARGET_ANY
} Rich4ToolPanelTargetMode;

typedef struct Rich4ToolPanelTargetSpec {
    Rich4ToolPanelTargetMode mode;
    int require_placeable;
} Rich4ToolPanelTargetSpec;

typedef struct Rich4ToolPanelConfirmResult {
    int command_ready;
    GameCommand command;
} Rich4ToolPanelConfirmResult;

/* App-owned tool inventory/target-selection state machine. SDL supplies
 * hit-tested targets and consumes the emitted audio/command side effects. */
void rich4_tool_panel_reset(ToolPanelState *panel);
void rich4_tool_panel_open(ToolPanelState *panel, GameSession *session);
void rich4_tool_panel_close(ToolPanelState *panel);
Rich4ToolPanelTargetSpec rich4_tool_panel_target_spec(
    const GameSession *session, const ToolPanelState *panel);
int rich4_tool_panel_select_inventory(
    GameSession *session, ToolPanelState *panel, int tool,
    Rich4AudioEventQueue *audio_events);
int rich4_tool_panel_activate_selected(
    GameSession *session, ToolPanelState *panel, uint32_t now,
    Rich4AudioEventQueue *audio_events);
int rich4_tool_panel_build_command(
    const ToolPanelState *panel, GameDomainRef target, int value,
    GameCommand *out_command);
int rich4_tool_panel_transfer_destination_valid(
    const GameSession *session, const ToolPanelState *panel,
    GameDomainRef destination);
int rich4_tool_panel_target_valid(const GameSession *session,
                                  const ToolPanelState *panel,
                                  GameDomainRef target, int value);
int rich4_tool_inventory_slot_at(const GameSession *session,
                                 const ToolPanelState *panel, int x, int y);
int rich4_tool_panel_remote_dice_face_at(int x, int y);
int rich4_tool_panel_worker_choice_at(int x, int y);
void rich4_tool_panel_set_hover(ToolPanelState *panel, int hover);
int rich4_tool_panel_confirm_target(
    const GameSession *session, ToolPanelState *panel,
    GameDomainRef target, int value,
    Rich4ToolPanelConfirmResult *out_result,
    Rich4AudioEventQueue *audio_events);
int rich4_tool_panel_complete_command(
    GameSession *session, ToolPanelState *panel,
    const GameCommand *command, int accepted, uint32_t now,
    Rich4AudioEventQueue *audio_events);
int rich4_tool_panel_finish_speech(
    ToolPanelState *panel, GameSession *session,
    int *remote_dice_ready, int *immediate_command_ready);
int rich4_tool_panel_cancel(
    GameSession *session, ToolPanelState *panel,
    Rich4AudioEventQueue *audio_events);
void rich4_tool_panel_update_edge_scroll(
    GameSession *session, ToolPanelState *panel,
    int cursor_x, int cursor_y, int inset, uint32_t now);

#endif
