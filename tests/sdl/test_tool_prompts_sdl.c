#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "app/tool_animation_flow.h"
#include "platform/sdl1/game_loop_mouse_button_down_input.h"
#include "platform/sdl1/game_loop_mouse_button_up_input.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_scene_view_builder.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/mkf.h"
#include <string.h>
#include "app/game_speech_flow.h"
#include "app/input_flow.h"
#include "app/map_input_flow.h"
#include "app/tool_panel_flow.h"
#include "core/tool.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_key_up_modals.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime_bootstrap.h"

static GameSession session;
static GamePreviewAssets assets;
static NewGameSettings settings;

static int dice_panel_visible(void)
{
    GameSceneView scene;
    assert(build_game_session_render_view(&assets, &settings, &session, &scene));
    return scene.game.dice.status_visible;
}

static GameCommand setup(int tool, int human)
{
    uint8_t legal[4] = {0, 1, 1, 1};
    int16_t x[4] = {0, 0, 50, 100}, y[4] = {0};
    uint16_t next[4][4] = {{0}, {2}, {3,1}, {2}};
    GameCommand command;
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.price_index = 1;
    session.state.players[0].play_state = human ? 1 : 2;
    session.state.players[0].is_ai = !human;
    session.state.players[0].node = 1;
    session.state.players[0].next_node = 2;
    session.state.players[0].ndices = 1;
    session.state.players[0].attached_timed_bomb_slot = -1;
    session.state.players[0].tool_inventory[tool] = 1;
    assert(rich4_tool_set_route_targets(&session.state, 4, legal, sizeof(legal)));
    assert(rich4_tool_set_map_geometry(&session.state, 4, x, y, next,
                                       0, NULL, NULL, 0, NULL, NULL));
    command = rich4_game_command_use_tool(
        human ? GAME_COMMAND_SOURCE_HUMAN : GAME_COMMAND_SOURCE_AI, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool), tool == 7 ? 4 : 0,
        rich4_game_domain_ref(tool == 1 ? GAME_DOMAIN_ROUTE_NODE : GAME_DOMAIN_NONE,
                             tool == 1 ? 2 : -1),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1));
    return command;
}

static void missile_frames_use_the_original_map(void)
{
    Rich4Mkf *mkf;
    assert(rich4_mkf_open("assets/original/Data.mkf", &mkf) == RICH4_MKF_OK);
    for (int nuclear = 0; nuclear <= 1; ++nuclear) {
        int tool = nuclear ? 12 : 6, resource = nuclear ? 530 : 528;
        GameCommand command = setup(tool, 1);
        GameSceneView scene;
        Rich4ToolAnimationCompletion completion;
        MapLandRecord lands[2] = {0};
        assets.map.maps[0].lands = lands;
        assets.map.maps[0].land_count = 2;
        session.state.land_count = 1;
        session.state.land_level[0] = 3;
        session.state.tool_land_x[0] = lands[1].x = 1024;
        session.state.tool_land_y[0] = lands[1].y = 1024;
        command.target = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
        assert(rich4_mkf_read_resource(mkf, resource, &assets.tool.tool_effect_data[0],
            &assets.tool.tool_effect_size[0], NULL) == RICH4_MKF_OK);
        assets.tool.tool_effect_resource[0] = resource;
        assets.tool.has_tool_effect[0] = 1;
        int frames = game_graph_frame_count_data(assets.tool.tool_effect_data[0], assets.tool.tool_effect_size[0]);
        int delay = game_graph_frame_delay_ms_data(assets.tool.tool_effect_data[0], assets.tool.tool_effect_size[0]);
        Rich4AppToolbarAction open_tools = rich4_app_top_panel_action_at(300, 20);
        assert(frames == (nuclear ? 26 : 19) && delay == 114);
        session.presentation.dice.status_visible = 1;
        assert(rich4_app_apply_top_panel_action(&session, &open_tools));
        assert(game_session_apply_command(&session, &assets, &settings, &command, 100));
        assert(rich4_tool_panel_complete_command(&session,
            &session.presentation.tool_panel, &command, 1, 100, NULL));
        assert(session.presentation.tool_animation_count == 1 && session.missile_scene_active);
        assert(!rich4_app_apply_top_panel_action(&session, &open_tools));
        for (int i = 0; i < frames; ++i) {
            assert(rich4_tool_animation_update(&session, frames, delay, 100 + i * delay, NULL, &completion));
            assert(build_game_session_render_view(&assets, &settings, &session, &scene));
            assert(scene.game.lands[0].level == (i < 8 ? 3 : nuclear ? 0 : 2));
            assert(scene.game.viewport.center_world_x == 1024);
            assert(!scene.game.dice.status_visible);
        }
        assert(!rich4_tool_animation_update(&session, frames, delay, 100 + frames * delay, NULL, &completion));
        assert(!session.missile_scene_active && !session.presentation.has_view_override);
        /* A follow-up reaction can outlive the effect queue. */
        session.presentation.speech.active = 1;
        (void)rich4_tool_panel_finish_speech(&session.presentation.tool_panel,
                                            &session, NULL, NULL);
        assert(!dice_panel_visible());
        session.presentation.speech.active = 0;
        if (nuclear) session.state.players[0].days_in_hospital = 3;
        (void)rich4_tool_panel_finish_speech(&session.presentation.tool_panel,
                                            &session, NULL, NULL);
        assert(dice_panel_visible() == !nuclear);
        if (nuclear)
            assert(session.presentation.actors[0].dirty_flags & GAME_PLAYER_DIRTY_TURN);
        free(assets.tool.tool_effect_data[0]);
        memset(&assets.tool, 0, sizeof(assets.tool));
        memset(&assets.map.maps[0], 0, sizeof(assets.map.maps[0]));
    }
    rich4_mkf_close(mkf);
}

static void physical_selector_releases(void)
{
    Rich4SceneId scene = RICH4_SCENE_GAME;
    VirtualCursor cursor = {0};
    int held = 0;
    ToolPanelState *panel = &session.presentation.tool_panel;
    GameMouseButtonDownInputContext down = {0};
    GameMouseButtonUpInputContext up = {0};
    down.app_state = &scene;
    up.app_state = scene;
    down.session = up.session = &session;
    down.settings = up.settings = &settings;
    down.assets = up.assets = &assets;
    down.tool_panel = up.tool_panel = panel;
    down.cursor = up.cursor = &cursor;
    down.mouse_left_down = up.mouse_left_down = &held;
    for (int tool = 0; tool < GAME_TOOL_COUNT; ++tool) {
        setup(tool, 1);
        session.state.tool_route_x[1] = session.state.tool_route_y[1] = 1000;
        session.presentation.dice.status_visible = 1;
        rich4_tool_panel_open(panel, &session);
        panel->selected_tool = tool;
        panel->phase = tool == 7 ? TOOL_PANEL_REMOTE_DICE : TOOL_PANEL_MAP_TARGET;
        down.button = up.button = SDL_BUTTON_RIGHT;
        cursor.x = 0; cursor.y = 200;
        assert(game_handle_mouse_button_down_input(&down) && !cursor.cancel_pressed);
        assert(panel->phase != TOOL_PANEL_INVENTORY);
        assert(game_handle_mouse_button_up_input(&up));
        if (tool == 6 || tool == 12) {
            assert(panel->phase == TOOL_PANEL_MAP_TARGET && panel->edge_scroll.direction == 2);
            cursor.x = 200;
            assert(game_handle_mouse_button_up_input(&up));
        }
        assert(panel->phase == TOOL_PANEL_INVENTORY && session.state.players[0].tool_inventory[tool] == 1);
        assert(!dice_panel_visible());
        assert(game_handle_mouse_button_up_input(&up));
        assert(panel->phase == TOOL_PANEL_CLOSED && dice_panel_visible());
    }
    for (int stopped = 0; stopped <= 1; ++stopped) {
        setup(7, 1);
        session.state.players[0].days_stopping = stopped;
        session.presentation.dice.status_visible = 1;
        rich4_tool_panel_open(panel, &session);
        panel->selected_tool = 7; panel->phase = TOOL_PANEL_REMOTE_DICE;
        down.button = up.button = SDL_BUTTON_LEFT;
        cursor.x = 105; cursor.y = 320;
        assert(game_handle_mouse_button_down_input(&down));
        assert(held && !cursor.confirm_pressed && session.state.players[0].tool_inventory[7] == 1);
        cursor.x = 305;
        assert(game_handle_mouse_button_up_input(&up));
        assert(!held && session.state.players[0].remote_dice_value == 6 && panel->phase == TOOL_PANEL_CLOSED);
        assert(session.flow.turn_flow_active && dice_panel_visible() == stopped);
    }
}

int main(void)
{
    physical_selector_releases();
    missile_frames_use_the_original_map();
    GameCommand command = setup(1, 0);
    assert(rich4_ai_tool_begin(&session, &command, 100));
    assert(!rich4_app_input_ui_active(RICH4_SCENE_GAME, &settings, &session, 0));
    assert(game_update_ai_tool(&session, &assets, &settings, NULL, 1599));
    assert(session.state.players[0].tool_inventory[1] == 1);
    assert(!session.presentation.speech.active);
    assert(game_update_ai_tool(&session, &assets, &settings, NULL, 1600));
    assert(session.presentation.speech.active);
    assert(game_update_ai_tool(&session, &assets, &settings, NULL, 2599));
    assert(session.state.players[0].tool_inventory[1] == 1);
    assert(!session.flow.turn_flow_active);
    /* Voice disabled: the full one-second text wait still applies. */
    assert(game_update_ai_tool(&session, &assets, &settings, NULL, 2600));
    assert(session.state.players[0].tool_inventory[1] == 0);
    assert(session.presentation.tool_placement_animation.active);
    assert(!game_update_ai_tool(&session, &assets, &settings, NULL, 2700));
    assert(!session.flow.turn_flow_active);
    session.presentation.tool_placement_animation.active = 0;
    assert(game_update_ai_tool(&session, &assets, &settings, NULL, 3000));
    assert(session.presentation.ai_tool.phase == GAME_AI_TOOL_NONE);
    assert(session.flow.turn_flow_active);
    assert(!game_update_ai_tool(&session, &assets, &settings, NULL, 3100));

    command = setup(7, 0);
    assert(rich4_ai_tool_begin(&session, &command, 100));
    game_handle_mouse_button_up_modals(RICH4_SCENE_GAME, &session, NULL,
                                       SDL_BUTTON_RIGHT);
    assert(game_update_ai_tool(&session, &assets, &settings, NULL, 101));
    assert(!session.presentation.speech.active);
    assert(session.state.players[0].tool_inventory[7] == 0);
    assert(session.state.players[0].remote_dice_value == 4);

    command = setup(4, 0);
    assert(rich4_ai_tool_begin(&session, &command, 100));
    assert(game_update_ai_tool(&session, &assets, &settings, NULL, 1600));
    assert(session.state.players[0].traffic_method == 1);
    assert(session.presentation.speech.active);
    assert(!session.flow.turn_flow_active);

    command = setup(0, 1);
    session.presentation.dice.status_visible = 1;
    rich4_tool_panel_open(&session.presentation.tool_panel, &session);
    assert(rich4_tool_panel_select_inventory(&session,
        &session.presentation.tool_panel, 0, NULL));
    assert(rich4_tool_panel_activate_selected(&session,
        &session.presentation.tool_panel, 100, NULL));
    assert(!session.presentation.speech.active);
    assert(game_session_apply_command(&session, &assets, &settings, &command, 100));
    assert(rich4_tool_panel_complete_command(&session,
        &session.presentation.tool_panel, &command, 1, 100, NULL));
    assert(session.state.players[0].tool_inventory[0] == 0);
    assert(session.presentation.speech.active);
    assert(!session.presentation.tool_robot_motion.initialized);
    assert(!dice_panel_visible());
    assert(rich4_presentation_update_step(&session.presentation, &session.state)
           == RICH4_PRESENTATION_UPDATE_PROPERTY_SPEECH);
    uint32_t now = 1100;
    for (int frame = 0; frame < 250 && session.state.tool_robot.active; ++frame) {
        now += 100;
        update_game_session(&session, &assets, &settings, NULL, NULL, now, 0);
        (void)rich4_tool_panel_finish_speech(&session.presentation.tool_panel,
                                            &session, NULL, NULL);
        assert(!dice_panel_visible());
    }
    assert(!session.state.tool_robot.active && session.state.current_player == 0);
    update_game_session(&session, &assets, &settings, NULL, NULL, now + 100, 0);
    assert(dice_panel_visible());

    command = setup(9, 1);
    session.time_machine_valid = 1;
    session.time_machine_state = session.state;
    session.time_machine_state.ai_rng_state = 1;
    session.state.ai_rng_state = UINT32_C(2524885223);
    session.time_machine_flow = session.flow;
    session.time_machine_presentation = session.presentation;
    session.time_machine_presentation.ai_tool.phase = GAME_AI_TOOL_COMMAND;
    assert(game_session_apply_command(&session, &assets, &settings, &command, 100));
    assert(session.presentation.ai_tool.phase == GAME_AI_TOOL_NONE);
    assert(session.state.players[0].tool_inventory[9] == 0);
    assert(session.state.ai_rng_state == UINT32_C(2524885223));
    puts("SDL tool prompts: notice -> speech -> effect -> roll: ok");
    return 0;
}
