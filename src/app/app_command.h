#ifndef RICH4_APP_COMMAND_H
#define RICH4_APP_COMMAND_H

#include "core/game_command.h"

/* Platform-neutral intent emitted by an input adapter.  Coordinates are in
 * the fixed logical viewport; SDL key codes and SDL_Event never cross this
 * boundary. */
typedef enum Rich4AppCommandType {
    RICH4_APP_COMMAND_NONE = 0,
    RICH4_APP_COMMAND_POINTER_MOVE,
    RICH4_APP_COMMAND_POINTER_DOWN,
    RICH4_APP_COMMAND_POINTER_UP,
    RICH4_APP_COMMAND_CONFIRM,
    RICH4_APP_COMMAND_CANCEL,
    RICH4_APP_COMMAND_SCENE_REQUEST,
    RICH4_APP_COMMAND_GAME,
    RICH4_APP_COMMAND_KEY_DOWN,
    RICH4_APP_COMMAND_KEY_UP,
    RICH4_APP_COMMAND_JOY_BUTTON_DOWN
} Rich4AppCommandType;

/* Normalized key identity. SDLKey is intentionally kept inside the SDL
 * adapter/configuration layer; app and feature flows consume this vocabulary
 * instead of backend key constants. */
typedef enum Rich4AppKey {
    RICH4_APP_KEY_UNKNOWN = 0,
    RICH4_APP_KEY_UP,
    RICH4_APP_KEY_DOWN,
    RICH4_APP_KEY_LEFT,
    RICH4_APP_KEY_RIGHT,
    RICH4_APP_KEY_RETURN,
    RICH4_APP_KEY_KP_ENTER,
    RICH4_APP_KEY_ESCAPE,
    RICH4_APP_KEY_BACKSPACE,
    RICH4_APP_KEY_SPACE,
    RICH4_APP_KEY_TAB,
    RICH4_APP_KEY_DELETE,
    RICH4_APP_KEY_PAGEUP,
    RICH4_APP_KEY_PAGEDOWN,
    RICH4_APP_KEY_COMMA,
    RICH4_APP_KEY_PERIOD,
    RICH4_APP_KEY_LETTER_A = 40,
    RICH4_APP_KEY_LETTER_B,
    RICH4_APP_KEY_LETTER_C,
    RICH4_APP_KEY_LETTER_D,
    RICH4_APP_KEY_LETTER_E,
    RICH4_APP_KEY_LETTER_F,
    RICH4_APP_KEY_LETTER_G,
    RICH4_APP_KEY_LETTER_H,
    RICH4_APP_KEY_LETTER_I,
    RICH4_APP_KEY_LETTER_J,
    RICH4_APP_KEY_LETTER_K,
    RICH4_APP_KEY_LETTER_L,
    RICH4_APP_KEY_LETTER_M,
    RICH4_APP_KEY_LETTER_N,
    RICH4_APP_KEY_LETTER_O,
    RICH4_APP_KEY_LETTER_P,
    RICH4_APP_KEY_LETTER_Q,
    RICH4_APP_KEY_LETTER_R,
    RICH4_APP_KEY_LETTER_S,
    RICH4_APP_KEY_LETTER_T,
    RICH4_APP_KEY_LETTER_U,
    RICH4_APP_KEY_LETTER_V,
    RICH4_APP_KEY_LETTER_W,
    RICH4_APP_KEY_LETTER_X,
    RICH4_APP_KEY_LETTER_Y,
    RICH4_APP_KEY_LETTER_Z,
    RICH4_APP_KEY_DIGIT_0,
    RICH4_APP_KEY_DIGIT_1,
    RICH4_APP_KEY_DIGIT_2,
    RICH4_APP_KEY_DIGIT_3,
    RICH4_APP_KEY_DIGIT_4,
    RICH4_APP_KEY_DIGIT_5,
    RICH4_APP_KEY_DIGIT_6,
    RICH4_APP_KEY_DIGIT_7,
    RICH4_APP_KEY_DIGIT_8,
    RICH4_APP_KEY_DIGIT_9,
    RICH4_APP_KEY_KP_0,
    RICH4_APP_KEY_KP_1,
    RICH4_APP_KEY_KP_2,
    RICH4_APP_KEY_KP_3,
    RICH4_APP_KEY_KP_4,
    RICH4_APP_KEY_KP_5,
    RICH4_APP_KEY_KP_6,
    RICH4_APP_KEY_KP_7,
    RICH4_APP_KEY_KP_8,
    RICH4_APP_KEY_KP_9,
    RICH4_APP_KEY_CONTROL,
    RICH4_APP_KEY_END,
    RICH4_APP_KEY_HOME,
    RICH4_APP_KEY_INSERT,
    RICH4_APP_KEY_KP_MULTIPLY,
    RICH4_APP_KEY_KP_PLUS,
    RICH4_APP_KEY_KP_MINUS,
    RICH4_APP_KEY_KP_DIVIDE,
    RICH4_APP_KEY_F1,
    RICH4_APP_KEY_F2,
    RICH4_APP_KEY_F3,
    RICH4_APP_KEY_F4,
    RICH4_APP_KEY_F5,
    RICH4_APP_KEY_F6,
    RICH4_APP_KEY_F7,
    RICH4_APP_KEY_F8,
    RICH4_APP_KEY_F9,
    RICH4_APP_KEY_F10,
    RICH4_APP_KEY_F11,
    RICH4_APP_KEY_F12,
    RICH4_APP_KEY_SEMICOLON,
    RICH4_APP_KEY_EQUALS,
    RICH4_APP_KEY_MINUS,
    RICH4_APP_KEY_SLASH,
    RICH4_APP_KEY_BACKQUOTE,
    RICH4_APP_KEY_LEFTBRACKET,
    RICH4_APP_KEY_BACKSLASH,
    RICH4_APP_KEY_RIGHTBRACKET,
    RICH4_APP_KEY_QUOTE,
} Rich4AppKey;

#define RICH4_APP_KEY_C RICH4_APP_KEY_LETTER_C
#define RICH4_APP_KEY_D RICH4_APP_KEY_LETTER_D
#define RICH4_APP_KEY_H RICH4_APP_KEY_LETTER_H
#define RICH4_APP_KEY_M RICH4_APP_KEY_LETTER_M

typedef struct Rich4AppCommand {
    Rich4AppCommandType type;
    int x;
    int y;
    int button;
    Rich4AppKey key;
    int modifiers;
    int scene;
    GameCommand game;
} Rich4AppCommand;

Rich4AppCommand rich4_app_pointer_command(Rich4AppCommandType type,
                                          int x, int y, int button);
Rich4AppCommand rich4_app_key_command(Rich4AppCommandType type,
                                      Rich4AppKey key, int modifiers);
Rich4AppCommand rich4_app_joy_command(int button);
Rich4AppCommand rich4_app_scene_request(int scene);
Rich4AppCommand rich4_app_game_command(const GameCommand *game);
int rich4_app_command_is_valid(const Rich4AppCommand *command);

#endif
