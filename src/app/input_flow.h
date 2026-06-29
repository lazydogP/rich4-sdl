#ifndef RICH4_APP_INPUT_FLOW_H
#define RICH4_APP_INPUT_FLOW_H

#include "app/app_command.h"
#include "app/game_session.h"
#include "app/scene.h"

typedef struct NewGameSettings NewGameSettings;

/* App-owned input intent state. It contains no backend key or SDL surface;
 * SDL adapters only translate events and poll devices. Evidence/tests: A6,
 * test_app_input_flow. */
typedef struct VirtualCursor {
    int x;
    int y;
    int confirm_pressed;
    int confirm_pointer_pressed;
    int cancel_pressed;
} VirtualCursor;

int rich4_app_input_ui_active(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active);
int rich4_app_input_display_visible(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active);
void rich4_app_input_set_top_button_hover(GameSession *session, int hover);
void rich4_app_input_set_top_button_pressed(GameSession *session, int button);
int rich4_app_input_take_top_button_pressed(GameSession *session);
void rich4_app_input_request_map_info(GameSession *session);
int rich4_app_input_take_map_info_request(GameSession *session);
void rich4_app_input_clear_map_info(GameSession *session);
void rich4_app_input_set_map_info(GameSession *session,
                                  int object_id,
                                  int cursor_x,
                                  int cursor_y);
int rich4_app_input_apply_pointer(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active, VirtualCursor *cursor,
    const Rich4AppCommand *command);
int rich4_app_input_apply_key(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active, VirtualCursor *cursor,
    Rich4AppKey key, Rich4AppKey confirm_key, Rich4AppKey cancel_key);
int rich4_app_input_apply_joy(
    Rich4SceneId app_state, const NewGameSettings *settings,
    const GameSession *session, int ai_settings_active, VirtualCursor *cursor,
    int button, int confirm_button, int cancel_button);

#endif
