#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_monthly_finance.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "platform/sdl1/render/blit.h"
#include "platform/sdl1/runtime_render_api.h"
#include "app/monthly_finance_flow.h"
#include "data/monthly_finance_text.h"
#include "resource/spr_smp.h"
#include "resource/flc_animation.h"
#include "view/view_constants.h"
#include "view/map_renderer.h"

#include <stdio.h>
#include <stdlib.h>

static void finance_pixels(SDL_Surface *screen,const unsigned char *data,
    const Rich4SprSmpChunk *c,int sx,int sy,int w,int h,int dx,int dy,int transparent)
{
    if (sx<0 || sy<0 || c->width<=0 || c->height<=0 ||
        c->data_size<(uint32_t)c->width*(uint32_t)c->height*2) return;
    if (w>c->width-sx) w=c->width-sx;
    if (h>c->height-sy) h=c->height-sy;
    if (dx<0) { sx-=dx; w+=dx; dx=0; }
    if (dy<0) { sy-=dy; h+=dy; dy=0; }
    if (w>screen->w-dx) w=screen->w-dx;
    if (h>screen->h-dy) h=screen->h-dy;
    if (w<=0 || h<=0) return;
    SDL_LockSurface(screen);
    for (int y=0;y<h;++y) {
        uint16_t *row=(uint16_t *)((unsigned char *)screen->pixels+(dy+y)*screen->pitch);
        const unsigned char *src=data+c->data_offset+2*((sy+y)*c->width+sx);
        for (int x=0;x<w;++x) {
            uint16_t v=(uint16_t)(src[2*x]|((uint16_t)src[2*x+1]<<8));
            if (!transparent || v)
                /* 0x451801 pixel_fmt=1: green's added low bit stays zero. */
                row[dx+x]=(uint16_t)(((v&0x7fe0u)<<1)|(v&0x001fu));
        }
    }
    SDL_UnlockSurface(screen);
}

int draw_finance_panel_chunk_at(SDL_Surface *screen,const GamePreviewAssets *a,
    int resource,size_t index,int x,int y,int transparent)
{
    Rich4SprSmpChunk c;
    if (screen==NULL || a==NULL || resource<0 || resource>=GAME_PANEL_RESOURCE_COUNT ||
        !game_asset_ready(a, &a->ui.has_panel[resource]) || rich4_spr_smp_get_chunk(a->ui.panel_data[resource],
        a->ui.panel_size[resource],&a->ui.panel_spr[resource],index,&c)!=RICH4_SPR_SMP_OK) return 0;
    finance_pixels(screen,a->ui.panel_data[resource],&c,0,0,c.width,c.height,
                   x-c.x,y-c.y,transparent);
    return 1;
}

static void text(SDL_Surface *screen, int x, int y, const char *value,
    int height, unsigned int align)
{
    draw_big5_text(screen, x, y, value, height,
        rich4_character_rgb565_color(0x00101010u), 0, 0, align);
}

static void amount(SDL_Surface *screen, int x, int y, int value)
{
    char number[32];
    format_hud_number(number, sizeof(number), value, 0);
    text(screen, x, y, number, 16, 6);
}

static void bubble(SDL_Surface *screen, const GamePreviewAssets *a,
    int panel, int index, int x, int y, int x_offset, int y_offset, int speech)
{
    Rich4SprSmpChunk chunk;
    if (speech == 0 || !game_asset_ready(a, &a->ui.has_panel[panel])) return;
    if (rich4_spr_smp_get_chunk(a->ui.panel_data[panel], a->ui.panel_size[panel],
        &a->ui.panel_spr[panel], (size_t)index, &chunk) != RICH4_SPR_SMP_OK) return;
    draw_finance_panel_chunk_at(screen, a, panel, (size_t)index, x, y, 1);
    text(screen, x - chunk.x + chunk.width / 2 + x_offset,
         y - chunk.y + chunk.height / 2 + y_offset,
         rich4_monthly_finance_text(speech), 20, 4);
}

static void dividends(SDL_Surface *screen, const GamePreviewAssets *a,
    const Rich4GameEventPresentationSnapshot *s)
{
    const Rich4CompanyDividend *d = &s->presentation->periodic_finance.dividend;
    int stock, player, x = 184, y = 140;
    SDL_FillRect(screen, NULL, 0);
    draw_finance_panel_chunk_at(screen, a, 76, 0, 24, 24, 0);
    /* Text positions are Panel[76] chunk-0 coordinates plus (24,24),
     * exactly as 0x42B483 blits the pre-rendered report. */
    draw_big5_text(screen, 320, 49, "\xa4\x57\xa5\xab\xa4\xbd\xa5\x71\xa4\xc0\xac\xf5",
        28, rich4_character_rgb565_color(0x00f0f0f0u),
        rich4_character_rgb565_color(0x00101010u), 3, 2);
    text(screen, 128, 106, "\xa4\x48\xa6\x57", 12, 6);
    text(screen, 42, 118, "\xa4\xbd\xa5\x71", 12, 5);
    for (player = 0; player < s->state->player_count; ++player)
        if ((d->player_mask & (1u << player)) != 0) {
            text(screen, x, 112,
                new_game_character_name(s->state->players[player].character), 16, 2);
            x += 98;
        }
    text(screen, 566, 112, "\xa5\xbb\xa4\xeb\xac\xd5\xbe\x6c", 16, 2);
    text(screen, 86, 428, "\xac\xf5  \xa7\x51", 16, 2);
    for (stock = 0; stock < GAME_STOCK_COUNT; ++stock) {
        const char *name;
        int company;
        if ((d->stock_mask & (1u << stock)) == 0) continue;
        company = d->company[stock];
        name = company >= 0 ? s->presentation->map_company_names[company] : "";
        if (*name == '\0') name = game_stock_name_for_state(s->state, stock);
        text(screen, 86, y, name, 16, 2);
        x = 222;
        for (player = 0; player < s->state->player_count; ++player)
            if ((d->player_mask & (1u << player)) != 0) {
                amount(screen, x, y, d->amount[stock][player]); x += 98;
            }
        amount(screen, 596, y, d->profit[stock]);
        y += 24;
    }
    x = 222;
    for (player = 0; player < s->state->player_count; ++player)
        if ((d->player_mask & (1u << player)) != 0) {
            amount(screen, x, 428, d->total[player]); x += 98;
        }
}

static void crop(SDL_Surface *screen, const GamePreviewAssets *a,
    int panel, int index, int x, int y, int sx, int sy, int w, int h, int transparent)
{
    Rich4SprSmpChunk c;
    if (game_asset_ready(a, &a->ui.has_panel[panel]) && rich4_spr_smp_get_chunk(a->ui.panel_data[panel],
        a->ui.panel_size[panel], &a->ui.panel_spr[panel], (size_t)index, &c) == RICH4_SPR_SMP_OK)
        finance_pixels(screen,a->ui.panel_data[panel],&c,sx,sy,w,h,x,y,transparent);
}

static void darken_tickets(SDL_Surface *screen, int x, int y)
{
    /* 0x42F478 pushes -16: LUT 0x485B68 is [0,0,1,1,...,15,15]. */
    SDL_LockSurface(screen);
    for (int row = y; row < y + 60; ++row) {
        uint16_t *pixels = (uint16_t *)((unsigned char *)screen->pixels + row * screen->pitch);
        for (int col = x; col < x + 296; ++col)
            pixels[col] = (uint16_t)((pixels[col] & 0xf79eu) >> 1);
    }
    SDL_UnlockSurface(screen);
}

static void tickets(SDL_Surface *screen, const GamePreviewAssets *a, const GameState *s)
{
    static const int positions[4][2] = {{16,340},{16,410},{328,340},{328,410}};
    int ordinal = 0;
    for (int p = 0; p < s->player_count; ++p) {
        int sold[12], n = 0, x, y;
        if (!s->players[p].play_state) continue;
        x = positions[ordinal][0]; y = positions[ordinal++][1];
        darken_tickets(screen, x, y);
        draw_finance_panel_chunk_at(screen, a, 15, (size_t)(25 + s->players[p].character),
                                 x + 20, y + 30, 1);
        for (int i = 0; i < 36 && n < 12; ++i)
            if (s->lottery_owner[i] == p + 1) sold[n++] = i + 1;
        for (int i = 0; i < n; ++i) {
            int px = x + 54 + 40 * (i % 6);
            int py = y + (n > 6 ? 15 + 30 * (i / 6) : 30);
            draw_finance_panel_chunk_at(screen, a, 13, (size_t)(sold[i] / 10), px, py, 1);
            draw_finance_panel_chunk_at(screen, a, 13, (size_t)(sold[i] % 10), px + 16, py, 1);
        }
    }
}

static void reveal_digit(SDL_Surface *screen, const GamePreviewAssets *a, int digit, int x)
{
    Rich4SprSmpChunk c;
    /* 0x430CBA/0x430CF4: 0x48BAD8 is preloaded Data.MKF[517]. */
    if (game_asset_ready(a, &a->ui.has_worker_choice_ui) && rich4_spr_smp_get_chunk(a->ui.worker_choice_data,
        a->ui.worker_choice_size, &a->ui.worker_choice_spr, (size_t)(8 + digit), &c) == RICH4_SPR_SMP_OK)
        finance_pixels(screen,a->ui.worker_choice_data,&c,0,0,c.width,c.height,x-c.x,220-c.y,1);
}

static void roll_frame(SDL_Surface *screen, const GamePreviewAssets *a, int frame)
{
    Rich4FlcHeader h;
    Rich4FlcDecoder d;
    uint16_t *pixels;
    if (!game_asset_require_data(a, &a->ui.panel_data[16])) return;
    if (rich4_flc_parse_header(a->ui.panel_data[16], a->ui.panel_size[16], &h) != RICH4_FLC_OK)
        return;
    pixels = calloc((size_t)h.width * h.height, sizeof(*pixels));
    if (pixels == NULL) return;
    /* 0x450CED flags=8: index zero is opaque, unlike the flags=1/3 animations. */
    if (rich4_flc_decoder_init(&d, a->ui.panel_data[16], a->ui.panel_size[16],
        pixels, (size_t)h.width * h.height) == RICH4_FLC_OK) {
        int ok = 1;
        for (int i = 0; i <= frame && i < h.frame_count; ++i)
            if (rich4_flc_decode_next(&d, NULL) != RICH4_FLC_OK) { ok = 0; break; }
        if (ok) {
            SDL_LockSurface(screen);
            for (int y = 0; y < h.height; ++y) {
                uint16_t *row = (uint16_t *)((unsigned char *)screen->pixels + (75+y)*screen->pitch);
                for (int x = 0; x < h.width; ++x) row[183+x] = pixels[(size_t)y*h.width+x];
            }
            SDL_UnlockSurface(screen);
        }
    }
    free(pixels);
}

void draw_periodic_finance(SDL_Surface *screen, const GamePreviewAssets *a,
    const Rich4GameEventPresentationSnapshot *s)
{
    const GamePeriodicFinancePresentationState *p;
    int winning, no_winner, rolling, revealed;
    char number[32];
    if (screen == NULL || a == NULL || s == NULL ||
        !s->presentation->periodic_finance.active) return;
    p = &s->presentation->periodic_finance;
    if (p->phase < 0) return;
    if (p->phase == 0) { dividends(screen, a, s); return; }
    winning = p->phase == 5 || p->phase == 6;
    no_winner = p->winner < 0 && (p->phase == 7 || p->phase == 8);
    rolling = p->phase == 3 || p->phase == 4 || p->phase == RICH4_FINANCE_NO_WINNER_REVEAL;
    revealed = p->phase == 4 || p->phase == RICH4_FINANCE_NO_WINNER_REVEAL;
    draw_finance_panel_chunk_at(screen, a, 15, 0, 0, 0, 0);
    draw_finance_panel_chunk_at(screen, a, 15, winning ? 6u : 3u,
                             winning ? 0 : 7, winning ? 0 : 66, 1);
    draw_finance_panel_chunk_at(screen, a, 15, winning ? 5u : no_winner ? 4u : 1u,
                             winning ? 505 : no_winner ? 489 : 472, no_winner ? 116 : 66, 1);
    if (rolling) {
        crop(screen, a, 15, 0, 472,116,472,116,45,90,0);
        draw_finance_panel_chunk_at(screen, a, 15, 2, 418,66,1);
        if (p->ticks >= 20 || revealed) {
            crop(screen, a, 15, 0, 418,171,418,171,80,80,0);
            crop(screen, a, 15, 1, 472,117,0,51,38,90,1);
        }
    }
    if (no_winner) draw_finance_panel_chunk_at(screen, a, 15, 21,52,89,0);
    if (p->phase == 6) crop(screen, a, 15, 0,150,0,150,0,330,360,0);
    if (p->phase <= 3 || p->phase >= 9 || p->phase == 4) {
        if (p->right_expression)
            draw_finance_panel_chunk_at(screen, a, 15, (size_t)p->right_expression,512,102,0);
        if (p->left_expression)
            draw_finance_panel_chunk_at(screen, a, 15, (size_t)p->left_expression,52,89,0);
        if (p->mouth_chunk)
            draw_finance_panel_chunk_at(screen, a, 15, (size_t)p->mouth_chunk,512,119,0);
    }
    draw_big5_text_with_spacing(screen, winning ? 91 : 77, winning ? 19 : 193,
        "\xb2\xd6\xbf\x6e\xbc\xfa\xaa\xf7",20,rich4_character_rgb565_color(0x4f35b1),0,0,0,2);
    format_hud_number(number, sizeof(number), p->pool, 1);
    draw_big5_text_with_spacing(screen, winning ? 91 : 77, winning ? 56 : 228,
        number,20,rich4_character_rgb565_color(0xff0000),0,0,0,2);
    if (p->phase == 5 && p->winner >= 0) {
        draw_finance_panel_chunk_at(screen, a, 15, 24, 320, 200, 1);
        draw_big5_text_with_spacing(screen, 320, 180,
            new_game_character_name(s->state->players[p->winner].character), 28,
            rich4_character_rgb565_color(0x00ff0000u),
            rich4_character_rgb565_color(0x00400000u),3,4,2);
    }
    if (revealed || (p->winner < 0 && p->phase >= 7))
        roll_frame(screen,a,game_asset_graph_frames(a, &a->ui.panel_data[16], a->ui.panel_size[16])-1);
    if (p->number > 0) {
        draw_finance_panel_chunk_at(screen,a,15,(size_t)(37+p->number/10),286,405,1);
        draw_finance_panel_chunk_at(screen,a,15,(size_t)(37+p->number%10),358,405,1);
    }
    tickets(screen, a, s->state);
    if (p->phase == 3 && p->animation_frame > 0) roll_frame(screen,a,p->animation_frame-1);
    if (revealed || (p->winner < 0 && p->phase >= 7)) {
        reveal_digit(screen,a,p->number/10,300); reveal_digit(screen,a,p->number%10,340);
    }
    if (p->phase == 5 && p->animation_frame > 0)
        draw_game_graph_frame_data(screen,a->ui.panel_data[17],a->ui.panel_size[17],
                                   p->animation_frame-1,205,0);
    if (no_winner) bubble(screen,a,15,23,320,200,0,0,p->speech_id);
    else bubble(screen,a,15,22,300,47,-10,0,p->speech_id);
}

static void banker_face(SDL_Surface *screen, const GamePreviewAssets *a,
    const GameBankSettlementPresentationState *p)
{
    static const int eye_xy[5][2] = {{76,120},{103,120},{87,118},{0,0},{83,123}};
    static const int mouth_rect[6][4] = {{76,160,80,20},{103,155,60,25},
                                        {87,161,76,27},{0,0,0,0},{0,0,0,0},{90,134,60,40}};
    int mode = p->face_mode;
    if (mode >= 0 && mode < 5 && p->eye_chunk)
        draw_finance_panel_chunk_at(screen,a,25,(size_t)p->eye_chunk,eye_xy[mode][0],eye_xy[mode][1],0);
    if (mode >= 0 && mode < 6) {
        int open = p->mouth_chunk & 255, closed = p->mouth_chunk >> 8;
        const int *r = mouth_rect[mode];
        if (open) draw_finance_panel_chunk_at(screen,a,25,(size_t)open,r[0],r[1],0);
        if (closed) draw_finance_panel_chunk_at(screen,a,25,(size_t)closed,r[0]+r[2],r[1]+r[3],0);
    }
}

int draw_monthly_award(SDL_Surface *screen, const GamePreviewAssets *a,
    const Rich4GameEventPresentationSnapshot *s)
{
    static const int x_positions[3][4] = {{407,490,0,0},{324,407,490,0},{324,407,490,573}};
    static const int background_x[3][4] = {{334,414,0,0},{259,334,414,0},{259,334,414,494}};
    static const int background_chunk[3][4] = {{16,17,0,0},{15,16,17,0},{15,16,17,18}};
    static const int offsets[12][6] = {
        {-50,-94,1539,-77,-148,3},{-48,-97,1027,-48,-90,1539},
        {-38,-97,1539,-50,-87,1539},{-31,-61,1539,-64,-109,771},
        {-41,-87,1027,-42,-77,1539},{-44,-76,1027,-31,-60,515},
        {-36,-100,1539,-36,-103,1539},{-50,-89,1027,-50,-87,3},
        {-51,-90,1027,-51,-92,3},{-31,-60,1027,-51,-95,771},
        {-103,-188,1027,-39,-87,1539},{-40,-75,1027,-40,-75,1027}
    };
    const GameBankSettlementPresentationState *p;
    Rich4SprSmpChunk chunk;
    int count = 0, ordinal = 0, selected, selected_ordinal = 0, i, row, stage;
    if (screen == NULL || a == NULL || s == NULL || !s->presentation->bank_settlement.active)
        return 0;
    p = &s->presentation->bank_settlement;
    stage = p->display_stage != 0 ? p->display_stage : p->stage;
    if (stage < 5) return 0;
    selected = stage == 5 ? -1 : stage <= 9 ? p->unlucky_player : p->richest_player;
    if (stage != 5 && (selected < 0 || selected >= s->state->player_count)) return 0;
    for (i = 0; i < s->state->player_count; ++i) if ((p->player_mask & (1u << i)) != 0) {
        if (i == selected) selected_ordinal = count;
        ++count;
    }
    row = count < 2 ? 0 : count - 2;
    draw_finance_panel_chunk_at(screen, a, 25, 0, 0, 0, 0);
    if (selected >= 0) draw_finance_panel_chunk_at(screen, a, 25,
        (size_t)background_chunk[row][selected_ordinal], background_x[row][selected_ordinal], 0, 0);
    draw_finance_panel_chunk_at(screen, a, 25,
        stage == 5 ? 19u : stage == 6 || stage == 8 ? 32u : stage == 7 ? 37u :
        stage == 9 || stage == 16 ? 38u : stage == 17 || stage == 18 ? 42u : 45u,
        stage == 5 ? 24 : stage == 6 || stage == 8 ? 35 : stage == 7 ? 0 :
        stage == 9 || stage == 16 ? 17 : stage == 17 || stage == 18 ? 27 : 6,
        stage == 5 ? 70 : stage == 6 || stage == 8 ? 67 : stage == 7 ? 89 :
        stage == 9 || stage == 16 ? 65 : stage == 17 || stage == 18 ? 64 : 60, 1);
    for (i = 0; i < s->state->player_count; ++i) {
        int c, actor;
        if ((p->player_mask & (1u << i)) == 0) continue;
        c = s->state->players[i].character;
        actor = 47 + 3 * c;
        if ((stage == 8 || stage == 18) && i == selected) {
            int kind = stage == 8 ? 1 : 0;
            if (!game_asset_require_data(a, &a->ui.monthly_award_data[c][kind]))
                continue;
            int frames = game_asset_graph_frames(a, &a->ui.monthly_award_data[c][kind], a->ui.monthly_award_size[c][kind]);
            if (p->animation_frame >= 0 && frames > 0)
                draw_game_graph_frame_data(screen, a->ui.monthly_award_data[c][kind],
                a->ui.monthly_award_size[c][kind], p->animation_frame %
                    frames,
                x_positions[row][ordinal] + offsets[c][kind * 3],
                330 + offsets[c][kind * 3 + 1]);
        } else if (rich4_spr_smp_get_chunk(a->ui.panel_data[25], a->ui.panel_size[25],
            &a->ui.panel_spr[25], (size_t)actor, &chunk) == RICH4_SPR_SMP_OK)
            draw_finance_panel_chunk_at(screen, a, 25, (size_t)actor,
                x_positions[row][ordinal], 330 + chunk.y - chunk.height, 1);
        ++ordinal;
    }
    if (stage == 8 || stage == 18) {
        char number[32];
        int values[3];
        static const char *const unlucky[3] = {
            "\xa5\xbb\xa4\xeb\xb7\x4e\xa5\x7e\xb7\x6c\xa5\xa2\xa1\x47",
            "\xa5\xbb\xa4\xeb\xb7\x4e\xa5\x7e\xa4\xa7\xb0\x5d\xa1\x47",
            "\xa5\xbb\xa4\xeb\xad\xcb\xb7\xb0\xa4\xd1\xbc\xc6\xa1\x47"};
        static const char *const wealthy[3] = {
            "\xb2\x7b\xaa\xf7\xa1\x47", "\xa6\x73\xb4\xda\xa1\x47",
            "\xc1\x60\xb8\xea\xb2\xa3\xa1\x47"};
        values[0] = stage == 8 ? p->monthly_loss[selected] : p->cash[selected];
        values[1] = stage == 8 ? p->monthly_income[selected] : p->balance[selected];
        values[2] = stage == 8 ? p->monthly_bad_days[selected] : p->assets[selected];
        draw_finance_panel_chunk_at(screen, a, 25, 2, 440, 405, 0);
        text(screen, 320, 370, "\xc0\xf2\xbc\xfa\xad\xec\xa6\x5d\xa1\x47", 16, 0);
        for (i = 0; i < 3; ++i) {
            text(screen, 320, 388 + i * 18, stage == 8 ? unlucky[i] : wealthy[i], 16, 0);
            if (stage == 8 && i == 2) snprintf(number, sizeof(number), "%d\xa4\xd1", values[i]);
            else format_hud_number(number, sizeof(number), values[i], 1);
            text(screen, 560, 388 + i * 18, number, 16, 1);
        }
    }
    banker_face(screen,a,p);
    if (stage == 9 || stage == 19)
        bubble(screen,a,25,stage==19?4:3,stage==19?213:190,stage==19?37:10,20,0,p->display_speech_id);
    else bubble(screen,a,25,1,190,10,0,-30,p->display_speech_id);
    return 1;
}

void draw_monthly_bank_speech(SDL_Surface *screen, const GamePreviewAssets *a,
    const Rich4GameEventPresentationSnapshot *s)
{
    const GameBankSettlementPresentationState *p = &s->presentation->bank_settlement;
    banker_face(screen,a,p);
    bubble(screen, a, 25, 1, 190, 10, 0, -30, p->display_speech_id);
}
