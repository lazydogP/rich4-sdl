#ifndef RICH4_SDL_INPUT_H
#define RICH4_SDL_INPUT_H

#include "app/app_command.h"

typedef struct Rich4InputConfig {
    Rich4AppKey cursor_up;
    Rich4AppKey cursor_down;
    Rich4AppKey cursor_left;
    Rich4AppKey cursor_right;
    Rich4AppKey confirm;
    Rich4AppKey cancel;
    int joy_confirm;
    int joy_cancel;
    int cursor_speed;
} Rich4InputConfig;

void rich4_input_set_defaults(Rich4InputConfig *config);
int rich4_input_load_file(Rich4InputConfig *config, const char *path);

#endif
