#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>
#include "platform/sdl1/render/game_hud_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/runtime_render_api.h"

/* sub_4169BC / Panel.mkf[2], always at (440,280), including combined view. */
void draw_game_calendar(SDL_Surface *screen, const GamePreviewAssets *assets,
                        const GameViewModel *view, int monthly)
{
    static const int season[] = {3,0,0,0,1,1,1,2,2,2,3,3};
    static const char *weekday[] = {
        "\xac\x50\xb4\xc1\xa4\xe9", "\xac\x50\xb4\xc1\xa4\x40",
        "\xac\x50\xb4\xc1\xa4\x47", "\xac\x50\xb4\xc1\xa4\x54",
        "\xac\x50\xb4\xc1\xa5\x7c", "\xac\x50\xb4\xc1\xa4\xad",
        "\xac\x50\xb4\xc1\xa4\xbb"
    };
    const GameCalendarView *date;
    char text[32];
    int special = 0;
    int i;
    uint16_t red = 0xf800;
    if (screen == NULL || assets == NULL || view == NULL) return;
    date = &view->calendar;
    if (date->year <= 0 || date->month < 1 || date->month > 12) return;
    if (!monthly && date->map_index >= 0 &&
        date->map_index < GAME_BACKGROUND_COUNT && date->event_index >= 0 &&
        date->event_index < GAME_BANK_CALENDAR_EVENT_COUNT &&
        game_asset_ready(assets, &assets->ui.has_bank_calendar[date->map_index][date->event_index])) {
        special = draw_raw_rgb565_at(screen,
            assets->ui.bank_calendar_data[date->map_index][date->event_index],
            assets->ui.bank_calendar_size[date->map_index][date->event_index],
            200, 200, 440, 280);
    }
    if (!special)
        draw_game_panel_chunk(screen, assets, 2,
            (size_t)(season[date->month - 1] + (monthly ? 4 : 0)), 440, 280, 0);
    if (monthly) {
        for (i = 1; i <= date->month_days; i++) {
            int cell = date->first_weekday + i - 1;
            int x = 470 + 23 * (cell % 7);
            int y = 378 + 14 * (cell / 7);
            if (i == date->day) {
                SDL_LockSurface(screen);
                stroke_rect_thin(screen, x - 10, y - 6, 20, 14, red);
                SDL_UnlockSurface(screen);
            }
            snprintf(text, sizeof(text), "%d", i);
            draw_big5_text_with_spacing(screen, x, y, text, 12,
                (date->closed_days & (UINT32_C(1) << (i - 1))) ? red : RICH4_FONT_DARK,
                0, 2, 1, 2);
        }
    } else {
        int highlighted = date->weekday == 0 || date->event_index >= 0;
        uint16_t color = highlighted ? red : RICH4_FONT_DARK;
        uint16_t outline = highlighted ? RICH4_FONT_DARK : RICH4_FONT_LIGHT;
        draw_game_panel_chunk_at(screen, assets, 2, 8, 462, 300, 1);
        draw_game_panel_chunk_at(screen, assets, 2, 11, 492, 301, 1);
        draw_big5_text_with_spacing(screen, 454, 352, weekday[date->weekday],
                                   16, color, outline, 6, 1, 3);
        snprintf(text, sizeof(text), "%d", date->day);
        draw_big5_text_with_spacing(screen, 500, 376, text, 60,
                                   color, outline, 6, 1, 2);
    }
    snprintf(text, sizeof(text), "%d", date->year);
    draw_big5_text_with_spacing(screen, 580, 288, text, 24,
                               RICH4_FONT_DARK, RICH4_FONT_LIGHT, 6, 1, 0);
    snprintf(text, sizeof(text), "%d\xa4\xeb", date->month);
    draw_big5_text_with_spacing(screen, 500, 328, text, 28,
                               RICH4_FONT_DARK, RICH4_FONT_LIGHT, 6, 1, 2);
}
