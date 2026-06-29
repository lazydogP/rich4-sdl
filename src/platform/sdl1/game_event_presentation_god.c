#include "platform/sdl1/game_lazy_assets.h"
#include <SDL/SDL.h>

#include <stdio.h>

#include "core/game_constants.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"

/* These are the strings passed to 0x40E2A2 by 0x40EAD7.  The game stores its
 * text in Big5; keep the bytes here so the SDL1 renderer follows the original
 * text pipeline. */
static const char *god_effect_message(int god_type)
{
    switch (god_type) {
    case 1:
        return "\xcb\xe7\xb3\xdc\xa1\x49\xa1\xe3\xb5\x6f\xb0\x5d\xb5\x6f\xb0\x5d\xa1\x49\n\n"
               "\xa7\xda\xc1\xf6\xb5\x4d\xc1\xd9\xa4\x70\xa1\x41\xa8\x53\xa4\xb0\xbb\xf2\xaf\xab\xa4\x4f\n\n"
               "\xa6\xfd\xac\x4f\xa7\xda\xa5\x69\xa5\x48\xc5\xfd\xa7\x41\xb7\x6c\xa5\xa2\xb4\xee\xa5\x62\xa1\x43";
    case 2:
        return "\xa8\xfe\xa8\xfe\xa1\xe3\xae\xa5\xb3\xdf\xb5\x6f\xb0\x5d\xa1\x43\n\n"
               "\xa7\xda\xab\x4f\xa6\xf6\xa7\x41\xb0\x5d\xb7\xbd\xba\x75\xba\x75\n\n"
               "\xbd\xd6\xa4\x5d\xa4\xa3\xaf\xe0\xbb\x40\xa7\x41\xaa\xba\xbf\xfa\xa1\x43";
    case 3:
        return "\xa2\xe7\xa2\xed\xa2\xe9\xa2\xf0\x21\x21\xa4\xa3\xad\x6e\xac\xdd\xa7\xda\xa4\x70\xb3\xe1\n\n"
               "\xa7\xda\xa5\x69\xa5\x48\xc5\xfd\xa7\x41\xa7\xeb\xb8\xea\xa8\xc6\xa5\x62\xa5\x5c\xad\xbf\xa1\x49";
    case 4:
        return "\xa8\xfe\xa8\xfe\xa8\xfe\xa1\xe3\xa4\xd1\xa9\x78\xbd\xe7\xba\xd6\xa6\x6e\xb9\x42\xa8\xec\n\n"
               "\xa7\xda\xab\x4f\xa6\xf6\xa7\x41\xba\xd6\xac\x50\xb0\xaa\xb7\xd3\n\n"
               "\xa7\xeb\xb8\xea\xa5\x5b\xad\xbf\xb6\xb6\xa7\x51\xa1\x42\xb6\x52\xa6\x61\xa4\xa3\xa5\xce\xbf\xfa\xa1\x43";
    case 5:
        return "\xbc\x4f\xbc\x4f\xbc\x4f\xa1\xe3\xbd\xdf\xbf\xfa\xa1\x49\xbd\xdf\xbf\xfa\xa1\x49";
    case 6:
        return "\xb3\xe1\xa1\xe3\xbf\xfa\xb1\xbc\xa4\x46\xa1\x43";
    case 7:
        return "\xa6\xb3\xa7\xda\xa4\x70\xa4\x70\xb0\x49\xaf\xab\xa1\x41\xae\x60\xa7\x41\xa4\x40\xa8\xc6\xb5\x4c\xa6\xa8\xa1\x43";
    case 8:
        return "\xab\xa2\xab\xa2\xab\xa2\xa1\x49\xaf\x75\xac\x4f\xa6\x6e\xb0\x49\xb0\xda\xa1\x49";
    case 9:
        return "\xab\xa2\xb9\x70\xb8\xf4\xa8\xc8\xa1\xe3\n\n\xa7\xda\xb1\x4e\xa8\xcf\xb1\x7a\xb8\xa8\xb8\x7d\xaa\xba\xa4\x67\xa6\x61\xa7\xf3\xa5\x5b\xc1\x63\xba\x61\xa1\x43";
    case 10:
        return "\xab\xa2\xa1\xe3\xab\xa2\xa1\xe3\xbb\x50\xa7\xda\xa6\x50\xa5\xcd\xa6\x40\xa6\xba\xa1\x43\n\n\xa7\xda\xad\x6e\xaa\x75\xb8\xf4\xaf\x7d\xc3\x61\xa7\x41\xa9\xd2\xb8\x67\xb9\x4c\xaa\xba\xa4\x67\xa6\x61\xa1\x43";
    case 12:
        return "\xa8\xfe\xa1\xe3\xa8\xfe\xa1\x41\xa6\xb3\xa4\x67\xb4\xb5\xa6\xb3\xb0\x5d\n\n"
               "\xa7\xda\xb4\x4e\xac\x4f\xa4\x67\xa6\x61\xa4\xbd\xa4\xbd\xa1\x43\n\n"
               "\xa6\xb3\xa7\xda\xa6\x62\xa1\x41\xa7\x41\xb7\x51\xad\x6e\xa6\x68\xa4\xd6\xa4\x67\xa6\x61\xb4\x4e\xa6\xb3\xa6\x68\xa4\xd6\xa1\x43";
    case 15:
        return "\xbc\x4b\xbc\x4b\xbc\x4b\xbc\x4b\xa1\x4f\xa1\x4f\xa7\x41\xba\x47\xa4\x46\xa1\x49\n\n"
               "\xb3\x51\xa7\xda\xa8\x6e\xa4\x57\xa1\x41\xa7\x41\xa7\xb9\xb3\x4a\xa4\x46\xa1\x43";
    default:
        return NULL;
    }
}

/* 0x4632FD, 0x463353, 0x4633AB and 0x4633D5.  These are separate
 * 0x440CAC follow-ups; they must not be appended to the 0x40E2A2 message. */
static const char *god_result_message(
    const GameGodAcquisitionPresentationState *god,
    char *out,
    size_t out_size)
{
    static const char type3_subject[] =
        "\xa4\x70\xba\xd6\xaf\xab";
    static const char type3_format[] =
        "%s\xaa\xfe\xa8\xad\n\n\xb1\x6f\xa8\xec%s\xa1\x49";
    static const char type4_format[] =
        "\xa4\x6a\xba\xd6\xaf\xab\xaa\xfe\xa8\xad\n\n"
        "\xb1\x6f\xa8\xec%s\xa4\xce%s\xa1\x49";
    static const char type7_format[] =
        "\xa4\x70\xb0\x49\xaf\xab\xaa\xfe\xa8\xad\n\n"
        "\xbf\xf2\xa5\xa2%s\xa1\x49";
    static const char type8_message[] =
        "\xa4\x6a\xb0\x49\xaf\xab\xaa\xfe\xa8\xad\n\n"
        "\xbf\xf2\xa5\xa2\xa4\x40\xa5\x62\xa5\x64\xa4\xf9\xa1\x49";

    if (god == NULL || out == NULL || out_size == 0) return NULL;
    switch (god->god_type) {
    case 3:
        if (god->result_card_1 < 0) return NULL;
        (void)snprintf(out, out_size, type3_format, type3_subject,
                       game_card_big5_name(god->result_card_1));
        return out;
    case 4:
        if (god->result_card_1 < 0 || god->result_card_2 < 0) return NULL;
        (void)snprintf(out, out_size, type4_format,
                       game_card_big5_name(god->result_card_1),
                       game_card_big5_name(god->result_card_2));
        return out;
    case 7:
        if (god->result_card_1 < 0) return NULL;
        (void)snprintf(out, out_size, type7_format,
                       game_card_big5_name(god->result_card_1));
        return out;
    case 8:
        if (god->result_lost_count <= 0) return NULL;
        (void)snprintf(out, out_size, "%s", type8_message);
        return out;
    default:
        return NULL;
    }
}

void draw_god_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot)
{
    const GameGodAcquisitionPresentationState *god;
    Rich4SprSmpChunk background;
    const char *message;
    char result_message[320];

    if (screen == NULL || assets == NULL || snapshot == NULL ||
        snapshot->presentation == NULL)
        return;
    god = &snapshot->presentation->god_acquisition;
    if (!god->active || god->god_type <= 0 ||
        god->god_type > GAME_GOD_TYPE_COUNT || god->waiting_for_detach ||
        god->arrival_speech_phase)
        return;
    if (god->money.active) {
        if (god->money.phase >= 10) return;
        draw_god_money_roulette(screen, assets, snapshot);
        return;
    }

    /* 0x40EC14..0x40F2EB draws Data.MKF 540..551 in the game viewport
     * before calling 0x40E2A2.  Missing animation assets do not suppress the
     * text, which is useful for reduced/test asset packs. */
    /* 0x40EDDF/0x40EEA0/0x40F104/0x40F1D6 redraw the map before
     * the card result popup. Do not repaint the final FLC over that map. */
    if (god->message_visible != RICH4_GOD_MESSAGE_RESULT &&
        game_asset_ready(assets, &assets->ui.has_god_effect[god->god_type]))
        draw_game_graph_frame_data(
            screen, assets->ui.god_effect_data[god->god_type],
            assets->ui.god_effect_size[god->god_type], god->frame, 0, 40);

    if (god->message_visible == RICH4_GOD_MESSAGE_RESULT) {
        message = god_result_message(god, result_message,
                                     sizeof(result_message));
        if (message == NULL) return;
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(
                assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, &background) ==
                    RICH4_SPR_SMP_OK)
            draw_chunk_at(screen, assets->ui.common_dialog_data, &background,
                          220, 140, 1);
        draw_big5_text_with_spacing(
            screen, 220, 140, message, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 1, 4);
        return;
    }
    if (god->message_visible != RICH4_GOD_MESSAGE_MAIN) return;
    message = god_effect_message(god->god_type);
    if (message == NULL) return;
    draw_big5_text_with_spacing(
        screen, 220, 460, message, 28,
        rich4_character_rgb565_color(0x00ffffffu),
        rich4_character_rgb565_color(0x00101010u), 6, 0, 7);
}
