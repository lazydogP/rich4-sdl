#include <SDL/SDL.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_panel_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/card.h"
#include "platform/sdl1/render/game_map_objects.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/game_loop_tool_panel_internal.h"
#include "platform/sdl1/runtime.h"

static int game_tool_panel_apply_command(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    const GameCommand *command, Uint32 now, Rich4Audio *audio)
{
    Rich4AudioEventQueue *audio_events;
    int accepted;
    if (session == NULL || assets == NULL || settings == NULL ||
        panel == NULL || command == NULL) return 0;
    accepted = game_session_apply_command(
        session, assets, settings, command, now);
    audio_events = rich4_app_session_audio_borrow(session);
    rich4_tool_panel_complete_command(
        session, panel, command, accepted, now, audio_events);
    game_session_audio_flush(session, audio);
    return accepted;
}

static int game_tool_panel_submit(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    GameDomainRef target, int value, Uint32 now, Rich4Audio *audio)
{
    GameCommand command;
    if (!rich4_tool_panel_build_command(panel, target, value, &command))
        return 0;
    return game_tool_panel_apply_command(
        session, assets, settings, panel, &command, now, audio);
}

static GameDomainRef game_tool_panel_target_at(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, const ToolPanelState *panel,
    int x, int y)
{
    GameDomainRef none = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    GameSceneView view;
    uint32_t mask = rich4_tool_panel_map_mask(panel);
    uint32_t kinds = (mask & 0x40) ? 0x37 : mask;
    int raw, slot;
    if (!mask || !build_game_session_render_view(assets, settings, session, &view))
        return none;
    raw = game_map_object_at(assets, settings, &view.game, view.dynamic_objects,
                             x, y, NULL);
    if (raw > 0 && raw < 2000 && (kinds & 1))
        return rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, raw);
    GameDomainRef hit = rich4_card_map_hit(&session->state, panel->player, kinds,
                                           panel->primary_target, raw);
    if (hit.kind != GAME_DOMAIN_NONE || !(raw & 0x8000) || !(kinds & 0x20))
        return hit;
    slot = ((raw >> 8) & 0x3f) - 1;
    if (slot < 0 || slot >= GAME_MAP_DYNAMIC_OBJECT_COUNT) return none;
    if (slot >= 16)
        return rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, slot - 16);
    if (mask & 0x40)
        return rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, view.dynamic_objects[slot].node);
    return rich4_game_domain_ref(GAME_DOMAIN_LOOSE_MAP_OBJECT, slot);
}

void game_tool_panel_finish_speech(
    ToolPanelState *panel, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    VirtualCursor *cursor, int *host_pointer_sync_needed,
    Rich4Audio *audio)
{
    int remote_dice_ready = 0;
    int immediate_command_ready = 0;
    if (!rich4_tool_panel_finish_speech(
            panel, session, &remote_dice_ready,
            &immediate_command_ready)) return;
    if (immediate_command_ready)
        game_tool_panel_submit(
            session, assets, settings, panel,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0,
            rich4_sdl1_ticks(), audio);
    if (remote_dice_ready && cursor != NULL) {
        cursor->x = 220;
        cursor->y = 320;
    }
    if (remote_dice_ready && host_pointer_sync_needed != NULL)
        *host_pointer_sync_needed = 1;
}

int game_tool_panel_finish_inventory_release(
    Rich4SceneId app_state, GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    ToolPanelState *panel, VirtualCursor *cursor, Rich4Audio *audio,
    int button)
{
    Rich4AudioEventQueue *audio_events;
    if (app_state != RICH4_SCENE_GAME ||
        session == NULL || assets == NULL || settings == NULL ||
        panel == NULL || cursor == NULL ||
        panel->phase != TOOL_PANEL_INVENTORY) return 0;
    if (button == SDL_BUTTON_LEFT) {
        /* Match toolsProc(WM_LBUTTONUP): consume the physical release and
         * defer the actual tool handler until the event callback has
         * returned.  The pressed slot, rather than the current cursor
         * position, owns this click. */
        if (panel->pressed_tool >= 0) {
            panel->pending_tool_release = panel->pressed_tool;
            panel->pressed_tool = -1;
        }
        cursor->confirm_pressed = 0;
        cursor->confirm_pointer_pressed = 0;
        return 1;
    }
    if (button != SDL_BUTTON_RIGHT) return 0;

    /* The original tools window consumes WM_RBUTTONUP as its cancel message.
     * Keep that release out of the generic game-level button-up path. */
    audio_events = rich4_app_session_audio_borrow(session);
    (void)rich4_tool_panel_cancel(session, panel, audio_events);
    game_session_audio_flush(session, audio);
    cursor->cancel_pressed = 0;
    cursor->confirm_pressed = 0;
    cursor->confirm_pointer_pressed = 0;
    return 1;
}

void game_tool_panel_dispatch_pending_inventory_release(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    Rich4Audio *audio)
{
    int pending_tool;
    if (session == NULL || assets == NULL || settings == NULL ||
        panel == NULL || panel->pending_tool_release < 0)
        return;
    pending_tool = panel->pending_tool_release;
    panel->pending_tool_release = -1;
    if (panel->phase != TOOL_PANEL_INVENTORY ||
        pending_tool < 0 || pending_tool >= GAME_TOOL_COUNT)
        return;
    panel->selected_tool = pending_tool;
    /* This is intentionally called after SDL_MOUSEBUTTONUP dispatch has
     * returned.  It is the SDL equivalent of tools_ui() receiving the
     * private posted message after toolsProc has consumed WM_LBUTTONUP. */
    game_tool_panel_activate_selected(
        session, assets, settings, panel, rich4_sdl1_ticks(), audio);
}

size_t game_tool_panel_cursor_chunk_index(
    const GamePreviewAssets *assets, const NewGameSettings *settings,
    const GameSession *session, const ToolPanelState *panel,
    int cursor_x, int cursor_y, size_t default_chunk)
{
    static const size_t edge_cursor[5] = {41, 34, 40, 38, 36};
    GameDomainRef target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    if (assets == NULL || settings == NULL || session == NULL ||
        panel == NULL ||
        (panel->phase != TOOL_PANEL_MAP_TARGET &&
         panel->phase != TOOL_PANEL_TRANSFER_DESTINATION))
        return default_chunk;
    uint32_t mask = rich4_tool_panel_map_mask(panel);
    if ((mask & 0x80) && panel->edge_scroll.direction >= 1 &&
        panel->edge_scroll.direction <= 4)
        return edge_cursor[panel->edge_scroll.direction];
    target = game_tool_panel_target_at(assets, settings, session, panel, cursor_x, cursor_y);
    if (target.kind == GAME_DOMAIN_NONE) return 5;
    if (panel->phase == TOOL_PANEL_TRANSFER_DESTINATION) {
        if (!rich4_tool_panel_transfer_destination_valid(session, panel, target)) return 5;
    } else if (panel->selected_tool != 10 &&
               !rich4_tool_panel_target_valid(session, panel, target, 0)) return 5;
    return (mask >> 16) & 0xff;
}

int game_tool_panel_press_inventory(GameSession *session,
                                     ToolPanelState *panel,
                                     int x, int y, Rich4Audio *audio)
{
    Rich4AudioEventQueue *audio_events;
    int selected;
    int accepted;
    selected = rich4_tool_inventory_slot_at(session, panel, x, y);
    audio_events = rich4_app_session_audio_borrow(session);
    accepted = rich4_tool_panel_select_inventory(
        session, panel, selected, audio_events);
    panel->pressed_tool = accepted ? panel->selected_tool : -1;
    game_session_audio_flush(session, audio);
    return accepted;
}

void game_tool_panel_activate_selected(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    Uint32 now, Rich4Audio *audio)
{
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    int immediate = rich4_tool_panel_activate_selected(
        session, panel, now, audio_events);
    panel->pressed_tool = -1;
    game_session_audio_flush(session, audio);
    if (immediate)
        game_tool_panel_submit(
            session, assets, settings, panel,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, now, audio);
}

void game_tool_panel_confirm(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, ToolPanelState *panel,
    int x, int y, Uint32 now, Rich4Audio *audio)
{
    Rich4ToolPanelTargetSpec spec;
    Rich4ToolPanelConfirmResult result;
    Rich4AudioEventQueue *audio_events;
    GameDomainRef target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    int value = 0;
    int confirmed;
    if (panel == NULL || panel->phase == TOOL_PANEL_CLOSED) return;
    if (panel->phase == TOOL_PANEL_INVENTORY) {
        if (game_tool_panel_press_inventory(session, panel, x, y, audio))
            game_tool_panel_activate_selected(
                session, assets, settings, panel, now, audio);
        return;
    }
    spec = rich4_tool_panel_target_spec(session, panel);
    if (spec.mode == RICH4_TOOL_PANEL_TARGET_REMOTE_DICE) {
        int face = rich4_tool_panel_remote_dice_face_at(x, y);
        if (face < 0 || face >= 6) return;
        value = face + 1;
    } else if (spec.mode == RICH4_TOOL_PANEL_TARGET_WORKER_CHOICE) {
        int choice = rich4_tool_panel_worker_choice_at(x, y);
        if (choice < 0 || choice >= 5) return;
        value = choice;
        target = panel->primary_target;
    } else {
        if (spec.mode == RICH4_TOOL_PANEL_TARGET_NONE) return;
        target = game_tool_panel_target_at(
            assets, settings, session, panel, x, y);
    }
    audio_events = rich4_app_session_audio_borrow(session);
    confirmed = rich4_tool_panel_confirm_target(
        session, panel, target, value, &result, audio_events);
    game_session_audio_flush(session, audio);
    if (confirmed && result.command_ready)
        game_tool_panel_apply_command(
            session, assets, settings, panel, &result.command, now, audio);
}
