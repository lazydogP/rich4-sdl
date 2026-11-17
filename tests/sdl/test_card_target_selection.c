#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "platform/sdl1/game_loop_panels.h"

static void demolition_targets_follow_top_object_id(void)
{
    static GameState state;
    GameDomainRef target;

    memset(&state, 0, sizeof(state));
    state.player_count = 2;
    state.land_count = 2;
    state.facility_count = 2;
    state.land_owner[0] = 1;
    state.land_level[0] = 2;
    state.land_owner[1] = 0;
    state.land_level[1] = 2;
    state.facility_owner[0] = 1;
    state.facility_level[0] = 1;
    state.facility_owner[1] = 1;

    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x7d1);
    assert(target.kind == GAME_DOMAIN_LAND && target.id == 0);
    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x7d2);
    assert(target.kind == GAME_DOMAIN_NONE);
    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0xfa1);
    assert(target.kind == GAME_DOMAIN_FACILITY && target.id == 0);
    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0xfa2);
    assert(target.kind == GAME_DOMAIN_NONE);

    state.tool_placed_objects[0].kind = 0x10;
    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x9100);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 0);
    state.tool_placed_objects[10].kind = 0x11;
    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x9b00);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 10);
    state.tool_placed_objects[20].kind = 0x12;
    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0xa500);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 20);
    target = game_card_panel_demolish_target_from_object_id(
        &state, 0, 0xe500);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 20);
    assert(game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x9200).kind == GAME_DOMAIN_NONE);

    /* God slot 15, tool robot, and an ordinary actor all occlude a
     * property but are not valid demolition targets. */
    assert(game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x9000).kind == GAME_DOMAIN_NONE);
    assert(game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x8100).kind == GAME_DOMAIN_NONE);
    assert(game_card_panel_demolish_target_from_object_id(
        &state, 0, 0x8001).kind == GAME_DOMAIN_NONE);
}

static void demolition_cursor_uses_selector_resources(void)
{
    static GamePreviewAssets assets;
    static GameSession session;
    NewGameSettings settings = {0};
    CardPanelState *panel = &session.presentation.card_panel;
    static const size_t edge_cursor[5] = {41, 34, 40, 38, 36};
    int direction;

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    panel->phase = CARD_PANEL_MAP_TARGET;
    panel->selected_card = 11;
    panel->player = 0;
    for (direction = 1; direction <= 4; direction++) {
        panel->edge_scroll.direction = direction;
        assert(game_card_panel_cursor_chunk_index(
            &assets, &settings, &session, panel, 220, 240, 41) ==
            edge_cursor[direction]);
    }
    panel->edge_scroll.direction = 0;
    assert(game_card_panel_cursor_chunk_index(
        &assets, &settings, &session, panel, 220, 240, 41) == 5);
    panel->selected_card = 10;
    assert(game_card_panel_cursor_chunk_index(
        &assets, &settings, &session, panel, 220, 240, 41) == 41);
}

int main(void)
{
    demolition_targets_follow_top_object_id();
    demolition_cursor_uses_selector_resources();
    puts("card demolition top-object targets and selector cursors: ok");
    return 0;
}
