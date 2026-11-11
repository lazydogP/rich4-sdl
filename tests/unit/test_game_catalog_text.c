#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "platform/sdl1/game_asset_api.h"
#undef main

int main(void)
{
    static const char padded_name[] =
        "\xae\x5d\x20\xa4\x70\x20\xac\xfc"; /* 孫 小 美 */
    static const char compact_name[] =
        "\xae\x5d\xa4\x70\xac\xfc"; /* 孫小美 */
    char output[64];
    char truncated[3];

    /* Canonical catalog names remain padded for original label screens. */
    assert(strcmp(new_game_character_name(9), padded_name) == 0);

    /* Sentence/prompt call sites explicitly apply original 0x452946. */
    game_compact_display_name(
        output, sizeof(output), new_game_character_name(9));
    assert(strcmp(output, compact_name) == 0);
    assert(strchr(output, ' ') == NULL);

    game_compact_display_name(truncated, sizeof(truncated), " A B C ");
    assert(strcmp(truncated, "AB") == 0);

    strcpy(output, "stale");
    game_compact_display_name(output, sizeof(output), NULL);
    assert(output[0] == '\0');

    puts("game-catalog text tests passed");
    return 0;
}
