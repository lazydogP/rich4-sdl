#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_JOY_BUTTON_DOWN_MODALS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_JOY_BUTTON_DOWN_MODALS_H

#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: joystick modal ownership gates moved out of runtime_main.c. */
typedef struct GameJoyButtonDownModalsContext {
    Rich4SceneId app_state;
    GameSession *session;
    SmallMapPanelState *small_map_panel;
    HelpPanelState *help_panel;
    VirtualCursor *cursor;
    Rich4SetupTransitionInput *setup_transition_input;
} GameJoyButtonDownModalsContext;

/* Returns non-zero when a modal owns the joystick button-down event. */
int game_handle_joy_button_down_modals(
    GameJoyButtonDownModalsContext *context);

#endif
