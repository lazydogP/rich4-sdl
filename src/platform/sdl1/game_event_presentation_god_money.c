#include "platform/sdl1/game_lazy_assets.h"
#include <stdio.h>

#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "view/map_renderer.h"

/* 0x440706 loads Panel.mkf[67] through 0x48A05C. 0x475CE0/0x475CE8
 * supply the two lever positions and the three/four-digit positions. */
void draw_god_money_roulette(SDL_Surface *screen,
    const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot)
{
    static const int digit_x[2][4] = {
        {145, 182, 219, 256}, {0, 163, 200, 237}
    };
    static const char *const name[4] = {
        "\xa4\x70\xb0\x5d\xaf\xab", "\xa4\x6a\xb0\x5d\xaf\xab",
        "\xa4\x70\xbd\x61\xaf\xab", "\xa4\x6a\xbd\x61\xaf\xab"
    };
    /* 0x4652A9/0x4652C1/0x4652D1/0x4652E7. */
    static const char *const format[4] = {
        "%s\xaa\xfe\xa8\xad\n\n\xa6\x56\xa9\xd2\xa6\xb3\xb9\xef\xa4\xe2\xa6\xac...",
        "%s\xaa\xfe\xa8\xad\n\n\xb0\x65\xb1\x7a...",
        "%s\xaa\xfe\xa8\xad\n\n\xa5\x49\xb5\xb9\xa8\x43\xad\xd3\xa4\x48...",
        "%s\xaa\xfe\xa8\xad\n\n\xb7\x6c\xa5\xa2..."
    };
    const GameGodAcquisitionPresentationState *god;
    const GameGodMoneyPresentationState *wheel;
    Rich4SprSmpChunk background;
    char message[128];
    int index;
    int digit;
    if (screen == NULL || assets == NULL || snapshot == NULL ||
        snapshot->presentation == NULL) return;
    god = &snapshot->presentation->god_acquisition;
    wheel = &god->money;
    if (!god->active || !wheel->active || wheel->mode < 0 || wheel->mode > 1)
        return;
    index = god->god_type == 1 ? 0 : god->god_type == 2 ? 1 :
            god->god_type == 5 ? 2 : god->god_type == 6 ? 3 : -1;
    if (index < 0) return;
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) && rich4_spr_smp_get_chunk(
            assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5, &background) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.common_dialog_data, &background,
                      220, 140, 1);
    draw_game_panel_chunk_at(screen, assets, 67, (size_t)wheel->mode,
                             220, 320, 0);
    draw_game_panel_chunk_at(screen, assets, 67,
                             wheel->phase == 3 ? 3u : 2u,
                             wheel->mode == 0 ? 317 : 298, 240, 1);
    for (digit = wheel->mode; digit < 4; digit++)
        draw_game_panel_chunk_at(screen, assets, 67,
                                 (size_t)(4 + wheel->digits[digit]),
                                 digit_x[wheel->mode][digit], 320, 0);
    if (wheel->phase >= 9)
        snprintf(message, sizeof(message), "%d\xa4\xb8", wheel->amount);
    else
        snprintf(message, sizeof(message), format[index], name[index]);
    draw_big5_text_with_spacing(
        screen, 220, 140, message, 16,
        rich4_character_rgb565_color(0x00f0f0f0u),
        rich4_character_rgb565_color(0x00101010u), 3, 1, 4);
}
