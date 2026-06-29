#include "platform/sdl1/text_rules.h"

int rich4_text_is_bold(int style)
{
    return (style & RICH4_TEXT_STYLE_BOLD) != 0;
}

Rich4TextEffect rich4_text_effect(int style)
{
    if ((style & RICH4_TEXT_STYLE_SHADOW) != 0)
        return RICH4_TEXT_EFFECT_SHADOW;
    if ((style & RICH4_TEXT_STYLE_OUTLINE) != 0)
        return RICH4_TEXT_EFFECT_OUTLINE;
    return RICH4_TEXT_EFFECT_NONE;
}

int rich4_text_main_offset(int style)
{
    return (style & RICH4_TEXT_STYLE_OUTLINE) != 0 ? 1 : 0;
}

int rich4_text_is_vertical(int mode)
{
    return mode == 3;
}

int rich4_text_centers_lines(int mode)
{
    return mode == 4 || mode == 7;
}

int rich4_text_character_extra(int spacing)
{
    return spacing - 1;
}

int rich4_text_vertical_advance(int height, int style, int spacing)
{
    return height + spacing +
           ((style & (RICH4_TEXT_STYLE_BOLD |
                      RICH4_TEXT_STYLE_OUTLINE)) != 0 ? 1 : 0);
}

void rich4_text_apply_anchor(int mode,
                             int width,
                             int height,
                             int *x,
                             int *y)
{
    if (x == 0 || y == 0)
        return;

    switch (mode) {
    case 1:
        *x -= width;
        break;
    case 2:
    case 3:
    case 4:
        *x -= width / 2;
        *y -= height / 2;
        break;
    case 5:
        *y -= height / 2;
        break;
    case 6:
        *x -= width;
        *y -= height / 2;
        break;
    case 7:
        *x -= width / 2;
        *y -= height;
        break;
    default:
        break;
    }
}
