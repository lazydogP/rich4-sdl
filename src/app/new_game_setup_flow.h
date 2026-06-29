#ifndef RICH4_APP_NEW_GAME_SETUP_FLOW_H
#define RICH4_APP_NEW_GAME_SETUP_FLOW_H

#include <stdint.h>

#include "app/game_session.h"

/* App-owned setup/transition state.  SDL only borrows it for hit testing and
 * rendering; all values use platform-neutral fixed-width time types. */
typedef struct NewGameSettings {
    int stage;
    int map;
    int player_extra;
    int fund_index;
    int traffic_index;
    int land_index;
    int days_index;
    int goal_index;
    int selected_row;
    int selected_count;
    int selected_characters[4];
    int selected_ai[4];
    int selected_frame[4];
    int dropdown_row;
    int dropdown_hover;
    int pressed_control;
    int auto_fill_active;
    int auto_fill_ticks;
    int transition_wait_active;
    int transition_wait_ticks;
    int transition_active;
    int transition_timer;
    int transition_panel_left_x;
    int transition_panel_left_y;
    int transition_panel_right_x;
    int transition_player_x[4];
    int transition_panel_speed;
    int transition_player_speed;
    int scroll_offset;
    uint32_t last_anim_tick;
    unsigned char character_used[12];
} NewGameSettings;

void rich4_new_game_settings_init(NewGameSettings *settings, int stage);
void rich4_new_game_settings_from_session(NewGameSettings *settings,
                                          const GameSession *session);
void rich4_new_game_settings_set(NewGameSettings *settings, int row, int value);
void rich4_new_game_set_selected_row(NewGameSettings *settings, int row);
void rich4_new_game_set_dropdown_hover(NewGameSettings *settings, int option);
void rich4_new_game_close_dropdown(NewGameSettings *settings);
void rich4_new_game_cancel_transition(NewGameSettings *settings);

typedef enum Rich4NewGameSetupAction {
    RICH4_NEW_GAME_SETUP_NO_ACTION = 0,
    RICH4_NEW_GAME_SETUP_CHARACTER_TOGGLED,
    RICH4_NEW_GAME_SETUP_SUBMIT,
    RICH4_NEW_GAME_SETUP_EXIT,
    RICH4_NEW_GAME_SETUP_DROPDOWN_OPENED,
    RICH4_NEW_GAME_SETUP_MAP_CHANGED,
    RICH4_NEW_GAME_SETUP_DROPDOWN_CHANGED
} Rich4NewGameSetupAction;

/* Apply the semantic part of a normalized left-button release. Platform
 * code consumes the action for audio, scene and trace side effects. */
Rich4NewGameSetupAction rich4_new_game_apply_pointer_up(
    NewGameSettings *settings, int x, int y);

/* Record the pressed setup control on pointer-down. Platform consumes the
 * later pointer-up action for audio, scene and trace side effects. */
void rich4_new_game_apply_pointer_down(NewGameSettings *settings,
                                       int x, int y);

int rich4_new_game_character_at(int x, int y);
int rich4_new_game_control_at(int x, int y);
int rich4_new_game_dropdown_option_at(const NewGameSettings *settings,
                                      int x, int y);
int rich4_new_game_toggle_character(NewGameSettings *settings, int character);
void rich4_new_game_start_transition(NewGameSettings *settings);
int rich4_new_game_update_setup_animation(NewGameSettings *settings,
                                          uint32_t now);

typedef struct Rich4NewGameSetupFrameResult {
    int animation_flags;
    int transition_complete;
} Rich4NewGameSetupFrameResult;

/* Advance setup-owned animation and apply the frame's pointer-derived
 * selection state. SDL consumes only animation/transition side effects. */
int rich4_new_game_update_setup_frame(
    NewGameSettings *settings, int cursor_x, int cursor_y, uint32_t now,
    Rich4NewGameSetupFrameResult *out_result);

int rich4_new_game_update_transition(NewGameSettings *settings,
                                     uint32_t now);

/* sub_404E44 stage 1 advances its counter before comparing the previous
 * value with 10, so work becomes due on the eleventh 100 ms callback. */
int rich4_setup_stage1_tick_due(int *tick_counter);

int rich4_setup_background_resource(int stage, int map);
int rich4_setup_character_resource(int character, int option);

typedef enum Rich4SetupTransitionInput {
    RICH4_SETUP_TRANSITION_INPUT_NONE = 0,
    RICH4_SETUP_TRANSITION_INPUT_MOUSE_BUTTON_UP,
    RICH4_SETUP_TRANSITION_INPUT_KEY_DOWN,
    RICH4_SETUP_TRANSITION_INPUT_CONTROLLER_BUTTON_DOWN
} Rich4SetupTransitionInput;

/* Stage 2 completes on its animation boundary or on the SDL equivalents of
 * the original mouse-button-up/key-down interruption messages. */
int rich4_setup_transition_should_complete(int animation_complete,
                                           Rich4SetupTransitionInput input);

#endif
