#include "core/game_constants.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"

/* R10.1d: extracted verbatim from src/platform/sdl1/runtime_main.c. These
 * immutable Big5 labels are shared by event, card-panel, tool-panel, and shop
 * presentations. */

const char *game_card_big5_name(int card)
{
    static const char *const names[GAME_CARD_COUNT] = {
        "\xa7\xa1\xb4\x49\xa5\x64", "\xa7\xa1\xb3\x68\xa5\x64",
        "\xc1\xca\xa6\x61\xa5\x64", "\xb4\xab\xa6\x61\xa5\x64",
        "\xb4\xab\xab\xce\xa5\x64", "\xc2\xe0\xa6\x56\xa5\x64",
        "\xa7\xef\xab\xd8\xa5\x64", "\xa9\xe7\xbd\xe6\xa5\x64",
        "\xa4\xd1\xa8\xcf\xa5\x64", "\xb4\x63\xc5\x5d\xa5\x64",
        "\xa9\xc7\xc3\x7e\xa5\x64", "\xa9\xee\xb0\xa3\xa5\x64",
        "\xb7\x6d\xb9\xdc\xa5\x64", "\xb0\xb1\xaf\x64\xa5\x64",
        "\xa5\x56\xaf\x76\xa5\x64", "\xb9\xda\xb9\x43\xa5\x64",
        "\xb3\xb4\xae\x60\xa5\x64", "\xb4\x5f\xa4\xb3\xa5\x64",
        "\xb6\xf9\xba\xd7\xa5\x64", "\xa7\x4b\xb6\x4f\xa5\x64",
        "\xa7\x4b\xb8\x6f\xa5\x64", "\xb0\x65\xaf\xab\xb2\xc5",
        "\xbd\xd0\xaf\xab\xb2\xc5", "\xac\xf5\xa5\x64",
        "\xb6\xc2\xa5\x64", "\xac\x64\xb5\x7c\xa5\x64",
        "\xba\xa6\xbb\xf9\xa5\x64", "\xac\x64\xab\xca\xa5\x64",
        "\xa6\x50\xb7\xf9\xa5\x64", "\xaf\x51\xc0\x74\xa5\x64"
    };
    return card >= 0 && card < GAME_CARD_COUNT ? names[card] : "";
}

const char *game_tool_big5_name(int tool)
{
    static const char *const names[GAME_TOOL_COUNT] = {
        "\xbe\xf7\xbe\xb9\xab\xbd\xab\xbd", "\xb8\xf4\xbb\xd9",
        "\xa6\x61\xb9\x70", "\xa9\x77\xae\xc9\xac\xb5\xbc\x75",
        "\xbe\xf7\xa8\xae", "\xa8\x54\xa8\xae",
        "\xad\xb8\xbc\x75", "\xbb\xbb\xb1\xb1\xbb\xeb\xa4\x6c",
        "\xbe\xf7\xbe\xb9\xa4\x75\xa4\x48", "\xae\xc9\xa5\xfa\xbe\xf7",
        "\xb6\xc7\xb0\x65\xbe\xf7", "\xa4\x75\xb5\x7b\xa8\xae",
        "\xae\xd6\xa4\x6c\xad\xb8\xbc\x75"
    };
    return tool >= 0 && tool < GAME_TOOL_COUNT ? names[tool] : "";
}

const char *new_game_character_name(int character)
{
    static const char *names[] = {
        "\xac\xf9\x20\xbf\xab\x20\xb3\xec",
        "\xa8\x46\xb6\xa9\xa4\xda\xb4\xb5",
        "\xa7\xd4\x20\xa4\xd3\x20\xad\xa6",
        "\xbf\xfa\x20\xa4\xd2\x20\xa4\x48",
        "\xaa\xfc\x20\xa4\x67\x20\xa7\x42",
        "\xb2\xef\xa9\xd4\xa4\xbd\xa5\x44",
        "\xae\x63\xa5\xbb\xc4\x5f\xc2\xc3",
        "\xbf\x7d\x20\x20\xbf\x7d",
        "\xaf\x51\x20\x20\xab\x7d",
        "\xae\x5d\x20\xa4\x70\x20\xac\xfc",
        "\xa4\x70\x20\xa4\xa6\x20\xa5\xa7",
        "\xaa\xf7\x20\xa8\xa9\x20\xa8\xa9"
    };
    if (character < 0 || character >= (int)(sizeof(names) / sizeof(names[0]))) {
        return "";
    }
    return names[character];
}

const char *game_land_development_big5_name(int type, int level)
{
    /* 0x475138; converted plots use 0x463E30. */
    static const char *const names[] = {
        "\xaa\xc5  \xa6\x61", "\xa5\xad  \xa9\xd0",
        "\xa9\xb1  \xe7\x45", "\xb0\xd3  \xb3\xf5",
        "\xb0\xd3\xb7\x7e\xa4\x6a\xbc\xd3",
        "\xbc\xaf\xa4\xd1\xa4\x6a\xbc\xd3"
    };
    if (type != 0) return "\xb3\x73\xc2\xea\xa9\xb1";
    if (level < 0) level = 0;
    if (level > 5) level = 5;
    return names[level];
}

const char *game_facility_big5_name(int type, int level)
{
    /* 0x475150; a level-zero plot is labeled 空  地. */
    static const char *const names[] = {
        "\xa4\xbd  \xb6\xe9", "\xae\xc8  \xc0\x5d",
        "\xc1\xca\xaa\xab\xa4\xa4\xa4\xdf",
        "\xa5\x5b\xaa\x6f\xaf\xb8", "\xac\xe3\xa8\x73\xa9\xd2"
    };
    if (level == 0) return game_land_development_big5_name(0, 0);
    if (type < 0 || type >= (int)(sizeof(names) / sizeof(names[0]))) type = 0;
    return names[type];
}

const char *game_facility_level_big5_name(int level)
{
    /* 0x475164. */
    static const char *const names[] = {
        "\xa2\xaf\xaf\xc5", "\xa4\x40\xaf\xc5", "\xa4\x47\xaf\xc5",
        "\xa4\x54\xaf\xc5", "\xa5\x7c\xaf\xc5", "\xa4\xad\xaf\xc5"
    };
    return level >= 0 && level <= 5 ? names[level] : "";
}

void game_compact_display_name(char *out, size_t capacity,
                               const char *source)
{
    size_t length = 0;

    if (out == NULL || capacity == 0) return;
    while (source != NULL && *source != '\0' && length + 1 < capacity) {
        if (*source != ' ')
            out[length++] = *source;
        source++;
    }
    out[length] = '\0';
}
