#include <stdio.h>
#include <SDL/SDL.h>

#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/scene_new_game_setup_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"

static void adjust_screen_rect(SDL_Surface *screen, int x, int y, int w, int h, int adjust)
{
    int px;
    int py;
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (screen == NULL || w <= 0 || h <= 0) {
        return;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    SDL_LockSurface(screen);
    for (py = y0; py < y1; py++) {
        uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + py * screen->pitch);
        for (px = x0; px < x1; px++) {
            Uint8 r;
            Uint8 g;
            Uint8 b;
            int nr;
            int ng;
            int nb;
            SDL_GetRGB(row[px], screen->format, &r, &g, &b);
            nr = (int)r;
            ng = (int)g;
            nb = (int)b;
            if (adjust < -32) adjust = -32;
            if (adjust > 32) adjust = 32;
            if (adjust < 0) {
                int scale = 32 + adjust;
                nr = (nr * scale) / 32;
                ng = (ng * scale) / 32;
                nb = (nb * scale) / 32;
            } else if (adjust > 0) {
                nr += ((255 - nr) * adjust + 31) / 32;
                ng += ((255 - ng) * adjust + 31) / 32;
                nb += ((255 - nb) * adjust + 31) / 32;
            }
            if (nr < 0) nr = 0;
            if (ng < 0) ng = 0;
            if (nb < 0) nb = 0;
            if (nr > 255) nr = 255;
            if (ng > 255) ng = 255;
            if (nb > 255) nb = 255;
            row[px] = (uint16_t)SDL_MapRGB(screen->format, (Uint8)nr, (Uint8)ng, (Uint8)nb);
        }
    }
    SDL_UnlockSurface(screen);
}

static void format_new_game_value(const NewGameSettings *settings, int row, char *out, size_t out_size)
{
    static const char *funds[] = { "300K", "200K", "100K", "50K", "30K", "10K" };
    static const char *traffic[] = { "WALK", "BIKE", "MOTOR", "CAR" };
    static const char *periods[] = { "FREE", "730", "365", "182", "91", "30" };
    static const char *goals[] = { "FREE", "X100", "X50", "X10", "X5", "X3" };

    switch (row) {
    case 0:
        snprintf(out, out_size, "STAGE %d", settings->stage + 1);
        break;
    case 1:
        snprintf(out, out_size, "MAP %d", settings->map + 1);
        break;
    case 2:
        snprintf(out, out_size, "%d PLAYERS", settings->player_extra + 2);
        break;
    case 3:
        snprintf(out, out_size, "%s", funds[settings->fund_index]);
        break;
    case 4:
        snprintf(out, out_size, "%s", traffic[settings->traffic_index]);
        break;
    case 5:
        snprintf(out, out_size, "%s", periods[settings->land_index]);
        break;
    case 6:
        snprintf(out, out_size, "%s DAYS", periods[settings->days_index]);
        break;
    case 7:
        snprintf(out, out_size, "%s", goals[settings->goal_index]);
        break;
    default:
        if (out_size > 0) {
            out[0] = '\0';
        }
        break;
    }
}

static int new_game_setting_value(const NewGameSettings *settings, int row)
{
    if (row == 1) return settings->map;
    if (row == 2) return settings->player_extra;
    if (row == 3) return settings->fund_index;
    if (row == 4) return settings->traffic_index;
    if (row == 5) return settings->land_index;
    if (row == 6) return settings->days_index;
    if (row == 7) return settings->goal_index;
    return 0;
}

static void draw_new_game_character_hover(SDL_Surface *screen,
                                          const GamePreviewAssets *assets,
                                          int character)
{
    Rich4SprSmpChunk character_chunk;
    int col;
    int row;
    int base_x;
    int base_y;
    int label_y;

    if (assets == NULL || !assets->character.has_character_ui || character < 0 || character >= 12) {
        return;
    }
    if (rich4_spr_smp_get_chunk(assets->character.character_data,
                                    assets->character.character_size,
                                    &assets->character.character_spr,
                                    (size_t)character,
                                    &character_chunk) != RICH4_SPR_SMP_OK) {
        return;
    }
    col = character % 6;
    row = character / 6;
    base_x = col * 72;
    base_y = row * 72;

    draw_chunk_topleft(screen,
                       assets->character.character_data,
                       &character_chunk,
                       base_x + 12,
                       base_y + 11,
                       0);

    label_y = row == 0 ? base_y + 83 : 63;
    adjust_screen_rect(screen, base_x + 13, label_y, 70, 20, -20);
    draw_big5_text(screen,
                   base_x + 48,
                   label_y + 10,
                   new_game_character_name(character),
                   16,
                   RICH4_FONT_LIGHT,
                   RICH4_FONT_DARK,
                   3,
                   2);
}

static const char *new_game_setting_label_big5(int list)
{
    static const char *labels[] = {
        "\xb9\x43\xc0\xb8\xa4\x48\xbc\xc6",
        "\xc1\x60\x20\xb8\xea\x20\xaa\xf7",
        "\xa6\xe6\xb6\x69\xa4\xe8\xa6\xa1",
        "\xa4\x67\xa6\x61\xc5\x76\xad\xad",
        "\xb9\x43\xc0\xb8\xae\xc9\xb6\xa1",
        "\xb3\xd3\xa7\x51\xb1\xf8\xa5\xf3"
    };
    if (list < 0 || list >= 6) {
        return "";
    }
    return labels[list];
}

static const char *new_game_setting_value_big5(const NewGameSettings *settings, int row, int option)
{
    static const char *players[] = {
        "\xa4\x47\xa4\x48",
        "\xa4\x54\xa4\x48",
        "\xa5\x7c\xa4\x48"
    };
    static const char *traffic[] = {
        "\xa8\x42\xa6\xe6",
        "\xbe\xf7\xa8\xae",
        "\xa8\x54\xa8\xae"
    };
    static const char *periods[] = {
        "\xb5\x4c\xad\xad\xb4\xc1",
        "\xa4\x47\xa6\x7e",
        "\xa4\x40\xa6\x7e",
        "\xa4\xbb\xad\xd3\xa4\xeb",
        "\xa4\x54\xad\xd3\xa4\xeb",
        "\xa4\x40\xad\xd3\xa4\xeb"
    };
    static const char *goals[] = {
        "\xb5\x4c\xad\xad",
        "\x31\x30\x30\xad\xbf",
        "\x35\x30\xad\xbf",
        "\x31\x30\xad\xbf",
        "\x35\xad\xbf",
        "\x33\xad\xbf"
    };
    static char number_buf[32];
    int value = option;

    if (settings == NULL) {
        return "";
    }
    if (value < 0) {
        value = new_game_setting_value(settings, row);
    }
    if (row == 2 && value >= 0 && value < 3) {
        return players[value];
    }
    if (row == 3) {
        static const int funds[] = { 300000, 200000, 100000, 50000, 30000, 10000 };
        snprintf(number_buf, sizeof(number_buf), "%d", funds[wrap_index(value, 6)]);
        return number_buf;
    }
    if (row == 4 && value >= 0 && value < 3) {
        return traffic[value];
    }
    if ((row == 5 || row == 6 || row == 7) && value >= 0 && value < 6) {
        if (row == 5 || row == 6) {
            return periods[value];
        }
        if (value == 0) {
            return goals[0];
        }
        {
            static const int funds[] = { 300000, 200000, 100000, 50000, 30000, 10000 };
            static const int goal_multipliers[] = { 0, 100, 50, 10, 5, 3 };
            int fund = funds[wrap_index(settings->fund_index, 6)];
            snprintf(number_buf, sizeof(number_buf), "%d", fund * goal_multipliers[value]);
            return number_buf;
        }
    }
    if (row == 1) {
        snprintf(number_buf, sizeof(number_buf), "%d", value + 1);
        return number_buf;
    }
    return "";
}

static void draw_new_game_dropdown(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   const NewGameSettings *settings)
{
    static const struct {
        int x;
        int y;
        int right;
        int bottom;
    } option_rects[] = {
        { 561, 251, 602, 320 },
        { 536, 287, 602, 425 },
        { 561, 323, 602, 392 },
        { 536, 195, 602, 333 },
        { 536, 231, 602, 369 },
        { 516, 267, 602, 405 }
    };
    static const int dropdown_chunks[] = { 5, 6, 5, 6, 6, 7 };
    static const int setting_to_row[] = { 2, 3, 4, 5, 6, 7 };
    int list;
    int row;
    int option_count;
    int selected;
    int i;

    if (settings->dropdown_row < 0 || settings->dropdown_row >= 6) {
        return;
    }
    list = settings->dropdown_row;
    row = setting_to_row[list];
    option_count = (list == 0 || list == 2) ? 3 : 6;
    selected = new_game_setting_value(settings, row);

    draw_setup_chunk(screen,
                     assets,
                     (size_t)dropdown_chunks[list],
                     option_rects[list].x,
                     option_rects[list].y,
                     0);

    SDL_LockSurface(screen);
    for (i = 0; i < option_count; i++) {
        int y = option_rects[list].y + i * 23;
        int highlight = i == settings->dropdown_hover || (settings->dropdown_hover < 0 && i == selected);
        if (highlight) {
            fill_rect(screen,
                      option_rects[list].x + 1,
                      y + 2,
                      option_rects[list].right - option_rects[list].x - 3,
                      20,
                      0xa800);
        }
    }
    SDL_UnlockSurface(screen);
    for (i = 0; i < option_count; i++) {
        int y = option_rects[list].y + i * 23;
        int highlight = i == settings->dropdown_hover || (settings->dropdown_hover < 0 && i == selected);
        draw_big5_text(screen,
                       option_rects[list].x + 6,
                       y + 5,
                       new_game_setting_value_big5(settings, row, i),
                       15,
                       highlight ? 0xffff : RICH4_FONT_DARK,
                       RICH4_FONT_DARK,
                       0,
                       0);
    }
}

static void draw_selected_character_animations(SDL_Surface *screen,
                                               const GamePreviewAssets *assets,
                                               const NewGameSettings *settings,
                                               const int *override_x)
{
    static const int x_positions[3][4] = {
        { 330, 110, 0, 0 },
        { 366, 220, 74, 0 },
        { 385, 275, 165, 55 }
    };
    int i;
    int traffic;
    int group;

    if (assets == NULL || settings == NULL) {
        return;
    }
    traffic = settings->traffic_index;
    if (traffic < 0 || traffic > 2) {
        traffic = 0;
    }
    group = settings->player_extra;
    if (group < 0) group = 0;
    if (group > 2) group = 2;
    for (i = 0; i < settings->selected_count && i < 4; i++) {
        int character = settings->selected_characters[i];
        int frame;
        Rich4SprSmpChunk chunk;
        if (character < 0 || character >= 12 || !assets->character.has_walk[character][traffic]) {
            continue;
        }
        frame = settings->selected_frame[i] % (int)assets->character.walk_spr[character][traffic].num_chunks;
        if (rich4_spr_smp_get_chunk(assets->character.walk_data[character][traffic],
                                        assets->character.walk_size[character][traffic],
                                        &assets->character.walk_spr[character][traffic],
                                        (size_t)frame,
                                        &chunk) == RICH4_SPR_SMP_OK) {
            draw_spr_chunk_at(screen,
                              assets->character.walk_data[character][traffic],
                              assets->character.walk_size[character][traffic],
                              &assets->character.walk_spr[character][traffic],
                              &chunk,
                              override_x != NULL ? override_x[i] : x_positions[group][i],
                              440,
                              1);
        }
    }
}

void draw_new_game_setup(SDL_Surface *screen,
                         const GamePreviewAssets *assets,
                         const NewGameSettings *settings,
                         const VirtualCursor *cursor,
                         int transition)
{
    static const char *labels[] = {
        "STAGE",
        "MAP",
        "PLAYERS",
        "FUND",
        "TRAFFIC",
        "DAYS",
        "GOAL"
    };
    int background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    int hover = rich4_new_game_control_at(cursor->x, cursor->y);
    int pressed = settings->pressed_control;
    int characters_in_front = transition || settings->auto_fill_active ||
                              settings->transition_wait_active;
    int i;

    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT ||
        assets == NULL || !assets->intro.has_jump_background[background_index] ||
        !draw_raw_rgb565_scrolled(screen,
                                  assets->intro.jump_data[background_index],
                                  assets->intro.jump_size[background_index],
                                  settings->scroll_offset,
                                  transition ? 0 : -16)) {
        fill_background(screen);
    }

    if (assets != NULL && assets->menu.has_setup_ui) {
        static const int value_y[] = { 238, 274, 310, 346, 382, 418 };
        static const int map_check_y[] = { 20, 52, 84, 116 };
        static const int setting_to_row[] = { 2, 3, 4, 5, 6, 7 };
        static const int arrow_left[] = { 602, 602, 602, 602, 602, 602 };
        static const int control_top[] = { 226, 262, 298, 334, 370, 406 };
        int character_hover = rich4_new_game_character_at(cursor->x, cursor->y);
        int left_x = 4;
        int left_y = transition ? settings->transition_panel_left_y : 10;
        int right_x = transition ? settings->transition_panel_right_x : 445;

        /* 0x405019: normal selection draws actors before both panels.
         * 0x405D8F / 0x405F80: AI fill and exit draw actors after them. */
        if (!characters_in_front) {
            draw_selected_character_animations(screen, assets, settings, NULL);
        }
        draw_setup_chunk(screen,
                         assets,
                         0,
                         left_x,
                         left_y,
                         0);
        if (assets->character.has_character_ui) {
            for (i = 0; i < 12; i++) {
                Rich4SprSmpChunk character_chunk;
                if (!transition && i == character_hover && !settings->character_used[i]) {
                    continue;
                }
                if (rich4_spr_smp_get_chunk(assets->character.character_data,
                                                assets->character.character_size,
                                                &assets->character.character_spr,
                                                (size_t)i,
                                                &character_chunk) == RICH4_SPR_SMP_OK) {
                    draw_chunk_topleft(screen,
                                       assets->character.character_data,
                                       &character_chunk,
                                       left_x + 4 + (i % 6) * 72,
                                       left_y + 5 + (i / 6) * 72,
                                       0);
                }
            }
        }
        /* 0x40423C composites portrait shading and hover into the left
         * panel, so they must not darken or cover foreground actors. */
        for (i = 0; i < 12; i++) {
            int x = left_x + 4 + (i % 6) * 72;
            int y = left_y + 5 + (i / 6) * 72;
            if (settings->character_used[i]) {
                adjust_screen_rect(screen, x, y, 72, 72, -16);
            }
        }
        if (!transition && character_hover >= 0 &&
            !settings->character_used[character_hover]) {
            draw_new_game_character_hover(screen, assets, character_hover);
        }
        draw_setup_chunk(screen,
                         assets,
                         settings->stage == 0 ? 1 : 21,
                         right_x,
                         10,
                         0);
        if (settings->map >= 0 && settings->map < 4) {
            draw_setup_chunk(screen,
                             assets,
                             8,
                             right_x + 150,
                             10 + map_check_y[settings->map],
                             1);
        }

        if (pressed == 1) {
            draw_setup_chunk_crop(screen,
                                  assets,
                                  settings->stage == 0 ? 1 : 21,
                                  11,
                                  166,
                                  79,
                                  39,
                                  456,
                                  176,
                                  0);
            draw_setup_chunk(screen, assets, 2, 456, 176, 1);
        } else if (pressed == 2) {
            draw_setup_chunk_crop(screen,
                                  assets,
                                  settings->stage == 0 ? 1 : 21,
                                  99,
                                  166,
                                  79,
                                  39,
                                  544,
                                  176,
                                  0);
            draw_setup_chunk(screen, assets, 3, 544, 176, 1);
        } else if (pressed >= 3 && pressed <= 8) {
            draw_setup_chunk(screen,
                             assets,
                             4,
                             arrow_left[pressed - 3],
                             control_top[pressed - 3],
                             1);
        } else if (settings->dropdown_row >= 0 && settings->dropdown_row < 6) {
            draw_setup_chunk(screen,
                             assets,
                             4,
                             arrow_left[settings->dropdown_row],
                             control_top[settings->dropdown_row],
                             1);
        }
        /* Original callback 0x404E44 renders these strings into
         * dword_48A3A0 before transition state 2 blits that whole panel at
         * n445. Keep the text attached to the moving right panel. */
        for (i = 0; i < 6; i++) {
            draw_big5_text(screen,
                           right_x + 8,
                           value_y[i],
                           new_game_setting_label_big5(i),
                           15,
                           0xffff,
                           RICH4_FONT_DARK,
                           1,
                           5);
            draw_big5_text(screen,
                           right_x + 155,
                           value_y[i],
                           new_game_setting_value_big5(settings, setting_to_row[i], -1),
                           15,
                           RICH4_FONT_DARK,
                           RICH4_FONT_DARK,
                           0,
                           6);
        }
        if (!transition) {
            draw_new_game_dropdown(screen, assets, settings);
        }
        if (characters_in_front) {
            draw_selected_character_animations(screen, assets, settings,
                transition ? settings->transition_player_x : NULL);
        }
        return;
    }

    SDL_LockSurface(screen);
    fill_rect(screen, 28, 24, 584, 432, 0x18c3);
    stroke_rect(screen, 28, 24, 584, 432, 0xffff);
    draw_text(screen, 50, 42, "NEW GAME SETUP", 2, 0xffff);

    for (i = 0; i < 7; i++) {
        char value[32];
        uint16_t row_color = (hover == i || settings->selected_row == i) ? 0xffe0 : 0x39e7;
        int y = 76 + i * 42;
        format_new_game_value(settings, i, value, sizeof(value));
        fill_rect(screen, 50, y, 540, 34, row_color);
        stroke_rect(screen, 50, y, 540, 34, 0xffff);
        draw_text(screen, 66, y + 10, labels[i], 2, 0x0000);
        draw_text(screen, 354, y + 10, value, 2, 0x0000);
        draw_text(screen, 530, y + 10, "X", 2, 0x0000);
    }

    fill_rect(screen, 350, 398, 112, 42, hover == 100 ? 0xffe0 : 0x07e0);
    stroke_rect(screen, 350, 398, 112, 42, 0xffff);
    draw_text(screen, 374, 412, "START", 2, 0x0000);

    fill_rect(screen, 478, 398, 112, 42, hover == 101 ? 0xffe0 : 0xf800);
    stroke_rect(screen, 478, 398, 112, 42, 0xffff);
    draw_text(screen, 506, 412, "BACK", 2, 0x0000);
    SDL_UnlockSurface(screen);
}
