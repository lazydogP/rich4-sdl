#include <stdio.h>
#include <string.h>

#include "core/character_template.h"
#include "core/game_constants.h"
#include "platform/sdl1/runtime_render_api.h"
#include "view/view_constants.h"

/* R10.1d: extracted verbatim from src/platform/sdl1/runtime_main.c.
 * These are stateless display-data and coordinate helpers shared by the
 * minimap, main map, HUD, intro, setup, and session renderers. */

uint16_t game_player_color(int player)
{
    static const uint16_t colors[GAME_MAX_PLAYERS] = { 0xf800, 0x07e0, 0x001f, 0xffe0 };
    return colors[wrap_index(player, GAME_MAX_PLAYERS)];
}

int game_world_to_minimap_x(int world_x)
{
    return GAME_MINIMAP_X +
        ((world_x * GAME_ORIGINAL_MINIMAP_SCALE) >> 16);
}

int game_world_to_minimap_y_at(int world_y, int minimap_y)
{
    return minimap_y +
        ((world_y * GAME_ORIGINAL_MINIMAP_SCALE) >> 16);
}

void format_hud_number(char *out, size_t out_size, int value, int money_prefix)
{
    char digits[32];
    char formatted[40];
    char *dst = formatted;
    const char *src;
    int len;
    int i;
    int negative = value < 0;
    unsigned int magnitude;

    if (out == NULL || out_size == 0) {
        return;
    }
    magnitude = negative ? (unsigned int)(0 - (value + 1)) + 1u : (unsigned int)value;
    snprintf(digits, sizeof(digits), "%u", magnitude);
    if (money_prefix) {
        *dst++ = '$';
    }
    if (negative) {
        *dst++ = '-';
    }
    src = digits;
    len = (int)strlen(src);
    for (i = 0; i < len; i++) {
        if (i > 0 && ((len - i) % 3) == 0) {
            *dst++ = ',';
        }
        *dst++ = src[i];
    }
    *dst = '\0';
    snprintf(out, out_size, "%s", formatted);
}
