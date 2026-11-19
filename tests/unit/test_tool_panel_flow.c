#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/tool_panel_flow.h"
#include "app/game_command_flow.h"
#include "core/tool.h"
#include "app/dice_flow.h"
#include "app/turn_status_flow.h"

static void missile_uses_live_map_direction(void)
{
    static GameSession session;
    GameCommand command;
    GameEventQueue events;
    int direction, source;
    for (source = 0; source < 2; source++) {
        for (direction = 0; direction < 2; direction++) {
            memset(&session, 0, sizeof(session));
            session.state.active = 1;
            session.state.player_count = 2;
            session.state.price_index = 1;
            session.state.tool_route_node_count = 8;
            session.state.tool_route_x[7] = 3000;
            session.state.tool_route_y[7] = 3000;
            session.state.players[0].node = session.state.players[1].node = 7;
            session.state.players[0].play_state =
                session.state.players[1].play_state = 1;
            session.state.players[0].is_ai = source;
            session.state.players[0].tool_inventory[6] = 1;
            session.state.land_count = 2;
            session.state.tool_land_x[0] = 647;
            session.state.tool_land_y[0] = 659;
            session.state.tool_land_x[1] = 724;
            session.state.tool_land_y[1] = 692;
            session.state.land_level[0] = session.state.land_level[1] = 3;
            session.presentation.map_direction = direction;
            /* Selection can leave a scrolled camera. Original 0x446FBC
             * always refocuses the target before scanning the window. */
            session.presentation.has_view_override = 1;
            session.presentation.view_world_x = 1500;
            session.presentation.view_world_y = 1500;
            command = rich4_game_command_use_tool(
                (GameCommandSource)source, 0,
                rich4_game_domain_ref(GAME_DOMAIN_TOOL, 6), 0,
                rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1));
            command = rich4_game_command_with_missile_direction(
                command, direction ^ 1);
            assert(rich4_app_apply_game_command(&session, &command, &events));
            assert(session.state.land_level[0] == 2);
            /* Original IDB: direction 0 puts the second anchor at dx=100
             * (excluded); direction 1 projects it inside the square. */
            assert(session.state.land_level[1] == (direction == 0 ? 3 : 2));
        }
    }
}

static void edge_scroll_is_owned_by_the_selector(void)
{
    static GameSession session;
    ToolPanelState panel = {0};
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].node = 1;
    session.state.tool_route_node_count = 2;
    session.state.tool_route_x[1] = session.state.tool_route_y[1] = 1000;
    for (int tool = 0; tool < GAME_TOOL_COUNT; ++tool) {
        for (int phase = TOOL_PANEL_CLOSED; phase <= TOOL_PANEL_WORKER_CHOICE; ++phase) {
            panel.selected_tool = tool;
            panel.phase = (ToolPanelPhase)phase;
            panel.edge_scroll.direction = 0;
            session.presentation.has_view_override = 0;
            int allowed = phase == TOOL_PANEL_MAP_TARGET && (tool == 6 || tool == 12);
            rich4_tool_panel_update_edge_scroll(&session, &panel, 0, 200, 0, 100);
            assert((panel.edge_scroll.direction != 0) == allowed);
            rich4_tool_panel_update_edge_scroll(&session, &panel, 0, 200, 0, 149);
            assert(!session.presentation.has_view_override);
            rich4_tool_panel_update_edge_scroll(&session, &panel, 0, 200, 0, 150);
            assert(session.presentation.has_view_override == allowed);
            if (allowed) assert(session.presentation.view_world_x == 988);
        }
    }
}

static void engineering_expires_on_seventh_owner_tick(void)
{
    static GameSession session;
    session.state.active = 1;
    session.state.player_count = 2;
    GamePlayerState *owner = &session.state.players[1];
    for (int has_car = 0; has_car <= 1; ++has_car) {
        owner->traffic_method = 0x1f;
        owner->ndices = 1;
        owner->engineering_saved_traffic_method = 2;
        owner->engineering_saved_ndices = 3;
        owner->tool_inventory[5] = has_car;
        for (int i = 0; i < 7; ++i) {
            rich4_app_update_player_status_countdowns(&session, 0, i);
            assert(owner->traffic_method == 0x1f - 4 * i);
            rich4_app_update_player_status_countdowns(&session, 1, i);
        }
        assert(owner->traffic_method == (has_car ? 2 : 0));
        assert(owner->ndices == (has_car ? 3 : 1));
        assert(!owner->tool_inventory[5]);
    }
}

static void worker_debits_before_the_mandatory_choice(void)
{
    static GameSession session;
    Rich4ToolPanelConfirmResult result;
    GameEventQueue events;
    session.state.active = session.state.player_count = session.state.facility_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].tool_inventory[8] = 1;
    session.state.tool_facility_x[0] = 720;
    session.state.tool_facility_y[0] = 640;
    ToolPanelState *panel = &session.presentation.tool_panel;
    rich4_tool_panel_open(panel, &session);
    panel->selected_tool = 8;
    panel->phase = TOOL_PANEL_MAP_TARGET;
    GameDomainRef target = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0);
    assert(rich4_tool_panel_confirm_target(&session, panel, target, 0, &result, NULL));
    assert(!result.command_ready && panel->phase == TOOL_PANEL_WORKER_CHOICE);
    assert(!session.state.players[0].tool_inventory[8] && session.presentation.view_world_x == 720);
    assert(rich4_tool_panel_worker_choice_at(389, 353) == 4);
    assert(rich4_tool_panel_worker_choice_at(389, 354) == -1);
    assert(rich4_tool_panel_cancel(&session, panel, NULL) && panel->phase == TOOL_PANEL_WORKER_CHOICE);
    assert(rich4_tool_panel_confirm_target(&session, panel, target, 4, &result, NULL));
    assert(result.command_ready && rich4_app_apply_game_command(&session, &result.command, &events));
    assert(!session.state.players[0].tool_inventory[8] && session.state.facility_type[0] == 4 &&
           session.state.facility_level[0] == 1);
}

static void rewind_after_remote_roll(void)
{
    static GameSession session;
    GameEventQueue events;
    GameDomainRef none = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    session.state.active = session.state.player_count = 1;
    GamePlayerState *p = &session.state.players[0];
    p->play_state = 1;
    p->traffic_method = 2;
    p->ndices = 3;
    p->tool_inventory[7] = 1;
    GameCommand roll = rich4_game_command_use_tool(GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 7), 5, none, none);
    assert(rich4_app_apply_game_command(&session, &roll, &events));
    assert(p->tool_inventory[7] == 0 && p->remote_dice_value == 5);
    rich4_dice_begin_roll(&session, 500);
    assert(session.presentation.dice.count == 1 && session.flow.dice_value == 5);
    assert(session.time_machine_state.players[0].tool_inventory[7] == 1);
    assert(session.time_machine_state.players[0].remote_dice_value == 0);
    /* Acquired after the snapshot. The restore still succeeds. */
    p->tool_inventory[9] = 1;
    session.presentation.dice.popup_active = 0;
    GameCommand rewind = rich4_game_command_use_tool(GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 9), 0, none, none);
    assert(rich4_app_apply_game_command(&session, &rewind, &events));
    assert(!p->tool_inventory[9] && p->tool_inventory[7] == 1 && !p->remote_dice_value);
}

int main(void)
{
    GameSession session;
    ToolPanelState panel;
    GameCommand command;
    Rich4AudioEventQueue audio_events;
    Rich4AudioEvent event;
    GameDomainRef target = rich4_game_domain_ref(
        GAME_DOMAIN_ROUTE_NODE, 4);
    ToolPanelState reset_panel;

    edge_scroll_is_owned_by_the_selector();
    engineering_expires_on_seventh_owner_tick();
    worker_debits_before_the_mandatory_choice();
    rewind_after_remote_roll();
    missile_uses_live_map_direction();
    memset(&reset_panel, 0xA5, sizeof(reset_panel));
    rich4_tool_panel_reset(&reset_panel);
    assert(reset_panel.phase == TOOL_PANEL_CLOSED);
    assert(reset_panel.selected_tool == -1 && reset_panel.hover == -1);
    assert(rich4_game_domain_ref_is_none(reset_panel.primary_target));

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].play_state = 1;
    session.state.players[0].tool_inventory[6] = 1;
    session.state.players[0].tool_inventory[7] = 1;
    {
        uint8_t legal[3] = {0, 1, 1};
        assert(rich4_tool_set_route_targets(
            &session.state, 3, legal, sizeof(legal)));
    }
    rich4_tool_panel_open(&panel, &session);
    assert(panel.phase == TOOL_PANEL_INVENTORY && panel.player == 0);
    assert(rich4_tool_inventory_slot_at(&session, &panel, 19, 135) == 6);
    assert(rich4_tool_inventory_slot_at(&session, &panel, 99, 135) == 7);
    assert(rich4_tool_inventory_slot_at(&session, &panel, 418, 302) == -1);
    assert(rich4_tool_panel_remote_dice_face_at(104, 314) == 0);
    assert(rich4_tool_panel_remote_dice_face_at(304, 314) == 5);
    assert(rich4_tool_panel_remote_dice_face_at(344, 314) == -1);
    assert(rich4_tool_panel_worker_choice_at(50, 286) == 0);
    assert(rich4_tool_panel_worker_choice_at(389, 352) == 4);
    assert(rich4_tool_panel_worker_choice_at(390, 286) == -1);

    session.state.players[0].tool_inventory[1] = 1;
    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_tool_panel_select_inventory(
        &session, &panel, 1, &audio_events));
    assert(rich4_audio_event_pop(&audio_events, &event));
    assert(!rich4_tool_panel_activate_selected(
        &session, &panel, 900, &audio_events));
    assert(panel.phase == TOOL_PANEL_SPEECH);
    assert(panel.phase_after_speech == TOOL_PANEL_MAP_TARGET);
    session.presentation.speech.active = 0;
    assert(rich4_tool_panel_finish_speech(
        &panel, &session, &(int){0}, &(int){0}));
    assert(panel.phase == TOOL_PANEL_MAP_TARGET);
    assert(rich4_tool_panel_cancel(&session, &panel, &audio_events));
    assert(panel.phase == TOOL_PANEL_INVENTORY);

    session.presentation.speech.last_text = NULL;
    rich4_audio_event_queue_reset(&audio_events);
    assert(rich4_tool_panel_select_inventory(
        &session, &panel, 1, &audio_events));
    assert(rich4_audio_event_pop(&audio_events, &event));
    assert(!rich4_tool_panel_activate_selected(
        &session, &panel, 950, &audio_events));
    if (panel.phase == TOOL_PANEL_SPEECH) {
        session.presentation.speech.active = 0;
        assert(rich4_tool_panel_finish_speech(
            &panel, &session, &(int){0}, &(int){0}));
    }
    assert(panel.phase == TOOL_PANEL_MAP_TARGET);
    {
        Rich4ToolPanelConfirmResult result;
        rich4_audio_event_queue_reset(&audio_events);
        session.presentation.dynamic_objects[0].node = 1;
        assert(rich4_tool_panel_confirm_target(
            &session, &panel,
            rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 1), 0,
            &result, &audio_events));
        assert(!result.command_ready);
        assert(rich4_audio_event_pop(&audio_events, &event) &&
               event.type == RICH4_AUDIO_EVENT_INVALID);
        session.presentation.dynamic_objects[0].node = 0;
        rich4_audio_event_queue_reset(&audio_events);
        assert(rich4_tool_panel_confirm_target(
            &session, &panel,
            rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 1), 0,
            &result, &audio_events));
        assert(result.command_ready);
        assert(rich4_audio_event_pop(&audio_events, &event) && event.type == RICH4_AUDIO_EVENT_TARGET_CONFIRM);
        assert(rich4_tool_panel_complete_command(
            &session, &panel, &result.command, 1, 975, &audio_events));
        assert(panel.phase == TOOL_PANEL_CLOSED);
        assert(!session.presentation.speech.active);
    }

    rich4_tool_panel_open(&panel, &session);

    rich4_audio_event_queue_init(&audio_events);
    assert(rich4_tool_panel_select_inventory(
        &session, &panel, 6, &audio_events));
    assert(rich4_audio_event_pop(&audio_events, &event));
    assert(event.type == RICH4_AUDIO_EVENT_OK);
    assert(!rich4_tool_panel_activate_selected(
        &session, &panel, 1000, &audio_events));
    if (panel.phase == TOOL_PANEL_SPEECH) {
        session.presentation.speech.active = 0;
        assert(rich4_tool_panel_finish_speech(
            &panel, &session, &(int){0}, &(int){0}));
    }
    assert(panel.phase == TOOL_PANEL_MAP_TARGET);
    assert(rich4_tool_panel_target_spec(&session, &panel).mode ==
           RICH4_TOOL_PANEL_TARGET_ANY);
    assert(rich4_tool_panel_build_command(&panel, target, 0, &command));
    assert(command.type == GAME_COMMAND_USE_TOOL &&
           command.subject.id == 6 && command.target.id == 4);
    assert(!rich4_tool_panel_complete_command(
        &session, &panel, &command, 0, 1100, &audio_events));
    assert(panel.phase == TOOL_PANEL_INVENTORY);
    assert(!rich4_audio_event_pop(&audio_events, &event));

    assert(rich4_tool_panel_select_inventory(
        &session, &panel, 7, &audio_events));
    rich4_tool_panel_activate_selected(&session, &panel, 2000, &audio_events);
    assert(panel.phase == TOOL_PANEL_SPEECH ||
           panel.phase == TOOL_PANEL_REMOTE_DICE);
    if (panel.phase == TOOL_PANEL_SPEECH) {
        session.presentation.speech.active = 0;
        assert(rich4_tool_panel_finish_speech(
            &panel, &session, &(int){0}, &(int){0}));
    }
    assert(panel.phase == TOOL_PANEL_REMOTE_DICE);
    assert(rich4_tool_panel_target_spec(&session, &panel).mode ==
           RICH4_TOOL_PANEL_TARGET_REMOTE_DICE);
    {
        Rich4ToolPanelConfirmResult result;
        rich4_audio_event_queue_init(&audio_events);
        assert(rich4_tool_panel_confirm_target(
            &session, &panel,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 4,
            &result, &audio_events));
        assert(result.command_ready &&
               result.command.type == GAME_COMMAND_USE_TOOL &&
               result.command.value == 4);
    }
    assert(rich4_tool_panel_cancel(&session, &panel, &audio_events));
    assert(panel.phase == TOOL_PANEL_INVENTORY);

    {
        static const int pre_target[] = {1, 2, 3, 6, 8, 10, 12};
        static const int post_apply[] = {0, 4, 5, 11};
        size_t i;
        int immediate_ready;
        for (i = 0; i < GAME_TOOL_COUNT; i++)
            session.state.players[0].tool_inventory[i] = 1;
        for (i = 0; i < sizeof(pre_target) / sizeof(pre_target[0]); i++) {
            session.presentation.dice.status_visible = 1;
            rich4_tool_panel_open(&panel, &session);
            assert(!session.presentation.dice.status_visible);
            session.presentation.speech.active = 0;
            session.presentation.speech.last_text = NULL;
            assert(rich4_tool_panel_select_inventory(
                &session, &panel, pre_target[i], NULL));
            assert(!rich4_tool_panel_activate_selected(
                &session, &panel, 3000 + (uint32_t)i, NULL));
            assert(!session.presentation.dice.status_visible);
            if (panel.phase == TOOL_PANEL_SPEECH) {
                session.presentation.speech.active = 0;
                immediate_ready = 0;
                assert(rich4_tool_panel_finish_speech(
                    &panel, &session, &(int){0}, &immediate_ready));
                assert(!immediate_ready);
            }
            assert(panel.phase == TOOL_PANEL_MAP_TARGET);
        }
        for (i = 0; i < 1; i++) {
            int tool = 9;
            rich4_tool_panel_open(&panel, &session);
            session.presentation.speech.active = 0;
            session.presentation.speech.last_text = NULL;
            assert(rich4_tool_panel_select_inventory(
                &session, &panel, tool, NULL));
            if (!rich4_tool_panel_activate_selected(
                    &session, &panel, 3100 + (uint32_t)i, NULL)) {
                session.presentation.speech.active = 0;
                immediate_ready = 0;
                assert(rich4_tool_panel_finish_speech(
                    &panel, &session, &(int){0}, &immediate_ready));
                assert(immediate_ready);
            }
            assert(panel.phase == TOOL_PANEL_IMMEDIATE_COMMAND);
        }
        for (i = 0; i < sizeof(post_apply) / sizeof(post_apply[0]); i++) {
            rich4_tool_panel_open(&panel, &session);
            assert(rich4_tool_panel_select_inventory(
                &session, &panel, post_apply[i], NULL));
            session.presentation.speech.active = 0;
            session.presentation.speech.last_text = NULL;
            assert(rich4_tool_panel_activate_selected(
                &session, &panel, 3200 + (uint32_t)i, NULL));
            assert(!session.presentation.speech.active);
            assert(rich4_tool_panel_build_command(&panel,
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, &command));
            assert(rich4_tool_panel_complete_command(
                &session, &panel, &command, 1, 3200 + (uint32_t)i, NULL));
            assert(session.presentation.speech.active);
            assert(panel.phase_after_speech == TOOL_PANEL_CLOSED);
            session.presentation.speech.active = 0;
            assert(rich4_tool_panel_finish_speech(&panel, &session, NULL, NULL));
        }
        session.presentation.dice.status_visible = 1;
        rich4_tool_panel_open(&panel, &session);
        assert(!session.presentation.dice.status_visible);
        assert(rich4_tool_panel_select_inventory(&session, &panel, 1, NULL));
        assert(!rich4_tool_panel_activate_selected(
            &session, &panel, 3300, NULL));
        assert(!session.presentation.dice.status_visible);
        if (panel.phase == TOOL_PANEL_SPEECH) {
            session.presentation.speech.active = 0;
            assert(rich4_tool_panel_finish_speech(&panel, &session, NULL, NULL));
        }
        assert(rich4_tool_panel_cancel(&session, &panel, NULL));
        assert(rich4_tool_panel_cancel(&session, &panel, NULL));
        assert(session.presentation.dice.status_visible);
    }

    puts("app tool-panel flow: ok");
    return 0;
}
