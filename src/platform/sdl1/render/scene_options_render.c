#include "platform/sdl1/game_lazy_assets.h"
#include <string.h>
#include <stdio.h>
#include "platform/sdl1/render/options_original_text.h"

#include <SDL/SDL.h>

#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/input_flow.h"
#include "app/options_flow.h"
#include "data/music.h"
#include "view/map_renderer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_config_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"

/* R10.1b: extracted verbatim from src/platform/sdl1/runtime_main.c. The three
 * chunk helpers remain file-static (used only inside this scene); the five
 * panel entrypoints are declared in scene_options_render.h for the loop. */

static int draw_options_chunk(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              size_t chunk_index,
                              int x,
                              int y,
                              int transparent_zero)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || assets == NULL || !game_asset_ready(assets, &assets->menu.has_options_ui)) {
        return 0;
    }
    if (rich4_spr_smp_get_chunk(assets->menu.options_data,
                                    assets->menu.options_size,
                                    &assets->menu.options_spr,
                                    chunk_index,
                                    &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    if (memcmp(assets->menu.options_spr.signature, "SPR", 4) == 0) {
        return draw_spr_chunk_at(screen,
                                 assets->menu.options_data,
                                 assets->menu.options_size,
                                 &assets->menu.options_spr,
                                 &chunk,
                                 x + chunk.x,
                                 y + chunk.y,
                                 transparent_zero);
    }
    return draw_chunk_topleft(screen,
                              assets->menu.options_data,
                              &chunk,
                              x,
                              y,
                              transparent_zero);
}

static int draw_options_chunk_crop(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   size_t chunk_index,
                                   int src_x,
                                   int src_y,
                                   int width,
                                   int height,
                                   int dst_x,
                                   int dst_y)
{
    Rich4SprSmpChunk chunk;

    if (screen == NULL || assets == NULL || !game_asset_ready(assets, &assets->menu.has_options_ui) ||
        memcmp(assets->menu.options_spr.signature, "SMP", 4) != 0) {
        return 0;
    }
    if (rich4_spr_smp_get_chunk(assets->menu.options_data,
                                    assets->menu.options_size,
                                    &assets->menu.options_spr,
                                    chunk_index,
                                    &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    return draw_chunk_crop_topleft(screen,
                                   assets->menu.options_data,
                                   &chunk,
                                   src_x,
                                   src_y,
                                   width,
                                   height,
                                   dst_x,
                                   dst_y,
                                   0);
}

static int options_panel_chunk_rect(const GamePreviewAssets *assets,
                                    int *out_left,
                                    int *out_top,
                                    int *out_width,
                                    int *out_height)
{
    Rich4SprSmpChunk chunk;

    if (assets == NULL || !game_asset_ready(assets, &assets->menu.has_options_ui) ||
        rich4_spr_smp_get_chunk(assets->menu.options_data,
                                    assets->menu.options_size,
                                    &assets->menu.options_spr,
                                    0,
                                    &chunk) != RICH4_SPR_SMP_OK) {
        return 0;
    }
    if (out_width != NULL) {
        *out_width = chunk.width;
    }
    if (out_height != NULL) {
        *out_height = chunk.height;
    }
    if (out_left != NULL) {
        *out_left = SCREEN_W / 2 - chunk.width / 2;
    }
    if (out_top != NULL) {
        *out_top = SCREEN_H / 2 - chunk.height / 2;
    }
    return 1;
}

void open_options_panel(Rich4OptionsPanelState *panel,
                        const GamePreviewAssets *assets,
                        const Rich4Config *config,
                        const Rich4SdlConfig *sdl_config,
                        Rich4SceneId return_app_state)
{
    if (panel == NULL || config == NULL || sdl_config == NULL) {
        return;
    }
    memset(panel, 0, sizeof(*panel));
    panel->active = 1;
    rich4_options_panel_set_hover(panel, -1);
    rich4_options_panel_set_pressed(panel, -1);
    panel->return_app_state = return_app_state;
    /* The original 72-byte RICH4.CFG written by 0x411F80 has no current-BGM
     * field. Track selection in this extended panel is preview-only. */
    panel->music_track = 1;
    panel->music_bank = sdl_config->music_bank;
    panel->saved_music_bank = sdl_config->music_bank;
    panel->saved = *config;
    panel->draft = *config;
    if (!options_panel_chunk_rect(assets,
                                  &panel->left,
                                  &panel->top,
                                  &panel->width,
                                  &panel->height)) {
        panel->width = 350;
        panel->height = 260;
        panel->left = (SCREEN_W - panel->width) / 2;
        panel->top = (SCREEN_H - panel->height) / 2;
    }
}

void cancel_options_panel(Rich4OptionsPanelState *panel,
                          Rich4Audio *audio,
                          const char *music_dir)
{
    if (panel == NULL) {
        return;
    }
    if (panel->return_app_state == RICH4_SCENE_MENU) {
        if (panel->saved.music != 0) {
            start_menu_music(audio, music_dir);
        } else {
            rich4_audio_stop_music(audio);
        }
    }
    rich4_options_panel_restore(panel);
    rich4_options_panel_cancel_action(panel);
    panel->active = 0;
}

static const char *options_key_name(int vk)
{
    size_t i;
    for (i = 0; i < sizeof(options_keys)/sizeof(options_keys[0]); ++i)
        if (options_keys[i].vk == vk) return options_keys[i].label;
    return "";
}

static void draw_options_child(SDL_Surface *screen, const GamePreviewAssets *assets,
                               const Rich4OptionsPanelState *p)
{
    int x = p->child_left, y = p->child_top, i;
    char text[64];
    if (p->child != 3 && p->child != 4) return;
    draw_options_chunk(screen, assets, p->child == 3 ? 2 : 1, x, y, 0);
    if (p->child == 3) {
        int first = rich4_options_calendar_first(p->child_draft.year, p->child_draft.month);
        int count = rich4_options_calendar_days(p->child_draft.year, p->child_draft.month);
        if (p->child_draft.month >= 1 && p->child_draft.month <= 12)
            draw_big5_text(screen, x+48, y+32, options_months[p->child_draft.month-1],
                15, RICH4_FONT_DARK, RICH4_FONT_DARK, 2, 2);
        snprintf(text, sizeof(text), "%u", p->child_draft.year);
        draw_big5_text(screen, x+133, y+30, text, 15, RICH4_FONT_DARK, RICH4_FONT_DARK, 2, 2);
        for (i = 1; i <= count; ++i) {
            int cell = first+i-1, cx = x+28+cell%7*23, cy = y+80+cell/7*18;
            if (i == p->child_draft.day)
                fill_rect(screen, cx-10, cy-6, 20, 16,
                    rich4_character_rgb565_color(5345644));
            snprintf(text, sizeof(text), "%d", i);
            draw_big5_text(screen, cx, cy, text, 15,
                i == p->child_draft.day ? 0xffff : RICH4_FONT_DARK,
                RICH4_FONT_DARK, 2, 2);
        }
        if (p->child_pressed >= 0 && p->child_pressed <= 3) {
            int c = p->child_pressed;
            draw_options_chunk(screen, assets, c%2 ? 13 : 12,
                x+(c<2 ? 74 : 160), y+(c%2 ? 31 : 21), 1);
        }
        for (i = 0; i < 3; ++i) {
            int pressed = p->child_pressed == i+4;
            if (pressed) draw_options_chunk(screen, assets, 14, x+9+i*63, y+180, 1);
            draw_big5_text(screen, x+(i==0 ? 38 : i==1 ? 101 : 163)+pressed,
                y+196+pressed, options_date_buttons[i], 15,
                RICH4_FONT_DARK, RICH4_FONT_DARK, 2, 2);
        }
    } else {
        for (i = 0; i < 28; ++i) {
            int col = i >= 15, row = col ? i-15 : i;
            int cx = x+(col ? 284 : 132), cy = y+33+row*16;
            const Rich4Key *key = &p->child_draft.hotkeys[i];
            draw_big5_text(screen, x+(col ? 208 : 62), cy, options_key_labels[i],
                12, 0xf79e, RICH4_FONT_DARK, 1, 2);
            if (p->editing_key == i && (rich4_sdl1_ticks()/250)%2)
                fill_rect(screen, cx-26, cy-7, 53, 13, 0xf79e);
            else {
                snprintf(text, sizeof(text), "%s%s", key->key ? options_key_name(key->mod) : "",
                         options_key_name(key->key));
                draw_big5_text(screen, cx, cy, text, 12,
                    rich4_character_rgb565_color(i<8 ? 61680 : 15790080),
                    RICH4_FONT_DARK, 1, 2);
            }
        }
        for (i = 0; i < 3; ++i)
            draw_big5_text(screen, x+52+i*113, y+296, options_key_buttons[i],
                15, RICH4_FONT_DARK, RICH4_FONT_DARK, 2, 2);
    }
}

void draw_options_panel(SDL_Surface *screen,
                        const GamePreviewAssets *assets,
                        const Rich4OptionsPanelState *panel)
{
    static const char *main_labels[] = {
        "\xb9\x43\xc0\xb8\xb3\x74\xab\xd7",
        "\xb0\xca\xb5\x65\xb9\x4c\xb5\x7b",
        "\xad\xb5\x20\xbc\xd6",
        "\xad\xb5\x20\xae\xc4",
        "\xa6\xdb\xb0\xca\xa6\x73\xc0\xc9",
        "\xbc\xd6\x20\x20\xa6\xb1",
        "\xb5\xf8\x20\x20\xb5\xa1",
        "\xa4\xe9\xa1\x42\xa4\xeb\xbe\xe4",
        "\xc1\x59\xa4\x70\xa6\x61\xb9\xcf",
        "\xb2\xd5\xa6\x58\xb5\x65\xad\xb1"
    };
    static const int label_pos[][3] = {
        { 14,  25, 5 },
        { 14,  58, 5 },
        { 14,  90, 5 },
        { 14, 122, 5 },
        { 14, 155, 5 },
        { 49, 202, 2 },
        {209, 202, 2 },
        {286, 226, 2 },
        {286, 258, 2 },
        {286, 290, 2 }
    };
    static const int view_marker_y[] = { 0xda, 0xfa, 0x119 };
    static const char *menu_action_labels[] = {
        "\xa4\xe9\xb4\xc1\xa7\xf3\xa7\xef",
        "\xbc\xf6\xc1\xe4\xb3\x5d\xa9\x77",
        "\xb9\x43\xc0\xb8\xbb\xa1\xa9\xfa"
    };
    static const char *game_action_labels[] = {
        "\xad\xab\xb7\x73\xb9\x43\xc0\xb8",
        "\xbb\x7b\xbf\xe9\xa7\xeb\xad\xb0",
        "\xb5\xb2\xa7\xf4\xb9\x43\xc0\xb8"
    };
    static const int action_pos[][2] = {
        { 276,  33 },
        { 276,  87 },
        { 276, 138 }
    };
    static const int action_chunk_pos[][2] = {
        { 0xe3, 0x0e },
        { 0xe3, 0x44 },
        { 0xe3, 0x77 }
    };
    static const char multiverse_label[] =
        "\xb6\x57\xae\xc9\xaa\xc5";
    int i;
    int x;
    int y;
    int music;
    int sound;
    int view;
    int context_variant;
    const char *const *action_labels;

    if (screen == NULL || panel == NULL || !panel->active) {
        return;
    }
    x = panel->left;
    y = panel->top;
    context_variant = rich4_options_panel_context_variant(panel);
    action_labels = context_variant ?
        game_action_labels : menu_action_labels;
    if (!draw_options_chunk(screen, assets, 0, x, y, 0)) {
        fill_rect(screen, x, y, panel->width, panel->height, 0xdedb);
        stroke_rect(screen, x, y, panel->width, panel->height, 0x0000);
        return;
    }

    /* 0x411D2B selects Data.mkf[3] chunk 10 for title-menu options and
     * chunk 11 for in-game options, at (168, 2).  This resource is paired
     * with the context-specific action labels below. */
    draw_options_chunk(screen, assets, 10u + (size_t)context_variant,
                       x + 0xa8, y + 2, 0);

    for (i = 0; i < (int)(sizeof(main_labels) / sizeof(main_labels[0])); i++) {
        draw_big5_text(screen,
                       x + label_pos[i][0],
                       y + label_pos[i][1],
                       main_labels[i],
                       15,
                       RICH4_FONT_DARK,
                       RICH4_FONT_DARK,
                       2,
                       label_pos[i][2]);
    }
    for (i = 0; i < 3; i++) {
        /* 0x411C73 selects 20 px before 0x411D32 draws both sets of
         * action labels; their anchors include the chunk's (168, 2). */
        draw_big5_text(screen,
                       x + action_pos[i][0],
                       y + action_pos[i][1],
                       action_labels[i],
                       20,
                       RICH4_FONT_DARK,
                       RICH4_FONT_DARK,
                       2,
                       2);
    }

    /* fcn_004105F4 paints Data.mkf[3] + 0x54, i.e. chunk 6, over the
     * selected action rectangle while the physical button is held. */
    if (panel->pressed_control >= 3 &&
        panel->pressed_control <= 5 &&
        !rich4_options_panel_has_pending_action(panel)) {
        i = panel->pressed_control - 3;
        draw_options_chunk(screen, assets, 6u,
                           x + action_chunk_pos[i][0],
                           y + action_chunk_pos[i][1], 1);
    }

    draw_big5_text(screen,
                   x + 123,
                   y + 202,
                   multiverse_label,
                   12,
                   RICH4_FONT_DARK,
                   RICH4_FONT_DARK,
                   2,
                   5);
    draw_options_chunk_crop(screen,
                            assets,
                            0,
                            0xda,
                            0xda,
                            0x0f,
                            0x0f,
                            x + 0x68,
                            y + 0xc3);

    for (i = 0; i <= clamp_int(panel->draft.game_speed, 0, 2); i++) {
        draw_options_chunk(screen, assets, 5, x + 0x51 + i * 0x10, y + 0x11, 1);
    }
    if (panel->draft.animation) {
        draw_options_chunk(screen, assets, 9, x + 0x62, y + 0x32, 1);
    }

    music = clamp_int(panel->draft.music, 0, 4);
    for (i = 0; i < music; i++) {
        draw_options_chunk(screen, assets, 5, x + 0x59 + i * 0x10, y + 0x51, 1);
    }
    if (music != 0) {
        draw_options_chunk(screen, assets, 9, x + 0x42, y + 0x52, 1);
    }

    sound = clamp_int(panel->draft.sound_effect, 0, 4);
    for (i = 0; i < sound; i++) {
        draw_options_chunk(screen, assets, 5, x + 0x59 + i * 0x10, y + 0x71, 1);
    }
    if (sound != 0) {
        draw_options_chunk(screen, assets, 9, x + 0x42, y + 0x72, 1);
    }
    if (panel->draft.auto_save) {
        draw_options_chunk(screen, assets, 9, x + 0x62, y + 0x92, 1);
    }
    if (panel->music_bank == RICH4_MUSIC_BANK_MULTIVERSE_JOURNEY) {
        draw_options_chunk(screen, assets, 9, x + 0x68, y + 0xc3, 1);
    }

    view = clamp_int(panel->draft.view, 0, 2);
    draw_options_chunk(screen, assets, 9, x + 0xda, y + view_marker_y[view], 1);

    for (i = 0; i < (int)rich4_music_bank_track_count(); i++) {
        int row_y = y + 0xe9 + i * 0x0f;
        const char *music_label =
            rich4_music_bank_track_title_big5(panel->music_bank, (size_t)i);
        uint16_t fg = 0xf79e;
        if (i + 1 == clamp_int(panel->music_track, 1, 8)) {
            fill_rect(screen, x + 0x12, row_y - 7, 0x9f, 0x0e, 0xf800);
            fg = 0xffff;
        }
        draw_big5_text(screen,
                       x + 0x1a,
                       row_y,
                       music_label,
                       12,
                       fg,
                       RICH4_FONT_DARK,
                       3,
                       5);
    }

    if (rich4_options_panel_has_pending_action(panel)) {
        Rich4SprSmpChunk buttons;
        int dialog_left;
        int dialog_top;
        size_t chunk_index = panel->confirm_hover == 1 ? 1u
            : panel->confirm_hover == 2 ? 2u : 0u;
        if (game_options_confirm_dialog_bounds(
                assets, &dialog_left, &dialog_top, NULL, NULL) &&
            game_asset_ready(assets, &assets->ui.has_confirm_dialog) &&
            rich4_spr_smp_get_chunk(
                assets->ui.confirm_dialog_data,
                assets->ui.confirm_dialog_size,
                &assets->ui.confirm_dialog_spr,
                chunk_index, &buttons) == RICH4_SPR_SMP_OK)
            draw_chunk_topleft(screen, assets->ui.confirm_dialog_data,
                               &buttons, dialog_left, dialog_top, 1);
    }
    /* Keep the action labels above every dynamic overlay. The original Big5
     * strings contain no ASCII space between their two DBCS characters. */
    draw_big5_text(screen, x + 224, y + 328, "\xa8\xfa\xae\xf8", 20,
                   RICH4_FONT_DARK, RICH4_FONT_DARK, 2, 2);
    draw_big5_text(screen, x + 296, y + 328, "\xbd\x54\xa9\x77", 20,
                   RICH4_FONT_DARK, RICH4_FONT_DARK, 2, 2);
    draw_options_child(screen, assets, panel);
}
