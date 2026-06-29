#ifndef RICH4_PLATFORM_SDL1_INPUT_ADAPTER_H
#define RICH4_PLATFORM_SDL1_INPUT_ADAPTER_H

#include "app/app_command.h"

enum {
    RICH4_INTERNAL_WIDTH = 640,
    RICH4_INTERNAL_HEIGHT = 480
};

typedef enum Rich4ConfirmDialogHit {
    RICH4_CONFIRM_DIALOG_NONE = 0,
    RICH4_CONFIRM_DIALOG_ACCEPT,
    RICH4_CONFIRM_DIALOG_DECLINE
} Rich4ConfirmDialogHit;

int rich4_sdl1_pointer_command(Rich4AppCommand *out_command,
                               Rich4AppCommandType type,
                               int window_x,
                               int window_y,
                               int window_width,
                               int window_height,
                               int button);
int rich4_sdl1_key_command(Rich4AppCommand *out_command,
                           Rich4AppCommandType type,
                           int key, int modifiers);
/* Returns the backend key code for the platform's polling seam. */
int rich4_sdl1_original_hotkey(Rich4AppKey key);
int rich4_sdl1_native_key(Rich4AppKey key);
int rich4_sdl1_joy_command(Rich4AppCommand *out_command,
                           int button);
int rich4_sdl1_virtual_cursor_move(int *cursor_x,
                                   int *cursor_y,
                                   int delta_x,
                                   int delta_y);
Rich4ConfirmDialogHit rich4_sdl1_confirm_dialog_hit(int x,
                                                    int y,
                                                    int left,
                                                    int top,
                                                    int width,
                                                    int height);
Rich4ConfirmDialogHit rich4_sdl1_confirm_dialog_activation(
    Rich4AppCommandType type,
    int x,
    int y,
    int left,
    int top,
    int width,
    int height);
int rich4_sdl1_confirm_dialog_initial_cursor(int left,
                                            int top,
                                            int *out_x,
                                            int *out_y);
int rich4_sdl1_route_minigame_wrap_cursor(int right_edge,
                                         int current_y,
                                         int *out_x,
                                         int *out_y);
int rich4_sdl1_actor_panel_cursor(int panel_x,
                                 int panel_y,
                                 int *out_x,
                                 int *out_y);
int rich4_sdl1_selector_initial_cursor(int *out_x, int *out_y);

#endif
