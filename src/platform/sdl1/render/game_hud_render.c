#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdio.h>
#include <limits.h>

#include <SDL/SDL.h>

#include "core/game_constants.h"
#include "core/character_template.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_hud_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/spr_smp.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"
#include "platform/sdl1/runtime_trace.h"

/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c. */

static void draw_game_top_panel_icons(SDL_Surface *screen,
                                      const GamePreviewAssets *assets,
                                      int hover)
{
    size_t i;

    if (screen == NULL || assets == NULL || !game_asset_ready(assets, &assets->ui.has_panel[1])) {
        return;
    }
    for (i = 0; i < 11; i++) {
        Rich4SprSmpChunk chunk;
        int slot_x = (int)i * 40;
        int anchor_x = slot_x + 20;
        int anchor_y = 20;
        size_t chunk_index = i + 1;
        if ((int)i == hover && assets->ui.panel_spr[1].num_chunks > i + 12) {
            chunk_index = i + 12;
        }
        if (rich4_spr_smp_get_chunk(assets->ui.panel_data[1],
                                        assets->ui.panel_size[1],
                                        &assets->ui.panel_spr[1],
                                        chunk_index,
                                        &chunk) != RICH4_SPR_SMP_OK) {
            continue;
        }
        draw_chunk_topleft(screen,
                           assets->ui.panel_data[1],
                           &chunk,
                           anchor_x - chunk.x,
                           anchor_y - chunk.y,
                           1);
    }
}

static void draw_game_top_panel_background(
    SDL_Surface *screen, const GamePreviewAssets *assets)
{
    if (screen == NULL) return;
    if (!draw_game_panel_chunk(screen, assets, 1, 0, 0, 0, 0)) {
        SDL_LockSurface(screen);
        fill_rect(screen, 0, 0, 440, 40, 0x18c3);
        draw_text(screen, 12, 12, "MAP", 2, 0xffff);
        SDL_UnlockSurface(screen);
    }
}

void draw_game_top_panel_normal(SDL_Surface *screen,
                                const GamePreviewAssets *assets)
{
    if (screen == NULL) return;
    draw_game_top_panel_background(screen, assets);
    draw_game_top_panel_icons(screen, assets, -1);
}

static int draw_game_hud_portrait(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  int character)
{
    if (screen == NULL || assets == NULL || character < 0 || character >= 12) {
        return 0;
    }
    if (assets->character.has_map_character[character] &&
        assets->character.map_character_spr[character].num_chunks > 0) {
        return draw_spr_smp_center(screen,
                                   assets->character.map_character_resource[character].data,
                                   assets->character.map_character_resource[character].size,
                                   &assets->character.map_character_spr[character],
                                   0,
                                   482,
                                   40);
    }
    return 0;
}

static void draw_game_hud_ally(SDL_Surface *screen,
    const GamePreviewAssets *assets, int character)
{
    if (assets == NULL || character < 0 || character >= 12 ||
        !assets->character.has_map_character[character]) return;
    draw_spr_smp_center(screen,
        assets->character.map_character_resource[character].data,
        assets->character.map_character_resource[character].size,
        &assets->character.map_character_spr[character], 1, 524, 64);
}

static void draw_game_compact_labels(SDL_Surface *screen)
{
    /* 0x41805D/0x41808A writes these into chunk 4 before any portrait. */
    draw_big5_text(screen, 530, 40, "\xb2\x7b\xaa\xf7", 12, RICH4_FONT_DARK, 0, 2, 0);
    draw_big5_text(screen, 530, 62, "\xa6\x73\xb4\xda", 12, RICH4_FONT_DARK, 0, 2, 0);
}

/* dword_47ED5A[4..7], sub_415F69/sub_4166F8 special-actor branch. */
static void draw_game_special_hud(SDL_Surface *screen,
    const GamePreviewAssets *assets, const GameHudView *hud, int compact)
{
    static const char *name[] = {
        "\xa4\x70\xb0\xbd", "\xb1\x6a\xb5\x73",
        "\xac\x79\xaa\x5d", "\xb6\xa1\xbf\xd2"
    };
    draw_game_panel_chunk(screen, assets, 0, compact ? 4 : 5, 440, 0, 0);
    if (compact) draw_game_compact_labels(screen);
    draw_big5_text_with_spacing(screen, 582, compact ? 20 : 40,
        name[hud->special_actor - 4], compact ? 20 : 22,
        RICH4_FONT_DARK, 0, 2, 0, 2);
    draw_game_hud_ally(screen, assets, hud->special_portrait_character);
}

static void draw_big5_text_vertical(SDL_Surface *screen,
                                    int x,
                                    int y,
                                    const char *text,
                                    int height,
                                    uint16_t fg,
                                    uint16_t shadow,
                                    int style,
                                    int spacing)
{
    draw_big5_text_with_spacing(
        screen, x, y, text, height, fg, shadow, style, spacing, 3);
}

static void draw_game_hud_tab_names(SDL_Surface *screen, int active)
{
    static const char *tabs[] = {
        "\xb8\xea\xaa\xf7",
        "\xa6\x61\xb2\xa3",
        "\xaa\xd1\xb2\xbc",
        "\xa8\xe4\xa5\x4c"
    };
    /* 0x415D0D stores 15, 88, 158 and 230; 0x4161D7 adds 20. */
    static const int y[] = { 35, 108, 178, 250 };
    int i;

    for (i = 0; i < 4; i++) {
        uint16_t color = i == active ? RICH4_FONT_DARK : 0x4208;
        draw_big5_text_vertical(
            screen, 627, y[i], tabs[i], 18, color, 0, 2, 0);
    }
}

static void draw_game_hud_labels(SDL_Surface *screen, int active)
{
    static const char *labels[4][3] = {
        {
            "\xb2\x7b\x20\x20\xaa\xf7",
            "\xa6\x73\x20\x20\xb4\xda",
            "\xc1\x60\xb8\xea\xb2\xa3"
        },
        {
            "\xa4\x67\x20\x20\xa6\x61",
            "\xb3\x73\xc2\xea\xa9\xb1",
            "\xb3\x5d\x20\x20\xac\x49"
        },
        {
            "\xc1\x60\xa5\xab\xad\xc8",
            "\xa6\xa8\x20\x20\xa5\xbb",
            "\xb8\x67\xc0\xe7\xc5\x76"
        },
        {
            "\xc2\x49\x20\x20\xa8\xf7",
            "\xb6\x55\x20\x20\xb4\xda",
            "\xab\x4f\xc0\x49\xb4\xc1"
        }
    };
    static const int y[] = { 80, 145, 208 };
    int i;

    for (i = 0; i < 3; i++) {
        draw_big5_text(screen, 450, y[i], labels[active][i], 12, RICH4_FONT_DARK, 0, 2, 0);
    }
}

static void draw_game_hud_price_index(SDL_Surface *screen, int price_index)
{
    char text[32];
    if (price_index <= 0) price_index = 1;
    snprintf(text, sizeof(text), "\xaa\xab\xbb\xf9\xab\xfc\xbc\xc6\x20\x20%d", price_index);
    draw_big5_text(screen, 450, 260, text, 12, RICH4_FONT_DARK, 0, 2, 0);
}

static void draw_game_hud_value(SDL_Surface *screen, int right_x, int y, const char *text)
{
    const int height = 22;
    draw_big5_text_with_spacing(screen,
                   right_x,
                   y,
                   text,
                   height,
                   RICH4_FONT_DARK,
                   0,
                   2,
                   0,
                   1);
}

static void draw_game_info_tabs(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                const GameViewModel *view)
{
    int active;
    char value[32];

    if (view == NULL) return;
    if (view->hud.special_actor >= 4 && view->hud.special_actor < 8) {
        draw_game_special_hud(screen, assets, &view->hud, 0);
        return;
    }
    active = view->hud.active_tab;
    if (!draw_game_panel_chunk(screen, assets, 0, (size_t)active, 440, 0, 0)) {
        SDL_LockSurface(screen);
        fill_rect(screen, 440, 0, 200, 280, 0x2104);
        SDL_UnlockSurface(screen);
    }
    draw_game_hud_labels(screen, active);
    draw_game_hud_tab_names(screen, active);
    draw_game_hud_price_index(screen, view->hud.price_index);
    if (!view->hud.visible || view->actor_count <= 0) {
        return;
    }

    SDL_LockSurface(screen);
    fill_rect(screen, 523, 57, 86, 12, 0);
    /* 0x4161F8 reads the current player's +4 character color and passes it
     * through sub_4551F0 before filling this exact 86x12 HUD rectangle. */
    fill_rect(screen, 522, 56, 86, 12,
              rich4_character_rgb565_color(
                  rich4_character_template(view->hud.character)->color));
    SDL_UnlockSurface(screen);
    draw_game_hud_portrait(screen, assets, view->hud.character);
    draw_game_hud_ally(screen, assets, view->hud.ally_character);
    draw_big5_text_with_spacing(screen,
                   564,
                   40,
                   new_game_character_name(view->hud.character),
                   22,
                   RICH4_FONT_DARK,
                   0,
                   2,
                   0,
                   2);
    if (active == 0) {
        format_hud_number(value, sizeof(value), view->hud.cash, 1);
        draw_game_hud_value(screen, 600, 102, value);
        format_hud_number(value, sizeof(value), view->hud.bank, 1);
        draw_game_hud_value(screen, 600, 166, value);
        format_hud_number(value, sizeof(value), view->hud.total_assets, 1);
        draw_game_hud_value(screen, 600, 230, value);
    } else if (active == 1) {
        format_hud_number(value, sizeof(value), view->hud.owned_property_count, 0);
        draw_game_hud_value(screen, 600, 102, value);
        format_hud_number(value, sizeof(value), view->hud.owned_chain_count, 0);
        draw_game_hud_value(screen, 600, 166, value);
        format_hud_number(value, sizeof(value), view->hud.built_facility_count, 0);
        draw_game_hud_value(screen, 600, 230, value);
    } else if (active == 2) {
        /* 0x41646C: 股票 tab displays value at the current quote; unlike the
         * integer-truncated bank total, this path rounds the accumulated
         * floating-point market value. */
        format_hud_number(value, sizeof(value), view->hud.stock_market_value, 1);
        draw_game_hud_value(screen, 600, 102, value);
        format_hud_number(value, sizeof(value), view->hud.stock_cost, 1);
        draw_game_hud_value(screen, 600, 166, value);
        format_hud_number(value, sizeof(value),
                          view->hud.stock_company_control_count, 0);
        draw_game_hud_value(screen, 600, 230, value);
    } else if (active == 3) {
        /* 0x4165E1: 點券, 貸款, 保險期. */
        format_hud_number(value, sizeof(value), view->hud.points, 0);
        draw_game_hud_value(screen, 600, 102, value);
        format_hud_number(value, sizeof(value), view->hud.loan, 1);
        draw_game_hud_value(screen, 600, 166, value);
        /* 0x416604 formats this row with ref_00463902 ("%d天"), rather
         * than the grouped numeric formatter used by the other HUD values. */
        snprintf(value, sizeof(value), "%d\xa4\xd1", view->hud.insurance_days);
        draw_game_hud_value(screen, 600, 230, value);
        if (view->hud.loan != 0 && view->hud.loan_days_remaining != 0 &&
            view->hud.loan_days_remaining != INT_MAX) {
            snprintf(value, sizeof(value), "%d\xa4\xd1", view->hud.loan_days_remaining);
            draw_big5_text(screen, 500, 146, value, 12, RICH4_FONT_DARK, 0, 2, 0);
        }
    }
}

static void draw_game_compact_hud(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  const GameViewModel *view)
{
    char value[32];
    if (screen == NULL || view == NULL || !view->hud.visible) return;
    if (view->hud.special_actor >= 4 && view->hud.special_actor < 8) {
        draw_game_special_hud(screen, assets, &view->hud, 1);
        return;
    }
    draw_game_panel_chunk(screen, assets, 0, 4, 440, 0, 0);
    draw_game_compact_labels(screen);
    SDL_LockSurface(screen);
    fill_rect(screen, 529, 33, 106, 4, 0);
    fill_rect(screen, 528, 32, 106, 4, rich4_character_rgb565_color(
        rich4_character_template(view->hud.character)->color));
    SDL_UnlockSurface(screen);
    draw_game_hud_portrait(screen, assets, view->hud.character);
    draw_game_hud_ally(screen, assets, view->hud.ally_character);
    draw_big5_text_with_spacing(screen, 582, 16,
        new_game_character_name(view->hud.character), 20, RICH4_FONT_DARK, 0, 2, 0, 2);
    format_hud_number(value, sizeof(value), view->hud.cash, 1);
    draw_big5_text(screen, 634, 41, value, 12, RICH4_FONT_DARK, 0, 2, 1);
    format_hud_number(value, sizeof(value), view->hud.bank, 1);
    draw_big5_text(screen, 634, 63, value, 12, RICH4_FONT_DARK, 0, 2, 1);
}

void draw_game_layout(SDL_Surface *screen,
                      const GamePreviewAssets *assets,
                      const GameViewModel *view,
                      int view_mode)
{
    unsigned int started = runtime_profile_now();
    draw_game_top_panel_background(screen, assets);
    draw_game_top_panel_icons(screen, assets,
                              view != NULL ? view->hud.top_button_hover : -1);
    runtime_profile_detail(PROFILE_UI_TOOLBAR, started);
    started = runtime_profile_now();
    if (view_mode == 2)
        draw_game_compact_hud(screen, assets, view);
    else
        draw_game_info_tabs(screen, assets, view);
    runtime_profile_detail(PROFILE_UI_HUD, started);
}
