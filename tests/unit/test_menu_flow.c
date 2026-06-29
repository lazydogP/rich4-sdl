#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/menu_flow.h"

int main(void)
{
    GameSession session;
    NewGameSettings settings;

    memset(&session, 0xff, sizeof(session));
    rich4_new_game_settings_init(&settings, 1);
    assert(rich4_menu_fallback_button_at(44, 80) == 0);
    assert(rich4_menu_fallback_button_at(44, 136) == 2);
    assert(rich4_menu_fallback_button_at(44, 192) == 3);
    assert(rich4_menu_fallback_button_at(194, 80) == -1);
    assert(rich4_menu_apply_button(&session, &settings, 0) ==
           RICH4_MENU_START_NEW_GAME);
    assert(settings.stage == 0 && session.state.active == 0);
    assert(rich4_menu_apply_button(&session, &settings, 4) ==
           RICH4_MENU_START_NEW_GAME);
    assert(settings.stage == 1);
    assert(rich4_menu_apply_button(&session, &settings, 2) ==
           RICH4_MENU_OPEN_OPTIONS);
    assert(rich4_menu_apply_button(&session, &settings, 3) ==
           RICH4_MENU_QUIT);
    assert(rich4_menu_apply_button(&session, &settings, 1) ==
           RICH4_MENU_OPEN_LOAD);
    puts("app menu flow: ok");
    return 0;
}
