#include "platform/sdl1/input_adapter.h"

#include <SDL/SDL.h>

static Rich4AppKey normalize_key(int key)
{
    if (key == SDLK_LCTRL) return RICH4_APP_KEY_CONTROL;
    if (key == SDLK_END) return RICH4_APP_KEY_END;
    if (key == SDLK_HOME) return RICH4_APP_KEY_HOME;
    if (key == SDLK_INSERT) return RICH4_APP_KEY_INSERT;
    if (key == SDLK_KP_MULTIPLY) return RICH4_APP_KEY_KP_MULTIPLY;
    if (key == SDLK_KP_PLUS) return RICH4_APP_KEY_KP_PLUS;
    if (key == SDLK_KP_MINUS) return RICH4_APP_KEY_KP_MINUS;
    if (key == SDLK_KP_DIVIDE) return RICH4_APP_KEY_KP_DIVIDE;
    if (key == SDLK_F1) return RICH4_APP_KEY_F1;
    if (key == SDLK_F2) return RICH4_APP_KEY_F2;
    if (key == SDLK_F3) return RICH4_APP_KEY_F3;
    if (key == SDLK_F4) return RICH4_APP_KEY_F4;
    if (key == SDLK_F5) return RICH4_APP_KEY_F5;
    if (key == SDLK_F6) return RICH4_APP_KEY_F6;
    if (key == SDLK_F7) return RICH4_APP_KEY_F7;
    if (key == SDLK_F8) return RICH4_APP_KEY_F8;
    if (key == SDLK_F9) return RICH4_APP_KEY_F9;
    if (key == SDLK_F10) return RICH4_APP_KEY_F10;
    if (key == SDLK_F11) return RICH4_APP_KEY_F11;
    if (key == SDLK_F12) return RICH4_APP_KEY_F12;
    if (key == SDLK_SEMICOLON) return RICH4_APP_KEY_SEMICOLON;
    if (key == SDLK_EQUALS) return RICH4_APP_KEY_EQUALS;
    if (key == SDLK_MINUS) return RICH4_APP_KEY_MINUS;
    if (key == SDLK_SLASH) return RICH4_APP_KEY_SLASH;
    if (key == SDLK_BACKQUOTE) return RICH4_APP_KEY_BACKQUOTE;
    if (key == SDLK_LEFTBRACKET) return RICH4_APP_KEY_LEFTBRACKET;
    if (key == SDLK_BACKSLASH) return RICH4_APP_KEY_BACKSLASH;
    if (key == SDLK_RIGHTBRACKET) return RICH4_APP_KEY_RIGHTBRACKET;
    if (key == SDLK_QUOTE) return RICH4_APP_KEY_QUOTE;
    if (key == SDLK_RCTRL) return RICH4_APP_KEY_CONTROL;
    if (key == SDLK_UP) return RICH4_APP_KEY_UP;
    if (key == SDLK_DOWN) return RICH4_APP_KEY_DOWN;
    if (key == SDLK_LEFT) return RICH4_APP_KEY_LEFT;
    if (key == SDLK_RIGHT) return RICH4_APP_KEY_RIGHT;
    if (key == SDLK_RETURN) return RICH4_APP_KEY_RETURN;
    if (key == SDLK_KP_ENTER) return RICH4_APP_KEY_KP_ENTER;
    if (key == SDLK_ESCAPE) return RICH4_APP_KEY_ESCAPE;
    if (key == SDLK_BACKSPACE) return RICH4_APP_KEY_BACKSPACE;
    if (key == SDLK_SPACE) return RICH4_APP_KEY_SPACE;
    if (key == SDLK_TAB) return RICH4_APP_KEY_TAB;
    if (key == SDLK_DELETE) return RICH4_APP_KEY_DELETE;
    if (key == SDLK_PAGEUP) return RICH4_APP_KEY_PAGEUP;
    if (key == SDLK_PAGEDOWN) return RICH4_APP_KEY_PAGEDOWN;
    if (key == SDLK_COMMA) return RICH4_APP_KEY_COMMA;
    if (key == SDLK_PERIOD) return RICH4_APP_KEY_PERIOD;
    if (key >= SDLK_a && key <= SDLK_z)
        return (Rich4AppKey)(RICH4_APP_KEY_LETTER_A + key - SDLK_a);
    if (key >= SDLK_0 && key <= SDLK_9)
        return (Rich4AppKey)(RICH4_APP_KEY_DIGIT_0 + key - SDLK_0);
    if (key >= SDLK_KP0 && key <= SDLK_KP9)
        return (Rich4AppKey)(RICH4_APP_KEY_KP_0 + key - SDLK_KP0);
    return RICH4_APP_KEY_UNKNOWN;
}

int rich4_sdl1_native_key(Rich4AppKey key)
{
    switch (key) {
    case RICH4_APP_KEY_CONTROL: return SDLK_LCTRL;
    case RICH4_APP_KEY_END: return SDLK_END;
    case RICH4_APP_KEY_HOME: return SDLK_HOME;
    case RICH4_APP_KEY_INSERT: return SDLK_INSERT;
    case RICH4_APP_KEY_KP_MULTIPLY: return SDLK_KP_MULTIPLY;
    case RICH4_APP_KEY_KP_PLUS: return SDLK_KP_PLUS;
    case RICH4_APP_KEY_KP_MINUS: return SDLK_KP_MINUS;
    case RICH4_APP_KEY_KP_DIVIDE: return SDLK_KP_DIVIDE;
    case RICH4_APP_KEY_F1: return SDLK_F1;
    case RICH4_APP_KEY_F2: return SDLK_F2;
    case RICH4_APP_KEY_F3: return SDLK_F3;
    case RICH4_APP_KEY_F4: return SDLK_F4;
    case RICH4_APP_KEY_F5: return SDLK_F5;
    case RICH4_APP_KEY_F6: return SDLK_F6;
    case RICH4_APP_KEY_F7: return SDLK_F7;
    case RICH4_APP_KEY_F8: return SDLK_F8;
    case RICH4_APP_KEY_F9: return SDLK_F9;
    case RICH4_APP_KEY_F10: return SDLK_F10;
    case RICH4_APP_KEY_F11: return SDLK_F11;
    case RICH4_APP_KEY_F12: return SDLK_F12;
    case RICH4_APP_KEY_SEMICOLON: return SDLK_SEMICOLON;
    case RICH4_APP_KEY_EQUALS: return SDLK_EQUALS;
    case RICH4_APP_KEY_MINUS: return SDLK_MINUS;
    case RICH4_APP_KEY_SLASH: return SDLK_SLASH;
    case RICH4_APP_KEY_BACKQUOTE: return SDLK_BACKQUOTE;
    case RICH4_APP_KEY_LEFTBRACKET: return SDLK_LEFTBRACKET;
    case RICH4_APP_KEY_BACKSLASH: return SDLK_BACKSLASH;
    case RICH4_APP_KEY_RIGHTBRACKET: return SDLK_RIGHTBRACKET;
    case RICH4_APP_KEY_QUOTE: return SDLK_QUOTE;
    case RICH4_APP_KEY_UP: return SDLK_UP;
    case RICH4_APP_KEY_DOWN: return SDLK_DOWN;
    case RICH4_APP_KEY_LEFT: return SDLK_LEFT;
    case RICH4_APP_KEY_RIGHT: return SDLK_RIGHT;
    case RICH4_APP_KEY_RETURN: return SDLK_RETURN;
    case RICH4_APP_KEY_KP_ENTER: return SDLK_KP_ENTER;
    case RICH4_APP_KEY_ESCAPE: return SDLK_ESCAPE;
    case RICH4_APP_KEY_BACKSPACE: return SDLK_BACKSPACE;
    case RICH4_APP_KEY_SPACE: return SDLK_SPACE;
    case RICH4_APP_KEY_TAB: return SDLK_TAB;
    case RICH4_APP_KEY_DELETE: return SDLK_DELETE;
    case RICH4_APP_KEY_PAGEUP: return SDLK_PAGEUP;
    case RICH4_APP_KEY_PAGEDOWN: return SDLK_PAGEDOWN;
    case RICH4_APP_KEY_COMMA: return SDLK_COMMA;
    case RICH4_APP_KEY_PERIOD: return SDLK_PERIOD;
    default:
        if (key >= RICH4_APP_KEY_LETTER_A &&
            key <= RICH4_APP_KEY_LETTER_Z)
            return SDLK_a + key - RICH4_APP_KEY_LETTER_A;
        if (key >= RICH4_APP_KEY_DIGIT_0 &&
            key <= RICH4_APP_KEY_DIGIT_9)
            return SDLK_0 + key - RICH4_APP_KEY_DIGIT_0;
        if (key >= RICH4_APP_KEY_KP_0 &&
            key <= RICH4_APP_KEY_KP_9)
            return SDLK_KP0 + key - RICH4_APP_KEY_KP_0;
        return SDLK_UNKNOWN;
    }
}

static int map_coordinate(int value, int window_extent, int internal_extent)
{
    long mapped;
    if (value <= 0) return 0;
    if (value >= window_extent) return internal_extent - 1;
    mapped = (long)value * internal_extent / window_extent;
    if (mapped >= internal_extent) mapped = internal_extent - 1;
    return (int)mapped;
}

int rich4_sdl1_pointer_command(Rich4AppCommand *out_command,
                               Rich4AppCommandType type,
                               int window_x,
                               int window_y,
                               int window_width,
                               int window_height,
                               int button)
{
    if (out_command == 0 || window_width <= 0 || window_height <= 0 ||
        type < RICH4_APP_COMMAND_POINTER_MOVE ||
        type > RICH4_APP_COMMAND_POINTER_UP) return 0;
    *out_command = rich4_app_pointer_command(
        type,
        map_coordinate(window_x, window_width, RICH4_INTERNAL_WIDTH),
        map_coordinate(window_y, window_height, RICH4_INTERNAL_HEIGHT),
        button);
    return 1;
}

int rich4_sdl1_key_command(Rich4AppCommand *out_command,
                           Rich4AppCommandType type,
                           int key, int modifiers)
{
    if (out_command == 0 ||
        (type != RICH4_APP_COMMAND_KEY_DOWN &&
         type != RICH4_APP_COMMAND_KEY_UP) || key < 0)
        return 0;
    if (normalize_key(key) == RICH4_APP_KEY_UNKNOWN) return 0;
    *out_command = rich4_app_key_command(type, normalize_key(key), modifiers);
    return 1;
}

int rich4_sdl1_joy_command(Rich4AppCommand *out_command,
                           int button)
{
    if (out_command == 0 || button < 0) return 0;
    *out_command = rich4_app_joy_command(button);
    return 1;
}

int rich4_sdl1_virtual_cursor_move(int *cursor_x,
                                   int *cursor_y,
                                   int delta_x,
                                   int delta_y)
{
    int old_x;
    int old_y;
    if (cursor_x == 0 || cursor_y == 0) return 0;
    old_x = *cursor_x;
    old_y = *cursor_y;
    *cursor_x += delta_x;
    *cursor_y += delta_y;
    if (*cursor_x < 0) *cursor_x = 0;
    if (*cursor_y < 0) *cursor_y = 0;
    if (*cursor_x >= RICH4_INTERNAL_WIDTH) *cursor_x = RICH4_INTERNAL_WIDTH - 1;
    if (*cursor_y >= RICH4_INTERNAL_HEIGHT) *cursor_y = RICH4_INTERNAL_HEIGHT - 1;
    return *cursor_x != old_x || *cursor_y != old_y;
}

Rich4ConfirmDialogHit rich4_sdl1_confirm_dialog_hit(int x,
                                                    int y,
                                                    int left,
                                                    int top,
                                                    int width,
                                                    int height)
{
    if (width < 2 || height <= 0 || x < left || x >= left + width ||
        y < top || y >= top + height) {
        return RICH4_CONFIRM_DIALOG_NONE;
    }
    return x < left + width / 2 ? RICH4_CONFIRM_DIALOG_ACCEPT
                                : RICH4_CONFIRM_DIALOG_DECLINE;
}

Rich4ConfirmDialogHit rich4_sdl1_confirm_dialog_activation(
    Rich4AppCommandType type,
    int x,
    int y,
    int left,
    int top,
    int width,
    int height)
{
    /* sub_45367E confirms the hovered YES/NO half only for
     * WM_LBUTTONUP (0x202); WM_LBUTTONDOWN is not an activation. */
    if (type != RICH4_APP_COMMAND_POINTER_UP)
        return RICH4_CONFIRM_DIALOG_NONE;
    return rich4_sdl1_confirm_dialog_hit(x, y, left, top, width, height);
}

int rich4_sdl1_confirm_dialog_initial_cursor(int left,
                                            int top,
                                            int *out_x,
                                            int *out_y)
{
    if (out_x == 0 || out_y == 0) return 0;
    /* sub_45367E/0x453719 selects the first (yes) half by positioning the
     * pointer 22 pixels from the control resource's raw top-left corner. */
    *out_x = left + 22;
    *out_y = top + 22;
    return 1;
}

int rich4_sdl1_route_minigame_wrap_cursor(int right_edge,
                                         int current_y,
                                         int *out_x,
                                         int *out_y)
{
    if (out_x == 0 || out_y == 0) return 0;
    /* Route action 8: 0x41361C/0x41363C wrap only the X coordinate. */
    *out_x = right_edge ? RICH4_INTERNAL_WIDTH - 1 : 0;
    *out_y = current_y;
    return 1;
}

int rich4_sdl1_actor_panel_cursor(int panel_x,
                                 int panel_y,
                                 int *out_x,
                                 int *out_y)
{
    if (out_x == 0 || out_y == 0) return 0;
    /* 0x418DB0 uses the current actor control's dynamic origin. */
    *out_x = panel_x + 46;
    *out_y = panel_y + 34;
    return 1;
}

int rich4_sdl1_selector_initial_cursor(int *out_x, int *out_y)
{
    if (out_x == 0 || out_y == 0) return 0;
    /* Facility, player-target, tool-development and remote-dice selector
     * initialization (0x43FB54/0x43FFC2/0x440355/0x4467DE). */
    *out_x = 220;
    *out_y = 320;
    return 1;
}

/* Original accepted virtual-key identities, table 0x47EDFA. */
int rich4_sdl1_original_hotkey(Rich4AppKey key)
{
    if (key >= RICH4_APP_KEY_LETTER_A && key <= RICH4_APP_KEY_LETTER_Z)
        return 65 + key - RICH4_APP_KEY_LETTER_A;
    if (key >= RICH4_APP_KEY_DIGIT_0 && key <= RICH4_APP_KEY_DIGIT_9)
        return 48 + key - RICH4_APP_KEY_DIGIT_0;
    switch (key) {
    case RICH4_APP_KEY_BACKSPACE: return 8;
    case RICH4_APP_KEY_TAB: return 9;
    case RICH4_APP_KEY_RETURN: return 13;
    case RICH4_APP_KEY_KP_ENTER: return 13;
    case RICH4_APP_KEY_ESCAPE: return 27;
    case RICH4_APP_KEY_SPACE: return 32;
    case RICH4_APP_KEY_PAGEUP: return 33;
    case RICH4_APP_KEY_PAGEDOWN: return 34;
    case RICH4_APP_KEY_LEFT: return 37;
    case RICH4_APP_KEY_UP: return 38;
    case RICH4_APP_KEY_RIGHT: return 39;
    case RICH4_APP_KEY_DOWN: return 40;
    case RICH4_APP_KEY_COMMA: return 188;
    case RICH4_APP_KEY_PERIOD: return 190;
    case RICH4_APP_KEY_CONTROL: return 17;
    case RICH4_APP_KEY_END: return 35;
    case RICH4_APP_KEY_HOME: return 36;
    case RICH4_APP_KEY_INSERT: return 45;
    case RICH4_APP_KEY_KP_MULTIPLY: return 106;
    case RICH4_APP_KEY_KP_PLUS: return 107;
    case RICH4_APP_KEY_KP_MINUS: return 109;
    case RICH4_APP_KEY_KP_DIVIDE: return 111;
    case RICH4_APP_KEY_F1: return 112;
    case RICH4_APP_KEY_F2: return 113;
    case RICH4_APP_KEY_F3: return 114;
    case RICH4_APP_KEY_F4: return 115;
    case RICH4_APP_KEY_F5: return 116;
    case RICH4_APP_KEY_F6: return 117;
    case RICH4_APP_KEY_F7: return 118;
    case RICH4_APP_KEY_F8: return 119;
    case RICH4_APP_KEY_F9: return 120;
    case RICH4_APP_KEY_F10: return 121;
    case RICH4_APP_KEY_F11: return 122;
    case RICH4_APP_KEY_F12: return 123;
    case RICH4_APP_KEY_SEMICOLON: return 186;
    case RICH4_APP_KEY_EQUALS: return 187;
    case RICH4_APP_KEY_MINUS: return 189;
    case RICH4_APP_KEY_SLASH: return 191;
    case RICH4_APP_KEY_BACKQUOTE: return 192;
    case RICH4_APP_KEY_LEFTBRACKET: return 219;
    case RICH4_APP_KEY_BACKSLASH: return 220;
    case RICH4_APP_KEY_RIGHTBRACKET: return 221;
    case RICH4_APP_KEY_QUOTE: return 222;
    default: return 0;
    }
}
