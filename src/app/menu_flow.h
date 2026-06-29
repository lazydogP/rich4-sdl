#ifndef RICH4_APP_MENU_FLOW_H
#define RICH4_APP_MENU_FLOW_H

#include "app/game_session.h"
#include "app/new_game_setup_flow.h"

/* App-owned menu intent. Resource-backed hit testing remains in SDL because
 * it needs menu sprite bounds; the selected button is normalized first. */
typedef enum Rich4MenuAction {
    RICH4_MENU_NO_ACTION = 0,
    RICH4_MENU_START_NEW_GAME,
    RICH4_MENU_OPEN_LOAD,
    RICH4_MENU_OPEN_OPTIONS,
    RICH4_MENU_QUIT
} Rich4MenuAction;

int rich4_menu_fallback_button_at(int x, int y);
Rich4MenuAction rich4_menu_apply_button(
    GameSession *session, NewGameSettings *settings, int button);

#endif
