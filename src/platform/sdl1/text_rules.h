#ifndef RICH4_PLATFORM_SDL1_TEXT_RULES_H
#define RICH4_PLATFORM_SDL1_TEXT_RULES_H

typedef enum Rich4TextEffect {
    RICH4_TEXT_EFFECT_NONE = 0,
    RICH4_TEXT_EFFECT_SHADOW,
    RICH4_TEXT_EFFECT_OUTLINE
} Rich4TextEffect;

enum {
    RICH4_TEXT_STYLE_SHADOW = 0x01,
    RICH4_TEXT_STYLE_BOLD = 0x02,
    RICH4_TEXT_STYLE_OUTLINE = 0x04,
    RICH4_TEXT_STYLE_CLIP = 0x08
};

int rich4_text_is_bold(int style);
Rich4TextEffect rich4_text_effect(int style);
int rich4_text_main_offset(int style);
int rich4_text_is_vertical(int mode);
int rich4_text_centers_lines(int mode);
int rich4_text_character_extra(int spacing);
int rich4_text_vertical_advance(int height, int style, int spacing);
void rich4_text_apply_anchor(int mode,
                             int width,
                             int height,
                             int *x,
                             int *y);

#endif
