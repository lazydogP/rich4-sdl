#include "platform/sdl1/input/input_config_sdl.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

static void strip_quotes(char *text)
{
    size_t len = strlen(text);
    if (len >= 2 && ((text[0] == '"' && text[len - 1] == '"') ||
                     (text[0] == '\'' && text[len - 1] == '\''))) {
        memmove(text, text + 1, len - 2);
        text[len - 2] = '\0';
    }
}

static int parse_int_value(const char *text, int *out_value)
{
    char *end = NULL;
    long value;

    value = strtol(text, &end, 10);
    if (text == end || *end != '\0') {
        return 0;
    }
    *out_value = (int)value;
    return 1;
}

static Rich4AppKey parse_key_name(const char *name, Rich4AppKey fallback)
{
    if (strcmp(name, "up") == 0) return RICH4_APP_KEY_UP;
    if (strcmp(name, "down") == 0) return RICH4_APP_KEY_DOWN;
    if (strcmp(name, "left") == 0) return RICH4_APP_KEY_LEFT;
    if (strcmp(name, "right") == 0) return RICH4_APP_KEY_RIGHT;
    if (strcmp(name, "return") == 0 || strcmp(name, "enter") == 0)
        return RICH4_APP_KEY_RETURN;
    if (strcmp(name, "space") == 0) return RICH4_APP_KEY_SPACE;
    if (strcmp(name, "escape") == 0 || strcmp(name, "esc") == 0)
        return RICH4_APP_KEY_ESCAPE;
    if (strcmp(name, "backspace") == 0) return RICH4_APP_KEY_BACKSPACE;
    if (strlen(name) == 1) {
        unsigned char ch = (unsigned char)name[0];
        if (isalpha(ch)) {
            if (tolower(ch) >= 'a' && tolower(ch) <= 'z')
                return (Rich4AppKey)(RICH4_APP_KEY_LETTER_A +
                                      tolower(ch) - 'a');
        }
        if (isdigit(ch)) {
            return (Rich4AppKey)(RICH4_APP_KEY_DIGIT_0 + ch - '0');
        }
    }
    return fallback;
}

void rich4_input_set_defaults(Rich4InputConfig *config)
{
    if (config == NULL) {
        return;
    }
    config->cursor_up = RICH4_APP_KEY_UP;
    config->cursor_down = RICH4_APP_KEY_DOWN;
    config->cursor_left = RICH4_APP_KEY_LEFT;
    config->cursor_right = RICH4_APP_KEY_RIGHT;
    config->confirm = RICH4_APP_KEY_RETURN;
    config->cancel = RICH4_APP_KEY_ESCAPE;
    config->joy_confirm = 0;
    config->joy_cancel = 1;
    /* 0x40107A moves the Win32 cursor by exactly ten pixels for each
     * configured direction-key dispatch. */
    config->cursor_speed = 10;
}

int rich4_input_load_file(Rich4InputConfig *config, const char *path)
{
    FILE *fp;
    char line[256];

    if (config == NULL || path == NULL) {
        return 0;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq;
        char *key;
        char *value;
        char *comment;
        int int_value;

        comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }
        key = trim(line);
        if (*key == '\0' || *key == '[') {
            continue;
        }
        eq = strchr(key, '=');
        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        value = trim(eq + 1);
        key = trim(key);
        strip_quotes(value);

        if (strcmp(key, "cursor_up") == 0) {
            config->cursor_up = parse_key_name(value, config->cursor_up);
        } else if (strcmp(key, "cursor_down") == 0) {
            config->cursor_down = parse_key_name(value, config->cursor_down);
        } else if (strcmp(key, "cursor_left") == 0) {
            config->cursor_left = parse_key_name(value, config->cursor_left);
        } else if (strcmp(key, "cursor_right") == 0) {
            config->cursor_right = parse_key_name(value, config->cursor_right);
        } else if (strcmp(key, "confirm") == 0) {
            config->confirm = parse_key_name(value, config->confirm);
        } else if (strcmp(key, "cancel") == 0) {
            config->cancel = parse_key_name(value, config->cancel);
        } else if (strcmp(key, "joy_confirm") == 0 && parse_int_value(value, &int_value)) {
            config->joy_confirm = int_value;
        } else if (strcmp(key, "joy_cancel") == 0 && parse_int_value(value, &int_value)) {
            config->joy_cancel = int_value;
        } else if (strcmp(key, "cursor_speed") == 0 && parse_int_value(value, &int_value)) {
            if (int_value > 0 && int_value <= 40) {
                config->cursor_speed = int_value;
            }
        }
    }

    fclose(fp);
    return 1;
}
