#include "platform/sdl1/game_lazy_assets.h"
#include <string.h>
#include <stdio.h>

#include <SDL/SDL.h>

#include "app/ai_settings_flow.h"
#include "app/game_session.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"
#include "view/view_constants.h"

void game_draw_ai_settings_panel(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 const GameSession *session,
                                 const Rich4AiSettingsState *panel)
{
    static const int personality_y[3] = {195, 227, 259};
    /* sub_41E345 uses 0xF0F0F0 as the main colour and 0x101010 as
     * the one-pixel shadow colour for style 3.  These are their exact
     * RGB565 values on the SDL screen. */
    static const uint16_t label_main = 0xf79e;
    static const uint16_t label_shadow = 0x1082;
    Rich4SprSmpChunk chunk;
    int i;
    int row;
    if (screen == NULL || assets == NULL || panel == NULL || !panel->active ||
        !game_asset_ready(assets, &assets->menu.has_ai_settings_ui)) return;
    if (rich4_spr_smp_get_chunk(assets->menu.ai_settings_data,
                                assets->menu.ai_settings_size,
                                &assets->menu.ai_settings_spr, 0,
                                &chunk) != RICH4_SPR_SMP_OK) return;
    draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk, 102, 62, 0);
    /* sub_41E345 writes these labels into Panel.mkf[77] chunk 0;
     * add the sub_41DB91 screen origin (102,62). */
    draw_big5_text(screen, 275, 88, "\xb0\x55\xba\xde" "AI", 20,
                   label_main, label_shadow, 3, 2);
    draw_big5_text(screen, 275, 176, "\xad\xd3  \xa9\xca", 20,
                   label_main, label_shadow, 3, 2);
    draw_big5_text(screen, 351, 308,
                   "\xb8\xea\xaa\xf7\xb9\x42\xa5\xce\xa4\xf1\xa8\xd2", 20,
                   label_main, label_shadow, 3, 2);
    /* lpchText__80 @ 0x463CF3: 使用卡片; 卡 is A5 64, not A5 CF (甩). */
    draw_big5_text(screen, 346, 115, "\xa8\xcf\xa5\xce\xa5\x64\xa4\xf9", 16,
                   label_main, label_shadow, 3, 2);
    draw_big5_text(screen, 346, 146, "\xa8\xcf\xa5\xce\xb9\x44\xa8\xe3", 16,
                   label_main, label_shadow, 3, 2);
    draw_big5_text(screen, 346, 203, "\xa8\xc4\xc4\x5f\xc4\x5f", 16,
                   label_main, label_shadow, 3, 2);
    draw_big5_text(screen, 346, 235, "\xb4\xb6\xb3\x71\xa4\x48", 16,
                   label_main, label_shadow, 3, 2);
    draw_big5_text(screen, 346, 268, "\xa4\x6a\xa6\xd1\xa6\x6c", 16,
                   label_main, label_shadow, 3, 2);
    draw_big5_text(screen, 293, 339, "\xb2\x7b\xaa\xf7", 16,
                   label_main, label_shadow, 3, 6);
    draw_big5_text(screen, 407, 339, "\xa6\x73\xb4\xda", 16,
                   label_main, label_shadow, 3, 5);
    draw_big5_text(screen, 293, 372, "\xaa\xd1\xb2\xbc", 16,
                   label_main, label_shadow, 3, 6);
    draw_big5_text(screen, 407, 372, "\xb8\xea\xaa\xf7", 16,
                   label_main, label_shadow, 3, 5);
    for (i = 0; i < panel->player_count; i++) {
        int y = 70 + 83 * i;
        int player = panel->player_index[i];
        int character = session->state.players[player].character;
        if (rich4_spr_smp_get_chunk(assets->menu.ai_settings_data,
                                    assets->menu.ai_settings_size,
                                    &assets->menu.ai_settings_spr, 2,
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk,
                               110, y, 1);
        if (character >= 0 && character < 12 &&
            rich4_spr_smp_get_chunk(assets->menu.ai_settings_data,
                                    assets->menu.ai_settings_size,
                                    &assets->menu.ai_settings_spr,
                                    (size_t)(character + 6),
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->menu.ai_settings_data, &chunk,
                          182, y + 40, 1);
    }
    row = panel->selected_row;
    if (row < 0 || row >= panel->player_count) return;
    if (rich4_spr_smp_get_chunk(assets->menu.ai_settings_data,
                                assets->menu.ai_settings_size,
                                &assets->menu.ai_settings_spr, 1,
                                &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk,
                           110, 70 + 83 * row, 1);
    {
        int player = panel->player_index[row];
        int character = session->state.players[player].character;
        if (character >= 0 && character < 12 &&
            rich4_spr_smp_get_chunk(assets->menu.ai_settings_data,
                                    assets->menu.ai_settings_size,
                                    &assets->menu.ai_settings_spr,
                                    (size_t)(character + 6),
                                    &chunk) == RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->menu.ai_settings_data, &chunk,
                          182, 110 + 83 * row, 1);
    }
    if (rich4_spr_smp_get_chunk(assets->menu.ai_settings_data,
                                assets->menu.ai_settings_size,
                                &assets->menu.ai_settings_spr, 3,
                                &chunk) == RICH4_SPR_SMP_OK) {
        for (i = 0; i < panel->player_count; i++) {
            if (panel->hosted[i])
                draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk,
                                   121, 106 + 83 * i, 1);
        }
        if (panel->action_flags[row] & 1)
            draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk,
                               288, 107, 1);
        if (panel->action_flags[row] & 2)
            draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk,
                               288, 139, 1);
        draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk,
                           288, personality_y[panel->personality[row]], 1);
    }
    if (panel->hover >= 5 && panel->hover <= 8 &&
        rich4_spr_smp_get_chunk(assets->menu.ai_settings_data,
                                assets->menu.ai_settings_size,
                                &assets->menu.ai_settings_spr,
                                (size_t)(4 + ((panel->hover - 5) & 1)),
                                &chunk) == RICH4_SPR_SMP_OK) {
        static const int arrow_x[4] = {299, 393, 299, 393};
        static const int arrow_y[4] = {329, 329, 362, 362};
        int arrow = panel->hover - 5;
        draw_chunk_topleft(screen, assets->menu.ai_settings_data, &chunk,
                           arrow_x[arrow], arrow_y[arrow], 1);
    }
    SDL_LockSurface(screen);
    for (i = 0; i < panel->cash_ratio[row] / 10; i++)
        fill_rect(screen, 311 + i * 8, 328, 7, 22, 0xf800);
    for (i = 0; i < panel->stock_ratio[row] / 10; i++)
        fill_rect(screen, 311 + i * 8, 361, 7, 22, 0xf800);
    SDL_UnlockSurface(screen);
    /* lpchText__89 at 0x463D2E is BD 54 A9 77 00.  Do not insert an
     * ASCII space: mode 3 advances once for every SBCS/DBCS character,
     * so a space would add a bogus 22-pixel vertical row. */
    draw_big5_text(screen, 499, 187, "\xbd\x54\xa9\x77", 20,
                   label_shadow, 0, 2, 3);
    /* lpchText__90 at 0x463D33 is A8 FA AE F8 00. */
    draw_big5_text(screen, 499, 275, "\xa8\xfa\xae\xf8", 20,
                   label_shadow, 0, 2, 3);
}
