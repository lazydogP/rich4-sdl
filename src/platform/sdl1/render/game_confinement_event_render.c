#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdio.h>

#include <SDL/SDL.h>

#include "core/character_speech_rules.h"
#include "core/confinement.h"
#include "app/confinement_flow.h"
#include "app/special_place_notice_flow.h"
#include "core/game_event_payload.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "data/character_speech.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/render/game_confinement_event_render.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"

void draw_confinement_status_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameConfinementRenderView *view)
{
    static const char prison_format[] =
        "%s\xa7\xa4\xa8\x63\xa4\xa4\n\n"
        "\xc1\xd9\xb3\xd1%d\xa4\xd1\xa1\x49";
    static const char hospital_format[] =
        "%s\xa6\xed\xb0\x7c\xa4\xa4\n\n"
        "\xc1\xd9\xb3\xd1%d\xa4\xd1\xa1\x49";
    /* 0x4631E0 / 0x40C9D2: hotel countdown, with no random reaction voice. */
    static const char hotel_format[] =
        "%s\xa6\xed\xb1\x4a\xa4\xa4\n\n\xc1\xd9\xb3\xd1%d\xa4\xd1\xa1\x49";
    Rich4SprSmpChunk background;
    char message[128];
    int player;
    if (screen == NULL || assets == NULL || view == NULL ||
        !view->confinement_status_active ||
        view->confinement_status_until == 0 ||
        view->speech_active)
        return;
    player = view->confinement_status_player;
    if (player < 0 || player >= view->player_count)
        return;
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
        rich4_spr_smp_get_chunk(
            assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5,
            &background) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.common_dialog_data,
                      &background, 220, 140, 1);
    snprintf(
        message, sizeof(message),
        view->confinement_status_slot == 50 ? hotel_format : view->confinement_status_slot == 52
            ? prison_format : hospital_format,
        new_game_character_name(
            view->player_character[player]),
        view->confinement_status_days);
    draw_big5_text(
        screen, 220, 140, message, 16,
        rich4_character_rgb565_color(0x00f0f0f0u),
        rich4_character_rgb565_color(0x00101010u), 3, 4);
}

static const char *game_confinement_special_name(int slot)
{
    static const char *names[4] = {
        "\xa4\x70\xb0\xbd", "\xb1\x6a\xb5\x73",
        "\xac\x79\xaa\x5d", "\xb6\xa1\xbf\xd2"
    };
    int index = slot - GAME_MAX_PLAYERS;
    return index >= 0 && index < 4 ? names[index] : "";
}

void draw_special_place_notice(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot)
{
    const GameEvent *event;
    Rich4GameEventFields fields;
    Rich4SprSmpChunk background;
    char message[256];
    char name[64];
    const char *source;
    size_t length = 0;
    if (screen == NULL || assets == NULL || snapshot == NULL ||
        snapshot->presentation->special_place_notice.speech_pending) return;
    event = rich4_special_place_notice_current(snapshot->presentation);
    if (event == NULL) return;
    fields = rich4_game_event_special_place_notice_requested_fields(event);
    source = new_game_character_name(snapshot->state->players[event->player].character);
    if (fields.subject == GAME_SPECIAL_NOTICE_PRISON_RELEASE ||
        fields.subject == GAME_SPECIAL_NOTICE_HOSPITAL_RELEASE) {
        if (fields.target_ref.kind == GAME_DOMAIN_PLAYER && fields.target_ref.id >= 0 &&
            fields.target_ref.id < snapshot->state->player_count)
            source = new_game_character_name(
                snapshot->state->players[fields.target_ref.id].character);
        else if (fields.target_ref.kind == GAME_DOMAIN_SPECIAL_PLAYER)
            source = game_confinement_special_name(GAME_MAX_PLAYERS + fields.target_ref.id);
    }
    /* 0x452946 compacts the fixed-width name for release/owner messages. */
    while (source != NULL && *source != '\0' && length + 1 < sizeof(name)) {
        if (*source != ' ') name[length++] = *source;
        source++;
    }
    name[length] = '\0';
    switch (fields.subject) {
    case GAME_SPECIAL_NOTICE_PRISON_RELEASE:
    case GAME_SPECIAL_NOTICE_HOSPITAL_RELEASE:
        snprintf(message, sizeof(message), "\xab\x4f\xc4\xc0%s", name);
        break;
    case GAME_SPECIAL_NOTICE_SHOP_GIFT:
        source = fields.target_ref.kind == GAME_DOMAIN_CARD
            ? game_card_big5_name(fields.target_ref.id)
            : game_tool_big5_name(fields.target_ref.id);
        snprintf(message, sizeof(message),
            "\xc5\x77\xaa\xef\xb8\xb3\xa8\xc6\xaa\xf8\xa5\xfa\xc1\x7b\n\n"
            "\xb0\x65\xb1\x7a%s\xa1\x49", source);
        break;
    case GAME_SPECIAL_NOTICE_BANK_BORROW:
        snprintf(message, sizeof(message),
            "%s\n\n\xa6\x56\xbb\xc8\xa6\xe6\xb6\x55\xb4\xda\n\n%d\xa4\xb8",
            name, fields.value);
        break;
    case GAME_SPECIAL_NOTICE_BANK_REPAY:
        snprintf(message, sizeof(message),
            "%s\n\n\xc0\x76\xc1\xd9\xbb\xc8\xa6\xe6\xb6\x55\xb4\xda\n\n%d\xa4\xb8",
            name, fields.value);
        break;
    case GAME_SPECIAL_NOTICE_BANK_OWNER_SHORTFALL:
        snprintf(message, sizeof(message),
            "\xbb\xc8\xa6\xe6\xb8\xea\xaa\xf7\xb7\xc7\xb3\xc6\n\n"
            "\xa4\xa3\xa8\xac%d\xa4\xb8\n\n"
            "\xa5\xd1\xb8\x67\xc0\xe7\xaa\xcc%s\xb9\xd4\xa5\x49\xa1\x49",
            fields.value, name);
        break;
    default: return;
    }
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) && rich4_spr_smp_get_chunk(
            assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5, &background) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.common_dialog_data, &background, 220, 140, 1);
    draw_big5_text(screen, 220, 140, message, 16,
                  rich4_character_rgb565_color(0x00f0f0f0u),
                  rich4_character_rgb565_color(0x00101010u), 3, 4);
}

void draw_confinement_visit_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameConfinementRenderView *view)
{
    static const int prison_xy[8][2] = {
        {33,24},{185,24},{336,24},{487,24},
        {33,183},{185,183},{336,183},{487,183}
    };
    static const int hospital_xy[8][2] = {
        {297,1},{297,121},{297,241},{297,361},
        {481,1},{481,121},{481,241},{481,361}
    };
    static const char bail_points[] =
        "\xab\x4f\xc4\xc0\xc2\x49\xbc\xc6";
    const int (*xy)[2];
    int resource;
    int slot;
    int hover;
    if (screen == NULL || assets == NULL || view == NULL ||
        !view->confinement_visit_active)
        return;
    resource = view->confinement_visit_kind == GAME_CONFINEMENT_PRISON
        ? 63 : 65;
    xy = view->confinement_visit_kind == GAME_CONFINEMENT_PRISON
        ? prison_xy : hospital_xy;
    if (!draw_game_panel_chunk(screen, assets, resource, 0, 0, 0, 0))
        return;
    if (resource == 65) {
        if (view->confinement_visit_nurse_side)
            draw_confinement_panel_chunk(
                screen, assets, 65, 9, 91, 112);
        else
            draw_confinement_panel_chunk(
                screen, assets, 65, 4, 104, 110);
    }
    for (slot = 0; slot < 8; slot++) {
        int occupied = (view->visit_candidate_mask & (1u << slot)) != 0;
        if (occupied) {
            int chunk;
            if (slot < GAME_MAX_PLAYERS) {
                int character = view->player_character[slot];
                if (character < 0 || character >= 12) character = 0;
                chunk = resource == 63 ? character + 5 : character + 14;
            } else {
                chunk = resource == 63 ? slot + 13 : slot + 22;
            }
            draw_confinement_panel_chunk(
                screen, assets, resource, (size_t)chunk,
                xy[slot][0], xy[slot][1]);
            if (resource == 63)
                draw_confinement_panel_chunk(
                    screen, assets, 63, 3,
                    xy[slot][0], xy[slot][1]);
        } else if (resource == 63) {
            draw_confinement_panel_chunk(
                screen, assets, 63, 4,
                xy[slot][0], xy[slot][1]);
        }
    }
    draw_confinement_panel_chunk(
        screen, assets, resource, resource == 63 ? 21 : 30,
        resource == 63 ? 542 : 8, 432);
    {
        char points[24];
        int actor = view->route_execution_actor;
        unsigned int value = actor >= 0 && actor < view->player_count
            ? view->player_points[actor] : 0;
        snprintf(points, sizeof(points), "%u", value);
        draw_big5_text(screen, resource == 63 ? 622 : 88, 452,
                       points, 20,
                       rich4_character_rgb565_color(0x00ffffffu),
                       rich4_character_rgb565_color(0x00101010u), 3, 6);
    }
    if (resource == 65) {
        static const int left_gesture[3] = {5, 6, 5};
        static const int right_gesture[3] = {10, 11, 10};
        int frame = view->confinement_visit_nurse_gesture_frame;
        int side = view->confinement_visit_nurse_side != 0;
        if (frame >= 0 && frame < 3)
            draw_confinement_panel_chunk(
                screen, assets, 65,
                (size_t)(side ? right_gesture[frame] : left_gesture[frame]),
                side ? 165 : 139, 158);
        /* Panel[65]'s timer state selects one nurse sub-animation at a
         * time.  Do not paint a stale mouth/idle chunk over an active
         * three-frame gesture. */
        if (frame < 0 && view->confinement_visit_nurse_idle_chunk >= 0)
            draw_confinement_panel_chunk(
                screen, assets, 65,
                (size_t)view->confinement_visit_nurse_idle_chunk,
                side ? 165 : 139, side ? 183 : 180);
    }
    if ((view->confinement_visit_phase ==
                  RICH4_CONFINEMENT_VISIT_HOSPITAL_RESCUED ||
         view->confinement_visit_phase ==
                  RICH4_CONFINEMENT_VISIT_PRISON_RESCUED) &&
             view->confinement_visit_selected >= GAME_MAX_PLAYERS &&
             game_asset_ready(assets, &assets->ui.has_panel[64])) {
        Rich4SprSmpChunk release_actor;
        size_t chunk_index = (size_t)(
            view->confinement_visit_selected - GAME_MAX_PLAYERS);
        if (rich4_spr_smp_get_chunk(
                assets->ui.panel_data[64], assets->ui.panel_size[64],
                &assets->ui.panel_spr[64], chunk_index,
                &release_actor) == RICH4_SPR_SMP_OK) {
            int anchor_x = view->confinement_visit_kind ==
                    GAME_CONFINEMENT_PRISON ? 365 : 420;
            /* 0x43D1D0 uses (365,450) in prison; 0x43DCA2/0x43DCF8
             * use (420,450) in hospital, all minus Panel[64]'s hotspot. */
            draw_confinement_panel_chunk(
                screen, assets, 64, chunk_index, anchor_x, 450);
        }
    }
    if (view->confinement_visit_speech_active &&
        view->confinement_visit_speech != NULL) {
        Rich4PlayerSayText decoded;
        Rich4SprSmpChunk bubble;
        int special = view->confinement_visit_speech_special_actor;
        /* sub_44EC30's a5=-6 is used by the normal prison refusal too;
         * it is not an offset reserved for special-actor speech. */
        int text_y_offset = resource == 63 || special ? -6 : 0;
        /* sub_43CAAB/sub_43D1D0 pass Panel[63] + 24 to sub_44EC30.
         * Panel records are 0-based here, so +24 is the 185x81 speech
         * bubble at chunk 1; chunk 2 is the 100x95 hover detail card. */
        size_t bubble_index = resource == 63 ? 1u : (special ? 1u : 2u);
        int anchor_x = resource == 63 ? (special ? 210 : 230)
                                      : (special ? 200 : 8);
        int anchor_y = resource == 63 ? (special ? 150 : 300)
                                      : (special ? 200 : 8);
        rich4_player_say_decode(
            view->confinement_visit_speech->big5,
            view->confinement_visit_speech->speaking_resource, &decoded);
        if (rich4_spr_smp_get_chunk(
                assets->ui.panel_data[resource], assets->ui.panel_size[resource],
                &assets->ui.panel_spr[resource], bubble_index,
                &bubble) == RICH4_SPR_SMP_OK) {
            int left = anchor_x - bubble.x;
            int top = anchor_y - bubble.y;
            draw_confinement_panel_chunk(
                screen, assets, resource, bubble_index, anchor_x, anchor_y);
            if (decoded.display_text != NULL && decoded.display_text[0] != '\0')
                draw_big5_text(
                    screen, left + bubble.width / 2,
                    top + bubble.height / 2 + text_y_offset,
                    decoded.display_text, 20,
                    rich4_character_rgb565_color(0x00101010u),
                    0, 2, 4);
        }
    }

    hover = view->confinement_visit_phase ==
            RICH4_CONFINEMENT_VISIT_SELECT
        ? view->confinement_visit_hover : -1;
    if (hover >= 0 && hover < 8) {
        char cost_text[16];
        const char *name = hover < GAME_MAX_PLAYERS
            ? new_game_character_name(
                  view->player_character[hover])
            : game_confinement_special_name(hover);
        int left = resource == 63 ? xy[hover][0] + 20
                                  : xy[hover][0] - 80;
        int top = resource == 63 ? xy[hover][1] + 120
                                 : xy[hover][1];
        int text_x = left + (resource == 63 ? 57 : 41);
        draw_confinement_panel_chunk(
            screen, assets, resource, resource == 63 ? 2 : 3,
            left, top);
        draw_big5_text(screen, text_x, top + 26, name, 16,
                       rich4_character_rgb565_color(0x00101010u),
                       0, 0, 2);
        draw_big5_text(screen, text_x, top + 48, bail_points, 16,
                       rich4_character_rgb565_color(0x00101010u),
                       0, 0, 2);
        snprintf(cost_text, sizeof(cost_text), "%d",
                 rich4_confinement_visit_cost(
                     rich4_confinement_visit_target(hover)));
        draw_big5_text(screen, text_x, top + 70, cost_text, 16,
                       rich4_character_rgb565_color(0x00101010u),
                       0, 0, 2);
    }
    if (view->confinement_visit_phase ==
        RICH4_CONFINEMENT_VISIT_CONFIRM) {
        Rich4SprSmpChunk buttons;
        int left;
        int top;
        if (game_confirm_dialog_bounds(
                assets, GAME_COMMAND_CONFIRM,
                &left, &top, NULL, NULL) &&
            rich4_spr_smp_get_chunk(
                assets->ui.confirm_dialog_data, assets->ui.confirm_dialog_size,
                &assets->ui.confirm_dialog_spr,
                (size_t)(view->confinement_visit_confirm_hover ==
                              RICH4_CONFIRM_DIALOG_ACCEPT
                          ? 1
                          : view->confinement_visit_confirm_hover ==
                                RICH4_CONFIRM_DIALOG_DECLINE ? 2 : 0),
                &buttons) == RICH4_SPR_SMP_OK)
            draw_chunk_topleft(screen, assets->ui.confirm_dialog_data,
                               &buttons, left, top, 1);
    }
}
