#include <assert.h>
#include <stdio.h>

#include "app/new_game_scene_flow.h"
#include "app/new_game_setup_flow.h"
#include "core/rng.h"

typedef struct SetupFrameProbe {
    int ok;
    int transition;
    int draws;
    int transition_draws;
    int transition_complete;
    int scene_ready;
} SetupFrameProbe;

static void probe_ok(void *opaque)
{
    ((SetupFrameProbe *)opaque)->ok++;
}

static void probe_transition(void *opaque)
{
    ((SetupFrameProbe *)opaque)->transition++;
}

static void probe_draw(void *opaque, int transition)
{
    SetupFrameProbe *probe = opaque;
    probe->draws++;
    if (transition) probe->transition_draws++;
}

static void probe_transition_complete(void *opaque)
{
    ((SetupFrameProbe *)opaque)->transition_complete++;
}

static void probe_scene_ready(void *opaque)
{
    ((SetupFrameProbe *)opaque)->scene_ready++;
}

int main(void)
{
    int ticks = 0;
    int i;
    int stage;
    int map;
    int character;
    int option;
    NewGameSettings settings;
    Rich4NewGameSetupFrameResult frame;
    GameSession session = { 0 };
    Rich4SceneMachine machine;
    Rich4SceneId app_state;
    Rich4AppNewGameSetupServices services;
    Rich4AppNewGameTransitionServices transition_services;
    SetupFrameProbe probe = { 0, 0, 0, 0, 0, 0 };

    for (stage = 0; stage < 2; stage++)
        for (map = 0; map < 4; map++)
            assert(rich4_setup_background_resource(stage, map) == stage * 4 + map);
    for (character = 0; character < 12; character++)
        for (option = 0; option < 3; option++)
            assert(rich4_setup_character_resource(character, option) ==
                   9 + character * 3 + option);
    assert(rich4_setup_background_resource(2, 0) == -1);
    assert(rich4_setup_character_resource(12, 0) == -1);
    for (i = 0; i < 10; i++) {
        assert(!rich4_setup_stage1_tick_due(&ticks));
    }
    assert(ticks == 10);
    assert(rich4_setup_stage1_tick_due(&ticks));
    assert(ticks == 0);
    assert(!rich4_setup_stage1_tick_due(NULL));

    assert(!rich4_setup_transition_should_complete(
        0, RICH4_SETUP_TRANSITION_INPUT_NONE));
    assert(rich4_setup_transition_should_complete(
        1, RICH4_SETUP_TRANSITION_INPUT_NONE));
    assert(rich4_setup_transition_should_complete(
        0, RICH4_SETUP_TRANSITION_INPUT_MOUSE_BUTTON_UP));
    assert(rich4_setup_transition_should_complete(
        0, RICH4_SETUP_TRANSITION_INPUT_KEY_DOWN));
    assert(rich4_setup_transition_should_complete(
        0, RICH4_SETUP_TRANSITION_INPUT_CONTROLLER_BUTTON_DOWN));

    rich4_new_game_settings_init(&settings, 1);
    assert(settings.stage == 1 && settings.map == 0);
    assert(settings.player_extra == 2 && settings.fund_index == 1);
    assert(settings.dropdown_row == -1 && settings.pressed_control == -1);
    assert(settings.selected_characters[0] == -1);
    rich4_new_game_set_selected_row(&settings, 5);
    assert(settings.selected_row == 5);
    settings.dropdown_row = 0;
    rich4_new_game_set_dropdown_hover(&settings, 2);
    assert(settings.dropdown_hover == 2);
    rich4_new_game_close_dropdown(&settings);
    assert(settings.dropdown_row == -1 && settings.dropdown_hover == -1);
    assert(rich4_new_game_apply_pointer_up(&settings, 8, 15) ==
           RICH4_NEW_GAME_SETUP_CHARACTER_TOGGLED);
    assert(settings.selected_count == 1 && settings.selected_characters[0] == 0);
    assert(rich4_new_game_apply_pointer_up(&settings, 8, 15) ==
           RICH4_NEW_GAME_SETUP_CHARACTER_TOGGLED);
    assert(settings.selected_count == 0);
    assert(rich4_new_game_apply_pointer_up(&settings, 544, 226) ==
           RICH4_NEW_GAME_SETUP_DROPDOWN_OPENED);
    assert(settings.dropdown_row == 0);
    assert(rich4_new_game_apply_pointer_up(&settings, 561, 251) ==
           RICH4_NEW_GAME_SETUP_DROPDOWN_CHANGED);
    assert(settings.dropdown_row == -1 && settings.player_extra == 0);
    assert(rich4_new_game_apply_pointer_up(&settings, 457, 63) ==
           RICH4_NEW_GAME_SETUP_MAP_CHANGED);
    assert(settings.map == 1);
    rich4_new_game_apply_pointer_down(&settings, 457, 63);
    assert(settings.pressed_control == 10);
    rich4_new_game_apply_pointer_down(&settings, 1, 1);
    assert(settings.pressed_control == -1);
    assert(rich4_new_game_character_at(8, 15) == 0);
    assert(rich4_new_game_character_at(80, 87) == 7);
    assert(rich4_new_game_character_at(440, 15) == -1);
    assert(rich4_new_game_control_at(456, 176) == 1);
    assert(rich4_new_game_control_at(602, 406) == 8);
    assert(rich4_new_game_control_at(625, 406) == -1);
    settings.dropdown_row = 0;
    assert(rich4_new_game_dropdown_option_at(&settings, 561, 251) == 0);
    assert(rich4_new_game_dropdown_option_at(&settings, 561, 320) == -1);
    settings.dropdown_row = -1;
    settings.player_extra = 0;
    assert(rich4_new_game_toggle_character(&settings, 0));
    assert(settings.selected_count == 1 && settings.selected_characters[0] == 0);
    assert(rich4_new_game_toggle_character(&settings, 0));
    assert(settings.selected_count == 0 && settings.character_used[0] == 0);
    settings.selected_count = 4;
    settings.selected_characters[3] = 3;
    settings.character_used[3] = GAME_SETUP_CHARACTER_SELECTED;
    rich4_new_game_settings_set(&settings, 2, 0);
    assert(settings.player_extra == 0 && settings.selected_count == 2);
    assert(settings.character_used[3] == 0);

    settings.player_extra = 1;
    settings.selected_count = 1;
    settings.selected_characters[0] = 0;
    settings.character_used[0] = GAME_SETUP_CHARACTER_SELECTED;
    settings.auto_fill_active = 1;
    settings.auto_fill_ticks = 10;
    settings.last_anim_tick = 100;
    rich4_rng_seed(7);
    assert(rich4_new_game_update_setup_animation(&settings, 200) & 1);
    assert(settings.selected_count == 2 && settings.selected_ai[1] >= 1);
    settings.dropdown_row = 0;
    assert(rich4_new_game_update_setup_frame(
        &settings, 561, 251, 200, &frame));
    assert(frame.animation_flags == 0 && !frame.transition_complete);
    assert(settings.dropdown_hover == 0);
    rich4_new_game_start_transition(&settings);
    assert(settings.transition_active && settings.transition_timer == 0);
    rich4_new_game_cancel_transition(&settings);
    assert(!settings.transition_active);
    rich4_new_game_start_transition(&settings);
    assert(!rich4_new_game_update_transition(&settings, 100));
    assert(rich4_new_game_update_transition(&settings, 100 + 50 * 32));

    session.state.active = 1;
    session.state.stage = 1;
    session.state.map = 2;
    session.state.player_count = 2;
    session.state.initial_fund = 100000;
    session.state.initial_traffic_index = 2;
    session.state.land_period_index = 4;
    session.state.day_limit = 365;
    session.state.win_target = 500000;
    session.state.players[0].character = 7;
    session.state.players[1].character = 8;
    session.state.players[1].is_ai = 1;
    session.state.players[1].ai_personality = 2;
    rich4_new_game_settings_from_session(&settings, &session);
    assert(settings.stage == 1 && settings.map == 2);
    assert(settings.selected_count == 2);
    assert(settings.selected_characters[0] == 7 &&
           settings.selected_characters[1] == 8);
    assert(settings.selected_ai[1] == 3);
    rich4_scene_machine_init(&machine, RICH4_SCENE_NEW_GAME);
    app_state = RICH4_SCENE_NEW_GAME;
    {
        NewGameSettings input_settings;
        Rich4AppNewGameSetupInputResult input_result;
        rich4_new_game_settings_init(&input_settings, 0);
        input_result = rich4_app_new_game_setup_pointer_up(
            &input_settings, 8, 15, &machine, &app_state);
        assert(input_result.action == RICH4_NEW_GAME_SETUP_CHARACTER_TOGGLED);
        assert(input_result.selected_count == 1);
        input_result = rich4_app_new_game_setup_pointer_up(
            &input_settings, 544, 176, &machine, &app_state);
        assert(input_result.action == RICH4_NEW_GAME_SETUP_EXIT);
        assert(app_state == RICH4_SCENE_MENU);
    }
    rich4_scene_machine_init(&machine, RICH4_SCENE_NEW_GAME);
    app_state = RICH4_SCENE_NEW_GAME;
    settings.selected_count = 2;
    settings.player_extra = 0;
    settings.last_anim_tick = 100;
    settings.transition_wait_active = 1;
    settings.transition_wait_ticks = 10;
    services.audio_context = &probe;
    services.draw_context = &probe;
    services.play_ok = probe_ok;
    services.play_transition = probe_transition;
    services.draw = probe_draw;
    assert(rich4_app_new_game_setup_frame_update(
        &settings, 0, 0, 200, &machine, &app_state, &services));
    assert(probe.ok == 0 && probe.transition == 1);
    assert(probe.draws == 1 && probe.transition_draws == 1);
    assert(settings.transition_active && app_state == RICH4_SCENE_INTRO);
    rich4_scene_machine_init(&machine, RICH4_SCENE_INTRO);
    app_state = RICH4_SCENE_INTRO;
    settings.last_anim_tick = 100;
    settings.transition_timer = 0;
    transition_services.complete_context = &probe;
    transition_services.draw_context = &probe;
    transition_services.scene_ready_context = &probe;
    transition_services.complete = probe_transition_complete;
    transition_services.draw = NULL;
    transition_services.scene_ready = probe_scene_ready;
    assert(rich4_app_new_game_transition_frame_update(
        &settings, 100 + 50 * 32,
        RICH4_SETUP_TRANSITION_INPUT_NONE, &machine, &app_state,
        &transition_services));
    assert(probe.transition_complete == 1 && probe.scene_ready == 1);
    assert(app_state == RICH4_SCENE_GAME && !settings.transition_active);
    puts("new-game setup callback flow: ok");
    return 0;
}
