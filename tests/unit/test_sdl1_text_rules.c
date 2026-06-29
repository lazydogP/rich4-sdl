#include <assert.h>
#include <stdio.h>

#include "platform/sdl1/text_rules.h"

static void assert_anchor(int mode, int expected_x, int expected_y)
{
    int x = 100;
    int y = 80;
    rich4_text_apply_anchor(mode, 20, 10, &x, &y);
    assert(x == expected_x);
    assert(y == expected_y);
}

int main(void)
{
    assert(!rich4_text_is_bold(0));
    assert(!rich4_text_is_bold(RICH4_TEXT_STYLE_SHADOW));
    assert(rich4_text_is_bold(RICH4_TEXT_STYLE_BOLD));
    assert(rich4_text_is_bold(RICH4_TEXT_STYLE_BOLD |
                              RICH4_TEXT_STYLE_OUTLINE));

    assert(rich4_text_effect(0) == RICH4_TEXT_EFFECT_NONE);
    assert(rich4_text_effect(RICH4_TEXT_STYLE_SHADOW) ==
           RICH4_TEXT_EFFECT_SHADOW);
    assert(rich4_text_effect(RICH4_TEXT_STYLE_OUTLINE) ==
           RICH4_TEXT_EFFECT_OUTLINE);
    assert(rich4_text_effect(RICH4_TEXT_STYLE_SHADOW |
                             RICH4_TEXT_STYLE_OUTLINE) ==
           RICH4_TEXT_EFFECT_SHADOW);
    assert(rich4_text_main_offset(RICH4_TEXT_STYLE_SHADOW) == 0);
    assert(rich4_text_main_offset(RICH4_TEXT_STYLE_OUTLINE) == 1);

    assert(!rich4_text_is_vertical(2));
    assert(rich4_text_is_vertical(3));
    assert(rich4_text_centers_lines(4));
    assert(rich4_text_centers_lines(7));
    assert(!rich4_text_centers_lines(2));

    assert(rich4_text_character_extra(0) == -1);
    assert(rich4_text_character_extra(1) == 0);
    assert(rich4_text_character_extra(4) == 3);
    assert(rich4_text_vertical_advance(16, 0, 1) == 17);
    assert(rich4_text_vertical_advance(16, RICH4_TEXT_STYLE_BOLD, 1) == 18);
    assert(rich4_text_vertical_advance(
               18, RICH4_TEXT_STYLE_BOLD, 0) == 19);
    assert(rich4_text_vertical_advance(
               16, RICH4_TEXT_STYLE_OUTLINE, 0) == 17);

    assert_anchor(0, 100, 80);
    assert_anchor(1, 80, 80);
    assert_anchor(2, 90, 75);
    assert_anchor(3, 90, 75);
    assert_anchor(4, 90, 75);
    assert_anchor(5, 100, 75);
    assert_anchor(6, 80, 75);
    assert_anchor(7, 90, 70);
    assert_anchor(99, 100, 80);

    puts("sdl1 text rules: ok");
    return 0;
}
