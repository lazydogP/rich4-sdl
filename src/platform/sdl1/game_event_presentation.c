#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#include "core/card.h"
#include "core/confinement.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/special_actor_action.h"
#include "core/special_event.h"
#include "core/tool.h"
#include "app/game_presentation_feature_states.h"
#include "data/character_speech.h"
#include "data/special_event_text.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/game_monthly_finance.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "resource/byte_reader.h"
#include "resource/flc_animation.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"
/*
 * sub_441F73 is shared by card rewards and active-card use.  Data.MKF
 * [card_id + 570] is assigned directly to the 165x256 graph template at
 * 0x441204; it has no width/height prefix.
 */
void draw_original_card_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    int card, const char *title_big5)
{
    Rich4SprSmpChunk background;
    Rich4SprSmpChunk art;
    const size_t art_size =
        (size_t)ORIGINAL_CARD_ART_WIDTH *
        (size_t)ORIGINAL_CARD_ART_HEIGHT * 2u;
    if (screen == NULL || assets == NULL ||
        card < 0 || card >= GAME_CARD_COUNT ||
        !game_asset_ready(assets, &assets->ui.has_card_effect[card]) ||
        assets->ui.card_effect_data[card] == NULL ||
        assets->ui.card_effect_size[card] < art_size)
        return;
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
        rich4_spr_smp_get_chunk(
            assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5,
            &background) == RICH4_SPR_SMP_OK)
        draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 129);
    if (title_big5 != NULL)
        draw_big5_text(
            screen, 220, 129, title_big5, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
    memset(&art, 0, sizeof(art));
    art.width = ORIGINAL_CARD_ART_WIDTH;
    art.height = ORIGINAL_CARD_ART_HEIGHT;
    art.data_offset = 0;
    art.data_size = (uint32_t)art_size;
    /* 0x44205C calls the opaque blitter 0x4563F5: zero is black here. */
    draw_chunk_topleft(
        screen, assets->ui.card_effect_data[card], &art, 138, 200, 0);
}
void draw_special_reward_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    int action;
    int reward_index;
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->special_reward.active)
        return;
    action = session->presentation->special_reward.action;
    reward_index = action == 13 ? 0 : 1;
    if (action != 14 && action != 15 && session->presentation->special_reward.phase <= 1 &&
        game_asset_ready(assets, &assets->ui.has_special_reward[reward_index])) {
        draw_game_graph_frame_data(
            screen, assets->ui.special_reward_data[reward_index],
            assets->ui.special_reward_size[reward_index],
            session->presentation->special_reward.frame,
            action == 13 ? 208 : 204, 180);
    }
    if (session->presentation->special_reward.phase == 1) {
        Rich4SprSmpChunk background;
        char gift_message[96];
        const char *message = action == 14
            ? "\xb1\x6f\xa8\xec\xa2\xb4\xa2\xaf\xa2\xaf\xc2\x49\xa8\xe9\xa1\x49"
            : action == 10
            ? "\xb1\x6f\xc2\x49\xa8\xe9\xa2\xb4\xa2\xaf\xc2\x49"
            : action == 11
                ? "\xb1\x6f\xc2\x49\xa8\xe9\xa2\xb2\xa2\xaf\xc2\x49"
                : "\xb1\x6f\xc2\x49\xa8\xe9\xa2\xb0\xa2\xaf\xc2\x49";
        if (action == 15) {
            snprintf(gift_message, sizeof(gift_message),
                     "\xb1\x6f\xa8\xec%s\xa1\x49",
                     game_tool_big5_name(session->presentation->special_reward.card));
            message = gift_message;
        }
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
            rich4_spr_smp_get_chunk(
                assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5,
                &background) == RICH4_SPR_SMP_OK)
            draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
        /* ref_463A81/ref_463A8E/ref_463A9B are passed to sub_440CAC
         * with a 1000 ms hold. */
        draw_big5_text(screen, 220, 140, message, 16,
                       rich4_character_rgb565_color(0x00f0f0f0u),
                       rich4_character_rgb565_color(0x00101010u), 3, 4);
    } else if (session->presentation->special_reward.phase == 2) {
        int card = session->presentation->special_reward.card;
        if (card >= 0 && card < GAME_CARD_COUNT &&
            game_asset_ready(assets, &assets->ui.has_card_effect[card])) {
            char title[96];
            /* ref_463AA8 is Big5 "得到%s！". */
            snprintf(title, sizeof(title), "\xb1\x6f\xa8\xec%s\xa1\x49",
                     game_card_big5_name(card));
            draw_original_card_presentation(
                screen, assets, card, title);
        }
    }
}
void draw_bank_rejection_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    static const char format[] =
        "\xbb\xc8\xa6\xe6\xa9\xda\xb5\xb4\xa9\xb9\xa8\xd3\n\n"
        "\xc1\xd9\xb3\xd1%d\xa4\xd1\xa1\x49";
    Rich4SprSmpChunk background;
    char message[64];
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->bank_rejection.active)
        return;
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
        rich4_spr_smp_get_chunk(
            assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5,
            &background) == RICH4_SPR_SMP_OK)
        draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
    snprintf(message, sizeof(message), format,
             session->presentation->bank_rejection.days);
    draw_big5_text(screen, 220, 140, message, 16,
                   rich4_character_rgb565_color(0x00f0f0f0u),
                   rich4_character_rgb565_color(0x00101010u), 3, 4);
}

void draw_bankruptcy_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    Rich4FlcHeader header;
    int frame;
    int x;
    int y;
    if (screen == NULL || assets == NULL || session == NULL ||
        session->presentation == NULL ||
        !session->presentation->bankruptcy.active ||
        session->presentation->bankruptcy.phase ==
            RICH4_BANKRUPTCY_PHASE_SPEECH ||
        !game_asset_ready(assets, &assets->ui.has_bankruptcy_animation) ||
        rich4_flc_parse_header(
            assets->ui.bankruptcy_data, assets->ui.bankruptcy_size,
            &header) != RICH4_FLC_OK || header.frame_count == 0)
        return;
    frame = session->presentation->bankruptcy.frame;
    if (frame < 0) frame = 0;
    if (frame >= header.frame_count) frame = header.frame_count - 1;
    x = ((int)SCREEN_W - (int)header.width) / 2;
    y = 40 + ((440 - (int)header.height) / 2);
    draw_game_graph_frame_data(
        screen, assets->ui.bankruptcy_data, assets->ui.bankruptcy_size,
        frame, x, y);
}

void draw_bank_settlement_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    static const char balance_label[] =
        "\xa6\x73\xb4\xda\xa1\x47";
    static const char interest_label[] =
        "\xa7\x51\xae\xa7\xa1\x47";
    static const char loan_pending[] =
        "\xb6\x55\xb4\xda\xa4\xa4";
    Rich4SprSmpChunk chunk;
    uint16_t red = rich4_character_rgb565_color(0x00ff0000u);
    unsigned int player_mask;
    int active_count = 0;
    int ordinal = 0;
    int player;
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->bank_settlement.active ||
        !game_asset_ready(assets, &assets->ui.has_panel[25]))
        return;
    if (draw_monthly_award(screen, assets, session)) return;
    if (rich4_spr_smp_get_chunk(
            assets->ui.panel_data[25], assets->ui.panel_size[25],
            &assets->ui.panel_spr[25], 0, &chunk) == RICH4_SPR_SMP_OK)
          draw_finance_panel_chunk_at(screen,assets,25,0,0,0,0);
    /* sub_439BFA/sub_437E61 reveal the left-side banker in two 100 ms
     * stages: chunk 19 at (24,70), then chunk 24 at (28,70). */
    if (rich4_spr_smp_get_chunk(
            assets->ui.panel_data[25], assets->ui.panel_size[25],
            &assets->ui.panel_spr[25],
            session->presentation->bank_settlement.frame == 0 ? 19u : 24u,
            &chunk) == RICH4_SPR_SMP_OK)
          draw_finance_panel_chunk_at(screen,assets,25,
                        session->presentation->bank_settlement.frame==0?19u:24u,
                        session->presentation->bank_settlement.frame==0?24:28,70,
                        session->presentation->bank_settlement.frame==0);
    player_mask = session->presentation->bank_settlement.player_mask;
    for (player = 0; player < GAME_MAX_PLAYERS; player++)
        if ((player_mask & (1u << player)) != 0) active_count++;
    for (player = 0; player < GAME_MAX_PLAYERS; player++) {
        unsigned int bit = 1u << player;
        int y;
        int character;
        int before;
        char amount[32];
        size_t row_chunk;
        size_t actor_chunk;
        if ((session->presentation->bank_settlement.player_mask & bit) == 0)
            continue;
        if (active_count == 2)
            y = ordinal == 0 ? 120 : 360;
        else if (active_count == 3)
            y = 100 + ordinal * 140;
        else if (active_count >= 4)
            y = 60 + ordinal * 120;
        else
            y = 240;
        row_chunk = (size_t)(11 + ordinal);
        if (session->presentation->bank_settlement.frame >= 2 &&
            row_chunk < assets->ui.panel_spr[25].num_chunks &&
            rich4_spr_smp_get_chunk(
                assets->ui.panel_data[25], assets->ui.panel_size[25],
                &assets->ui.panel_spr[25], row_chunk,
                &chunk) == RICH4_SPR_SMP_OK)
              draw_finance_panel_chunk_at(screen,assets,25,row_chunk,360,y-36,1);
        character = clamp_int(
            session->state->players[player].character, 0, 11);
        actor_chunk = (size_t)(47 + 3 * character +
            (session->presentation->bank_settlement.frame >= 2 ? 2 : 0));
        if (actor_chunk < assets->ui.panel_spr[25].num_chunks &&
            rich4_spr_smp_get_chunk(
                assets->ui.panel_data[25], assets->ui.panel_size[25],
                &assets->ui.panel_spr[25], actor_chunk,
                &chunk) == RICH4_SPR_SMP_OK)
              draw_finance_panel_chunk_at(screen,assets,25,actor_chunk,600,y,1);
        if (session->presentation->bank_settlement.frame < 2) {
            ordinal++;
            continue;
        }
        before = session->presentation->bank_settlement.balance[player] -
                 session->presentation->bank_settlement.interest[player];
          draw_big5_text_with_spacing(screen, 364, y - 30, balance_label, 18,
                         rich4_character_rgb565_color(0x101010u), 0x0000, 0, 0, 0);
        format_hud_number(amount, sizeof(amount), before, 1);
          draw_big5_text_with_spacing(screen, 514, y - 30, amount, 18,
                         rich4_character_rgb565_color(0x101010u), 0x0000, 0, 0, 1);
          draw_big5_text_with_spacing(screen, 364, y + 10, interest_label, 18,
                         rich4_character_rgb565_color(0x101010u), 0x0000, 0, 0, 0);
        if ((session->presentation->bank_settlement.loan_mask & bit) != 0) {
              draw_big5_text_with_spacing(screen, 514, y + 10, loan_pending, 18,
                             red, 0x0000, 0, 0, 1);
        } else {
            format_hud_number(amount, sizeof(amount), (int)((double)before * 0.1), 1);
              draw_big5_text_with_spacing(screen, 514, y + 10, amount, 18,
                             rich4_character_rgb565_color(0x101010u), 0x0000, 0, 0, 1);
        }
        ordinal++;
    }
    draw_monthly_bank_speech(screen, assets, session);
}
void draw_bank_loan_due_presentation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    static const char reminder_format[] =
        "\xb6\x55\xb4\xda\xc1\xd9\xa6\xb3%d"
        "\xa4\xd1\xa8\xec\xb4\xc1";
    /* 0x436A5A: days 0/1/2 use 0x464B2C/0x464B43/0x464B5C.
     * The existing SDL three-day reminder is not an original string. */
    static const char *const original_notices[] = {
        "\xb6\x55\xb4\xda\xa8\xec\xb4\xc1\xa4\xe9\n\n"
        "\xb1\x6a\xa8\xee\xb0\xf5\xa6\xe6\xa1\x49",
        "\xb6\x5a\xb6\x55\xb4\xda\xa8\xec\xb4\xc1\xa4\xe9\n\n"
        "\xc1\xd9\xb3\xd1\xa2\xb0\xa4\xd1\xa1\x49",
        "\xb6\x5a\xb6\x55\xb4\xda\xa8\xec\xb4\xc1\xa4\xe9\n\n"
        "\xc1\xd9\xb3\xd1\xa2\xb1\xa4\xd1\xa1\x49"
    };
    Rich4SprSmpChunk background;
    char message[96];
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->bank_loan_due.active ||
        session->presentation->bank_settlement.active)
        return;
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
        rich4_spr_smp_get_chunk(
            assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5,
            &background) == RICH4_SPR_SMP_OK)
        draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
    if (session->presentation->bank_loan_due.forced)
        snprintf(message, sizeof(message), "%s", original_notices[0]);
    else if (session->presentation->bank_loan_due.days == 1 ||
             session->presentation->bank_loan_due.days == 2)
        snprintf(message, sizeof(message), "%s", original_notices[
                 session->presentation->bank_loan_due.days]);
    else
        snprintf(message, sizeof(message), reminder_format,
                 session->presentation->bank_loan_due.days);
    draw_big5_text(screen, 220, 140, message, 16,
                   rich4_character_rgb565_color(0x00f0f0f0u),
                   rich4_character_rgb565_color(0x00101010u), 3, 4);
}
void draw_insurance_notice(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    static const char fee_format[] =
        "%s\n\n\xb8\xb3\xa8\xc6\xaa\xf8%s\n\n"
        "\xbd\xd0\xa5\x49%d\xa4\xb8\xab\x4f\xc0\x49\xb6\x4f";
    static const char claim_format[] =
        "\xab\x4f\xc0\x49\xb4\xc1\xb6\xa1\n\n"
        "\xb1\x6f\xa8\xec\xb2\x7a\xbd\xdf\xaa\xf7\n\n%d\xa4\xb8";
    const GameInsuranceNoticeEntry *notice;
    Rich4SprSmpChunk background;
    const char *company_name = "";
    const char *chairman_name = "";
    char message[192];
    int stock = -1;
    if (screen == NULL || assets == NULL || session == NULL ||
        session->presentation->insurance.count <= 0)
        return;
    notice = &session->presentation->insurance.queue[
        session->presentation->insurance.head];
    if (notice->company >= 0 &&
        notice->company < session->state->company_count) {
        stock = session->state->company_stock_index[notice->company];
        company_name = session->presentation->map_company_names[
            notice->company];
        if (company_name[0] == '\0')
            company_name = game_stock_name_for_state(session->state, stock);
    }
    if (notice->chairman >= 0 &&
        notice->chairman < session->state->player_count)
        chairman_name = new_game_character_name(
            session->state->players[notice->chairman].character);
    if (game_asset_ready(assets, &assets->ui.has_worker_choice_ui) &&
        rich4_spr_smp_get_chunk(
            assets->ui.worker_choice_data, assets->ui.worker_choice_size,
            &assets->ui.worker_choice_spr, 5,
            &background) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.worker_choice_data,
                      &background, 220, 140, 1);
    if (notice->kind == GAME_INSURANCE_NOTICE_FEE)
        snprintf(message, sizeof(message), fee_format,
                 company_name, chairman_name, notice->amount);
    else
        snprintf(message, sizeof(message), claim_format, notice->amount);
    draw_big5_text(screen, 220, 140, message, 16,
                   rich4_character_rgb565_color(0x00f0f0f0u),
                   rich4_character_rgb565_color(0x00101010u), 3, 4);
}
