#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "app/card_panel_flow.h"
#include "app/game_speech_flow.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "resource/mkf.h"
#include <string.h>

#include "platform/sdl1/game_loop_panels.h"
#include "core/card.h"

static void demolition_targets_follow_top_object_id(void)
{
    static GameState state;
    GameDomainRef target, none = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    uint32_t mask = rich4_card_map_mask(11, GAME_DOMAIN_NONE);

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

    target = rich4_card_map_hit(
        &state, 0, mask, none, 0x7d1);
    assert(target.kind == GAME_DOMAIN_LAND && target.id == 0);
    target = rich4_card_map_hit(
        &state, 0, mask, none, 0x7d2);
    assert(target.kind == GAME_DOMAIN_NONE);
    target = rich4_card_map_hit(
        &state, 0, mask, none, 0xfa1);
    assert(target.kind == GAME_DOMAIN_FACILITY && target.id == 0);
    target = rich4_card_map_hit(
        &state, 0, mask, none, 0xfa2);
    assert(target.kind == GAME_DOMAIN_NONE);

    state.tool_placed_objects[0].kind = 0x10;
    target = rich4_card_map_hit(
        &state, 0, mask, none, 0x9100);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 0);
    state.tool_placed_objects[10].kind = 0x11;
    target = rich4_card_map_hit(
        &state, 0, mask, none, 0x9b00);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 10);
    state.tool_placed_objects[20].kind = 0x12;
    target = rich4_card_map_hit(
        &state, 0, mask, none, 0xa500);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 20);
    target = rich4_card_map_hit(
        &state, 0, mask, none, 0xe500);
    assert(target.kind == GAME_DOMAIN_NONE);
    target = rich4_card_map_hit(&state, 0, mask, none, 0xa501);
    assert(target.kind == GAME_DOMAIN_MAP_OBJECT && target.id == 20);
    assert(rich4_card_map_hit(
        &state, 0, mask, none, 0x9200).kind == GAME_DOMAIN_NONE);

    /* God slot 15, tool robot, and an ordinary actor all occlude a
     * property but are not valid demolition targets. */
    assert(rich4_card_map_hit(
        &state, 0, mask, none, 0x9000).kind == GAME_DOMAIN_NONE);
    assert(rich4_card_map_hit(
        &state, 0, mask, none, 0x8100).kind == GAME_DOMAIN_NONE);
    assert(rich4_card_map_hit(
        &state, 0, mask, none, 0x8001).kind == GAME_DOMAIN_NONE);
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
        &assets, &settings, &session, panel, 220, 240, 41) == 5);
}

static void all_card_selectors(void)
{
    static GameState state;
    static const uint32_t masks[GAME_CARD_COUNT] = {
        0,0x0e0c0410,0,0x0e0c0202,0x0e0c0202,0x0e0c0010,0,0,0x0e0c0006,0x0e0c0006,
        0x0e0c0506,0x0e0c0626,0x0e0c0410,0x0e0c0010,0,0x0e0c0710,0x0e0c0710,0,0,0,
        0,0,0,0,0,0x0e0c0410,0x0e0c0006,0x0e0c0006,0x0e0c0410,0x0e0c0010
    };
    GameDomainRef source = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
    state.player_count = state.land_count = state.facility_count = 2;
    state.players[0].play_state = state.players[1].play_state = 1;
    state.land_level[0] = state.land_level[1] = 2;
    state.land_owner[0] = 0;
    state.land_owner[1] = -1;
    for (int card = 0; card < GAME_CARD_COUNT; ++card) {
        uint32_t mask = rich4_card_map_mask(card, source.kind);
        assert(mask == masks[card]);
        if ((mask & 0x10) != 0) {
            int rule = (mask >> 8) & 255;
            assert(rich4_card_map_hit(&state,0,mask,source,0x8002).id == 1);
            assert(rich4_card_map_hit(&state,0,mask,source,0x8003).kind ==
                (rule == 0 ? GAME_DOMAIN_PLAYER : GAME_DOMAIN_NONE));
            assert(rich4_card_map_hit(&state,0,mask,source,0x8010).kind ==
                (rule == 4 ? GAME_DOMAIN_NONE : GAME_DOMAIN_SPECIAL_PLAYER));
            assert(rich4_card_map_hit(&state,0,mask,source,2001).kind == GAME_DOMAIN_NONE);
        } else if (mask) {
            assert(rich4_card_map_hit(&state,0,mask,source,0x8001).kind == GAME_DOMAIN_NONE);
            assert(rich4_card_map_hit(&state,0,mask,source,2002).id == 1);
            assert(rich4_card_map_hit(&state,0,mask,source,2001).kind ==
                (mask == 0x0e0c0006 ? GAME_DOMAIN_LAND : GAME_DOMAIN_NONE));
            if (card == 3 || card == 4) {
                assert(rich4_card_map_hit(&state,0,mask,source,4002).kind == GAME_DOMAIN_NONE);
                GameDomainRef facility = rich4_game_domain_ref(GAME_DOMAIN_FACILITY,1);
                mask = rich4_card_map_mask(card, facility.kind);
                assert(mask == 0x0e0c0204);
                assert(rich4_card_map_hit(&state,0,mask,facility,4001).kind == GAME_DOMAIN_FACILITY);
                assert(rich4_card_map_hit(&state,0,mask,facility,4002).kind == GAME_DOMAIN_NONE);
                assert(!rich4_card_map_mask(card, GAME_DOMAIN_NONE));
            }
        }
    }
}

static void property_pixels_drive_hover_and_release(void)
{
    static GameSession session;
    static GamePreviewAssets assets;
    MapLandRecord lands[2] = {0};
    MapRouteNode routes[2] = {0};
    NewGameSettings settings = {0};
    CardPanelState *panel = &session.presentation.card_panel;
    MapData *map = &assets.map.maps[0];
    Rich4Mkf *mkf = NULL;
    unsigned char *pixels = NULL;
    size_t size;
    Rich4SprSmpChunk mask;
    GameSceneView view;
    int cx, cy, opaque_x = -1, opaque_y = -1, clear_x = -1, clear_y = -1;

    assert(rich4_mkf_open("assets/original/Map.mkf", &mkf) == RICH4_MKF_OK);
    assert(rich4_mkf_read_resource(mkf, 26, &pixels, &size, NULL) == RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(pixels, size, &assets.map.map_common_spr[2]));
    assert(rich4_spr_smp_get_chunk(pixels, size, &assets.map.map_common_spr[2], 0, &mask) == RICH4_SPR_SMP_OK);
    assets.map.has_map_common_graph[2] = 1;
    assets.map.map_common_data[2] = pixels;
    assets.map.map_common_size[2] = size;
    map->lands = lands;
    map->land_count = 2;
    lands[1].x = 352;
    lands[1].y = 640;
    session.state.active = session.state.player_count = session.state.land_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].card_inventory[0] = 27;
    session.state.land_owner[0] = -1;
    rich4_card_panel_open(panel, &session);
    panel->selected_card = 26;
    panel->phase = CARD_PANEL_MAP_TARGET;
    game_focus_view_at(&session, 352, 640);
    assert(build_game_session_render_view(&assets, &settings, &session, &view));
    rich4_game_viewport_project(&view.game.viewport, 352, 640, &cx, &cy);
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            int dx = x - mask.x, dy = y - mask.y;
            if (pixels[mask.data_offset + y * mask.width + x]) {
                if (dx * dx + dy * dy > 24 * 24) {
                    opaque_x = cx + dx;
                    opaque_y = cy + dy;
                }
            } else if (dx * dx + dy * dy <= 24 * 24) {
                clear_x = cx + dx;
                clear_y = cy + dy;
            }
        }
    }
    assert(opaque_x >= 0 && clear_x >= 0);
    assert(game_card_panel_cursor_chunk_index(&assets, &settings, &session, panel, cx, cy, 41) == 12);
    routes[1].x = lands[1].x;
    routes[1].y = lands[1].y;
    map->routes = routes;
    map->route_count = 2;
    session.state.players[0].node = 1;
    session.presentation.actors[0].intro_visible = 1;
    assert(game_card_panel_cursor_chunk_index(&assets, &settings, &session, panel, cx, cy, 41) == 5);
    game_card_panel_confirm(&session, &assets, &settings, panel, cx, cy, 100, NULL);
    assert(panel->phase == CARD_PANEL_MAP_TARGET);
    panel->selected_card = 5; /* turn-around accepts the actor covering the land */
    assert(game_card_panel_cursor_chunk_index(&assets, &settings, &session, panel, cx, cy, 41) == 12);
    panel->selected_card = 26;
    assert(game_card_panel_cursor_chunk_index(&assets, &settings, &session, panel, clear_x, clear_y, 41) == 5);
    game_card_panel_confirm(&session, &assets, &settings, panel, clear_x, clear_y, 100, NULL);
    assert(panel->phase == CARD_PANEL_MAP_TARGET && session.state.players[0].card_inventory[0] == 27);
    assert(game_card_panel_cursor_chunk_index(&assets, &settings, &session, panel, opaque_x, opaque_y, 41) == 12);
    game_card_panel_confirm(&session, &assets, &settings, panel, opaque_x, opaque_y, 110, NULL);
    assert(panel->phase == CARD_PANEL_WAIT_HANDLER);
    assert(session.presentation.card_use.command.target.kind == GAME_DOMAIN_LAND);
    assert(session.presentation.card_use.command.target.id == 0 && session.state.card_amount[26] == 1);
    free(pixels);
    rich4_mkf_close(mkf);
}

int main(void)
{
    all_card_selectors();
    property_pixels_drive_hover_and_release();
    demolition_targets_follow_top_object_id();
    demolition_cursor_uses_selector_resources();
    puts("card masks, topmost pixel selection and selector cursors: ok");
    return 0;
}
