#include "platform/sdl1/game_lazy_assets.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "app/stock_interaction_flow.h"
#include "core/character_speech_rules.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_bank_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

/* R10.1c: extracted verbatim from src/platform/sdl1/runtime_main.c. Only
 * draw_game_bank_interaction is public (dispatched from the route-interaction
 * renderer); the transfer/value/loan helpers stay file-static. */

static void draw_game_bank_transfer(SDL_Surface *screen,
                                    const GamePreviewAssets *assets,
                                    const GameSceneView *scene)
{
    Rich4SprSmpChunk chunk;
    char amount[16];
    int i;
    int x = 304;
    int maximum;
    int slider_width;
    if (screen == NULL || assets == NULL || scene == NULL ||
        !game_asset_ready(assets, &assets->ui.has_panel[24])) return;
    /* 0x436EF8 is the pass/stop transfer page and exclusively owns
     * Panel.MKF[24]. */
    if (rich4_spr_smp_get_chunk(assets->ui.panel_data[24],
                                assets->ui.panel_size[24],
                                &assets->ui.panel_spr[24], 0,
                                &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_topleft(screen, assets->ui.panel_data[24], &chunk, 60, 71, 1);
    if (rich4_spr_smp_get_chunk(assets->ui.panel_data[24],
                                assets->ui.panel_size[24],
                                &assets->ui.panel_spr[24],
                                (size_t)(scene->bank_interaction_mode + 1),
                                &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.panel_data[24], &chunk,
                      scene->bank_interaction_mode == 0 ? 117 : 199,
                      120, 1);
    maximum = scene->bank_interaction_mode == 0
        ? scene->bank_savings : scene->bank_cash;
    snprintf(amount, sizeof(amount), "%d", scene->bank_interaction_amount);
    for (i = (int)strlen(amount) - 1; i >= 0 && x >= 124; i--, x -= 20) {
        int digit = amount[i] - '0';
        if (digit >= 0 && digit <= 9 &&
            rich4_spr_smp_get_chunk(assets->ui.panel_data[24],
                                    assets->ui.panel_size[24],
                                    &assets->ui.panel_spr[24],
                                    (size_t)(19 + digit),
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->ui.panel_data[24], &chunk, x, 172, 1);
    }
    slider_width = maximum > 0
        ? (int)(((long long)scene->bank_interaction_amount * 34 / maximum) * 6)
        : 0;
    if (slider_width < 0) slider_width = 0;
    if (slider_width > 204) slider_width = 204;
    if (slider_width > 0 &&
        rich4_spr_smp_get_chunk(assets->ui.panel_data[24],
                                assets->ui.panel_size[24],
                                &assets->ui.panel_spr[24], 4,
                                &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_crop_topleft(screen, assets->ui.panel_data[24], &chunk,
                                0, 0, slider_width, 26, 118, 210, 1);
    if (scene->bank_interaction_pressed_control >= 0 &&
        scene->bank_interaction_pressed_control < 18 &&
        scene->bank_interaction_pressed_control != 3 &&
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[24], assets->ui.panel_size[24],
            &assets->ui.panel_spr[24],
            (size_t)(scene->bank_interaction_pressed_control + 1),
            &chunk) == RICH4_SPR_SMP_OK) {
        /* 0x436EF8 passes Rect.left/Rect.top to sub_4563F5.  That renderer
         * applies each chunk's own hotspot; these are therefore the hit
         * rectangle origins, not visual centers. */
        static const int pressed_origins[18][2] = {
            {117,120},{199,120},{281,120},{190,218},
            {118,282},{157,282},{196,282},{118,301},{157,301},{196,301},
            {118,320},{157,320},{196,320},{118,339},{157,339},{196,339},
            {243,304},{235,331}
        };
        draw_chunk_at(screen, assets->ui.panel_data[24], &chunk,
                      pressed_origins[scene->bank_interaction_pressed_control][0],
                      pressed_origins[scene->bank_interaction_pressed_control][1],
                      1);
    }
}

static void format_game_bank_money(char *out, size_t out_capacity, int value)
{
    char raw[32];
    char grouped[32];
    size_t length;
    size_t start;
    size_t i;
    size_t written = 0;

    if (out == NULL || out_capacity == 0) return;
    snprintf(raw, sizeof(raw), "%d", value);
    length = strlen(raw);
    start = raw[0] == '-' ? 1u : 0u;
    if (raw[0] == '-' && written + 1 < sizeof(grouped))
        grouped[written++] = '-';
    for (i = start; i < length && written + 1 < sizeof(grouped); i++) {
        if (i > start && (length - i) % 3u == 0)
            grouped[written++] = ',';
        grouped[written++] = raw[i];
    }
    grouped[written] = '\0';
    snprintf(out, out_capacity, "$%s", grouped);
}

static void draw_game_bank_value(SDL_Surface *screen, int x, int y, int value,
                                 int height, uint16_t foreground,
                                 uint16_t background, int style, int spacing,
                                 int anchor)
{
    char text[32];
    format_game_bank_money(text, sizeof(text), value);
    draw_big5_text_with_spacing(screen, x, y, text, height, foreground,
                                background, style, spacing, anchor);
}

static int game_bank_amount_maximum(const GameSceneView *scene)
{
    int maximum;
    if (scene == NULL) return 0;
    if (scene->bank_interaction_mode == 0)
        maximum = scene->bank_loan_available;
    else if (scene->bank_interaction_mode == 1)
        maximum = scene->bank_loan;
    else if (scene->bank_interaction_mode == 2)
        maximum = scene->bank_special_available;
    else
        maximum = scene->bank_special_finance;
    return maximum > 0 ? maximum : 0;
}

void draw_game_quantity_panel(SDL_Surface *screen, const GamePreviewAssets *assets,
    int amount, int maximum, int pressed_control, int x, int y)
{
    static const int pressed_origins[16][2] = {
        {0,0},{0,0},{264,207},{320,207},
        {264,239},{304,239},{344,239},
        {264,263},{304,263},{344,263},
        {264,287},{304,287},{344,287},
        {264,311},{304,311},{344,311}
    };
    Rich4SprSmpChunk bar;
    Rich4SprSmpChunk fill;
    char text[32];
    int bar_width;
    if (screen == NULL || assets == NULL ||
        !game_asset_ready(assets, &assets->ui.has_panel[21])) return;
    /* 0x453544/fcn_0045297e does not render this value through the font
     * path.  It draws Panel[21] digit chunks 16..25 from right to left,
     * starting at (363,155), with a 12px step.  The old SDL text call used
     * the stock panel's approximate 18px right-aligned glyphs, which makes
     * the loan amount visibly different from the original. */
    draw_game_panel_chunk_at(screen, assets, 21, 0, x, y, 1);
    if (rich4_spr_smp_get_chunk(
            assets->ui.panel_data[21], assets->ui.panel_size[21],
            &assets->ui.panel_spr[21], 1, &bar) != RICH4_SPR_SMP_OK)
        return;
    draw_game_panel_chunk_at(screen, assets, 21, 1, x + 10, y + 42, 1);
    bar_width = rich4_app_stock_quantity_bar_width(
        amount, maximum);
    if (bar_width > 0 &&
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[21], assets->ui.panel_size[21],
            &assets->ui.panel_spr[21], 0, &fill) == RICH4_SPR_SMP_OK)
        draw_spr_chunk_crop_topleft(
            screen, assets->ui.panel_data[21], assets->ui.panel_size[21],
            &assets->ui.panel_spr[21], &fill, 10, 42, bar_width, bar.height,
            x + 10, y + 42, 1);
    snprintf(text, sizeof(text), "%d", amount);
    {
        int digit_index = (int)strlen(text) - 1;
        int digit_count = 0;
        while (digit_index >= 0 && digit_count < 9) {
            int digit = text[digit_index] - '0';
            if (digit >= 0 && digit <= 9)
                draw_game_panel_chunk_at(
                    screen, assets, 21, (size_t)(16 + digit),
                    x + 107 - digit_count * 12, y + 11, 1);
            digit_index--;
            digit_count++;
        }
    }
    if (pressed_control >= 2 &&
        pressed_control <= 15)
        /* sub_452C02 indexes byte_47E6D8's x/y/w/h records and passes the
         * resulting top-left hotspot to sub_456512. */
        draw_game_panel_chunk_at(
            screen, assets, 21,
            (size_t)pressed_control,
            pressed_origins[pressed_control][0] + x - 256,
            pressed_origins[pressed_control][1] + y - 144, 1);
}

static void draw_game_bank_amount_panel(SDL_Surface *screen,
    const GamePreviewAssets *assets, const GameSceneView *scene)
{
    if (scene != NULL && scene->bank_interaction_amount_entry)
        draw_game_quantity_panel(screen, assets, scene->bank_interaction_amount,
            game_bank_amount_maximum(scene), scene->bank_interaction_pressed_control, 256, 144);
}

static void draw_game_bank_speech(SDL_Surface *screen,
                                  const GamePreviewAssets *assets,
                                  const GameSceneView *scene)
{
    Rich4PlayerSayText decoded;
    Rich4SprSmpChunk bubble;
    int left;
    int top;
    if (scene->bank_interaction_speech == NULL ||
        !game_asset_ready(assets, &assets->ui.has_panel[23])) return;
    rich4_player_say_decode(
        scene->bank_interaction_speech->big5,
        scene->bank_interaction_speech->speaking_resource, &decoded);
    if (decoded.display_text == NULL || decoded.display_text[0] == '\0' ||
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[23], assets->ui.panel_size[23],
            &assets->ui.panel_spr[23], 21, &bubble) != RICH4_SPR_SMP_OK)
        return;
    left = 240 - bubble.x;
    top = 80 - bubble.y;
    /* sub_434186 configures Panel23 chunk 21 at (240,80), then
     * sub_44ECB6 centers 20px text with the same +20 horizontal offset. */
    draw_game_panel_chunk_at(screen, assets, 23, 21, 240, 80, 1);
    draw_big5_text(
        screen, left + 20 + bubble.width / 2, top + bubble.height / 2,
        decoded.display_text, 20,
        rich4_character_rgb565_color(0x00101010u), 0, 2, 4);
}

static void draw_game_bank_loan(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                const GameSceneView *scene)
{
    static const unsigned char season_by_month[12] = {
        3, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3
    };
    static const char *const weekday_names[7] = {
        "\xac\x50\xb4\xc1\xa4\xe9", "\xac\x50\xb4\xc1\xa4\x40",
        "\xac\x50\xb4\xc1\xa4\x47", "\xac\x50\xb4\xc1\xa4\x54",
        "\xac\x50\xb4\xc1\xa5\x7c", "\xac\x50\xb4\xc1\xa4\xad",
        "\xac\x50\xb4\xc1\xa4\xbb"
    };
    static const char cash_label[] = "\xb2\x7b  \xaa\xf7";
    static const char savings_label[] = "\xa6\x73  \xb4\xda";
    static const char loan_label[] = "\xb6\x55  \xb4\xda";
    static const char apply_label[] =
        "\xa5\xd3\xbd\xd0\xb6\x55\xb4\xda";
    static const char repay_label[] =
        "\xc0\x76\xc1\xd9\xb6\x55\xb4\xda";
    static const char special_label[] =
        "\xaf\x53\xa7\x4f\xbf\xc4\xb8\xea";
    static const char customer_savings_label[] =
        "\xab\xc8\xa4\xe1\xa6\x73\xb4\xda\xc1\x60\xc3\x42";
    static const char special_current_label[] =
        "\xa5\xd8\xab\x65\xbf\xc4\xb8\xea\xaa\xf7\xc3\x42";
    static const char special_available_label[] =
        "\xa9\x7c\xa5\x69\xbf\xc4\xb8\xea\xaa\xf7\xc3\x42";
    static const char borrow_label[] =
        "\xb6\x67\xc2\xe0\xb2\x7b\xaa\xf7";
    static const char short_repay_label[] =
        "\xc2\x6b\xc1\xd9\xb4\xda\xb6\xb5";
    Rich4SprSmpChunk chunk;
    uint16_t white = rich4_character_rgb565_color(0x00f0f0f0u);
    uint16_t dark = rich4_character_rgb565_color(0x00101010u);
    uint16_t special_dark = rich4_character_rgb565_color(0x00202020u);
    uint16_t gray = rich4_character_rgb565_color(0x00808080u);
    int character;
    int card_x;
    if (screen == NULL || assets == NULL || scene == NULL ||
        !game_asset_ready(assets, &assets->ui.has_panel[23])) return;
    if (rich4_spr_smp_get_chunk(
            assets->ui.panel_data[23], assets->ui.panel_size[23],
            &assets->ui.panel_spr[23],
            scene->bank_interaction_special_page ? 2u : 0u,
            &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_topleft(screen, assets->ui.panel_data[23], &chunk, 0, 0, 0);
    if (!scene->bank_interaction_special_page)
        draw_game_panel_chunk_at(screen, assets, 23, 1, 320, 240, 1);
    if (scene->bank_interaction_special_page) {
        draw_game_panel_chunk(screen, assets, 23, 20, 10, 125, 1);
        draw_game_panel_chunk(screen, assets, 23, 16, 11, 305, 1);
        draw_game_panel_chunk(screen, assets, 23, 16, 11, 362, 1);
        draw_game_panel_chunk(screen, assets, 23, 18, 11, 419, 1);
    }
    if (scene->bank_interaction_special_page) {
        draw_big5_text_with_spacing(screen, 443, 427, special_label, 26,
                                    dark, 0x0000, 2, 1, 2);
        draw_big5_text_with_spacing(screen, 67, 324, borrow_label, 20,
                                    white, 0x0000, 2, 0, 2);
        draw_big5_text_with_spacing(screen, 67, 382, short_repay_label, 20,
                                    white, 0x0000, 2, 0, 2);
        draw_big5_text_with_spacing(screen, 78, 147, customer_savings_label,
                                    16, special_dark, 0x0000, 2, 0, 2);
        draw_big5_text_with_spacing(screen, 78, 195, special_current_label,
                                    16, special_dark, 0x0000, 2, 0, 2);
        draw_big5_text_with_spacing(screen, 78, 243, special_available_label,
                                    16, special_dark, 0x0000, 2, 0, 2);
        draw_game_bank_value(
            screen, 128, 163,
            scene->bank_special_available + scene->bank_special_finance,
            16, white, 0x0000, 2, 1, 1);
        draw_game_bank_value(screen, 128, 211,
                             scene->bank_special_finance,
                             16, white, 0x0000, 2, 1, 1);
        draw_game_bank_value(screen, 128, 259,
                             scene->bank_special_available,
                             16, white, 0x0000, 2, 1, 1);
        if (scene->bank_loan_disabled &&
            rich4_spr_smp_get_chunk(
                assets->ui.panel_data[23], assets->ui.panel_size[23],
                &assets->ui.panel_spr[23], 23,
                &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->ui.panel_data[23], &chunk,
                          67, 324, 1);
    } else {
        /* sub_434186 composes the static controls before the speech bubble,
         * and before the right-hand surfaces are blitted. */
        draw_big5_text_with_spacing(screen, 345, 345, apply_label, 26,
                                    dark, 0x0000, 2, 1, 2);
        draw_big5_text_with_spacing(screen, 530, 345, repay_label, 26,
                                    dark, 0x0000, 2, 1, 2);
        if (scene->bank_special_enabled)
            draw_big5_text_with_spacing(screen, 492, 245, special_label, 14,
                                        gray, 0x0000, 2, 1, 2);
        if (scene->bank_loan_disabled &&
            rich4_spr_smp_get_chunk(
                assets->ui.panel_data[23], assets->ui.panel_size[23],
                &assets->ui.panel_spr[23], 23, &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->ui.panel_data[23], &chunk,
                          345, 345, 1);
    }
    draw_game_bank_speech(screen, assets, scene);
    {
        card_x = scene->bank_interaction_card_x;
        if (rich4_spr_smp_get_chunk(
                assets->ui.panel_data[23], assets->ui.panel_size[23],
                &assets->ui.panel_spr[23], 15, &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_topleft(
                screen, assets->ui.panel_data[23], &chunk, card_x, 0, 1);
        character = clamp_int(scene->bank_character, 0, 11);
        if (assets->character.has_map_character[character] &&
            rich4_spr_smp_get_chunk(
                assets->character.map_character_data[character],
                assets->character.map_character_size[character],
                &assets->character.map_character_spr[character], 0,
                &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->character.map_character_data[character],
                          &chunk, card_x + 42, 40, 1);
        /* 0x433D6E draws all three labels before the name and numeric values.
         * The name uses its 22px bold outlined style. */
        draw_big5_text_with_spacing(screen, card_x + 10, 80, cash_label, 12,
                                    white, dark, 4, 0, 0);
        draw_big5_text_with_spacing(screen, card_x + 10, 145, savings_label,
                                    12, white, dark, 4, 0, 0);
        draw_big5_text_with_spacing(screen, card_x + 10, 208, loan_label, 12,
                                    white, dark, 4, 0, 0);
        draw_big5_text_with_spacing(screen, card_x + 82, 28,
                                    new_game_character_name(character), 22,
                                    white, dark, 6, 0, 0);
        draw_game_bank_value(screen, card_x + 180, 100,
                             scene->bank_cash, 22, white, dark, 6, 0, 1);
        draw_game_bank_value(screen, card_x + 180, 164,
                             scene->bank_savings, 22, white, dark, 6, 0, 1);
        draw_game_bank_value(screen, card_x + 180, 228,
                             scene->bank_loan, 22, white, dark, 6, 0, 1);
        if (scene->bank_calendar_map_index >= 0 &&
            scene->bank_calendar_map_index < GAME_BACKGROUND_COUNT &&
            scene->bank_calendar_event_index >= 0 &&
            scene->bank_calendar_event_index < GAME_BANK_CALENDAR_EVENT_COUNT &&
            game_asset_ready(assets, &assets->ui.has_bank_calendar[scene->bank_calendar_map_index]
                                        [scene->bank_calendar_event_index])) {
            draw_raw_rgb565_at(
                screen,
                assets->ui.bank_calendar_data[scene->bank_calendar_map_index]
                                             [scene->bank_calendar_event_index],
                assets->ui.bank_calendar_size[scene->bank_calendar_map_index]
                                             [scene->bank_calendar_event_index],
                200, 200, card_x, 280);
        } else if (game_asset_ready(assets, &assets->ui.has_panel[2]) &&
                   assets->ui.panel_spr[2].num_chunks > 0) {
            size_t season = scene->bank_calendar_month >= 1 &&
                    scene->bank_calendar_month <= 12
                ? season_by_month[scene->bank_calendar_month - 1] : 0u;
            if (season >= assets->ui.panel_spr[2].num_chunks) season = 0;
            if (rich4_spr_smp_get_chunk(
                    assets->ui.panel_data[2], assets->ui.panel_size[2],
                    &assets->ui.panel_spr[2], season,
                    &chunk) == RICH4_SPR_SMP_OK)
                draw_chunk_topleft(screen, assets->ui.panel_data[2], &chunk,
                                   card_x, 280, 1);
        }
        if (scene->bank_calendar_year > 0) {
            char calendar_text[48];
            snprintf(calendar_text, sizeof(calendar_text), "%d",
                     scene->bank_calendar_day);
            draw_big5_text_with_spacing(screen, card_x + 60, 376,
                                        calendar_text, 60,
                                        dark, 0xffff, 6, 1, 2);
            if (scene->bank_calendar_weekday >= 0 &&
                scene->bank_calendar_weekday < 7)
                draw_big5_text_with_spacing(
                    screen, card_x + 14, 352,
                    weekday_names[scene->bank_calendar_weekday], 16,
                    dark, 0xffff, 6, 1, 3);
            snprintf(calendar_text, sizeof(calendar_text), "%d",
                     scene->bank_calendar_year);
            draw_big5_text_with_spacing(screen, card_x + 140, 288,
                                        calendar_text, 24,
                                        dark, 0xffff, 6, 1, 0);
            snprintf(calendar_text, sizeof(calendar_text),
                     "%d\xa4\xeb", scene->bank_calendar_month);
            draw_big5_text_with_spacing(screen, card_x + 60, 328,
                                        calendar_text, 28,
                                        dark, 0xffff, 6, 1, 2);
            if (scene->bank_loan_days_remaining != INT_MAX) {
                snprintf(calendar_text, sizeof(calendar_text),
                         "\xb6\x5a\xc1\xd9\xb4\xda\xa4\xe9%d\xa4\xd1",
                         scene->bank_loan_days_remaining);
                draw_big5_text_with_spacing(screen, card_x + 20, 456,
                                            calendar_text, 20,
                                            dark, 0xffff, 6, 1, 5);
            }
        }
    }
    /* The timer callback blits the character after both right-hand surfaces;
     * keep the face and mouth above the card/calendar but below the input
     * panel, matching sub_435062/sub_436034. */
    if (scene->bank_interaction_face_chunk >= 0 &&
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[23], assets->ui.panel_size[23],
            &assets->ui.panel_spr[23],
            (size_t)scene->bank_interaction_face_chunk,
            &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.panel_data[23], &chunk,
                      scene->bank_interaction_special_page ? 496 : 140,
                      scene->bank_interaction_special_page ? 162 : 114, 1);
    if (scene->bank_interaction_mouth_chunk >= 0 &&
        rich4_spr_smp_get_chunk(
            assets->ui.panel_data[23], assets->ui.panel_size[23],
            &assets->ui.panel_spr[23],
            (size_t)scene->bank_interaction_mouth_chunk,
            &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.panel_data[23], &chunk,
                      140, 152, 1);
    draw_game_bank_amount_panel(screen, assets, scene);
}

void draw_game_bank_interaction(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                const GameSceneView *scene)
{
    if (scene != NULL &&
        scene->route_interaction_type == GAME_COMMAND_BANK_WITHDRAW)
        draw_game_bank_transfer(screen, assets, scene);
    else
        draw_game_bank_loan(screen, assets, scene);
}
