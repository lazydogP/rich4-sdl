#include "app/menu_flow.h"

#include "app/game_session.h"

static int point_in_rect(int x, int y, int left, int top,
                         int width, int height)
{
    return x >= left && x < left + width &&
           y >= top && y < top + height;
}

int rich4_menu_fallback_button_at(int x, int y)
{
    if (point_in_rect(x, y, 44, 80, 150, 44)) return 0;
    if (point_in_rect(x, y, 44, 136, 150, 44)) return 2;
    if (point_in_rect(x, y, 44, 192, 150, 44)) return 3;
    return -1;
}

Rich4MenuAction rich4_menu_apply_button(
    GameSession *session, NewGameSettings *settings, int button)
{
    if (button == 0 || button == 4) {
        if (session == NULL || settings == NULL)
            return RICH4_MENU_NO_ACTION;
        rich4_new_game_settings_init(settings, button == 4 ? 1 : 0);
        rich4_app_reset_session(session);
        return RICH4_MENU_START_NEW_GAME;
    }
    if (button == 1) return RICH4_MENU_OPEN_LOAD;
    if (button == 2) return RICH4_MENU_OPEN_OPTIONS;
    if (button == 3) return RICH4_MENU_QUIT;
    return RICH4_MENU_NO_ACTION;
}
