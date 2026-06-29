#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/periodic_finance.h"
#include "core/stock.h"
#include "core/rng.h"
#include "core/turn_rotation.h"
#include "core/ledger.h"
#include "core/game_event_payload.h"
#include "app/monthly_finance_flow.h"

static void setup(GameState *s)
{
    memset(s, 0, sizeof(*s));
    s->active = 1; s->player_count = 4; s->company_count = 2;
    s->price_index = 1;
    for (int i = 0; i < 4; ++i) { s->players[i].play_state = 1; s->players[i].bank = 100; }
    s->stocks[0].f4 = 1; s->stock_company_auxiliary[0] = 0;
    s->stocks[1].f4 = 2; s->stock_company_auxiliary[1] = 1;
}

static void dividends(void)
{
    GameState s;
    Rich4CompanyDividend d;
    GameEventQueue events;
    setup(&s);
    s.players[3].play_state = 0;
    for (int i = 0; i < 4; ++i) s.player_stock_amount[i][0] = 1;
    s.company_period_business[0] = 100;
    s.company_total_business[0] = 900;
    s.company_period_business[1] = 70;
    s.stocks[0].f6 = 10; s.stock_market_closed = 10;
    assert(rich4_company_dividend_prepare(&s, &d));
    assert(d.amount[0][0] == 33 && d.amount[0][1] == 33 && d.amount[0][2] == 33);
    assert(d.amount[0][3] == 0 && s.company_period_business[0] == 0);
    assert(s.company_total_business[0] == 900 && s.company_period_business[1] == 70);
    assert(s.players[0].bank == 100); /* not paid while report is open */
    rich4_game_event_queue_reset(&events);
    assert(rich4_company_dividend_commit(&s, &d, &events));
    assert(s.players[0].bank == 133 && s.players[0].monthly_income == 0);

    s.company_period_business[0] = (uint32_t)-100;
    assert(rich4_company_dividend_prepare(&s, &d));
    assert(d.amount[0][0] == -33); /* negative CHP rounds UP towards zero */
    assert(rich4_company_dividend_commit(&s, &d, &events));
    assert(s.players[0].bank == 100 && s.players[0].monthly_loss == 0);

    setup(&s);
    s.player_stock_amount[0][0] = 1;
    s.company_period_business[0] = 250;
    assert(rich4_company_dividend_prepare(&s, &d) && d.total[0] == 250);
    assert(rich4_company_dividend_commit(&s, &d, &events));
    assert(s.players[0].bank == 350);
    s.company_period_business[0] = (uint32_t)-400;
    s.players[0].cash = 51;
    assert(rich4_company_dividend_prepare(&s, &d));
    assert(rich4_company_dividend_commit(&s, &d, &events));
    assert(s.players[0].bank == 0 && s.players[0].cash == 1);
    s.company_period_business[0] = (uint32_t)-2;
    assert(rich4_company_dividend_prepare(&s, &d));
    rich4_game_event_queue_reset(&events);
    assert(rich4_company_dividend_commit(&s, &d, &events));
    assert(s.players[0].play_state == 0);
}

static void lottery(void)
{
    GameState s, before;
    GameEventQueue events;
    Rich4Rng rng = {1};
    int number, winner;
    assert(rich4_rng_original_state_next(&rng) == 16838);
    assert(rich4_rng_original_state_next(&rng) == 5758);
    setup(&s);
    s.ai_rng_state = 1;
    s.lottery_pool = 90000;
    before = s;
    assert(!rich4_lottery_draw(&s, &number, &winner));
    assert(memcmp(&before, &s, sizeof(s)) == 0);
    for (int i = 0; i < 10; ++i) s.lottery_owner[i] = 1;
    for (int i = 10; i < 20; ++i) s.lottery_owner[i] = 2;
    /* First ANSI rand is 16838: all-36 branch gives 27, unowned. */
    assert(rich4_lottery_draw(&s, &number, &winner));
    assert(number == 27 && winner == -1);
    before = s;
    rich4_game_event_queue_reset(&events);
    assert(rich4_lottery_claim(&s, number, &events));
    assert(memcmp(&before, &s, sizeof(s)) == 0);
    s.lottery_owner[20] = 1; /* one player now has 11 */
    s.ai_rng_state = 1;
    assert(rich4_lottery_draw(&s, &number, &winner));
    assert(number == 18 && winner == 1); /* any sold ticket can win */
    assert(s.players[1].cash == 0 && s.lottery_pool == 90000);
    assert(rich4_lottery_claim(&s, number, &events));
    assert(s.players[1].cash == 90000 && s.players[1].monthly_income == 90000);
    assert(s.lottery_pool == 0 && !rich4_lottery_has_tickets(&s));
}

static void timing(void)
{
    GameSession session;
    GameEventQueue events;
    memset(&session, 0, sizeof(session));
    setup(&session.state);
    session.state.player_stock_amount[0][0] = 1;
    session.state.company_period_business[0] = 100;
    rich4_game_event_queue_reset(&events);
    rich4_periodic_finance_begin(&session, 1000);
    assert(session.presentation.periodic_finance.active);
    assert(rich4_periodic_finance_update(&session, 3999, 0, 0, 0, 71, &events));
    assert(session.state.players[0].bank == 100);
    assert(rich4_periodic_finance_update(&session, 4000, 0, 0, 0, 71, &events));
    assert(session.state.players[0].bank == 200);
    assert(!rich4_periodic_finance_update(&session, 4001, 0, 0, 0, 71, &events));
    assert(!session.presentation.periodic_finance.active);

    session.presentation.bank_settlement.active = 1;
    session.presentation.bank_settlement.player_mask = 15;
    session.presentation.bank_settlement.tick = 5000;
    session.state.players[0].monthly_loss = 1000;
    session.state.players[1].monthly_loss = 100;
    rich4_monthly_finance_begin(&session);
    assert(session.presentation.bank_settlement.unlucky_player == 0);
    for (unsigned int tick = 5100; tick < 12000; tick += 100) {
        session.presentation.bank_settlement.speech_pending = 0;
        assert(rich4_monthly_finance_update(&session, tick, 0, 0, 0, 71));
        if (!session.presentation.bank_settlement.active) break;
    }
    assert(!session.presentation.bank_settlement.active);
    assert(session.state.players[0].monthly_loss == 0);
}

static void day_trigger_and_rollback(void)
{
    GameState s, before;
    GameEventQueue events;
    GameEvent event;
    TurnAdvanceResult result;
    int found = 0;
    setup(&s);
    s.calendar_start_year = 2000; s.calendar_start_month = 1;
    s.calendar_start_day = 14; s.stock_market_closed = 20;
    s.turn_rotation_index = 3;
    for (int i = 0; i < 4; ++i) s.special_player_f10[i] = 1;
    s.ai_rng_state = 123;
    rich4_game_event_queue_reset(&events);
    assert(rich4_turn_advance_once_events(&s, &result, &events));
    assert(s.elapsed_days == 1 && s.ai_rng_state == 123);
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_PERIODIC_FINANCE_REQUESTED) ++found;
    assert(found == 1); /* closure never suppresses the fifteenth's report */
    s.turn_rotation_index = 3; before = s;
    event = rich4_game_event_player_cash_changed_from_fields(
        (Rich4GameEventFields){.player=0});
    for (int i = 0; i < GAME_EVENT_QUEUE_CAPACITY; ++i)
        assert(rich4_game_event_queue_push(&events, &event));
    assert(!rich4_turn_advance_once_events(&s, &result, &events));
    assert(memcmp(&s, &before, sizeof(s)) == 0);
    rich4_game_event_queue_reset(&events);
    s.players[0].cash = 1000;
    assert(rich4_ledger_pay(&s, 0, rich4_ledger_system(), 75, 0, &events)
           == GAME_PAYMENT_COMMITTED);
    assert(s.lottery_pool == 75 && s.players[0].monthly_loss == 75);
}

static void ui_timing(void)
{
    static GameSession s;
    GameEventQueue events;
    GamePeriodicFinancePresentationState *p;
    GameBankSettlementPresentationState *b;
    memset(&s,0,sizeof(s)); setup(&s.state);
    rich4_game_event_queue_reset(&events);
    for (int i=0;i<36;++i) s.state.lottery_owner[i]=1;
    p=&s.presentation.periodic_finance;
    p->active=1; p->phase=3; p->tick=1000;
    for (int tick=1;tick<=60;++tick) {
        rich4_periodic_finance_update(&s,1000+50*tick,1,0,42,71,&events);
        assert(p->phase==3);
        assert(p->animation_frame==(tick<20?0:tick-19));
    }
    rich4_periodic_finance_update(&s,4050,1,0,42,71,&events);
    assert(p->phase==4 && p->speech_id==19);
    p->phase=5; p->tick=5000; p->ticks=p->animation_frame=0;
    p->speech_id=p->speech_pending=0;
    for (int tick=1;tick<=66;++tick) {
        rich4_periodic_finance_update(&s,5000+50*tick,1,0,37,1,&events);
        assert(p->phase==5 && p->animation_frame==(tick<=30?0:tick-30));
    }
    rich4_periodic_finance_update(&s,8350,1,0,37,1,&events);
    assert(p->phase==6 && p->speech_id==32);
    p->phase=RICH4_FINANCE_NO_WINNER_REVEAL; p->stage_started=9000;
    p->speech_id=p->speech_pending=0; p->winner=-1;
    rich4_periodic_finance_update(&s,9499,1,0,42,71,&events);
    assert(p->phase==RICH4_FINANCE_NO_WINNER_REVEAL && !p->speech_pending);
    rich4_periodic_finance_update(&s,9500,1,0,42,71,&events);
    assert(p->phase==7 && p->speech_pending && p->speech_id==33);

    p->active=0; b=&s.presentation.bank_settlement;
    b->active=1; b->stage=b->display_stage=18; b->richest_player=0;
    s.state.players[0].character=9;
    b->stage_started=b->tick=10000; b->animation_frame=-1;
    rich4_monthly_finance_update(&s,10099,1,0,8,71);
    assert(b->animation_frame==-1);
    rich4_monthly_finance_update(&s,10100,1,0,8,71);
    assert(b->animation_frame==0);
    for (int frame=1;frame<=31;frame++)
        rich4_monthly_finance_update(&s,10100+frame*71,1,0,8,71);
    assert(b->stage==18 && b->animation_frame==31); /* four full cycles */
    rich4_monthly_finance_update(&s,10100+32*71,1,0,8,71);
    assert(b->stage==19 && b->speech_id==122);
    b->stage=b->display_stage=2; b->face_mode=1;
    b->eye_state=1; b->mouth_ticks=0;
    rich4_monthly_banker_update(&s,0); assert(b->eye_chunk==28);
    rich4_monthly_banker_update(&s,0); assert(b->eye_chunk==27);
    rich4_monthly_banker_update(&s,0); assert(b->eye_chunk==28);
    rich4_monthly_banker_update(&s,0); assert(b->eye_chunk==25 && b->eye_state==0);
}

int main(void)
{
    dividends(); lottery(); timing(); day_trigger_and_rollback(); ui_timing();
    puts("monthly finance original rules: ok");
    return 0;
}
