#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/tool_panel_flow.h"
#include "core/tool.h"

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
        assert(rich4_tool_panel_confirm_target(
            &session, &panel,
            rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 1), 0,
            &result, &audio_events));
        assert(result.command_ready);
        assert(!rich4_audio_event_pop(&audio_events, &event));
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
    assert(rich4_audio_event_pop(&audio_events, &event));
    assert(event.type == RICH4_AUDIO_EVENT_INVALID);

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
        }
        session.presentation.dice.status_visible = 1;
        rich4_tool_panel_open(&panel, &session);
        assert(!session.presentation.dice.status_visible);
        assert(rich4_tool_panel_select_inventory(&session, &panel, 1, NULL));
        assert(!rich4_tool_panel_activate_selected(
            &session, &panel, 3300, NULL));
        assert(!session.presentation.dice.status_visible);
        assert(rich4_tool_panel_cancel(&session, &panel, NULL));
        assert(rich4_tool_panel_cancel(&session, &panel, NULL));
        assert(session.presentation.dice.status_visible);
    }

    puts("app tool-panel flow: ok");
    return 0;
}
