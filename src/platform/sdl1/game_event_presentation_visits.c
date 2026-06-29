#include "platform/sdl1/game_lazy_assets.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#include "app/magic_house_flow.h"
#include "core/card.h"
#include "core/character_speech_rules.h"
#include "core/confinement.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/special_actor_action.h"
#include "core/special_event.h"
#include "core/tool.h"
#include "data/character_speech.h"
#include "data/special_event_text.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/game_event_presentation.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/game_bank_render.h"
#include "platform/sdl1/render/game_route_render.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "resource/byte_reader.h"
#include "resource/spr_smp.h"
#include "view/map_renderer.h"
#include "view/view_constants.h"
static void darken_lottery_number_rect(
    SDL_Surface *screen, int x, int y, int width, int height)
{
    int px;
    int py;
    if (screen == NULL) return;
    SDL_LockSurface(screen);
    for (py = y; py < y + height && py < SCREEN_H; py++) {
        uint16_t *row;
        if (py < 0) continue;
        row = (uint16_t *)((unsigned char *)screen->pixels +
                          py * screen->pitch);
        for (px = x; px < x + width && px < SCREEN_W; px++) {
            Uint8 r;
            Uint8 g;
            Uint8 b;
            int r5;
            int g5;
            int b5;
            if (px < 0) continue;
            SDL_GetRGB(row[px], screen->format, &r, &g, &b);
            /*
             * sub_4552E7(...,-10) uses the 32-entry table at
             * 0x485C28 for each RGB555 channel.  Its values are exactly
             * floor((22 * channel + 16) / 32); applying a percentage to
             * SDL's expanded 8-bit channels loses the low table entries.
             */
            r5 = ((int)r * 31 + 127) / 255;
            g5 = ((int)g * 31 + 127) / 255;
            b5 = ((int)b * 31 + 127) / 255;
            r5 = (22 * r5 + 16) >> 5;
            g5 = (22 * g5 + 16) >> 5;
            b5 = (22 * b5 + 16) >> 5;
            row[px] = (uint16_t)SDL_MapRGB(
                screen->format,
                (Uint8)(r5 * 255 / 31),
                (Uint8)(g5 * 255 / 31),
                (Uint8)(b5 * 255 / 31));
        }
    }
    SDL_UnlockSurface(screen);
}

static void draw_lottery_pool(
    SDL_Surface *screen, const GamePreviewAssets *assets, int pool)
{
    char value[32];
    int index;
    int x = 184;
    if (screen == NULL || assets == NULL ||
        !game_asset_ready(assets, &assets->ui.has_panel[12]) || !game_asset_ready(assets, &assets->ui.has_panel[13]))
        return;
    /* 0x42FDE1 uses opaque, hotspot-anchored sub_4563F5. */
    draw_game_panel_chunk_at(screen, assets, 12, 9, 28, 27, 0);
    format_hud_number(value, sizeof(value), pool, 1);
    for (index = (int)strlen(value) - 1; index >= 0; index--) {
        int chunk;
        int step = 18;
        if (value[index] >= '0' && value[index] <= '9') {
            chunk = value[index] - '0';
        } else if (value[index] == ',') {
            chunk = 10;
            step = 12;
            x += 6;
        } else {
            chunk = 11;
        }
        draw_confinement_panel_chunk(
            screen, assets, 13, (size_t)chunk, x, 41);
        x -= step;
    }
}

/* 0x452946 removes the padding spaces from the fixed-width character name
 * before 0x44090E formats the hotel/department-store roulette message. */
static void roulette_compact_character_name(
    char *out_name, size_t capacity, int character)
{
    const char *source = new_game_character_name(character);
    size_t length = 0;

    if (out_name == NULL || capacity == 0) return;
    while (source != NULL && *source != '\0' && length + 1 < capacity) {
        if (*source != ' ')
            out_name[length++] = *source;
        source++;
    }
    out_name[length] = '\0';
}

void draw_lottery_visit_panel(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    int number;
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->lottery.active || !game_asset_ready(assets, &assets->ui.has_panel[12]))
        return;
    /*
     * sub_42F32C uses sub_4563F5 for chunk 0: it is hotspot-anchored at
     * (0,0) like sub_456418, but copies zero pixels instead of treating
     * them as transparent.
     */
    draw_game_panel_chunk_at(screen, assets, 12, 0, 0, 0, 0);
    for (number = 0; number < 36; number++) {
        if (session->state->lottery_owner[number] != 0) {
            int column = number % 9;
            int row = number / 9;
            /* 0x42F35D applies brightness -10 to the 62x46 cell. */
            darken_lottery_number_rect(
                screen, 31 + column * 64, 272 + row * 48, 62, 46);
        }
    }
    if (session->presentation->lottery.phase == RICH4_LOTTERY_VISIT_EXIT) {
        /* 0x42F9EF swaps in the waving/exit clerk before #0014/#0016. */
        draw_confinement_panel_chunk(screen, assets, 12, 2, 154, -8);
    } else {
        static const int blink_chunks[3] = {3, 4, 3};
        draw_confinement_panel_chunk(screen, assets, 12, 1, 210, -5);
        if (session->presentation->lottery.blink_frame >= 0 &&
            session->presentation->lottery.blink_frame < 3)
            draw_game_panel_chunk_at(
                screen, assets, 12,
                (size_t)blink_chunks[
                    session->presentation->lottery.blink_frame],
                273, 63, 0);
        if (session->presentation->lottery.gesture_chunk >= 5 &&
            session->presentation->lottery.gesture_chunk <= 6)
            draw_game_panel_chunk_at(
                screen, assets, 12,
                (size_t)session->presentation->lottery.gesture_chunk,
                273, 105, 0);
    }
    if (game_asset_ready(assets, &assets->ui.has_panel_graph[14]))
        draw_game_graph_frame_data(
            screen, assets->ui.panel_data[14], assets->ui.panel_size[14],
            session->presentation->lottery.animation_frame, 8, 8);
    if (session->presentation->lottery.animation_complete)
        draw_lottery_pool(screen, assets, session->state->lottery_pool);
    if (session->presentation->lottery.selected >= 0 &&
        session->presentation->lottery.selected < 36) {
        int column = session->presentation->lottery.selected % 9;
        int row = session->presentation->lottery.selected / 9;
        /* 0x42FF8E draws Panel[12]+96 (helper chunk 7). */
        draw_confinement_panel_chunk(
            screen, assets, 12, 7,
            column * 64 + 62, row * 48 + 295);
    }
    if (session->presentation->lottery.speech_active &&
        session->presentation->lottery.speech_index >= 0) {
        Rich4SprSmpChunk bubble;
        const Rich4CharacterSpeech *speech = rich4_lottery_counter_speech(
            session->presentation->lottery.speech_index);
        Rich4PlayerSayText decoded;
        rich4_player_say_decode(
            speech != NULL ? speech->big5 : NULL,
            speech != NULL ? speech->speaking_resource : -1, &decoded);
        if (rich4_spr_smp_get_chunk(
                assets->ui.panel_data[12], assets->ui.panel_size[12],
                &assets->ui.panel_spr[12], 8, &bubble) == RICH4_SPR_SMP_OK) {
            int left = 360 - bubble.x;
            int top = 20 - bubble.y;
            draw_confinement_panel_chunk(
                screen, assets, 12, 8, 360, 20);
            /*
             * sub_44EC30(...,360,20,20,0,0x101010,0) and
             * sub_44ECB6 center the text in the bubble with x offset 20.
             */
            draw_big5_text(
                screen, left + 20 + bubble.width / 2,
                top + bubble.height / 2, decoded.display_text, 20,
                rich4_character_rgb565_color(0x00101010u),
                0, 2, 4);
        }
    }
}

void draw_facility_roulette(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    const GameFacilityRoulettePresentationState *roulette;
    Rich4SprSmpChunk chunk;
    static const char *const owner_formats[3] = {
        NULL,
        "%s\n\n\xb0\x65\xb1\x7a\xa5\x58\xb0\xea\xae\xc8\xb9\x43...",
        "%s\xaa\xba\xae\xc8\xc0\x5d\n\n\xbd\xd0\xb6\x69\xa8\xd3\xa5\xf0\xae\xa7..."
    };
    static const char *const company_formats[4] = {
        "%s\n\n\xbd\xd0\xb6\x69\xa8\xd3\xa5\xf0\xae\xa7...",
        NULL,
        NULL,
        "%s\n\n\xb1\x7a\xaa\xba\xa7\xeb\xab\x4f"
        "\xa4\xd1\xbc\xc6\xac\xb0..."
    };
    char message[160];
    char owner_name[32];
    const char *owner_source = NULL;
    int owner = -1;
    int resource;
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->facility_roulette.active)
        return;
    roulette = &session->presentation->facility_roulette;
    resource = 68 + roulette->mode;
    if (game_asset_ready(assets, &assets->ui.has_worker_choice_ui) &&
        rich4_spr_smp_get_chunk(
            assets->ui.worker_choice_data, assets->ui.worker_choice_size,
            &assets->ui.worker_choice_spr, 5, &chunk) == RICH4_SPR_SMP_OK)
        draw_chunk_at(screen, assets->ui.worker_choice_data, &chunk,
                      220, 140, 1);
    draw_game_panel_chunk_at(
        screen, assets, resource,
        (size_t)((roulette->initial_frame_pending ? 0 : roulette->slot) + 2),
        220, 320, 1);
    draw_game_panel_chunk_at(
        screen, assets, resource,
        (size_t)(roulette->phase == RICH4_ROULETTE_SPINNING ? 0 : 1),
        265, 230, 1);

    /* The original mode 1/2 callers build this source from the property's
     * one-based owner field, then call 0x452946 before 0x44090E. */
    if (roulette->subject.kind == GAME_DOMAIN_FACILITY &&
        roulette->subject.id >= 0 &&
        roulette->subject.id < session->state->facility_count)
        owner = session->state->facility_owner[roulette->subject.id];
    if (owner >= 0 && owner < session->state->player_count) {
        roulette_compact_character_name(
            owner_name, sizeof(owner_name),
            session->state->players[owner].character);
        owner_source = owner_name;
    }
    if (roulette->mode >= 1 && roulette->mode <= 2 &&
        owner_source != NULL) {
        snprintf(message, sizeof(message), owner_formats[roulette->mode],
                 owner_source);
        draw_big5_text(
            screen, 220, 140, message, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
    } else if ((roulette->mode == 0 || roulette->mode == 3) &&
        roulette->subject.kind == GAME_DOMAIN_COMPANY &&
        roulette->subject.id >= 0 &&
        roulette->subject.id < session->state->company_count) {
        int stock = session->state->company_stock_index[roulette->subject.id];
        const char *company_name =
            session->presentation->map_company_names[roulette->subject.id];
        if (company_name[0] == '\0')
            company_name = game_stock_name_for_state(session->state, stock);
        snprintf(message, sizeof(message),
                 company_formats[roulette->mode],
                 company_name);
        draw_big5_text(
            screen, 220, 140, message, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
    } else if (!roulette->automatic &&
               roulette->phase == RICH4_ROULETTE_SPINNING)
        draw_big5_text(
            screen, 220, 140,
            "\xa5\xaa\xc1\xe4\xa9\xce\xbd\x54\xbb\x7b\xc1\xe4\xb0\xb1\xa4\xee",
            16, rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
}

static int insurance_candidates(const GameState *state, int actor, int *players)
{
    int i, count = 0;
    for (i = 0; i < state->player_count; i++)
        if (i != actor && (state->players[i].play_state & 0xff)) players[count++] = i;
    return count;
}

int game_insurance_target_at(const GamePreviewAssets *assets,
                            const GameState *state, int actor, int x, int y)
{
    int candidates[GAME_MAX_PLAYERS], count, left, top;
    Rich4SprSmpChunk frame;
    if (assets == NULL || state == NULL) return -1;
    count = insurance_candidates(state, actor, candidates);
    if (count < 2 || !game_asset_ready(assets, &assets->ui.has_player_choice) ||
        rich4_spr_smp_get_chunk(assets->ui.player_choice_data, assets->ui.player_choice_size,
            &assets->ui.player_choice_spr, (size_t)(count - 2), &frame) != RICH4_SPR_SMP_OK) return -1;
    /* 0x440E7F/0x440E93 and 0x440064: 80px cells, 72px hit height. */
    left = 220 - frame.x + 12; top = 320 - frame.y + 12;
    return x >= left && x < left + count * 80 && y >= top && y < top + 72 ?
        candidates[(x - left) / 80] : -1;
}

static void draw_passive_charge(SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot,
    const GamePropertyPresentationState *p)
{
    const GameState *state;
    Rich4SprSmpChunk frame;
    char message[192], compact_name[64];
    int prompt = 0, candidates[GAME_MAX_PLAYERS], count, i;
    if (screen == NULL || assets == NULL || snapshot == NULL ||
        snapshot->presentation == NULL || snapshot->state == NULL) return;
    state = snapshot->state;
    if (!p->insurance_active || snapshot->presentation->speech.active) return;
    if (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_CARD) {
        if (p->insurance_card == 19) snprintf(message, sizeof(message), "\xa8\xcf\xa5\xce%s", game_card_big5_name(19));
        else snprintf(message, sizeof(message), "%s\n\n\xb6\xf9\xba\xd7\xa5\x64\xa5\xcd\xae\xc4\xa1\x49",
            new_game_character_name(state->players[p->actor].character));
        draw_original_card_presentation(screen, assets, p->insurance_card, message);
        return;
    }
    count = insurance_candidates(state, p->actor, candidates);
    if (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM) {
        snprintf(message, sizeof(message), "%s\n\n\xac\x4f\xa7\x5f\xa8\xcf\xa5\xce\xa7\x4b\xb6\x4f\xa5\x64\xa1\x48",
            new_game_character_name(state->players[p->actor].character));
        prompt = 1;
    } else if (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_TARGET) {
        if (count == 1) {
            roulette_compact_character_name(compact_name, sizeof(compact_name), state->players[candidates[0]].character);
            snprintf(message, sizeof(message), "\xac\x4f\xa7\x5f\xb6\xf9\xba\xd7\xb5\xb9%s\xa1\x48",
                compact_name); prompt = 1;
        } else {
            snprintf(message, sizeof(message), "\xbd\xd0\xbf\xef\xbe\xdc\xb6\xf9\xba\xd7\xb9\xef\xb6\x48...");
            if (count >= 2 && game_asset_ready(assets, &assets->ui.has_player_choice) &&
                rich4_spr_smp_get_chunk(assets->ui.player_choice_data, assets->ui.player_choice_size,
                    &assets->ui.player_choice_spr, (size_t)(count - 2), &frame) == RICH4_SPR_SMP_OK) {
                int left = 220 - frame.x + 12, top = 320 - frame.y + 12;
                draw_chunk_at(screen, assets->ui.player_choice_data, &frame, 220, 320, 0);
                for (i = 0; i < count; i++) {
                    Rich4SprSmpChunk portrait;
                    if (game_asset_ready(assets, &assets->character.has_character_ui) &&
                        rich4_spr_smp_get_chunk(assets->character.character_data, assets->character.character_size,
                            &assets->character.character_spr, state->players[candidates[i]].character,
                            &portrait) == RICH4_SPR_SMP_OK)
                        draw_chunk_topleft(screen, assets->character.character_data, &portrait, left + 80*i, top, 0);
                    if (p->insurance_target == candidates[i]) {
                        int n;
                        for (n = 0; n < 3; n++) {
                            SDL_Rect r = {(Sint16)(left+80*i-3+n), (Sint16)(top-3+n), (Uint16)(78-2*n), 1};
                            Uint32 yellow = SDL_MapRGB(screen->format, 255, 255, 0);
                            SDL_FillRect(screen, &r, yellow); r.y = (Sint16)(top+74-n); SDL_FillRect(screen, &r, yellow);
                            r.y = (Sint16)(top-3+n); r.w = 1; r.h = (Uint16)(78-2*n); SDL_FillRect(screen, &r, yellow);
                            r.x = (Sint16)(left+80*i+74-n); SDL_FillRect(screen, &r, yellow);
                        }
                    }
                }
            }
        }
    } else if (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE) {
        roulette_compact_character_name(compact_name, sizeof(compact_name), state->players[p->insurance_target].character);
        snprintf(message, sizeof(message), "\xb6\xf9\xba\xd7\xb5\xb9%s\xa1\x49",
            compact_name);
    } else if (p->phase == GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE_NOTICE) {
        roulette_compact_character_name(compact_name, sizeof(compact_name), state->players[p->insurance_payer].character);
        snprintf(message, sizeof(message), "\xa6\xba\xaf\xab\xc5\xe3\xc6\x46\n\n\xa5\xd1%s\xbd\xdf\xc0\x76%s",
            compact_name, "\xab\x4f\xc0\x49\xb6\x4f");
    } else return;
    if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
        rich4_spr_smp_get_chunk(assets->ui.common_dialog_data, assets->ui.common_dialog_size,
            &assets->ui.common_dialog_spr, 5, &frame) == RICH4_SPR_SMP_OK)
        draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
    draw_big5_text(screen, 220, 140, message, 16,
        rich4_character_rgb565_color(0x00f0f0f0u), rich4_character_rgb565_color(0x00101010u), 3, 4);
    if (prompt) {
        int left, top;
        if (game_confirm_dialog_bounds(assets, GAME_COMMAND_PURCHASE_PROPERTY, &left, &top, NULL, NULL) &&
            rich4_spr_smp_get_chunk(assets->ui.confirm_dialog_data, assets->ui.confirm_dialog_size,
                &assets->ui.confirm_dialog_spr, 1, &frame) == RICH4_SPR_SMP_OK)
            draw_chunk_topleft(screen, assets->ui.confirm_dialog_data, &frame, left, top, 1);
    }
}

void draw_insurance_charge(SDL_Surface *screen, const GamePreviewAssets *assets,
                           const Rich4GameEventPresentationSnapshot *snapshot)
{
    if (snapshot == NULL || snapshot->presentation == NULL) return;
    draw_passive_charge(screen,assets,snapshot,&snapshot->presentation->property);
}

void draw_fortune_passive_choice(SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *snapshot)
{
    const GameSpecialEventPresentationState *s = &snapshot->presentation->special_event;
    int passive_actor = s->family == GAME_SPECIAL_EVENT_NEWS && s->resolved_id == 29
        ? s->reaction_player : s->actor;
    GamePropertyPresentationState p;
    char message[192];
    if (s->phase == RICH4_SPECIAL_PRESENT_GOD_NOTICE) {
        const char *format=rich4_fortune_god_notice_format_big5(s->resolved_id,s->defense);
        if (format==NULL) return;
        (void)rich4_special_event_format_big5(message,sizeof(message),format,
            rich4_special_event_god_name_big5(snapshot->state->players[s->actor].god_type),0);
        if (game_asset_ready(assets,&assets->ui.has_common_dialog))
            draw_spr_smp_center(screen,assets->ui.common_dialog_data,assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr,5,220,140);
        draw_big5_text(screen,220,140,message,16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u),3,4);
        return;
    }
    if (s->phase == RICH4_SPECIAL_PRESENT_PASSIVE_CARD && s->fortune_passive_card == 20) {
        /* 0x444BB2: Data[591], the automatic absolution notice. */
        snprintf(message,sizeof(message),"%s\n\n\xa7\x4b\xb8\x6f\xa5\x64\xa5\xcd\xae\xc4\xa1\x49",
            new_game_character_name(snapshot->state->players[passive_actor].character));
        draw_original_card_presentation(screen,assets,20,message);
        return;
    }
    memset(&p,0,sizeof(p));
    p.insurance_active = 1;
    p.actor = passive_actor;
    p.insurance_card = 18;
    p.insurance_target = s->phase == RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE ?
        s->fortune_redirect_target : s->fortune_choice_hover;
    p.phase = s->phase == RICH4_SPECIAL_PRESENT_PASSIVE_CARD
        ? GAME_PROPERTY_PRESENT_INSURANCE_CARD :
        s->phase == RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE ?
            GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE : GAME_PROPERTY_PRESENT_INSURANCE_TARGET;
    draw_passive_charge(screen,assets,snapshot,&p);
}

void draw_company_issue(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    const GameCompanyIssuePresentationState *issue;
    Rich4SprSmpChunk background;
    Rich4SprSmpChunk buttons;
    char message[160];
    int left;
    int top;
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->company_issue.active)
        return;
    issue = &session->presentation->company_issue;
    if (issue->phase == 1) {
        draw_game_quantity_panel(screen, assets, issue->quantity, issue->maximum,
            issue->entry_pressed, issue->entry_x, issue->entry_y);
        return;
    }
    if (game_asset_ready(assets, &assets->ui.has_worker_choice_ui) &&
        rich4_spr_smp_get_chunk(
            assets->ui.worker_choice_data, assets->ui.worker_choice_size,
            &assets->ui.worker_choice_spr, 5,
            &background) == RICH4_SPR_SMP_OK)
        draw_spr_smp_center(screen, assets->ui.worker_choice_data, assets->ui.worker_choice_size,
            &assets->ui.worker_choice_spr, 5, 220, 140);
    if (issue->phase == 2) {
        const char *notice = session->state->company_type[issue->company] == 12 ?
            "\xae\xa5\xb3\xdf\xb1\x7a\xa6\xa8\xac\xb0\xc0\xb0\xa5\x44\xa1\x49" :
            "\xae\xa5\xb3\xdf\xb1\x7a\xc0\xf2\xb1\x6f\xb8\x67\xc0\xe7\xc5\x76\xa1\x49";
        draw_big5_text(screen, 220, 140, notice, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
    } else if (issue->phase == 0) {
        snprintf(message, sizeof(message), "%s\n\n\xa8\x43\xaa\xd1\xb0\xe2\xbb\xf9%d\n\n\xac\x4f\xa7\x5f\xbb\x7b\xc1\xca\xaa\xd1\xa5\xf7\xa1\x48",
                 session->presentation->map_company_names[issue->company], issue->price);
        draw_big5_text(
            screen, 220, 140, message, 16,
            rich4_character_rgb565_color(0x00f0f0f0u),
            rich4_character_rgb565_color(0x00101010u), 3, 4);
        if (game_confirm_dialog_bounds(
                assets, GAME_COMMAND_PURCHASE_PROPERTY,
                &left, &top, NULL, NULL) &&
            rich4_spr_smp_get_chunk(
                assets->ui.confirm_dialog_data,
                assets->ui.confirm_dialog_size,
                &assets->ui.confirm_dialog_spr, 1,
                &buttons) == RICH4_SPR_SMP_OK)
            draw_chunk_topleft(
                screen, assets->ui.confirm_dialog_data, &buttons,
                left, top, 1);

    }
}

void draw_magic_house(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const Rich4GameEventPresentationSnapshot *session)
{
    static const int icon_center[12][2] = {
        {322,91}, {414,82}, {453,157}, {509,242},
        {458,314}, {415,387}, {322,394}, {239,393},
        {188,316}, {131,222}, {184,161}, {225,83}
    };
    static const int detail[12][3] = {
        {9,208,167}, {10,510,150}, {7,545,88}, {7,568,147},
        {7,550,234}, {7,510,320}, {7,422,320}, {6,134,318},
        {6,92,250}, {6,72,130}, {6,77,85}, {9,122,154}
    };
    const struct MagicHouseState *magic;
    if (screen == NULL || assets == NULL || session == NULL ||
        !session->presentation->magic_house.active)
        return;
    magic = &session->presentation->magic_house;
    if (magic->phase == RICH4_MAGIC_HOUSE_AI_NOTICE) {
        Rich4SprSmpChunk background;
        char message[256];
        const char *condition = rich4_magic_house_condition_big5(magic->condition);
        /* 0x43398F strips the five-byte speech marker before 0x440CAC. */
        if (condition[0] == '#') condition += 5;
        if (game_asset_ready(assets, &assets->ui.has_common_dialog) && rich4_spr_smp_get_chunk(
                assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, &background) == RICH4_SPR_SMP_OK)
            draw_spr_smp_center(screen, assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                &assets->ui.common_dialog_spr, 5, 220, 140);
        snprintf(message, sizeof(message), "%s\n\n%s", condition,
                 rich4_magic_house_action_big5(magic->selected_action));
        draw_big5_text(screen, 220, 140, message, 16,
                      rich4_character_rgb565_color(0x00f0f0f0u),
                      rich4_character_rgb565_color(0x00101010u), 3, 4);
        return;
    }
    if (magic->phase == RICH4_MAGIC_HOUSE_TARGET) {
        if (magic->condition == RICH4_MAGIC_HOUSE_SURRENDER) return;
        int player = magic->target_player;
        if (player >= 0 && player < session->state->player_count) {
            Rich4SprSmpChunk message_background;
            char message[128];
            if (game_asset_ready(assets, &assets->ui.has_common_dialog) &&
                rich4_spr_smp_get_chunk(
                    assets->ui.common_dialog_data, assets->ui.common_dialog_size,
                    &assets->ui.common_dialog_spr, 5,
                    &message_background) == RICH4_SPR_SMP_OK)
                draw_chunk_at(
                    screen, assets->ui.common_dialog_data,
                    &message_background, 220, 140, 1);
            snprintf(
                message, sizeof(message), "%s\n\n%s",
                new_game_character_name(
                    session->state->players[player].character),
                rich4_magic_house_action_big5(magic->selected_action));
            draw_big5_text(
                screen, 220, 140, message, 16,
                rich4_character_rgb565_color(0x00f0f0f0u),
                rich4_character_rgb565_color(0x00101010u), 3, 4);
        }
        return;
    }
    if (!game_asset_ready(assets, &assets->ui.has_panel[18])) return;
    draw_game_panel_chunk_at(screen, assets, 18, 0, 0, 0, 1);
    if (magic->phase != RICH4_MAGIC_HOUSE_FINALE)
        draw_game_panel_chunk_at(screen, assets, 18, 1, 241, 140, 1);
    if (magic->phase == RICH4_MAGIC_HOUSE_REVEAL) {
        draw_game_panel_chunk_at(screen, assets, 18, 3, 286, 188, 1);
    } else if (magic->phase >= RICH4_MAGIC_HOUSE_CONDITION &&
               magic->phase < RICH4_MAGIC_HOUSE_FINALE &&
               magic->condition != RICH4_MAGIC_HOUSE_SURRENDER) {
        draw_game_panel_chunk_at(
            screen, assets, 18, (size_t)(11 + magic->condition),
            326, 296, 1);
    }
    if (magic->phase == RICH4_MAGIC_HOUSE_CONFIRM)
        draw_game_panel_chunk_at(screen, assets, 18, 2, 182, 142, 1);
    if (magic->speech_started &&
        magic->phase != RICH4_MAGIC_HOUSE_REVEAL &&
        magic->phase != RICH4_MAGIC_HOUSE_SELECT &&
        magic->phase != RICH4_MAGIC_HOUSE_CONFIRM &&
        magic->phase != RICH4_MAGIC_HOUSE_FINALE &&
        magic->phase != RICH4_MAGIC_HOUSE_FACILITY_CHOICE) {
        if (magic->animation_frame == 4)
            draw_game_panel_chunk_at(
                screen, assets, 18, 4, 286, 220, 1);
        else if (magic->animation_frame == 5)
            draw_game_panel_chunk_at(
                screen, assets, 18, 5, 286, 217, 1);
    }
    if (magic->condition == RICH4_MAGIC_HOUSE_SURRENDER &&
        magic->phase == RICH4_MAGIC_HOUSE_SELECT) {
        int player;
        draw_game_panel_chunk_at(screen, assets, 18, 8, 320, 384, 1);
        for (player = 0; player < session->state->player_count; ++player) {
            int x = rich4_magic_house_surrender_target_x(magic->targets, player);
            int character = session->state->players[player].character;
            /* 0x433B10 composes Data.mkf[2] chunk character into Panel[18]. */
            if (x >= 0 && character >= 0 && character < 12 &&
                assets->character.has_character_ui)
                draw_spr_smp_center(screen, assets->character.character_data,
                    assets->character.character_size, &assets->character.character_spr,
                    (size_t)character, x, 348);
            if (x >= 0 && magic->hover == player) {
                uint16_t color = rich4_character_rgb565_color(16776960);
                stroke_rect(screen, x-2, 346, 76, 76, color);
                stroke_rect(screen, x-1, 347, 74, 74, color);
            }
        }
    }
    if (magic->condition != RICH4_MAGIC_HOUSE_SURRENDER &&
        magic->phase == RICH4_MAGIC_HOUSE_SELECT &&
        magic->hover >= 0 && magic->hover < 12) {
        int action = magic->hover;
        draw_game_panel_chunk_at(
            screen, assets, 18, (size_t)(23 + action),
            icon_center[action][0], icon_center[action][1], 1);
        draw_game_panel_chunk_at(
            screen, assets, 18, (size_t)detail[action][0],
            detail[action][1], detail[action][2], 1);
        draw_big5_text_with_spacing(
            screen, detail[action][1], detail[action][2],
            rich4_magic_house_action_big5(action), 14,
            rich4_character_rgb565_color(0x00e0e0e0u),
            rich4_character_rgb565_color(0x00202020u), 3, 0, 2);
    }
    if (magic->phase == RICH4_MAGIC_HOUSE_FINALE &&
        game_asset_ready(assets, &assets->ui.has_panel_graph[20])) {
        draw_game_panel_graph_frame(
            screen, assets, 20, magic->animation_frame, 0, 0);
    } else if (magic->phase == RICH4_MAGIC_HOUSE_FACILITY_CHOICE) {
        draw_facility_choice_selector(
            screen, assets, magic->facility_choice_hover);
    } else {
        const char *marked = rich4_magic_house_phase_big5(
            magic->phase, magic->condition);
        Rich4PlayerSayText decoded;
        Rich4SprSmpChunk bubble;
        rich4_player_say_decode(marked, -1, &decoded);
        if (decoded.display_text != NULL &&
            rich4_spr_smp_get_chunk(
                assets->ui.panel_data[18], assets->ui.panel_size[18],
                &assets->ui.panel_spr[18], 8, &bubble) == RICH4_SPR_SMP_OK) {
            int left = 320 - bubble.x;
            int top = 384 - bubble.y;
            draw_game_panel_chunk_at(
                screen, assets, 18, 8, 320, 384, 1);
            draw_big5_text(
                screen, left + bubble.width / 2,
                top + bubble.height / 2, decoded.display_text, 20,
                rich4_character_rgb565_color(0x00e0e0e0u),
                rich4_character_rgb565_color(0x00202020u), 3, 4);
        }
    }
}
