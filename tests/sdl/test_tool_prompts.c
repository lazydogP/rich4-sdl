#define SDL_MAIN_HANDLED
#include <SDL/SDL.h>
#undef main
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app/game_speech_flow.h"
#include "app/input_flow.h"
#include "app/tool_panel_flow.h"
#include "core/tool.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_key_up_modals.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime_bootstrap.h"

static GameSession session;
static GamePreviewAssets assets;
static NewGameSettings settings;

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

int main(void)
{
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
    assert(rich4_presentation_update_step(&session.presentation, &session.state)
           == RICH4_PRESENTATION_UPDATE_PROPERTY_SPEECH);
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
