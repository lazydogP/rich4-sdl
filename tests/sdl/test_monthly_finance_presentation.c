#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "app/game_save.h"
#include "app/monthly_finance_flow.h"
#include "platform/sdl1/game_monthly_finance.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/text/runtime_text.h"
#include "resource/mkf.h"
#include "data/monthly_finance_text.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/render/game_panel_render.h"
#include "view/map_renderer.h"

static void expected_chunk(SDL_Surface *screen,const GamePreviewAssets *a,
    int panel,int index,int ax,int ay,int transparent)
{
    Rich4SprSmpChunk c;
    assert(rich4_spr_smp_get_chunk(a->ui.panel_data[panel],a->ui.panel_size[panel],
        &a->ui.panel_spr[panel],(size_t)index,&c)==RICH4_SPR_SMP_OK);
    for (int y=0;y<c.height;++y) for (int x=0;x<c.width;++x) {
        int px=ax-c.x+x,py=ay-c.y+y;
        const unsigned char *src=a->ui.panel_data[panel]+c.data_offset+2*(y*c.width+x);
        uint16_t v=(uint16_t)(src[0]|((uint16_t)src[1]<<8));
        if (px>=0 && px<640 && py>=0 && py<480 && (!transparent||v)) {
            uint16_t *row=(uint16_t *)((unsigned char *)screen->pixels+py*screen->pitch);
            row[px]=(uint16_t)(((v>>10)&31)<<11|((v>>5)&31)<<6|(v&31));
        }
    }
}

static void overlay_matches(SDL_Surface *screen,const GamePreviewAssets *a,
    int panel,int chunk,int ax,int ay,int x,int y,int w,int h)
{
    SDL_Surface *expected=SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,16,0xf800,0x07e0,0x001f,0);
    int checked=0;
    assert(expected!=NULL);
    SDL_FillRect(expected,NULL,0);
    expected_chunk(expected,a,panel,chunk,ax,ay,1);
    for (int row=y;row<y+h;++row) {
        uint16_t *e=(uint16_t *)((unsigned char *)expected->pixels+row*expected->pitch);
        uint16_t *got=(uint16_t *)((unsigned char *)screen->pixels+row*screen->pitch);
        for (int col=x;col<x+w;++col) if (e[col]) { assert(got[col]==e[col]); ++checked; }
    }
    assert(checked>10); SDL_FreeSurface(expected);
}

static int saved_region_matches(SDL_Surface *screen,
    const unsigned char *saved,int x,int y,int w,int h)
{
    for (int row=y;row<y+h;++row)
        if (memcmp(saved+(size_t)row*(size_t)screen->pitch+2*(size_t)x,
                   (unsigned char *)screen->pixels+row*screen->pitch+2*x,
                   2*(size_t)w)!=0) return 0;
    return 1;
}

static void monthly_award_timing(const GamePreviewAssets *a)
{
    /* V3.11 Data[416+2*c+kind] headers and DWORD flags at
     * 0x4759F7+24*c+8 / +20, read through IDA Pro MCP.
     * 0x450CED uses the header speed when flags & 0xF0 == 0.
     * 0x4511B1..0x4511D5 plays BYTE1 rounds, or one when it is zero. */
    static const struct {
        int frames[2];
        unsigned int flags[2];
    } original[12] = {
        {{10,23},{0x0603,0x0003}}, {{10,8},{0x0403,0x0603}},
        {{6,10},{0x0603,0x0603}}, {{6,6},{0x0603,0x0303}},
        {{10,8},{0x0403,0x0603}}, {{16,21},{0x0403,0x0203}},
        {{5,8},{0x0603,0x0603}}, {{21,32},{0x0403,0x0003}},
        {{11,20},{0x0403,0x0003}}, {{8,12},{0x0403,0x0303}},
        {{15,8},{0x0403,0x0603}}, {{16,21},{0x0403,0x0403}}
    };
    static GameSession session;
    for (int c = 0; c < 12; ++c) for (int kind = 0; kind < 2; ++kind) {
        GameBankSettlementPresentationState *p = &session.presentation.bank_settlement;
        int stage = kind ? 8 : 18, speech = kind ? 108 : 122;
        int frames = game_asset_graph_frames(a, &a->ui.monthly_award_data[c][kind],
                                            a->ui.monthly_award_size[c][kind]);
        int delay = game_asset_graph_delay(a, &a->ui.monthly_award_data[c][kind],
                                          a->ui.monthly_award_size[c][kind]);
        int rounds = (original[c].flags[kind] >> 8) & 255;
        int total;
        uint32_t first = 10200;
        if (rounds == 0) rounds = 1;
        total = original[c].frames[kind] * rounds;
        assert(frames == original[c].frames[kind] && delay == 71);
        assert((original[c].flags[kind] & 0xF0) == 0);
        assert(rich4_monthly_animation_repeats(c, kind) == rounds);
        memset(&session, 0, sizeof(session));
        session.state.player_count = 2;
        session.state.players[0].character = c;
        p->active = 1;
        p->stage = p->display_stage = stage - 1;
        p->tick = p->stage_started = 10000;
        p->unlucky_player = p->richest_player = 0;
        rich4_monthly_finance_update(&session, 10100, 1, 0, frames, delay);
        assert(p->stage == stage && p->animation_frame == -1);
        /* 0x437F01: next 100 ms callback enters 0x45144F, which exposes
         * frame zero immediately and holds each frame, including the last. */
        rich4_monthly_finance_update(&session, first - 1, 1, 0, frames, delay);
        assert(p->animation_frame == -1);
        rich4_monthly_finance_update(&session, first, 1, 0, frames, delay);
        assert(p->animation_frame == 0);
        for (int frame = 1; frame <= total; ++frame) {
            uint32_t due = first + (uint32_t)(frame * delay);
            rich4_monthly_finance_update(&session, due - 1, 1, 0, frames, delay);
            assert(p->stage == stage && p->animation_frame == frame - 1);
            rich4_monthly_finance_update(&session, due, 1, 0, frames, delay);
            if (frame < total) {
                assert(p->stage == stage && p->animation_frame == frame);
            } else {
                assert(p->stage == stage + 1 && p->speech_id == speech);
                assert(p->animation_frame == total - 1);
            }
        }
        /* 0x451490..0x451528 timestamps each frame separately: a delayed
         * update must advance only once, with no burst of catch-up frames. */
        p->stage = p->display_stage = stage;
        p->animation_frame = -1;
        p->stage_started = 20000;
        p->speech_id = p->speech_pending = 0;
        rich4_monthly_finance_update(&session, 20100, 1, 0, frames, delay);
        assert(p->animation_frame == 0);
        rich4_monthly_finance_update(&session, 20100 + 5 * delay, 1, 0, frames, delay);
        assert(p->stage == stage && p->animation_frame == 1);
        rich4_monthly_finance_update(&session, 20100 + 6 * delay - 1, 1, 0, frames, delay);
        assert(p->animation_frame == 1);
        rich4_monthly_finance_update(&session, 20100 + 6 * delay, 1, 0, frames, delay);
        assert(p->animation_frame == 2);
        assert(rich4_monthly_finance_release(&session));
        rich4_monthly_finance_update(&session, 20100 + 6 * delay, 1, 0, frames, delay);
        assert(p->stage == stage + 1 && p->animation_frame == 2);
    }
}

int main(void)
{
    static GamePreviewAssets a;
    static GameSession s;
    static GameSession old_save;
    Rich4GameEventPresentationSnapshot snapshot = {&s.state, &s.presentation};
    Rich4Mkf *panel = NULL, *data = NULL;
    SDL_Surface *screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 16,
                                               0xf800, 0x07e0, 0x001f, 0);
    const int resources[] = {13,15,16,17,25,76};
    assert(screen != NULL);
    rich4_sdl_text_set_font_path("assets/fonts/rich4.ttc");
    assert(rich4_mkf_open("assets/original/Panel.mkf", &panel) == RICH4_MKF_OK);
    assert(rich4_mkf_open("assets/original/Data.mkf", &data) == RICH4_MKF_OK);
    for (int i = 0; i < 6; ++i) {
        int r = resources[i];
        assert(rich4_mkf_read_resource(panel, (size_t)r, &a.ui.panel_data[r],
            &a.ui.panel_size[r], NULL) == RICH4_MKF_OK);
        a.ui.has_panel[r] = rich4_spr_smp_probe(a.ui.panel_data[r], a.ui.panel_size[r],
                                               &a.ui.panel_spr[r]);
        if (r == 16 || r == 17)
            assert(game_graph_frame_count_data(a.ui.panel_data[r], a.ui.panel_size[r]) > 0);
        else assert(a.ui.has_panel[r]);
    }
    assert(rich4_mkf_read_resource(data,517,&a.ui.worker_choice_data,
        &a.ui.worker_choice_size,NULL)==RICH4_MKF_OK);
    assert(rich4_spr_smp_probe(a.ui.worker_choice_data,a.ui.worker_choice_size,&a.ui.worker_choice_spr));
    a.ui.has_worker_choice_ui=1;
    for (int c = 0; c < 12; ++c) for (int kind = 0; kind < 2; ++kind) {
        assert(rich4_mkf_read_resource(data, (size_t)(416 + 2*c + kind),
            &a.ui.monthly_award_data[c][kind], &a.ui.monthly_award_size[c][kind],
            NULL) == RICH4_MKF_OK);
        assert(game_graph_frame_count_data(a.ui.monthly_award_data[c][kind],
                                          a.ui.monthly_award_size[c][kind]) > 0);
    }
    monthly_award_timing(&a);
    s.state.active = 1; s.state.player_count = 4; s.state.company_count = 2;
    for (int i = 0; i < 4; ++i) {
        s.state.players[i].play_state = 1;
        s.state.players[i].character = 9 - i;
        s.state.players[i].bank = 10000;
        s.state.player_stock_amount[i][0] = i + 1;
    }
    s.state.stocks[0].f4 = 1; s.state.stock_company_auxiliary[0] = 0;
    s.state.stocks[1].f4 = 2; s.state.stock_company_auxiliary[1] = 1;
    s.state.player_stock_amount[0][1] = 1;
    s.state.company_period_business[0] = 100001;
    s.state.company_period_business[1] = (uint32_t)-1000;
    rich4_periodic_finance_begin(&s, 1000);
    draw_periodic_finance(screen, &a, &snapshot);
    assert(SDL_SaveBMP(screen, "build/monthly-dividend.bmp") == 0);
    s.presentation.periodic_finance.phase = 4;
    s.presentation.periodic_finance.number = 27;
    s.presentation.periodic_finance.winner = 0;
    s.presentation.periodic_finance.pool = 123456;
    s.presentation.periodic_finance.speech_id = 19;
    for (int i=0;i<13;++i) s.state.lottery_owner[i]=1;
    s.state.lottery_owner[20]=2; s.state.lottery_owner[25]=3;
    draw_periodic_finance(screen, &a, &snapshot);
    assert(SDL_SaveBMP(screen, "build/monthly-lottery.bmp") == 0);
    {
        unsigned char *before=malloc((size_t)screen->pitch*screen->h);
        assert(before!=NULL);
        memcpy(before,screen->pixels,(size_t)screen->pitch*screen->h);
        s.state.lottery_owner[12]=0; /* ticket 13 is outside the displayed first twelve */
        draw_periodic_finance(screen,&a,&snapshot);
        assert(memcmp(before,screen->pixels,(size_t)screen->pitch*screen->h)==0);
        free(before);
    }
    s.presentation.periodic_finance.phase=5;
    s.presentation.periodic_finance.speech_id=0;
    draw_periodic_finance(screen,&a,&snapshot);
    {
        size_t size = (size_t)screen->pitch * screen->h;
        unsigned char *before = malloc(size);
        assert(before != NULL);
        memcpy(before, screen->pixels, size);
        s.presentation.periodic_finance.speech_id =
            20 + s.state.players[0].character;
        draw_periodic_finance(screen,&a,&snapshot);
        assert(memcmp(before, screen->pixels, size) == 0);
        free(before);
    }
    overlay_matches(screen,&a,15,6,0,0,20,140,55,170);
    overlay_matches(screen,&a,15,5,505,66,520,180,65,130);
    assert(SDL_SaveBMP(screen,"build/monthly-lottery-winner.bmp")==0);
    s.presentation.periodic_finance.phase=7;
    s.presentation.periodic_finance.winner=-1;
    s.presentation.periodic_finance.speech_id=33;
    draw_periodic_finance(screen,&a,&snapshot);
    assert(SDL_SaveBMP(screen,"build/monthly-lottery-no-winner.bmp")==0);
    s.presentation.periodic_finance.active = 0;
    s.presentation.bank_settlement.active = 1;
    s.presentation.bank_settlement.player_mask = 15;
    s.presentation.bank_settlement.loan_mask = 2;
    s.presentation.bank_settlement.frame=0;
    s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=1;
    s.presentation.bank_settlement.display_speech_id=92;
    draw_bank_settlement_presentation(screen,&a,&snapshot);
    {
        SDL_Surface *e=SDL_CreateRGBSurface(SDL_SWSURFACE,640,480,16,0xf800,0x07e0,0x001f,0);
        assert(e!=NULL);
        expected_chunk(e,&a,25,0,0,0,0);
        expected_chunk(e,&a,25,1,190,10,1);
        draw_big5_text(e,335,80,rich4_monthly_finance_text(92),20,
                      rich4_character_rgb565_color(0x101010),0,0,4);
        for (int y=10;y<210;++y)
            assert(memcmp((unsigned char *)e->pixels+y*e->pitch+520,
                (unsigned char *)screen->pixels+y*screen->pitch+520,440)==0);
        SDL_FreeSurface(e);
    }
    {
        unsigned char *saved=malloc((size_t)screen->pitch*(size_t)screen->h);
        assert(saved!=NULL);
        memcpy(saved,screen->pixels,(size_t)screen->pitch*(size_t)screen->h);
        s.presentation.bank_settlement.face_mode=0;
        s.presentation.bank_settlement.mouth_chunk=22<<8;
        draw_bank_settlement_presentation(screen,&a,&snapshot);
        /* 0x43974D..0x439759 passes (76,160), not the IDA-decompiler
         * artifact (Rect.right,Rect.bottom) = (156,180). */
        assert(saved_region_matches(screen,saved,156,180,80,20));
        s.presentation.bank_settlement.mouth_chunk=0;
        free(saved);
    }
    assert(SDL_SaveBMP(screen,"build/monthly-bank-opening.bmp")==0);
    s.presentation.bank_settlement.display_speech_id=0;
    s.presentation.bank_settlement.frame = 2;
    s.presentation.bank_settlement.stage = 22;
    s.presentation.bank_settlement.display_stage = 2;
    for (int i = 0; i < 4; ++i) {
        s.presentation.bank_settlement.interest[i] = 1000;
        s.presentation.bank_settlement.balance[i] = 11000;
    }
    draw_bank_settlement_presentation(screen, &a, &snapshot);
    assert(SDL_SaveBMP(screen, "build/monthly-bank.bmp") == 0);
    s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=5;
    assert(draw_monthly_award(screen,&a,&snapshot));
    overlay_matches(screen,&a,25,74,324,299,301,269,46,61); /* bottom is exactly 330 */
    {
        unsigned char *saved=malloc((size_t)screen->pitch*(size_t)screen->h);
        assert(saved!=NULL);
        s.presentation.bank_settlement.unlucky_player=3;
        s.presentation.bank_settlement.richest_player=3;
        s.presentation.bank_settlement.display_speech_id=0;

        /* 0x438336/0x438B23: the introduction has no beam.  The beam first
         * appears with the name announcement at 0x438404/0x438BCE. */
        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=6;
        assert(draw_monthly_award(screen,&a,&snapshot));
        memcpy(saved,screen->pixels,(size_t)screen->pitch*(size_t)screen->h);
        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=7;
        assert(draw_monthly_award(screen,&a,&snapshot));
        assert(!saved_region_matches(screen,saved,494,0,146,348));

        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=16;
        assert(draw_monthly_award(screen,&a,&snapshot));
        memcpy(saved,screen->pixels,(size_t)screen->pitch*(size_t)screen->h);
        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=17;
        assert(draw_monthly_award(screen,&a,&snapshot));
        assert(!saved_region_matches(screen,saved,494,0,146,348));

        /* 0x43894E..0x438A14 / 0x439050..0x439115 replace only the host.
         * The beam, reason card and final FLC frame stay on the right. */
        s.presentation.bank_settlement.animation_frame=4;
        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=8;
        assert(draw_monthly_award(screen,&a,&snapshot));
        memcpy(saved,screen->pixels,(size_t)screen->pitch*(size_t)screen->h);
        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=9;
        assert(draw_monthly_award(screen,&a,&snapshot));
        assert(saved_region_matches(screen,saved,494,180,146,300));

        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=18;
        assert(draw_monthly_award(screen,&a,&snapshot));
        memcpy(saved,screen->pixels,(size_t)screen->pitch*(size_t)screen->h);
        s.presentation.bank_settlement.stage=s.presentation.bank_settlement.display_stage=19;
        assert(draw_monthly_award(screen,&a,&snapshot));
        assert(saved_region_matches(screen,saved,494,180,146,300));
        free(saved);
    }
    s.presentation.bank_settlement.stage = 18;
    s.presentation.bank_settlement.display_stage = 18;
    s.presentation.bank_settlement.richest_player = 0;
    s.presentation.bank_settlement.assets[0] = 999999;
    s.presentation.bank_settlement.cash[0] = 123456;
    assert(draw_monthly_award(screen, &a, &snapshot));
    assert(SDL_SaveBMP(screen, "build/monthly-award.bmp") == 0);
    /* Read-only compatibility check of the user's existing format-4 save.
     * Absence of this optional local save does not affect the resource test. */
    {
        FILE *file = fopen("saves/SAVE0.R4S", "rb");
        if (file != NULL) {
            unsigned char header[8];
            assert(fread(header, 1, 8, file) == 8); fclose(file);
            if (header[4] == 4) {
                assert(rich4_game_save_read("saves/SAVE0.R4S", 0, &old_save));
                assert(old_save.state.players[0].monthly_income == 0);
                puts("format-4 save compatibility: ok");
            }
        }
    }
    for (int i = 0; i < 6; ++i) free(a.ui.panel_data[resources[i]]);
    free(a.ui.worker_choice_data);
    for (int c = 0; c < 12; ++c) for (int k = 0; k < 2; ++k)
        free(a.ui.monthly_award_data[c][k]);
    rich4_mkf_close(panel); rich4_mkf_close(data);
    rich4_sdl_text_shutdown(); SDL_FreeSurface(screen);
    puts("monthly finance original-resource presentation: ok");
    return 0;
}
