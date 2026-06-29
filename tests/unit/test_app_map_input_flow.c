#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/card_panel_flow.h"
#include "app/map_input_flow.h"
#include "app/tool_panel_flow.h"

int main(void)
{
    GameSession session;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;

    {
        Rich4MapEdgeScrollState edge = {0, 0, 0};
        session.presentation.map_direction = 0;
        session.presentation.view_world_x = 1000;
        session.presentation.view_world_y = 1000;
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 0, 200, 16, 100));
        assert(edge.direction == 2);
        assert(edge.speed == 8);
        assert(session.presentation.view_world_x == 1000);
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 0, 200, 16, 150));
        assert(edge.speed == 12);
        assert(session.presentation.view_world_x == 988);
        assert(session.presentation.has_view_override == 1);
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 200, 55, 16, 200));
        assert(edge.direction == 1);
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 200, 56, 16, 201) == 0);
        assert(edge.direction == 0);
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 1, 200, 0, 300) == 0);
        assert(edge.direction == 0);
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 0, 200, 0, 301));
        assert(edge.direction == 2);
        session.presentation.view_world_x = 220;
        rich4_app_map_edge_scroll_reset(&edge);
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 0, 200, 16, 400) == 0);
        assert(edge.direction == 0);
    }

    {
        Rich4MapEdgeScrollState edge = {0, 0, 0};
        int rotation;
        for (rotation = 0; rotation < 8; rotation++) {
            session.presentation.map_direction = rotation;
            session.presentation.view_world_x = 1000;
            session.presentation.view_world_y = 1000;
            session.presentation.has_view_override = 0;
            session.presentation.top_button_hover =
                rich4_app_top_panel_button_at(80, 10);
            assert(session.presentation.top_button_hover == 2);
            assert(!rich4_app_map_edge_scroll_update(
                &session, &edge, 80, 10, 16, 500 + rotation * 100));
            assert(!rich4_app_map_edge_scroll_update(
                &session, &edge, 80, 10, 16, 550 + rotation * 100));
            assert(edge.direction == 0);
            assert(session.presentation.map_direction == rotation);
            assert(session.presentation.view_world_x == 1000);
            assert(session.presentation.view_world_y == 1000);
            assert(session.presentation.has_view_override == 0);
        }
        assert(rich4_app_map_edge_scroll_update(
            &session, &edge, 80, 40, 16, 1400));
        assert(edge.direction == 1);
        session.presentation.top_button_hover = -1;
    }

    {
        ToolPanelState tool = {0};
        CardPanelState card = {0};
        tool.phase = TOOL_PANEL_REMOTE_DICE;
        tool.edge_scroll.direction = 2;
        rich4_tool_panel_update_edge_scroll(
            &session, &tool, 0, 200, 16, 500);
        assert(tool.edge_scroll.direction == 0);
        card.phase = CARD_PANEL_STOCK_TARGET;
        card.edge_scroll.direction = 4;
        rich4_card_panel_update_edge_scroll(
            &session, &card, 639, 200, 16, 500);
        assert(card.edge_scroll.direction == 0);
    }

    assert(rich4_app_minimap_y_for_view(1) == 280);
    assert(rich4_app_minimap_y_for_view(2) == 80);
    assert(rich4_app_minimap_rotate_button_at(443, 283, 1) == 1);
    assert(rich4_app_minimap_rotate_button_at(468, 283, 1) == 2);
    assert(rich4_app_info_tab_at(616, 69) == 0);
    assert(rich4_app_info_tab_at(616, 70) == 1);
    assert(rich4_app_top_panel_button_at(80, 10) == 2);
    assert(rich4_app_player_info_tab_at(12, 282) == 0);
    assert(rich4_app_player_info_tab_at(109, 450) == 2);
    assert(rich4_app_player_info_tab_at(110, 282) == -1);
    assert(rich4_app_player_info_close_at(492, 9));
    assert(!rich4_app_player_info_close_at(603, 9));
    assert(rich4_app_player_info_property_filter_at(120, 64) == 0);
    assert(rich4_app_player_info_property_filter_at(495, 97) == 4);
    assert(rich4_app_player_info_property_filter_at(496, 97) == -1);
    assert(rich4_app_player_info_property_page_button_at(594, 370) == -1);
    assert(rich4_app_player_info_property_page_button_at(594, 418) == 1);
    {
        Rich4AppToolbarAction action = {
            RICH4_APP_TOOLBAR_INFO_TAB, 0
        };
        assert(action.type == RICH4_APP_TOOLBAR_INFO_TAB);
        assert(action.value == 0);
        assert(rich4_app_apply_top_panel_action(&session, &action));
        assert(session.presentation.active_info_tab == 0);
        action = rich4_app_top_panel_action_at(80, 10);
        assert(action.type == RICH4_APP_TOOLBAR_OPEN_AI_SETTINGS);
        assert(action.value == RICH4_APP_TOP_PANEL_AI_SETTINGS);
        session.presentation.top_button_hover = 2;
        assert(rich4_app_apply_top_panel_action(&session, &action));
        assert(session.presentation.top_button_hover == -1);
        session.state.player_count = 2;
        session.state.current_player = 1;
        session.state.players[0].play_state = 1;
        session.state.players[1].play_state = 0;
        action = rich4_app_top_panel_action_at(240, 10);
        assert(action.type == RICH4_APP_TOOLBAR_OPEN_PLAYER_INFO);
        assert(rich4_app_apply_top_panel_action(&session, &action));
        assert(session.presentation.player_info.active);
        assert(session.presentation.player_info.selected_player == 0);
        assert(session.presentation.player_info.tab == 0);
        assert(rich4_app_player_info_player_is_visible(&session, 0));
        assert(!rich4_app_player_info_player_is_visible(&session, 1));
        assert(rich4_app_player_info_player_at(&session, 16, 14) == 0);
        session.state.players[0].play_state = 0;
        session.state.players[1].play_state = 1;
        assert(rich4_app_player_info_player_at(&session, 16, 14) == 1);
        session.presentation.actors[1].first_turn_landing_pending = 1;
        assert(!rich4_app_player_info_player_is_visible(&session, 1));
        assert(rich4_app_player_info_player_at(&session, 16, 14) == -1);
        session.presentation.actors[1].first_turn_landing_pending = 0;
        session.state.land_count = 2;
        session.state.land_owner[0] = 1;
        session.state.land_owner[1] = 1;
        session.state.land_type[0] = 0;
        session.state.land_level[0] = 1;
        session.state.land_type[1] = 1;
        session.state.land_level[1] = 1;
        session.state.facility_count = 1;
        session.state.facility_owner[0] = 1;
        assert(rich4_app_player_info_property_count(&session, 1, 0) == 3);
        assert(rich4_app_player_info_property_count(&session, 1, 1) == 2);
        assert(rich4_app_player_info_property_count(&session, 1, 2) == 1);
        assert(rich4_app_player_info_property_count(&session, 1, 3) == 1);
        assert(rich4_app_player_info_property_count(&session, 1, 4) == 1);
        rich4_app_player_info_press(&session, 16, 14);
        assert(session.presentation.player_info.selected_player == 1);
        assert(session.presentation.player_info.pressed_control == 7);
        rich4_app_player_info_release(&session);
        rich4_app_player_info_press(&session, 16, 14);
        assert(session.presentation.player_info.pressed_control == 0);
        session.presentation.player_info.tab = 0;
        rich4_app_player_info_press(&session, 20, 350);
        assert(session.presentation.player_info.pressed_control == 3);
        assert(session.presentation.player_info.tab == 0);
        rich4_app_player_info_release(&session);
        assert(session.presentation.player_info.pressed_control == 0);
        assert(session.presentation.player_info.tab == 1);
        session.presentation.player_info.property_page = 1;
        rich4_app_player_info_press(&session, 594, 370);
        assert(session.presentation.player_info.pressed_control == 5);
        assert(session.presentation.player_info.property_page == 1);
        rich4_app_player_info_release(&session);
        assert(session.presentation.player_info.property_page == 0);
        rich4_app_player_info_press(&session, 492, 9);
        assert(session.presentation.player_info.active);
        assert(session.presentation.player_info.pressed_control == 1);
        rich4_app_player_info_release(&session);
        assert(!session.presentation.player_info.active);
        session.state.players[0].play_state = 0;
        session.state.players[1].play_state = 0;
        assert(!rich4_app_apply_top_panel_action(&session, &action));
        action = rich4_app_top_panel_action_at(443, 283);
        assert(action.type == RICH4_APP_TOOLBAR_NO_ACTION);
        assert(rich4_app_minimap_rotate_button_at(443, 283, 1) == 1);
        assert(rich4_app_minimap_rotate_button_at(80, 10, 1) == 0);
        session.presentation.map_direction = 0;
        rich4_app_rotate_map(&session, 1);
        assert(session.presentation.map_direction == 7);
        session.presentation.map_direction = 0;
        action = rich4_app_top_panel_action_at(300, 200);
        assert(action.type == RICH4_APP_TOOLBAR_NO_ACTION);
        assert(!rich4_app_apply_top_panel_action(&session, &action));
    }

    rich4_app_rotate_map(&session, 2);
    assert(session.presentation.map_direction == 1);
    rich4_app_rotate_map(&session, 1);
    assert(session.presentation.map_direction == 0);
    rich4_app_switch_info_tab(&session, 3);
    assert(session.presentation.active_info_tab == 3);
    rich4_app_set_view_from_minimap(&session, 540, 380, 1);
    assert(session.presentation.has_view_override == 1);
    assert(session.presentation.view_world_x >= 220);
    assert(session.presentation.view_world_y >= 220);
    assert(rich4_app_begin_minimap_drag(&session, 540, 380, 1));
    assert(session.presentation.dragging_minimap == 1);
    assert(rich4_app_update_minimap_drag(&session, 541, 381, 1));
    assert(!rich4_app_update_minimap_drag(&session, 100, 100, 1));
    assert(session.presentation.dragging_minimap == 0);
    assert(rich4_app_begin_minimap_drag(&session, 541, 381, 1));
    rich4_app_end_minimap_drag(&session);
    assert(session.presentation.dragging_minimap == 0);
    assert(!rich4_app_begin_minimap_drag(&session, 445, 283, 1));
    assert(rich4_app_begin_minimap_drag(&session, 540, 380, 1));
    rich4_app_end_minimap_drag(&session);
    assert(session.presentation.dragging_minimap == 0);
    puts("app map input flow: ok");
    return 0;
}
