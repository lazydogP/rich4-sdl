#include "app/options_flow.h"

#include <string.h>

void rich4_options_panel_reset(Rich4OptionsPanelState *panel)
{
    if (panel == NULL) return;
    memset(panel, 0, sizeof(*panel));
    panel->hover = -1;
    panel->pressed_control = -1;
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

int rich4_options_panel_hit(const Rich4OptionsPanelState *panel,
                            int x, int y)
{
    static const int rects[][4] = {
        { 0x51, 0x11, 0x80, 0x21 },
        { 0x59, 0x51, 0x98, 0x61 },
        { 0x59, 0x71, 0x98, 0x81 },
        { 0xe3, 0x0e, 0x147, 0x31 },
        { 0xe3, 0x44, 0x147, 0x67 },
        { 0xe3, 0x77, 0x147, 0x9a },
        { 0x12, 0xe2, 0xb1, 0x159 },
        { 0xc2, 0x13a, 0x100, 0x158 },
        { 0x10a, 0x13a, 0x148, 0x158 },
        { 0x62, 0x32, 0x71, 0x41 },
        { 0x42, 0x52, 0x51, 0x61 },
        { 0x42, 0x72, 0x51, 0x81 },
        { 0x62, 0x92, 0x71, 0xa1 },
        { 0xd9, 0xd6, 0x144, 0xec },
        { 0xd9, 0xf6, 0x144, 0x10c },
        { 0xd9, 0x116, 0x144, 0x12c },
        { 0x62, 0xbf, 0xb1, 0xd3 }
    };
    int rel_x;
    int rel_y;
    int i;

    if (panel == NULL || !panel->active) return -1;
    rel_x = x - panel->left;
    rel_y = y - panel->top;
    for (i = 0; i < (int)(sizeof(rects) / sizeof(rects[0])); i++)
        if (rel_x >= rects[i][0] && rel_y >= rects[i][1] &&
            rel_x < rects[i][2] && rel_y < rects[i][3])
            return i;
    return -1;
}

int rich4_options_panel_apply_click(Rich4OptionsPanelState *panel,
                                    int control, int x)
{
    int rel_x;

    if (panel == NULL || !panel->active || control < 0) return 0;
    rel_x = x - panel->left;
    switch (control) {
    case 0:
        panel->draft.game_speed = (unsigned char)clamp_int(
            (rel_x - 0x51) / 16, 0, 2);
        return 1;
    case 1:
        panel->draft.music = (unsigned char)clamp_int(
            (rel_x - 0x59) / 16 + 1, 1, 4);
        return 1;
    case 2:
        panel->draft.sound_effect = (unsigned char)clamp_int(
            (rel_x - 0x59) / 16 + 1, 1, 4);
        return 1;
    case 9: panel->draft.animation ^= 1; return 1;
    case 10: panel->draft.music = panel->draft.music == 0 ? 4 : 0; return 1;
    case 11:
        panel->draft.sound_effect = panel->draft.sound_effect == 0 ? 4 : 0;
        return 1;
    case 12: panel->draft.auto_save ^= 1; return 1;
    case 13:
    case 14:
    case 15: panel->draft.view = (unsigned char)(control - 13); return 1;
    case 16:
        panel->music_bank = panel->music_bank == RICH4_MUSIC_BANK_ORIGINAL
            ? RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY
            : RICH4_MUSIC_BANK_ORIGINAL;
        return 1;
    default: return 0;
    }
}

void rich4_options_panel_restore(Rich4OptionsPanelState *panel)
{
    if (panel == NULL) return;
    panel->draft = panel->saved;
    panel->music_bank = panel->saved_music_bank;
}

void rich4_options_panel_set_hover(Rich4OptionsPanelState *panel, int hover)
{
    if (panel != NULL) panel->hover = hover;
}

void rich4_options_panel_set_pressed(Rich4OptionsPanelState *panel,
                                     int control)
{
    if (panel != NULL) panel->pressed_control = control;
}

int rich4_options_panel_pressed(const Rich4OptionsPanelState *panel)
{
    return panel != NULL ? panel->pressed_control : -1;
}

int rich4_options_panel_begin_action(Rich4OptionsPanelState *panel,
                                     int control)
{
    if (panel == NULL || !panel->active ||
        !rich4_options_panel_context_variant(panel) ||
        control < 3 || control > 5)
        return 0;
    panel->pressed_control = -1;
    panel->pending_action = control;
    panel->confirm_key_action = 0;
    /* The original confirmation helper opens with the left/accept half
     * selected; pointer hover may replace this on the next motion frame. */
    panel->confirm_hover = 1;
    return 1;
}

void rich4_options_panel_cancel_action(Rich4OptionsPanelState *panel)
{
    if (panel == NULL) return;
    panel->pending_action = 0;
    panel->confirm_key_action = 0;
    panel->confirm_hover = 0;
    panel->pressed_control = -1;
}

int rich4_options_panel_has_pending_action(
    const Rich4OptionsPanelState *panel)
{
    return panel != NULL && panel->pending_action >= 3 &&
           panel->pending_action <= 5;
}

int rich4_options_panel_pending_action(
    const Rich4OptionsPanelState *panel)
{
    return rich4_options_panel_has_pending_action(panel)
        ? panel->pending_action : 0;
}

void rich4_options_panel_set_confirm_hover(
    Rich4OptionsPanelState *panel, int hover)
{
    if (panel != NULL) panel->confirm_hover = hover;
}

int rich4_options_panel_confirm_hover(
    const Rich4OptionsPanelState *panel)
{
    return panel != NULL ? panel->confirm_hover : 0;
}

int rich4_options_panel_context_variant(const Rich4OptionsPanelState *panel)
{
    return panel != NULL && panel->return_app_state == RICH4_SCENE_GAME;
}

/* 0x451F8C/0x4520A6: the original calendar uses year%4, from 1998. */
int rich4_options_calendar_days(int year, int month)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    return month == 2 && year % 4 == 0 ? 29 : days[month - 1];
}

int rich4_options_calendar_first(int year, int month)
{
    int y, m, days = 4;
    for (y = 1998; y < year; ++y) days += y % 4 ? 365 : 366;
    for (m = 1; m < month; ++m) days += rich4_options_calendar_days(year, m);
    return days % 7;
}

int rich4_options_child_hit(const Rich4OptionsPanelState *p, int x, int y)
{
    static const int rects[7][4] = {
        {74,21,90,31},{74,31,90,41},{160,21,176,31},{160,31,176,41},
        {9,180,64,210},{72,180,127,210},{134,180,189,210}
    }; /* 0x474CE8 */
    int i;
    x -= p->child_left; y -= p->child_top;
    if (p->child == 3) {
        int first = rich4_options_calendar_first(p->child_draft.year,
                                                p->child_draft.month);
        for (i = 0; i < 7; ++i)
            if (x >= rects[i][0] && y >= rects[i][1] &&
                x < rects[i][2] && y < rects[i][3]) return i;
        for (i = 1; i <= rich4_options_calendar_days(p->child_draft.year,
                                                    p->child_draft.month); ++i) {
            int cell = first + i - 1;
            int cx = 28 + cell % 7 * 23, cy = 80 + cell / 7 * 18;
            if (x >= cx-10 && x < cx+10 && y >= cy-8 && y < cy+8)
                return 100 + i;
        }
    } else if (p->child == 4) {
        if (y >= 25 && y <= 264 && p->editing_key < 0) {
            int row = (y-25)/16;
            if (x >= 105 && x <= 160 && row < 15) return 200 + row;
            if (x >= 257 && x <= 312 && row < 13) return 215 + row;
        }
        if (y >= 281 && y <= 311) {
            if (x >= 17 && x <= 87) return 4;
            if (x >= 130 && x <= 200) return 5;
            if (x >= 242 && x <= 312) return 6;
        }
    }
    return -1;
}

void rich4_options_child_cancel(Rich4OptionsPanelState *p)
{
    /* 0x41191D cancels the pending key edit before closing its window. */
    if (p->child == 4 && p->editing_key >= 0) {
        p->child_draft.hotkeys[p->editing_key] = p->editing_saved;
        p->editing_key = -1;
    } else p->child = 0;
    p->child_pressed = -1;
}

void rich4_options_child_apply(Rich4OptionsPanelState *p,
                              Rich4Config *config, int control)
{
    static const unsigned char defaults[28] = {
        0x26,0x27,0x28,0x25,0x0d,0x1b,9,9,0x59,0x4e,0x20,0x44,
        0x57,0x58,0x43,0x45,0x46,0x4d,0xbc,0xbe,0x41,0x56,
        0x53,0x4c,0x48,0x21,0x22,0x51
    }; /* 0x47EDC2 */
    int i;
    if (p->child == 3) {
        if (control == 0) p->child_draft.month = p->child_draft.month == 1 ? 12 : p->child_draft.month-1;
        if (control == 1) p->child_draft.month = p->child_draft.month == 12 ? 1 : p->child_draft.month+1;
        if (control == 2 && p->child_draft.year > 1998) --p->child_draft.year;
        if (control == 3) ++p->child_draft.year;
        if (control == 4) {
            p->child_draft.year = p->today.year;
            p->child_draft.month = p->today.month;
            p->child_draft.day = p->today.day;
        }
        if (control >= 101 && control <= 131) p->child_draft.day = control-100;
        if (control == 6) {
            /* 0x411A77 commits independently of the outer options draft. */
            config->year = p->saved.year = p->draft.year = p->child_draft.year;
            config->month = p->saved.month = p->draft.month = p->child_draft.month;
            config->day = p->saved.day = p->draft.day = p->child_draft.day;
            p->child = 0;
        }
    } else if (p->child == 4) {
        if (control >= 208 && control < 228) {
            p->editing_key = control - 200;
            p->editing_saved = p->child_draft.hotkeys[p->editing_key];
            memset(&p->child_draft.hotkeys[p->editing_key], 0, sizeof(Rich4Key));
        }
        if (control == 4) {
            for (i = 0; i < 28; ++i) {
                p->child_draft.hotkeys[i].key = defaults[i];
                p->child_draft.hotkeys[i].mod = i == 27 ? 0x11 : 0;
            }
            p->editing_key = -1;
        }
        if (control == 6) {
            memcpy(config->hotkeys, p->child_draft.hotkeys, sizeof(config->hotkeys));
            memcpy(p->saved.hotkeys, config->hotkeys, sizeof(config->hotkeys));
            memcpy(p->draft.hotkeys, config->hotkeys, sizeof(config->hotkeys));
            p->child = 0;
        }
    }
    if (control == 5) p->child = 0;
}

int rich4_options_child_key(Rich4OptionsPanelState *p, int vk)
{
    Rich4Key value;
    int i;
    if (p->child != 4 || p->editing_key < 8 || p->editing_key >= 28 || !vk)
        return 0;
    value = p->child_draft.hotkeys[p->editing_key];
    if (vk == 0x11) {
        p->child_draft.hotkeys[p->editing_key].mod = 0x11;
        return 1;
    }
    value.key = (unsigned char)vk;
    for (i = 0; i < 28; ++i)
        if (i != p->editing_key && p->child_draft.hotkeys[i].key == value.key &&
            p->child_draft.hotkeys[i].mod == value.mod) return 0;
    p->child_draft.hotkeys[p->editing_key] = value;
    p->editing_key = -1;
    return 1;
}
