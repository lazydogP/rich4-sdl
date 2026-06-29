#include "app/app_command.h"

#include <stddef.h>

#include "app/scene.h"

Rich4AppCommand rich4_app_pointer_command(Rich4AppCommandType type,
                                          int x, int y, int button)
{
    Rich4AppCommand command = { 0 };
    command.type = type;
    command.x = x;
    command.y = y;
    command.button = button;
    return command;
}

Rich4AppCommand rich4_app_key_command(Rich4AppCommandType type,
                                      Rich4AppKey key, int modifiers)
{
    Rich4AppCommand command = { 0 };
    command.type = type;
    command.key = key;
    command.modifiers = modifiers;
    return command;
}

Rich4AppCommand rich4_app_joy_command(int button)
{
    Rich4AppCommand command = { 0 };
    command.type = RICH4_APP_COMMAND_JOY_BUTTON_DOWN;
    command.button = button;
    return command;
}

Rich4AppCommand rich4_app_scene_request(int scene)
{
    Rich4AppCommand command = { 0 };
    command.type = RICH4_APP_COMMAND_SCENE_REQUEST;
    command.scene = scene;
    return command;
}

Rich4AppCommand rich4_app_game_command(const GameCommand *game)
{
    Rich4AppCommand command = { 0 };
    command.type = RICH4_APP_COMMAND_GAME;
    if (game != NULL) command.game = *game;
    return command;
}

int rich4_app_command_is_valid(const Rich4AppCommand *command)
{
    if (command == NULL || command->type <= RICH4_APP_COMMAND_NONE ||
        command->type > RICH4_APP_COMMAND_JOY_BUTTON_DOWN)
        return 0;
    if (command->type == RICH4_APP_COMMAND_POINTER_MOVE ||
        command->type == RICH4_APP_COMMAND_POINTER_DOWN ||
        command->type == RICH4_APP_COMMAND_POINTER_UP)
        return command->x >= 0 && command->x < 640 &&
               command->y >= 0 && command->y < 480;
    if (command->type == RICH4_APP_COMMAND_GAME)
        return rich4_game_command_is_valid(&command->game);
    if (command->type == RICH4_APP_COMMAND_SCENE_REQUEST)
        return command->scene >= 0 && command->scene < RICH4_SCENE_COUNT;
    if (command->type == RICH4_APP_COMMAND_KEY_DOWN ||
        command->type == RICH4_APP_COMMAND_KEY_UP)
        return command->key != RICH4_APP_KEY_UNKNOWN;
    if (command->type == RICH4_APP_COMMAND_JOY_BUTTON_DOWN)
        return command->button >= 0;
    return 1;
}
