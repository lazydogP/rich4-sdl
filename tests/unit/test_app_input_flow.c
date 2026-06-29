#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"

static void test_pointer_command(void)
{
    VirtualCursor cursor = { 0 };
    Rich4AppCommand command = rich4_app_pointer_command(
        RICH4_APP_COMMAND_POINTER_DOWN, 123, 234, 1);
    assert(rich4_app_input_apply_pointer(
        RICH4_SCENE_MENU, NULL, NULL, 0, &cursor, &command));
    assert(cursor.x == 123 && cursor.y == 234);
    command = rich4_app_pointer_command(
        RICH4_APP_COMMAND_POINTER_MOVE, 321, 432, 0);
    assert(rich4_app_input_apply_pointer(
        RICH4_SCENE_MENU, NULL, NULL, 0, &cursor, &command));
    assert(cursor.x == 321 && cursor.y == 432);
}

static void test_key_and_joy_commands(void)
{
    VirtualCursor cursor = { 0 };
    assert(rich4_app_input_apply_key(
        RICH4_SCENE_MENU, NULL, NULL, 0, &cursor, RICH4_APP_KEY_RETURN,
        RICH4_APP_KEY_RETURN, RICH4_APP_KEY_ESCAPE));
    assert(cursor.confirm_pressed && !cursor.cancel_pressed);
    cursor.confirm_pressed = 0;
    assert(rich4_app_input_apply_joy(
        RICH4_SCENE_MENU, NULL, NULL, 0, &cursor, 2, 1, 2));
    assert(!cursor.confirm_pressed && cursor.cancel_pressed);
}

static void test_gates(void)
{
    NewGameSettings settings;
    GameSession session;
    VirtualCursor cursor = { 7, 9, 0, 0, 0 };
    Rich4AppCommand pointer = rich4_app_pointer_command(
        RICH4_APP_COMMAND_POINTER_MOVE, 123, 234, 0);
    memset(&settings, 0, sizeof(settings));
    memset(&session, 0, sizeof(session));
    rich4_app_input_set_top_button_pressed(&session, 5);
    assert(rich4_app_input_take_top_button_pressed(&session) == 5 &&
           rich4_app_input_take_top_button_pressed(&session) == -1);
    settings.auto_fill_active = 1;
    assert(!rich4_app_input_ui_active(
        RICH4_SCENE_NEW_GAME, &settings, NULL, 0));
    settings.auto_fill_active = 0;
    session.presentation.point_minigame.active = 1;
    session.presentation.point_minigame.action = 8;
    assert(!rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.point_minigame.active = 0;

    session.flow.turn_flow_active = 1;
    session.flow.turn_state = GAME_TURN_STATE_MOVE;
    assert(!rich4_app_input_ui_active(
        RICH4_SCENE_GAME, NULL, &session, 0));
    assert(!rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    assert(!rich4_app_input_apply_pointer(
        RICH4_SCENE_GAME, NULL, &session, 0, &cursor, &pointer));
    assert(cursor.x == 7 && cursor.y == 9);

    session.state.player_count = 1;
    session.state.players[0].cash = 2000;
    session.presentation.intro.active = 1;
    session.presentation.auction.active = 1;
    session.presentation.auction.stage = RICH4_AUCTION_PRESENT_BIDDING;
    session.presentation.auction.bidding.current_player = 0;
    session.presentation.auction.bidding.current_price = 1000;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    assert(rich4_app_input_apply_pointer(
        RICH4_SCENE_GAME, NULL, &session, 0, &cursor, &pointer));
    assert(cursor.x == 123 && cursor.y == 234);
    session.state.players[0].is_ai = 1;
    assert(!rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.state.players[0].is_ai = 0;
    session.presentation.auction.stage = RICH4_AUCTION_PRESENT_INTRO;
    assert(!rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.auction.active = 0;
    session.presentation.intro.active = 0;
    /* Confinement status runs without an active movement flow. */
    session.flow.turn_flow_active = 0;
    session.presentation.confinement_status.active = 1;
    session.presentation.confinement_status.slot = 52;
    assert(!rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    assert(!rich4_app_input_apply_pointer(
        RICH4_SCENE_GAME, NULL, &session, 0, &cursor, &pointer));
    session.presentation.confinement_status.slot = 53;
    assert(!rich4_app_input_ui_active(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.confinement_status.active = 0;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.route_interaction.active = 1;
    assert(rich4_app_input_ui_active(
        RICH4_SCENE_GAME, NULL, &session, 0));
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    assert(!rich4_app_input_ui_active(
        RICH4_SCENE_INTRO, NULL, NULL, 0));
}

static void test_roulette_cursor_visibility(void)
{
    GameSession session;
    VirtualCursor cursor = { 0 };
    int mode, phase, automatic;
    memset(&session, 0, sizeof(session));
    session.flow.turn_flow_active = 1;
    session.presentation.facility_roulette.active = 1;
    for (mode = 0; mode < 4; ++mode)
        for (automatic = 0; automatic < 2; ++automatic)
            for (phase = RICH4_ROULETTE_SPINNING;
                 phase <= RICH4_ROULETTE_LANDED; ++phase) {
                session.presentation.facility_roulette.mode = mode;
                session.presentation.facility_roulette.phase = phase;
                session.presentation.facility_roulette.automatic = automatic;
                assert(!rich4_app_input_display_visible(
                    RICH4_SCENE_GAME, NULL, &session, 0));
                /* A hidden pointer still accepts the stop input. */
                assert(rich4_app_input_apply_key(
                    RICH4_SCENE_GAME, NULL, &session, 0, &cursor,
                    RICH4_APP_KEY_RETURN, RICH4_APP_KEY_RETURN,
                    RICH4_APP_KEY_ESCAPE));
                cursor.confirm_pressed = 0;
            }
    session.presentation.facility_roulette.active = 0;
    session.flow.turn_flow_active = 0;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));

    /* The money wheel also opens from a card without movement active. */
    session.presentation.god_acquisition.active = 1;
    session.presentation.god_acquisition.money.active = 1;
    for (mode = 0; mode < 2; ++mode)
        for (automatic = 0; automatic < 2; ++automatic)
            for (phase = 1; phase <= 10; ++phase) {
                session.presentation.god_acquisition.money.mode = mode;
                session.presentation.god_acquisition.money.automatic = automatic;
                session.presentation.god_acquisition.money.phase = phase;
                assert(!rich4_app_input_display_visible(
                    RICH4_SCENE_GAME, NULL, &session, 0));
            }
    session.state.player_count = 1;
    session.state.players[0].cash = 2000;
    session.presentation.auction.active = 1;
    session.presentation.auction.stage = RICH4_AUCTION_PRESENT_BIDDING;
    session.presentation.auction.bidding.current_player = 0;
    session.presentation.auction.bidding.current_price = 1000;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.auction.active = 0;
    assert(!rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.god_acquisition.active = 0;
    session.presentation.god_acquisition.money.active = 0;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
}

static void test_magic_house_cursor_visibility(void)
{
    GameSession session;
    int phase;
    memset(&session, 0, sizeof(session));
    session.flow.turn_flow_active = 1;
    session.presentation.magic_house.active = 1;
    for (phase = RICH4_MAGIC_HOUSE_INTRO_1;
         phase <= RICH4_MAGIC_HOUSE_AI_NOTICE; ++phase) {
        session.presentation.magic_house.phase = phase;
        assert(rich4_app_input_display_visible(
            RICH4_SCENE_GAME, NULL, &session, 0) ==
            (phase == RICH4_MAGIC_HOUSE_SELECT ||
             phase == RICH4_MAGIC_HOUSE_FACILITY_CHOICE));
    }
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_CONDITION;
    session.presentation.magic_house.return_to_selection = 1;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.magic_house.return_to_selection = 0;
    session.presentation.magic_house.phase = RICH4_MAGIC_HOUSE_TARGET;
    session.state.player_count = 1;
    session.state.players[0].cash = 2000;
    session.presentation.auction.active = 1;
    session.presentation.auction.stage = RICH4_AUCTION_PRESENT_BIDDING;
    session.presentation.auction.bidding.current_player = 0;
    session.presentation.auction.bidding.current_price = 1000;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.auction.active = 0;
    assert(!rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
    session.presentation.magic_house.active = 0;
    session.flow.turn_flow_active = 0;
    assert(rich4_app_input_display_visible(
        RICH4_SCENE_GAME, NULL, &session, 0));
}

int main(void)
{
    test_pointer_command();
    test_key_and_joy_commands();
    test_gates();
    test_roulette_cursor_visibility();
    test_magic_house_cursor_visibility();
    puts("app input flow: ok");
    return 0;
}
