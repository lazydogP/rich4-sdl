/* Original selection boundaries: docs/evidence/news-selection.md. */
static void news_ordinary_confinement_pool(void)
{
    static GameState s;
    GameEventQueue q;
    const int order[] = {0,1,2,3,24};
    base_state(&s,401);
    front_ids(s.news_event_order,GAME_NEWS_EVENT_COUNT,order,5);
    s.special_player_confinement[0] = GAME_CONFINEMENT_PRISON;
    s.special_player_confinement[1] = GAME_CONFINEMENT_HOSPITAL;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 24 && s.news_event_cursor == 5);
    assert(s.special_player_confinement[0] == GAME_CONFINEMENT_PRISON);
    assert(s.special_player_confinement[1] == GAME_CONFINEMENT_HOSPITAL);

    base_state(&s,402);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,1);
    s.players[1].days_in_prison = 0x80;
    s.players[1].confinement_actor_type = GAME_CONFINEMENT_PRISON;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 1 && s.players[1].days_in_prison == 3);
    assert(s.special_event_target_mask == (1u << 17));
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q) == GAME_SPECIAL_EVENT_APPLIED);
    s.news_event_cursor = 0;
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,0);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.players[1].days_in_prison == 0x80);
    assert(s.players[1].confinement_actor_type == GAME_CONFINEMENT_NONE);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q) == GAME_SPECIAL_EVENT_APPLIED);
    s.news_event_cursor = 0;
    {
        const int next[] = {1,24};
        front_ids(s.news_event_order,GAME_NEWS_EVENT_COUNT,next,2);
    }
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 24 && s.players[1].days_in_prison == 0x80);
}

static void news_company_candidates_and_passive_targets(void)
{
    static GameState s;
    GameEventQueue q;
    int mode;
    /* Existence of an available operator permits selection of a different,
     * confined operator, even on a company without a valid stock link. */
    base_state(&s,403);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,29);
    s.company_count = 2;
    s.company_stock_index[0] = s.company_stock_index[1] = -1;
    s.company_owner[0] = 0;
    s.company_owner[1] = 1;
    s.players[0].days_in_prison = 2;
    s.ai_rng_state = 0; /* First original rand = 0: first of two companies. */
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 29);
    assert((s.special_event_target_mask & 0xffu) == 1u);
    assert(((s.special_event_target_mask >> 8) & 0xffffu) == 1u);
    assert(s.ai_rng_state == 12345u);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].days_in_prison == 7);

    for (mode = 0; mode < 3; mode++) {
        base_state(&s,404);
        first(s.news_event_order,GAME_NEWS_EVENT_COUNT,29);
        s.company_count = 1;
        s.company_stock_index[0] = -1;
        s.company_owner[0] = 1;
        s.players[1].card_inventory[0] = mode == 0 ? 21 : 19;
        s.players[1].play_state = mode == 1 ? 2 : 1;
        s.players[1].relation_balance[0] = 50;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
        assert(rich4_special_event_commit_with_target(&s,GAME_SPECIAL_DEFENSE_NONE,
            -1,&q,0) == GAME_SPECIAL_EVENT_APPLIED);
        if (mode == 0) {
            assert(s.players[1].days_in_prison == 0 && !s.players[1].card_inventory[0]);
        } else if (mode == 1) {
            assert(s.players[0].days_in_prison == 5 && s.players[1].days_in_prison == 0);
            assert(!s.players[1].card_inventory[0]);
        } else {
            assert(s.players[1].days_in_prison == 5 && s.players[1].card_inventory[0] == 19);
        }
    }
    base_state(&s,411);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,29);
    s.company_count = 13;
    for (mode = 0; mode < 13; mode++) {
        s.company_stock_index[mode] = -1;
        s.company_owner[mode] = mode == 12 ? 1 : 0;
    }
    s.ai_rng_state = 7; /* Original first rand % 13 = 12. No 12-stock cap. */
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(((s.special_event_target_mask >> 8) & 0xffffu) == 13);
    assert((s.special_event_target_mask & 0xffu) == 2u);
}

static void news_company_profit_pool(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s,405);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,35);
    s.company_count = 3;
    s.company_stock_index[0] = s.company_stock_index[1] = s.company_stock_index[2] = -1;
    s.company_period_business[0] = 10000;
    s.company_period_business[1] = 10001;
    s.company_period_business[2] = UINT32_MAX; /* -1 is not positive profit. */
    s.company_total_business[1] = 7;
    s.stock_company_value[0] = 1000000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 35 && s.special_event_target_mask == 2);
    assert(s.company_period_business[1] == 20002 && s.company_total_business[1] == 20009);
    assert(s.company_period_business[0] == 10000 && s.company_period_business[2] == UINT32_MAX);
    assert(s.stock_company_value[0] == 1000000);

    base_state(&s,406);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,33);
    s.company_count = 1;
    s.company_stock_index[0] = -1;
    s.company_period_business[0] = 5000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_target_mask == 1 && (int32_t)s.company_period_business[0] == -5000);
}

static void news_phase1_center_and_anchor_area(void)
{
    static GameState s;
    GameEventQueue q;
    uint32_t rng;
    const GameNewsAreaAnchor anchors[] = {
        {2001,120,120}, {2001,120,120}, /* One cell is processed only once. */
        {2002,320,220}, {6001,220,220}, /* Upper boundary and companies excluded. */
        {0x8001,200,200}, {0x8002,200,200}
    };
    base_state(&s,407);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,4);
    s.land_count = 2;
    s.land_owner[0] = 0; s.land_owner[1] = 1;
    s.land_level[0] = s.land_level[1] = 3;
    s.ai_rng_state = 0;
    rng = s.ai_rng_state;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(s.ai_rng_state == rng && s.special_event_target_mask == 0);
    assert(rich4_special_event_prepare_news_phase1(&s));
    assert(s.ai_rng_state == 12345 && (s.special_event_target_mask & 0xffffu) == 1);
    assert(rich4_special_event_prepare_news_phase1(&s) && s.ai_rng_state == 12345);
    assert(rich4_special_event_apply_news_area(&s,anchors,6));
    assert(s.land_owner[0] == -1 && s.land_owner[1] == 1);
    assert((s.special_event_target_mask & 0xff0000u) == 0x30000u);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].days_in_hospital == 3 && s.players[1].days_in_hospital == 3);

    base_state(&s,408);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,20);
    s.land_count = 2;
    s.land_level[0] = s.land_level[1] = 3;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_apply_news_area(&s,anchors,6));
    assert(s.land_level[0] == 2 && s.land_level[1] == 3);
    assert(!(s.special_event_target_mask & 0xff0000u));
}

static void news_same_name_group_and_rank_ties(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s,409);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,18);
    s.land_count = 3;
    s.land_group[0] = s.land_group[1] = 7; s.land_group[2] = 8;
    s.land_owner[0] = 0; s.land_owner[1] = s.land_owner[2] = 1;
    s.land_level[0] = 0; s.land_level[1] = s.land_level[2] = 3;
    s.ai_rng_state = 0;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
    assert((s.special_event_target_mask & 0xffffu) == 1 && s.land_level[1] == 3);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_level[1] == 2 && s.land_level[2] == 3);

    base_state(&s,410);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,10);
    s.player_stock_amount[0][0] = 5; s.player_stock_amount[1][1] = 5;
    s.stocks[0].f20 = 1; s.stocks[1].f20 = 1000;
    {
        uint32_t before = s.ai_rng_state;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q) == GAME_SPECIAL_EVENT_PREPARED);
        assert((s.special_event_target_mask >> 24) == 1 && s.ai_rng_state == before);
    }
}

/* Callback effects: original functions and binary constants are recorded in
 * rich4/docs/evidence/news-selection-ida.json. */
static void news_finance_waits_for_phase1(void)
{
    static GameState s, before;
    GameEventQueue q;
    int id;
    for (id=8;id<=10;id++) {
        base_state(&s,420+id);
        first(s.news_event_order,GAME_NEWS_EVENT_COUNT,id);
        s.land_count=1; s.land_owner[0]=id==8 ? 1 : 0;
        s.player_stock_amount[1][0]=1;
        s.price_index=2;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        assert(s.players[1].cash==200 && s.players[1].monthly_income==0);
        assert(s.special_event_player_amount[1]==(id==9 ? 10000 : 20000));
        assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
        assert(s.players[1].cash==200+(id==9 ? 10000 : 20000));
    }
    base_state(&s,432);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,11);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.players[0].cash==1000 && s.players[1].cash==200 && s.lottery_pool==0);
    assert(s.special_event_player_amount[0]==50 && s.special_event_player_amount[1]==10);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].cash==950 && s.players[1].cash==190 && s.lottery_pool==60);
    assert(s.players[0].monthly_loss==50 && s.players[1].monthly_loss==10);

    base_state(&s,433);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,12);
    s.price_index=3; s.land_count=1; s.facility_count=1;
    s.land_owner[0]=0; s.land_type[0]=1; s.land_level[0]=3;
    s.land_price[0]=100; s.land_price_per_level[0]=50;
    s.facility_owner[0]=0; s.facility_level[0]=2;
    s.facility_price[0]=70; s.facility_price_per_level[0]=15;
    s.fixed_asset_value[0]=12345; /* Assessment scans records, ignoring this cache. */
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_player_amount[0]==51 && s.players[0].cash==1000);
    before=s;
    {
        GameEvent filler=q.events[q.head];
        while (q.count<GAME_EVENT_QUEUE_CAPACITY)
            assert(rich4_game_event_queue_push(&q,&filler));
    }
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)!=GAME_SPECIAL_EVENT_APPLIED);
    assert(memcmp(&s,&before,sizeof(s))==0);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].cash==949 && s.players[0].monthly_loss==51 && s.lottery_pool==51);

    base_state(&s,434);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,13);
    s.players[0].cash=20000000; s.price_index=2;
    for (id=0;id<GAME_STOCK_COUNT;id++) {
        s.player_stock_amount[0][id]=1;
        s.stocks[id].f20=id==0 ? 100000000.0f : 7.0f;
    }
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    /* float32 accumulator ends at 100000088, not double 100000077. */
    assert(s.special_event_player_amount[0]==10000008 && s.players[0].cash==20000000);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].cash==9999992 && s.players[0].monthly_loss==10000008);
    assert(s.lottery_pool==10000008 && s.system_cash==10000008);
}

static void news_rewards_use_dword_add(void)
{
    static GameState s;
    GameEventQueue q;
    int id;
    for (id=8;id<=10;id++) {
        base_state(&s,430+id);
        first(s.news_event_order,GAME_NEWS_EVENT_COUNT,id);
        s.land_count=1; s.land_owner[0]=id==8 ? 1 : 0;
        s.player_stock_amount[1][0]=1;
        s.players[1].cash=INT32_MAX-100;
        s.players[1].monthly_income=INT32_MAX-50;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        assert(s.players[1].cash==INT32_MAX-100);
        assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
        /* 0x41D407 and 0x41D418: retain the low 32 bits of both ADDs. */
        assert(s.players[1].cash==(id==9 ? -2147478749 : -2147473749));
        assert(s.players[1].monthly_income==(id==9 ? -2147478699 : -2147473699));
        assert(s.special_event_phase==GAME_SPECIAL_PHASE_IDLE && !s.game_over);
        assert(s.players[0].cash==1000 && s.players[0].monthly_income==0);
    }

    base_state(&s,441);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,23);
    s.players[0].bank=INT32_MAX-100;
    s.players[0].monthly_income=INT32_MAX-50;
    s.players[1].loan=1; s.players[1].bank=INT32_MAX;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_player_amount[0]==214748354);
    assert(s.players[0].bank==-1932735395 && s.players[0].monthly_income==-1932735345);
    assert(s.players[1].bank==INT32_MAX && s.players[1].monthly_income==0);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].bank==-1932735395); /* Phase 1 must not pay twice. */

    base_state(&s,442);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,8);
    s.land_count=1; s.land_owner[0]=1; s.price_index=INT32_MAX;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_player_amount[1]==-10000); /* phase-0 IMUL */
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].cash==-9800 && s.players[1].monthly_income==-10000);
}

static void news_tax_bankruptcy_and_ending_order(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s,435);
    s.player_count=3; s.players[2].play_state=2;
    s.players[2].cash=500; s.players[2].linked_player=-1;
    s.players[1].cash=10; s.players[1].bank=20;
    s.players[1].attached_timed_bomb_slot=-1;
    s.land_count=2; s.land_owner[0]=1; s.land_price[0]=10000;
    s.land_owner[1]=2; s.land_price[1]=2000;
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,12);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_player_amount[1]==500 && s.players[1].cash==10);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].play_state==0 && s.players[1].monthly_loss==30);
    assert(s.players[2].cash==400 && !s.game_over);
    assert(s.lottery_pool==130 && s.players[0].play_state!=0);

    base_state(&s,436);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,12);
    s.players[0].cash=10; s.players[0].bank=20;
    s.land_count=2; s.land_owner[0]=0; s.land_price[0]=10000;
    s.land_owner[1]=1; s.land_price[1]=2000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_BANKRUPTCY);
    assert(s.game_over && s.players[0].play_state==0);
    assert(s.players[1].cash==200 && s.players[1].monthly_loss==0);
    assert(s.lottery_pool==30);
}

static void news_quotes_change_immediately(void)
{
    static const int ids[]={24,25,30,31,32,33,34,35};
    static GameState s;
    GameEventQueue q;
    int k,i;
    for (k=0;k<8;k++) {
        int rise=ids[k]==25 || ids[k]==31 || ids[k]==35;
        base_state(&s,440+k);
        first(s.news_event_order,GAME_NEWS_EVENT_COUNT,ids[k]);
        s.company_count=1; s.company_stock_index[0]=2;
        s.company_period_business[0]=20000;
        s.stock_history_cursor=0;
        for (i=0;i<GAME_STOCK_COUNT;i++) {
            s.stocks[i].f16=100; s.stocks[i].f20=95; s.stocks[i].f6=2;
            s.stock_history[i][143]=42;
        }
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        assert(s.stock_history_cursor==0);
        for (i=0;i<GAME_STOCK_COUNT;i++) {
            int affected=ids[k]<30 || i==2;
            assert(s.stocks[i].f20==(affected ? (rise ? 110.0f : 90.0f) : 95.0f));
            assert(s.stock_history[i][143]==(affected ? s.stocks[i].f20 : 42.0f));
            if (affected) assert(s.stocks[i].f28==(rise ? 10.0f : -10.0f));
        }
    }
    base_state(&s,449);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,27);
    s.ai_rng_state=0; s.stock_history_cursor=7;
    s.stocks[0].f16=100; s.stocks[0].f20=105;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_target_mask==1 && s.stocks[0].f6==15);
    assert(s.stocks[0].f20==100 && s.stock_history[0][6]==100 && s.stock_history_cursor==7);
}

static void news_price_word_and_destroyed_runtime(void)
{
    static GameState s;
    GameEventQueue q;
    int id;
    for (id=6;id<=14;id+=8) {
        base_state(&s,450+id);
        first(s.news_event_order,GAME_NEWS_EVENT_COUNT,id);
        s.land_count=2; s.land_group[0]=s.land_group[1]=5;
        s.land_price[0]=65000; s.land_price[1]=90;
        s.ai_rng_state=0;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        assert(s.land_price[0]==65000 && s.land_price[1]==90);
        assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
        assert(s.land_price[0]==(id==6 ? 18964 : 45499));
        assert(s.land_price[1]==(id==6 ? 117 : 62));
    }
    base_state(&s,465);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,14);
    s.facility_count=1; s.facility_price[0]=90;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.facility_price[0]==62);
    for (id=4;id<=19;id+=(id==4 ? 1 : 14)) {
        const GameNewsAreaAnchor anchors[]={{2001,220,220},{0x9100,210,210},{0xa500,319,319},{0x9200,320,220}};
        base_state(&s,470+id); first(s.news_event_order,GAME_NEWS_EVENT_COUNT,id);
        s.land_count=1; s.land_level[0]=2; s.land_runtime_value[0]=1234;
        s.tool_placed_objects[0].kind=16; s.tool_placed_objects[0].node=1;
        s.tool_placed_objects[1].kind=16; s.tool_placed_objects[1].node=2;
        s.tool_placed_objects[20].kind=18; s.tool_placed_objects[20].node=3;
        s.tool_placed_objects[20].attached_player=2;
        s.players[1].attached_timed_bomb_slot=20;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        if (id==4) {
            int cursor=-1; uint16_t raw;
            assert(rich4_special_event_prepare_news_phase1(&s));
            assert(rich4_special_event_news_area_next_raw(anchors,4,&cursor,&raw) && raw==0x9100);
            assert(rich4_special_event_apply_news_area(&s,anchors,4));
            assert(s.tool_placed_objects[0].kind==0 && s.tool_amount[1]==1);
            assert(s.tool_placed_objects[1].kind==16);
            assert(s.tool_placed_objects[20].kind==0 && s.tool_amount[3]==1);
            assert(s.players[1].attached_timed_bomb_slot==-1);
            assert(rich4_special_event_apply_news_area(&s,anchors,4) && s.tool_amount[1]==1);
        }
        assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
        assert(s.land_runtime_value[0]==0);
    }
}

static void news_original_numeric_reference(void)
{
#if LDBL_MANT_DIG == 64
    /* Independent native x87 reference for the portable limb arithmetic. */
    volatile double rise=1.3, fall=0.7, tax=0.05;
    uint32_t value;
    for (value=0;value<=65535;value++) {
        volatile long double a=(long double)value * rise;
        volatile long double b=(long double)value * fall;
        assert(rich4_special_event_original_scale((int)value,13,10)==(int)a);
        assert(rich4_special_event_original_scale((int)value,7,10)==(int)b);
    }
    for (value=0;value<2147483000u;value+=100003u) {
        volatile long double result=(long double)value * tax;
        float market_value=(float)value;
        volatile long double stock_result=(long double)market_value * tax;
        assert(rich4_special_event_original_scale((int)value,5,100)==(int)result);
        assert(rich4_special_event_original_stock_tax(market_value)==(int)stock_result);
    }
#endif
}

static void news_selection_regressions(void)
{
    static GameState s;
    static const uint8_t expected[36] = {
        33,7,4,9,28,15,17,18,24,19,1,10,12,13,35,30,25,2,
        14,11,31,32,6,22,29,5,26,16,3,20,34,27,8,23,0,21
    };
    base_state(&s,311);
    assert(memcmp(s.news_event_order,expected,sizeof(expected)) == 0);
    news_original_numeric_reference();
    news_finance_waits_for_phase1();
    news_rewards_use_dword_add();
    news_tax_bankruptcy_and_ending_order();
    news_quotes_change_immediately();
    news_price_word_and_destroyed_runtime();
    news_ordinary_confinement_pool();
    news_company_candidates_and_passive_targets();
    news_company_profit_pool();
    news_phase1_center_and_anchor_area();
    news_same_name_group_and_rank_ties();
}
