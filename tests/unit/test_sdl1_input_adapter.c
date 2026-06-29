#include <assert.h>
#include <stdio.h>

#include "platform/sdl1/input_adapter.h"

int main(void)
{
    Rich4AppCommand command;
    Rich4AppCommand app_command;
    int cursor_x;
    int cursor_y;
    assert(rich4_sdl1_original_hotkey(RICH4_APP_KEY_CONTROL) == 17);
    assert(rich4_sdl1_original_hotkey(RICH4_APP_KEY_F1) == 112);
    assert(rich4_sdl1_original_hotkey(RICH4_APP_KEY_F12) == 123);
    assert(rich4_sdl1_original_hotkey(RICH4_APP_KEY_QUOTE) == 222);
    assert(rich4_sdl1_pointer_command(&command,
                                      RICH4_APP_COMMAND_POINTER_MOVE,
                                      640, 360, 1280, 720, 0));
    assert(command.x == 320 && command.y == 240);
    assert(command.type == RICH4_APP_COMMAND_POINTER_MOVE);
    app_command = rich4_app_pointer_command(
        RICH4_APP_COMMAND_CONFIRM, 0, 0, 0);
    assert(rich4_app_command_is_valid(&app_command));
    assert(rich4_app_command_is_valid(&command));
    assert(rich4_sdl1_pointer_command(&command,
                                      RICH4_APP_COMMAND_POINTER_DOWN,
                                      -10, 900, 1280, 720, 1));
    assert(command.x == 0 && command.y == 479 && command.button == 1);
    assert(rich4_sdl1_pointer_command(&command,
                                      RICH4_APP_COMMAND_POINTER_UP,
                                      639, 479, 640, 480, 1));
    assert(command.x == 639 && command.y == 479);
    assert(rich4_sdl1_key_command(&command,
                                  RICH4_APP_COMMAND_KEY_DOWN,
                                  'a', 3));
    assert(command.type == RICH4_APP_COMMAND_KEY_DOWN &&
           command.key == RICH4_APP_KEY_LETTER_A && command.modifiers == 3);
    assert(rich4_sdl1_key_command(&command,
                                  RICH4_APP_COMMAND_KEY_UP,
                                  'a', 0));
    assert(command.type == RICH4_APP_COMMAND_KEY_UP &&
           command.key == RICH4_APP_KEY_LETTER_A);
    assert(rich4_sdl1_joy_command(&command, 7));
    assert(command.type == RICH4_APP_COMMAND_JOY_BUTTON_DOWN &&
           command.button == 7);
    assert(!rich4_sdl1_key_command(&command,
                                   RICH4_APP_COMMAND_KEY_DOWN,
                                   -1, 0));
    assert(!rich4_sdl1_joy_command(&command, -1));
    assert(!rich4_sdl1_pointer_command(&command,
                                       RICH4_APP_COMMAND_POINTER_MOVE,
                                       0, 0, 0, 480, 0));
    assert(!rich4_sdl1_pointer_command(NULL,
                                       RICH4_APP_COMMAND_POINTER_MOVE,
                                       0, 0, 640, 480, 0));
    assert(rich4_sdl1_confirm_dialog_hit(170, 300, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_ACCEPT);
    assert(rich4_sdl1_confirm_dialog_hit(219, 339, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_ACCEPT);
    assert(rich4_sdl1_confirm_dialog_hit(220, 300, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_DECLINE);
    assert(rich4_sdl1_confirm_dialog_hit(269, 339, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_DECLINE);
    assert(rich4_sdl1_confirm_dialog_hit(169, 320, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_NONE);
    assert(rich4_sdl1_confirm_dialog_hit(270, 320, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_NONE);
    assert(rich4_sdl1_confirm_dialog_hit(200, 299, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_NONE);
    assert(rich4_sdl1_confirm_dialog_hit(200, 340, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_NONE);
    assert(rich4_sdl1_confirm_dialog_activation(
               RICH4_APP_COMMAND_POINTER_DOWN,
               180, 320, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_NONE);
    assert(rich4_sdl1_confirm_dialog_activation(
               RICH4_APP_COMMAND_POINTER_UP,
               180, 320, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_ACCEPT);
    assert(rich4_sdl1_confirm_dialog_activation(
               RICH4_APP_COMMAND_POINTER_UP,
               240, 320, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_DECLINE);
    assert(rich4_sdl1_confirm_dialog_activation(
               RICH4_APP_COMMAND_POINTER_UP,
               280, 320, 170, 300, 100, 40) ==
           RICH4_CONFIRM_DIALOG_NONE);
    assert(rich4_sdl1_confirm_dialog_initial_cursor(
        172, 296, &cursor_x, &cursor_y));
    assert(cursor_x == 194 && cursor_y == 318);
    assert(rich4_sdl1_confirm_dialog_initial_cursor(
        272, 216, &cursor_x, &cursor_y));
    assert(cursor_x == 294 && cursor_y == 238);
    assert(!rich4_sdl1_confirm_dialog_initial_cursor(
        172, 296, NULL, &cursor_y));
    assert(rich4_sdl1_route_minigame_wrap_cursor(
        0, 237, &cursor_x, &cursor_y));
    assert(cursor_x == 0 && cursor_y == 237);
    assert(rich4_sdl1_route_minigame_wrap_cursor(
        1, 237, &cursor_x, &cursor_y));
    assert(cursor_x == 639 && cursor_y == 237);
    assert(rich4_sdl1_actor_panel_cursor(
        180, 120, &cursor_x, &cursor_y));
    assert(cursor_x == 226 && cursor_y == 154);
    assert(rich4_sdl1_selector_initial_cursor(&cursor_x, &cursor_y));
    assert(cursor_x == 220 && cursor_y == 320);
    assert(rich4_sdl1_virtual_cursor_move(&cursor_x, &cursor_y, 3, -5));
    assert(cursor_x == 223 && cursor_y == 315);
    assert(rich4_sdl1_virtual_cursor_move(&cursor_x, &cursor_y, -500, 500));
    assert(cursor_x == 0 && cursor_y == 479);
    assert(!rich4_sdl1_virtual_cursor_move(&cursor_x, &cursor_y, -1, 1));
    puts("SDL1 input adapter: ok");
    return 0;
}
