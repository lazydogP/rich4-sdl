#include "app/new_game_setup_flow.h"

#include <stddef.h>
#include <string.h>

#include "core/character_template.h"
#include "core/rng.h"

static int lookup_new_game_option_index(const int *values,
                                        int count,
                                        int value,
                                        int fallback)
{
    int i;
    for (i = 0; i < count; i++) {
        if (values[i] == value) return i;
    }
    return fallback;
}

static int new_game_fund_index_from_value(int value)
{
    static const int funds[] = { 300000, 200000, 100000, 50000, 30000, 10000 };
    return lookup_new_game_option_index(funds, 6, value, 1);
}

static int new_game_days_index_from_value(int value)
{
    static const int days[] = { 0, 730, 365, 182, 91, 30 };
    return lookup_new_game_option_index(days, 6, value, 0);
}

static int new_game_goal_index_from_target(int initial_fund, int win_target)
{
    static const int winning_condition[] = { 0, 100, 50, 10, 5, 3 };
    int multiplier = 0;
    if (initial_fund > 0 && win_target > 0) {
        multiplier = win_target / initial_fund;
        if (multiplier * initial_fund != win_target) multiplier = 0;
    }
    return lookup_new_game_option_index(winning_condition, 6, multiplier, 0);
}

void rich4_new_game_settings_init(NewGameSettings *settings, int stage)
{
    if (settings == NULL) return;
    memset(settings, 0, sizeof(*settings));
    settings->stage = stage != 0 ? 1 : 0;
    settings->map = 0;
    settings->player_extra = 2;
    settings->fund_index = 1;
    settings->selected_row = 0;
    settings->dropdown_row = -1;
    settings->dropdown_hover = -1;
    settings->pressed_control = -1;
    settings->transition_panel_left_x = 4;
    settings->transition_panel_left_y = 10;
    settings->transition_panel_right_x = 445;
    memset(settings->selected_characters, 0xff,
           sizeof(settings->selected_characters));
}

void rich4_new_game_settings_from_session(NewGameSettings *settings,
                                          const GameSession *session)
{
    int i;
    int count;
    int character;
    if (settings == NULL || session == NULL || !session->state.active) return;
    rich4_new_game_settings_init(settings, session->state.stage);
    settings->map = session->state.map < 0 ? 0 : session->state.map;
    if (settings->map >= GAME_MAPS_PER_STAGE)
        settings->map = GAME_MAPS_PER_STAGE - 1;
    count = session->state.player_count;
    if (count < 1) count = 1;
    if (count > GAME_MAX_PLAYERS) count = GAME_MAX_PLAYERS;
    settings->player_extra = count - 2;
    if (settings->player_extra < 0) settings->player_extra = 0;
    if (settings->player_extra > 2) settings->player_extra = 2;
    settings->fund_index = new_game_fund_index_from_value(
        session->state.initial_fund);
    settings->traffic_index = session->state.initial_traffic_index;
    if (settings->traffic_index < 0) settings->traffic_index = 0;
    if (settings->traffic_index > 2) settings->traffic_index = 2;
    settings->land_index = session->state.land_period_index;
    if (settings->land_index < 0) settings->land_index = 0;
    if (settings->land_index > 5) settings->land_index = 5;
    settings->days_index = new_game_days_index_from_value(
        session->state.day_limit);
    settings->goal_index = new_game_goal_index_from_target(
        session->state.initial_fund, session->state.win_target);
    settings->selected_count = count;
    for (i = 0; i < count; i++) {
        character = session->state.players[i].character;
        if (character < 0 || character >= 12 ||
            settings->character_used[character]) {
            character = i;
            while (character < 12 && settings->character_used[character])
                character++;
            if (character >= 12) character = 0;
        }
        settings->selected_characters[i] = character;
        settings->selected_ai[i] = session->state.players[i].is_ai
            ? session->state.players[i].ai_personality + 1 : 0;
        if (character >= 0 && character < 12)
            settings->character_used[character] = GAME_SETUP_CHARACTER_SELECTED;
    }
}

void rich4_new_game_settings_set(NewGameSettings *settings, int row, int value)
{
    static const int limits[] = { 2, 4, 3, 6, 3, 6, 6, 6 };
    if (settings == NULL || row < 0 || row >= 8) return;
    value %= limits[row];
    if (value < 0) value += limits[row];
    if (row == 0) settings->stage = value;
    if (row == 1) settings->map = value;
    if (row == 2) settings->player_extra = value;
    if (row == 3) settings->fund_index = value;
    if (row == 4) settings->traffic_index = value;
    if (row == 5) settings->land_index = value;
    if (row == 6) settings->days_index = value;
    if (row == 7) settings->goal_index = value;
    while (settings->selected_count > settings->player_extra + 2) {
        int character = settings->selected_characters[
            settings->selected_count - 1];
        if (character >= 0 && character < 12)
            settings->character_used[character] = 0;
        settings->selected_characters[settings->selected_count - 1] = -1;
        settings->selected_ai[settings->selected_count - 1] = 0;
        settings->selected_frame[settings->selected_count - 1] = 0;
        settings->selected_count--;
    }
}

void rich4_new_game_set_selected_row(NewGameSettings *settings, int row)
{
    if (settings != NULL) settings->selected_row = row;
}

void rich4_new_game_set_dropdown_hover(NewGameSettings *settings, int option)
{
    if (settings != NULL && settings->dropdown_row >= 0)
        settings->dropdown_hover = option;
}

void rich4_new_game_close_dropdown(NewGameSettings *settings)
{
    if (settings == NULL) return;
    settings->dropdown_row = -1;
    settings->dropdown_hover = -1;
}

void rich4_new_game_cancel_transition(NewGameSettings *settings)
{
    if (settings != NULL) settings->transition_active = 0;
}

Rich4NewGameSetupAction rich4_new_game_apply_pointer_up(
    NewGameSettings *settings, int x, int y)
{
    static const int control_to_row[] = { 2, 3, 4, 5, 6, 7 };
    int control;

    if (settings == NULL) return RICH4_NEW_GAME_SETUP_NO_ACTION;
    control = rich4_new_game_control_at(x, y);
    if (settings->dropdown_row >= 0) {
        int row = settings->dropdown_row;
        int option = rich4_new_game_dropdown_option_at(settings, x, y);
        settings->pressed_control = row + 3;
        if (option < 0) {
            settings->dropdown_row = -1;
            settings->dropdown_hover = -1;
            return RICH4_NEW_GAME_SETUP_NO_ACTION;
        }
        rich4_new_game_settings_set(settings, control_to_row[row], option);
        settings->dropdown_row = -1;
        settings->dropdown_hover = -1;
        return RICH4_NEW_GAME_SETUP_DROPDOWN_CHANGED;
    }
    if (control == 0) {
        settings->pressed_control = control;
        return rich4_new_game_toggle_character(
                   settings, rich4_new_game_character_at(x, y))
            ? RICH4_NEW_GAME_SETUP_CHARACTER_TOGGLED
            : RICH4_NEW_GAME_SETUP_NO_ACTION;
    }
    if (control == 1) {
        settings->pressed_control = control;
        if (settings->selected_count > 0) {
            settings->last_anim_tick = 0;
            if (settings->selected_count >= settings->player_extra + 2 ||
                settings->selected_count >= 4) {
                settings->auto_fill_active = 0;
                settings->auto_fill_ticks = 0;
                settings->transition_wait_active = 1;
                settings->transition_wait_ticks = 10;
            } else {
                settings->auto_fill_active = 1;
                settings->auto_fill_ticks = 10;
                settings->transition_wait_active = 0;
                settings->transition_wait_ticks = 0;
            }
        }
        return RICH4_NEW_GAME_SETUP_SUBMIT;
    }
    if (control == 2) {
        settings->pressed_control = control;
        return RICH4_NEW_GAME_SETUP_EXIT;
    }
    if (control >= 3 && control <= 8) {
        settings->pressed_control = control;
        settings->dropdown_row = control - 3;
        settings->dropdown_hover = rich4_new_game_dropdown_option_at(
            settings, x, y);
        return RICH4_NEW_GAME_SETUP_DROPDOWN_OPENED;
    }
    if (control >= 9 && control <= 12) {
        settings->pressed_control = control;
        if (settings->map == control - 9)
            return RICH4_NEW_GAME_SETUP_NO_ACTION;
        settings->map = control - 9;
        return RICH4_NEW_GAME_SETUP_MAP_CHANGED;
    }
    return RICH4_NEW_GAME_SETUP_NO_ACTION;
}

void rich4_new_game_apply_pointer_down(NewGameSettings *settings,
                                       int x, int y)
{
    if (settings == NULL) return;
    settings->pressed_control = rich4_new_game_control_at(x, y);
}

int rich4_new_game_character_at(int x, int y)
{
    int col;
    int row;
    if (x < 8 || x >= 440 || y < 15 || y >= 159) return -1;
    col = (x - 8) / 72;
    row = (y - 15) / 72;
    if (col < 0 || col >= 6 || row < 0 || row >= 2) return -1;
    return row * 6 + col;
}

int rich4_new_game_control_at(int x, int y)
{
    static const struct {
        int x;
        int y;
        int right;
        int bottom;
    } hotzones[] = {
        { 8, 15, 440, 159 },
        { 456, 176, 535, 215 },
        { 544, 176, 623, 215 },
        { 544, 226, 625, 250 },
        { 602, 262, 625, 286 },
        { 602, 298, 625, 322 },
        { 602, 334, 625, 358 },
        { 602, 370, 625, 394 },
        { 602, 406, 625, 430 },
        { 457, 31, 625, 62 },
        { 457, 63, 625, 94 },
        { 457, 95, 625, 126 },
        { 457, 127, 625, 158 }
    };
    int i;
    for (i = 0; i < (int)(sizeof(hotzones) / sizeof(hotzones[0])); i++) {
        if (x >= hotzones[i].x && x < hotzones[i].right &&
            y >= hotzones[i].y && y < hotzones[i].bottom)
            return i;
    }
    return -1;
}

int rich4_new_game_dropdown_option_at(const NewGameSettings *settings,
                                      int x, int y)
{
    static const struct {
        int x;
        int y;
        int right;
        int bottom;
    } option_rects[] = {
        { 561, 251, 602, 320 },
        { 536, 287, 602, 425 },
        { 561, 323, 602, 392 },
        { 536, 195, 602, 333 },
        { 536, 231, 602, 369 },
        { 516, 267, 602, 405 }
    };
    int option;
    int row;
    if (settings == NULL || settings->dropdown_row < 0 ||
        settings->dropdown_row >= 6)
        return -1;
    row = settings->dropdown_row;
    if (x < option_rects[row].x || x >= option_rects[row].right ||
        y < option_rects[row].y || y >= option_rects[row].bottom)
        return -1;
    option = (y - option_rects[row].y) / 23;
    if (option < 0 || option >= 6) return -1;
    if ((row == 0 || row == 2) && option >= 3) return -1;
    return option;
}

int rich4_new_game_toggle_character(NewGameSettings *settings, int character)
{
    int i;
    int max_players;
    if (settings == NULL || character < 0 || character >= 12) return 0;
    max_players = settings->player_extra + 2;
    if (settings->character_used[character]) {
        int found = -1;
        for (i = 0; i < settings->selected_count; i++) {
            if (settings->selected_characters[i] == character) {
                found = i;
                break;
            }
        }
        if (found >= 0) {
            for (i = found; i + 1 < settings->selected_count; i++) {
                settings->selected_characters[i] =
                    settings->selected_characters[i + 1];
                settings->selected_ai[i] = settings->selected_ai[i + 1];
                settings->selected_frame[i] = settings->selected_frame[i + 1];
            }
            settings->selected_count--;
            settings->selected_characters[settings->selected_count] = -1;
            settings->selected_ai[settings->selected_count] = 0;
            settings->selected_frame[settings->selected_count] = 0;
        }
        settings->character_used[character] = 0;
        return found >= 0;
    }
    if (settings->selected_count >= max_players ||
        settings->selected_count >= 4)
        return 0;
    settings->character_used[character] = GAME_SETUP_CHARACTER_SELECTED;
    settings->selected_characters[settings->selected_count] = character;
    settings->selected_ai[settings->selected_count] = 0;
    settings->selected_frame[settings->selected_count] = 0;
    settings->selected_count++;
    return 1;
}

static int add_new_game_ai_character(NewGameSettings *settings)
{
    int max_players;
    int choices[12];
    int choice_count = 0;
    int i;
    if (settings == NULL || settings->selected_count <= 0) return 0;
    max_players = settings->player_extra + 2;
    if (settings->selected_count >= max_players ||
        settings->selected_count >= 4)
        return 0;
    for (i = 0; i < 12; i++) {
        if (!settings->character_used[i]) choices[choice_count++] = i;
    }
    if (choice_count <= 0) return 0;
    i = choices[rich4_rng_range(choice_count)];
    settings->character_used[i] = GAME_SETUP_CHARACTER_SELECTED;
    settings->selected_characters[settings->selected_count] = i;
    settings->selected_ai[settings->selected_count] =
        rich4_character_template(i)->f23 + 1;
    settings->selected_frame[settings->selected_count] = 0;
    settings->selected_count++;
    return 1;
}

void rich4_new_game_start_transition(NewGameSettings *settings)
{
    static const int x_positions[3][4] = {
        { 330, 110, 0, 0 },
        { 366, 220, 74, 0 },
        { 385, 275, 165, 55 }
    };
    int group;
    int i;
    if (settings == NULL) return;
    group = settings->player_extra;
    if (group < 0) group = 0;
    if (group > 2) group = 2;
    settings->auto_fill_active = 0;
    settings->auto_fill_ticks = 0;
    settings->transition_wait_active = 0;
    settings->transition_wait_ticks = 0;
    settings->transition_active = 1;
    settings->transition_timer = 0;
    settings->transition_panel_left_x = 4;
    settings->transition_panel_left_y = 10;
    settings->transition_panel_right_x = 445;
    settings->transition_panel_speed = 6;
    settings->transition_player_speed = 4;
    settings->last_anim_tick = 0;
    for (i = 0; i < 4; i++)
        settings->transition_player_x[i] = x_positions[group][i];
}

int rich4_new_game_update_setup_animation(NewGameSettings *settings,
                                          uint32_t now)
{
    int flags = 0;
    if (settings == NULL) return 0;
    if (settings->last_anim_tick == 0) {
        settings->last_anim_tick = now;
        return 0;
    }
    while (now - settings->last_anim_tick >= 100) {
        int i;
        settings->scroll_offset += 4;
        if (settings->scroll_offset >= 0x500) settings->scroll_offset = 0;
        for (i = 0; i < settings->selected_count; i++)
            settings->selected_frame[i]++;
        if (settings->auto_fill_active) {
            if (rich4_setup_stage1_tick_due(&settings->auto_fill_ticks)) {
                if (add_new_game_ai_character(settings)) {
                    flags |= 1;
                } else {
                    settings->auto_fill_active = 0;
                    settings->transition_wait_active = 1;
                    settings->transition_wait_ticks = 0;
                }
                if (settings->selected_count >= settings->player_extra + 2 ||
                    settings->selected_count >= 4) {
                    settings->auto_fill_active = 0;
                    settings->transition_wait_active = 1;
                    settings->transition_wait_ticks = 0;
                }
            }
        } else if (settings->transition_wait_active) {
            if (rich4_setup_stage1_tick_due(&settings->transition_wait_ticks)) {
                settings->transition_wait_active = 0;
                if (settings->selected_count > 0 &&
                    (settings->selected_count >= settings->player_extra + 2 ||
                     settings->selected_count >= 4))
                    flags |= 2;
            }
        }
        settings->last_anim_tick += 100;
    }
    return flags;
}

int rich4_new_game_update_setup_frame(
    NewGameSettings *settings, int cursor_x, int cursor_y, uint32_t now,
    Rich4NewGameSetupFrameResult *out_result)
{
    Rich4NewGameSetupFrameResult result;
    int control;

    result.animation_flags = 0;
    result.transition_complete = 0;
    if (settings == NULL) {
        if (out_result != NULL) *out_result = result;
        return 0;
    }
    result.animation_flags = rich4_new_game_update_setup_animation(
        settings, now);
    if (result.animation_flags & 2) {
        result.transition_complete = 1;
    } else {
        control = rich4_new_game_control_at(cursor_x, cursor_y);
        if (settings->dropdown_row >= 0)
            rich4_new_game_set_dropdown_hover(
                settings, rich4_new_game_dropdown_option_at(
                    settings, cursor_x, cursor_y));
        if (control >= 3 && control <= 8) {
            static const int control_to_row[] = { 2, 3, 4, 5, 6, 7 };
            rich4_new_game_set_selected_row(
                settings, control_to_row[control - 3]);
        }
    }
    if (out_result != NULL) *out_result = result;
    return 1;
}

int rich4_new_game_update_transition(NewGameSettings *settings, uint32_t now)
{
    int i;
    if (settings == NULL) return 0;
    if (settings->last_anim_tick == 0) {
        settings->last_anim_tick = now;
        return 0;
    }
    while (now - settings->last_anim_tick >= 50) {
        settings->scroll_offset += 4;
        if (settings->scroll_offset >= 0x500) settings->scroll_offset = 0;
        for (i = 0; i < settings->selected_count; i++)
            settings->selected_frame[i]++;
        settings->transition_panel_speed += 2;
        if (settings->transition_player_speed < 30)
            settings->transition_player_speed++;
        settings->transition_panel_left_y -= settings->transition_panel_speed;
        settings->transition_panel_right_x += settings->transition_panel_speed;
        for (i = 0; i < 4; i++)
            settings->transition_player_x[i] += settings->transition_player_speed;
        settings->transition_timer++;
        settings->last_anim_tick += 50;
    }
    return settings->transition_timer >= 32;
}

int rich4_setup_background_resource(int stage, int map)
{
    if (stage < 0 || stage >= 2 || map < 0 || map >= 4) return -1;
    return stage * 4 + map;
}

int rich4_setup_character_resource(int character, int option)
{
    if (character < 0 || character >= 12 || option < 0 || option >= 3) return -1;
    return 9 + character * 3 + option;
}

int rich4_setup_stage1_tick_due(int *tick_counter)
{
    int previous;
    if (tick_counter == NULL) return 0;
    previous = *tick_counter;
    *tick_counter = previous + 1;
    if (previous < 10) return 0;
    *tick_counter = 0;
    return 1;
}

int rich4_setup_transition_should_complete(int animation_complete,
                                           Rich4SetupTransitionInput input)
{
    if (animation_complete) return 1;
    return input == RICH4_SETUP_TRANSITION_INPUT_MOUSE_BUTTON_UP ||
           input == RICH4_SETUP_TRANSITION_INPUT_KEY_DOWN ||
           input == RICH4_SETUP_TRANSITION_INPUT_CONTROLLER_BUTTON_DOWN;
}
