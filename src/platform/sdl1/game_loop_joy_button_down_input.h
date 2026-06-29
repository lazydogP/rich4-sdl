#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_JOY_BUTTON_DOWN_INPUT_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_JOY_BUTTON_DOWN_INPUT_H

#include "app/ai_settings_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: complete joystick button-down dispatch moved out of
 * runtime_main.c; modal ownership remains the first gate. */
typedef struct GameJoyButtonDownInputContext {
    Rich4SceneId app_state;
    GameSession *session;
    SmallMapPanelState *small_map_panel;
    HelpPanelState *help_panel;
    VirtualCursor *cursor;
    Rich4SetupTransitionInput *setup_transition_input;
    const NewGameSettings *settings;
    Rich4AiSettingsState *ai_settings_panel;
    int button;
    int confirm_button;
    int cancel_button;
} GameJoyButtonDownInputContext;

/* Returns non-zero when a modal owns the joystick button-down event. */
int game_handle_joy_button_down_input(
    GameJoyButtonDownInputContext *context);

#endif
