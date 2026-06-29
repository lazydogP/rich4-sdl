#ifndef RICH4_APP_AI_SETTINGS_STATE_H
#define RICH4_APP_AI_SETTINGS_STATE_H

#include "core/game_constants.h"

/* App-owned AI settings draft state. SDL only supplies pointer geometry and
 * renders the resulting values. */
typedef struct Rich4AiSettingsState {
    int active;
    int selected_row;
    int hover;
    int pressed_control;
    int player_count;
    int player_index[GAME_MAX_PLAYERS];
    int hosted[GAME_MAX_PLAYERS];
    int action_flags[GAME_MAX_PLAYERS];
    int personality[GAME_MAX_PLAYERS];
    int cash_ratio[GAME_MAX_PLAYERS];
    int stock_ratio[GAME_MAX_PLAYERS];
} Rich4AiSettingsState;

#endif
