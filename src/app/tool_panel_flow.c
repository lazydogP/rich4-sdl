#include "app/tool_panel_flow.h"

#include "app/game_speech_flow.h"
#include "app/map_input_flow.h"
#include "core/game_constants.h"
#include "core/tool.h"
#include "core/turn_flow.h"

#include <string.h>

static int rich4_tool_panel_wrap_index(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    return value < 0 ? value + count : value;
}

void rich4_tool_panel_reset(ToolPanelState *panel)
{
    if (panel == NULL) return;
    memset(panel, 0, sizeof(*panel));
    panel->selected_tool = -1;
    panel->pressed_tool = -1;
    panel->pending_tool_release = -1;
    panel->hover = -1;
    panel->primary_target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
}

static void rich4_tool_panel_restore_dice_status(
    GameSession *session, ToolPanelState *panel)
{
    if (session == NULL || panel == NULL || !panel->dice_status_hidden) return;
    session->presentation.dice.status_visible =
        panel->dice_status_was_visible;
    session->presentation.dice.status_dragging = 0;
    panel->dice_status_hidden = 0;
}

static void rich4_tool_panel_hide_dice_status(
    GameSession *session, ToolPanelState *panel)
{
    if (session == NULL || panel == NULL) return;
    if (!panel->dice_status_hidden) {
        panel->dice_status_was_visible =
            session->presentation.dice.status_visible;
        panel->dice_status_hidden = 1;
    }
    session->presentation.dice.status_visible = 0;
    session->presentation.dice.status_dragging = 0;
}

void rich4_tool_panel_open(ToolPanelState *panel, GameSession *session)
{
    if (panel == NULL || session == NULL || !session->state.active ||
        session->state.player_count <= 0) return;
    session->presentation.has_view_override = 0;
    rich4_tool_panel_reset(panel);
    /* The inventory panel occupies the same map area as the dice status
     * float. Keep the float hidden for the complete tool workflow. */
    rich4_tool_panel_hide_dice_status(session, panel);
    panel->phase = TOOL_PANEL_INVENTORY;
    panel->player = rich4_tool_panel_wrap_index(
        session->state.current_player, session->state.player_count);
}

void rich4_tool_panel_close(ToolPanelState *panel)
{
    if (panel == NULL) return;
    rich4_tool_panel_reset(panel);
}

Rich4ToolPanelTargetSpec rich4_tool_panel_target_spec(
    const GameSession *session, const ToolPanelState *panel)
{
    Rich4ToolPanelTargetSpec spec = {
        RICH4_TOOL_PANEL_TARGET_NONE, 0
    };
    const GameToolInfo *info;
    if (session == NULL || panel == NULL ||
        panel->phase == TOOL_PANEL_CLOSED) return spec;
    if (panel->phase == TOOL_PANEL_INVENTORY) return spec;
    if (panel->phase == TOOL_PANEL_REMOTE_DICE) {
        spec.mode = RICH4_TOOL_PANEL_TARGET_REMOTE_DICE;
        return spec;
    }
    if (panel->phase == TOOL_PANEL_WORKER_CHOICE) {
        spec.mode = RICH4_TOOL_PANEL_TARGET_WORKER_CHOICE;
        return spec;
    }
    if (panel->phase == TOOL_PANEL_TRANSFER_DESTINATION) {
        spec.mode = (panel->primary_target.kind == GAME_DOMAIN_LAND ||
                     panel->primary_target.kind == GAME_DOMAIN_FACILITY)
            ? RICH4_TOOL_PANEL_TARGET_PROPERTY
            : RICH4_TOOL_PANEL_TARGET_ROUTE;
        return spec;
    }
    if (panel->selected_tool == 10 || panel->selected_tool == 6 ||
        panel->selected_tool == 12) {
        spec.mode = RICH4_TOOL_PANEL_TARGET_ANY;
        return spec;
    }
    if (panel->selected_tool == 8) {
        spec.mode = RICH4_TOOL_PANEL_TARGET_PROPERTY;
        return spec;
    }
    info = rich4_tool_info(panel->selected_tool);
    if (info != NULL && info->target_kind == GAME_TOOL_TARGET_ROUTE_NODE) {
        spec.mode = RICH4_TOOL_PANEL_TARGET_ROUTE;
        spec.require_placeable = 1;
    }
    return spec;
}

int rich4_tool_panel_select_inventory(
    GameSession *session, ToolPanelState *panel, int tool,
    Rich4AudioEventQueue *audio_events)
{
    if (session == NULL || panel == NULL ||
        panel->phase != TOOL_PANEL_INVENTORY || tool < 0 ||
        tool >= GAME_TOOL_COUNT || panel->player < 0 ||
        panel->player >= session->state.player_count ||
        session->state.players[panel->player].tool_inventory[tool] == 0)
        return 0;
    panel->selected_tool = tool;
    panel->pressed_tool = -1;
    panel->pending_tool_release = -1;
    rich4_tool_panel_hide_dice_status(session, panel);
    if (audio_events != NULL)
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_OK, 1, 0);
    return 1;
}

int rich4_tool_panel_activate_selected(
    GameSession *session, ToolPanelState *panel, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    int tool;
    const GameToolInfo *info;
    (void)audio_events;
    if (session == NULL || panel == NULL ||
        panel->phase != TOOL_PANEL_INVENTORY ||
        panel->selected_tool < 0 || panel->selected_tool >= GAME_TOOL_COUNT ||
        panel->player < 0 || panel->player >= session->state.player_count ||
        session->state.players[panel->player]
            .tool_inventory[panel->selected_tool] == 0)
        return 0;
    tool = panel->selected_tool;
    info = rich4_tool_info(tool);
    /* The action/dice float is saved and hidden while every original tool
     * handler owns the screen, then restored when the handler closes. */
    rich4_tool_panel_hide_dice_status(session, panel);
    if (tool == 7) {
        session->presentation.dice.action_hint_active = 0;
        session->presentation.dice.input_refresh_active = 0;
        if (game_start_tool_speech(session, panel->player, tool, now)) {
            panel->phase = TOOL_PANEL_SPEECH;
            panel->phase_after_speech = TOOL_PANEL_REMOTE_DICE;
        } else {
            panel->phase = TOOL_PANEL_REMOTE_DICE;
        }
        panel->hover = -1;
    } else if (tool == 9) {
        if (game_start_tool_speech(session, panel->player, tool, now)) {
            panel->phase = TOOL_PANEL_SPEECH;
            panel->phase_after_speech = TOOL_PANEL_IMMEDIATE_COMMAND;
        } else {
            panel->phase = TOOL_PANEL_IMMEDIATE_COMMAND;
            return 1;
        }
        panel->hover = -1;
    } else if ((tool >= 1 && tool <= 3) || tool == 6 || tool == 8 ||
               tool == 10 || tool == 12) {
        /* These handlers call 0x44EF41 before opening their selector,
         * including machine worker (0x447295). */
        if (game_start_tool_speech(session, panel->player, tool, now)) {
            panel->phase = TOOL_PANEL_SPEECH;
            panel->phase_after_speech = TOOL_PANEL_MAP_TARGET;
        } else {
            panel->phase = TOOL_PANEL_MAP_TARGET;
        }
        panel->hover = -1;
    } else if (info != NULL &&
               (info->target_kind == GAME_TOOL_TARGET_ROUTE_NODE ||
                info->target_kind == GAME_TOOL_TARGET_PROPERTY)) {
        panel->phase = TOOL_PANEL_MAP_TARGET;
        panel->hover = -1;
    } else {
        /* The SDL adapter submits the immediate command after this state
         * transition; no target hit-test is needed for these tools. */
        return 1;
    }
    return 0;
}

int rich4_tool_panel_build_command(
    const ToolPanelState *panel, GameDomainRef target, int value,
    GameCommand *out_command)
{
    GameDomainRef primary_target;
    GameDomainRef secondary_target;

    if (out_command != NULL) memset(out_command, 0, sizeof(*out_command));
    if (panel == NULL || out_command == NULL || panel->selected_tool < 0 ||
        panel->selected_tool >= GAME_TOOL_COUNT || panel->player < 0)
        return 0;
    secondary_target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    primary_target = target;
    if (panel->selected_tool == 10 &&
        !rich4_game_domain_ref_is_none(panel->primary_target)) {
        primary_target = panel->primary_target;
        secondary_target = target;
    }
    *out_command = rich4_game_command_use_tool(
        GAME_COMMAND_SOURCE_HUMAN, panel->player,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, panel->selected_tool), value,
        primary_target, secondary_target);
    return 1;
}

int rich4_tool_panel_transfer_destination_valid(
    const GameSession *session, const ToolPanelState *panel,
    GameDomainRef destination)
{
    GameCommand command;
    if (session == NULL || panel == NULL ||
        rich4_game_domain_ref_is_none(destination)) return 0;
    command = rich4_game_command_use_tool(
        GAME_COMMAND_SOURCE_HUMAN, panel->player,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 10), 0,
        panel->primary_target, destination);
    return rich4_tool_inspect(&session->state, &command) ==
        GAME_TOOL_USE_APPLIED;
}

int rich4_tool_panel_target_valid(const GameSession *session,
                                  const ToolPanelState *panel,
                                  GameDomainRef target, int value)
{
    GameCommand command;
    if (session == NULL || panel == NULL ||
        rich4_game_domain_ref_is_none(target)) return 0;
    command = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_HUMAN, panel->player,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, panel->selected_tool),
        target, value);
    return rich4_tool_inspect(&session->state, &command) ==
        GAME_TOOL_USE_APPLIED;
}

int rich4_tool_inventory_slot_at(const GameSession *session,
                                 const ToolPanelState *panel, int x, int y)
{
    int packed, tool;
    if (session == NULL || panel == NULL || x < 19 || x >= 419 ||
        y < 135 || y >= 303 || panel->player < 0 ||
        panel->player >= session->state.player_count) return -1;
    packed = ((y - 135) / 56) * 5 + (x - 19) / 80;
    for (tool = 0; tool < GAME_TOOL_COUNT; tool++) {
        if (session->state.players[panel->player].tool_inventory[tool] == 0)
            continue;
        if (packed-- == 0) return tool;
    }
    return -1;
}

int rich4_tool_panel_remote_dice_face_at(int x, int y)
{
    int face;
    for (face = 0; face < 6; face++)
        if (x >= 104 + face * 40 && x < 134 + face * 40 &&
            y >= 314 && y < 343)
            return face;
    return -1;
}

int rich4_tool_panel_worker_choice_at(int x, int y)
{
    if (x < 50 || x >= 390 || y < 286 || y >= 353) return -1;
    return (x - 50) / 68;
}

void rich4_tool_panel_set_hover(ToolPanelState *panel, int hover)
{
    if (panel != NULL) panel->hover = hover;
}

int rich4_tool_panel_confirm_target(
    const GameSession *session, ToolPanelState *panel,
    GameDomainRef target, int value,
    Rich4ToolPanelConfirmResult *out_result,
    Rich4AudioEventQueue *audio_events)
{
    int selected_tool;
    int valid;
    Rich4ToolPanelTargetSpec spec;
    if (out_result != NULL) memset(out_result, 0, sizeof(*out_result));
    if (session == NULL || panel == NULL || out_result == NULL ||
        panel->phase == TOOL_PANEL_CLOSED) return 0;
    spec = rich4_tool_panel_target_spec(session, panel);
    if (spec.mode == RICH4_TOOL_PANEL_TARGET_REMOTE_DICE) {
        if (value < 1 || value > 6 ||
            !rich4_game_domain_ref_is_none(target)) return 0;
        return rich4_tool_panel_build_command(
            panel, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), value,
            &out_result->command)
            ? (out_result->command_ready = 1, 1) : 0;
    }
    if (spec.mode == RICH4_TOOL_PANEL_TARGET_WORKER_CHOICE) {
        if (value < 0 || value >= 5 ||
            rich4_game_domain_ref_is_none(panel->primary_target)) return 0;
        if (audio_events != NULL)
            rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_OK, 1, 0);
        if (!rich4_tool_panel_build_command(
                panel, panel->primary_target, value,
                &out_result->command)) return 0;
        out_result->command_ready = 1;
        return 1;
    }
    if (spec.mode == RICH4_TOOL_PANEL_TARGET_NONE) return 0;
    selected_tool = panel->selected_tool;
    if (selected_tool == 10 && spec.mode == RICH4_TOOL_PANEL_TARGET_ANY) {
        if (rich4_game_domain_ref_is_none(target) ||
            (target.kind != GAME_DOMAIN_LAND &&
             target.kind != GAME_DOMAIN_FACILITY &&
             target.kind != GAME_DOMAIN_PLAYER &&
             target.kind != GAME_DOMAIN_SPECIAL_PLAYER &&
             target.kind != GAME_DOMAIN_MAP_OBJECT)) {
            if (audio_events != NULL)
                rich4_audio_event_emit(
                    audio_events, RICH4_AUDIO_EVENT_INVALID, 1, 0);
            return 1;
        }
        if (audio_events != NULL)
            rich4_audio_event_emit(
                audio_events, RICH4_AUDIO_EVENT_TARGET_CONFIRM, 1, 0);
        panel->primary_target = target;
        panel->phase = TOOL_PANEL_TRANSFER_DESTINATION;
        panel->hover = -1;
        return 1;
    }
    valid = panel->phase == TOOL_PANEL_TRANSFER_DESTINATION
        ? rich4_tool_panel_transfer_destination_valid(
              session, panel, target)
        : rich4_tool_panel_target_valid(session, panel, target, value);
    if (!valid) {
        if (audio_events != NULL)
            rich4_audio_event_emit(
                audio_events, RICH4_AUDIO_EVENT_INVALID, 1, 0);
        return 1;
    }
    /* The valid branch of original selector 0x445E4D does not play the
     * Effect.MKF[2] target-confirm sound. */
    if (audio_events != NULL &&
        (selected_tool < 1 || selected_tool > 3))
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_TARGET_CONFIRM, 1, 0);
    if (selected_tool == 8 && target.kind == GAME_DOMAIN_FACILITY &&
        target.id >= 0 && target.id < session->state.facility_count &&
        session->state.facility_level[target.id] == 0) {
        panel->primary_target = target;
        panel->phase = TOOL_PANEL_WORKER_CHOICE;
        panel->hover = -1;
        return 1;
    }
    if (!rich4_tool_panel_build_command(panel, target, value,
                                        &out_result->command)) return 0;
    out_result->command_ready = 1;
    return 1;
}

int rich4_tool_panel_complete_command(
    GameSession *session, ToolPanelState *panel,
    const GameCommand *command, int accepted, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    int tool;
    int current;
    if (session == NULL || panel == NULL || command == NULL) return 0;
    if (!accepted) {
        panel->phase = TOOL_PANEL_INVENTORY;
        if (audio_events != NULL)
            rich4_audio_event_emit(audio_events,
                                   RICH4_AUDIO_EVENT_INVALID, 1, 0);
        return 0;
    }
    tool = panel->selected_tool;
    /* 0x446AFB starts the robot loop after player_say returns. Its first
     * movement update now owns that sound, for both human and AI use. */
    if (tool > 3) {
        if (audio_events != NULL)
            rich4_audio_event_emit(audio_events,
                                   RICH4_AUDIO_EVENT_TOOL_USE, 1, tool);
    }
    if (tool == 7) {
        session->presentation.dice.status_visible = 0;
        session->presentation.dice.status_dragging = 0;
        session->presentation.dice.action_hint_active = 0;
        session->presentation.dice.input_refresh_active = 0;
        current = rich4_tool_panel_wrap_index(
            session->state.current_player, session->state.player_count);
        rich4_turn_flow_begin(
            &session->state, &session->flow,
            session->presentation.actors[current].first_turn_landing_pending,
            now);
        rich4_tool_panel_close(panel);
    } else if ((tool >= 1 && tool <= 3) || tool == 6 ||
               tool == 8 || tool == 9 || tool == 10 || tool == 12) {
        /* Speech for these tools completed before command execution. */
        rich4_tool_panel_restore_dice_status(session, panel);
        rich4_tool_panel_close(panel);
    } else if (game_start_tool_speech(session, panel->player, tool, now)) {
        panel->phase = TOOL_PANEL_SPEECH;
        panel->phase_after_speech = TOOL_PANEL_CLOSED;
        panel->hover = -1;
    } else {
        rich4_tool_panel_restore_dice_status(session, panel);
        rich4_tool_panel_close(panel);
    }
    return 1;
}

int rich4_tool_panel_finish_speech(
    ToolPanelState *panel, GameSession *session,
    int *remote_dice_ready, int *immediate_command_ready)
{
    ToolPanelPhase next;
    if (remote_dice_ready != NULL) *remote_dice_ready = 0;
    if (immediate_command_ready != NULL) *immediate_command_ready = 0;
    if (panel == NULL || session == NULL ||
        panel->phase != TOOL_PANEL_SPEECH ||
        session->presentation.speech.active)
        return 0;
    next = panel->phase_after_speech;
    if (next == TOOL_PANEL_REMOTE_DICE ||
        next == TOOL_PANEL_MAP_TARGET ||
        next == TOOL_PANEL_IMMEDIATE_COMMAND) {
        panel->phase = next;
        panel->phase_after_speech = TOOL_PANEL_CLOSED;
        panel->hover = -1;
        if (next == TOOL_PANEL_REMOTE_DICE && remote_dice_ready != NULL)
            *remote_dice_ready = 1;
        if (next == TOOL_PANEL_IMMEDIATE_COMMAND &&
            immediate_command_ready != NULL)
            *immediate_command_ready = 1;
    } else {
        rich4_tool_panel_restore_dice_status(session, panel);
        rich4_tool_panel_close(panel);
    }
    return 1;
}

int rich4_tool_panel_cancel(
    GameSession *session, ToolPanelState *panel,
    Rich4AudioEventQueue *audio_events)
{
    if (panel == NULL || panel->phase == TOOL_PANEL_CLOSED) return 0;
    panel->pressed_tool = -1;
    panel->pending_tool_release = -1;
    if (panel->phase == TOOL_PANEL_WORKER_CHOICE) return 1;
    if (panel->phase == TOOL_PANEL_INVENTORY) {
        rich4_tool_panel_restore_dice_status(session, panel);
        rich4_tool_panel_close(panel);
    } else {
        panel->phase = TOOL_PANEL_INVENTORY;
        panel->primary_target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        panel->hover = -1;
    }
    if (audio_events != NULL)
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_CANCEL, 1, 0);
    return 1;
}

void rich4_tool_panel_update_edge_scroll(
    GameSession *session, ToolPanelState *panel,
    int cursor_x, int cursor_y, int inset, uint32_t now)
{
    if (panel == NULL ||
        (panel->phase != TOOL_PANEL_MAP_TARGET &&
         panel->phase != TOOL_PANEL_TRANSFER_DESTINATION)) {
        if (panel != NULL)
            rich4_app_map_edge_scroll_reset(&panel->edge_scroll);
        return;
    }
    rich4_app_map_edge_scroll_update(
        session, &panel->edge_scroll, cursor_x, cursor_y, inset, now);
}
