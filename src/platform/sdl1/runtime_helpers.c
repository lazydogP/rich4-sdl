#include "platform/sdl1/runtime_render_api.h"

/* R10.1d: extracted verbatim from src/platform/sdl1/runtime_main.c. These
 * stateless geometry/index primitives are shared by input, render, and game
 * loop modules. */

int point_in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

int wrap_index(int value, int count)
{
    while (value < 0) {
        value += count;
    }
    while (value >= count) {
        value -= count;
    }
    return value;
}

int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}
