#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

#include "core/auction.h"
#include "core/card.h"
#include "core/game_event_payload.h"
#include "core/special_event.h"
#include "core/special_event_internal.h"

static void base_state(GameState *s, unsigned int seed)
{
    Rich4Rng rng;
    memset(s,0,sizeof(*s));
    s->active=1; s->player_count=2; s->current_player=0;
    s->players[0].play_state=1; s->players[1].play_state=2;
    s->players[0].linked_player=s->players[1].linked_player=-1;
    s->players[0].cash=1000; s->players[0].bank=1000;
    s->players[1].cash=200; s->players[1].bank=500;
    s->price_index=1; s->stage=0; s->map=0;
    s->prison_node=70; s->hospital_node=80;
    rich4_rng_state_seed(&rng,seed);
    rich4_special_event_sequences_init(s,&rng);
}

static void first(uint8_t *order, int count, int id)
{
    int i;
    for (i=0;i<count;i++) if (order[i]==id) {
        uint8_t t=order[0]; order[0]=order[i]; order[i]=t; return;
    }
    assert(0);
}

static void front_ids(uint8_t *order, int count,
                      const int *ids, int id_count)
{
    int slot;
    for (slot = 0; slot < id_count; slot++) {
        int i;
        for (i = slot; i < count; i++)
            if (order[i] == ids[slot]) {
                uint8_t t = order[slot];
                order[slot] = order[i];
                order[i] = t;
                break;
            }
        assert(i < count);
    }
}

static void deterministic_sequences(void)
{
    static GameState a,b;
    base_state(&a,311); base_state(&b,311);
    assert(memcmp(a.news_event_order,b.news_event_order,GAME_NEWS_EVENT_COUNT)==0);
    assert(memcmp(a.fortune_event_order,b.fortune_event_order,GAME_FORTUNE_EVENT_COUNT)==0);
    assert(a.ai_rng_state==b.ai_rng_state);
    {
        uint8_t order[3]={2,0,1}, eligible[3]={0,0,0}; int cursor=1,id=-1;
        assert(!rich4_special_event_select(order,3,&cursor,eligible,&id));
        assert(cursor==1 && id==-1);
        eligible[1]=1;
        assert(rich4_special_event_select(order,3,&cursor,eligible,&id));
        assert(id==1 && cursor==0);
    }
}

static void automatic_god_defense(void)
{
    GameState s;
    uint32_t before_rng;
    GameSpecialDefenseOutcome defense;

    base_state(&s, 314);
    s.special_event_phase = GAME_SPECIAL_PHASE_PREPARED;
    s.special_event_family = GAME_SPECIAL_EVENT_FORTUNE;
    s.special_event_actor = 0;
    s.special_event_resolved_id = 2;
    s.players[0].god_investment_modifier = 150;
    assert(rich4_special_event_default_defense(&s) ==
           GAME_SPECIAL_DEFENSE_BLOCKED);

    s.players[0].god_investment_modifier = -60;
    assert(rich4_special_event_default_defense(&s) ==
           GAME_SPECIAL_DEFENSE_DOUBLED);

    s.players[0].god_investment_modifier = 60;
    before_rng = s.ai_rng_state;
    defense = rich4_special_event_default_defense(&s);
    assert(defense == GAME_SPECIAL_DEFENSE_NONE ||
           defense == GAME_SPECIAL_DEFENSE_BLOCKED);
    assert(s.ai_rng_state != before_rng);

    s.special_event_resolved_id = 32;
    s.players[0].god_investment_modifier = 150;
    s.players[0].god_land_modifier = 150;
    assert(rich4_special_event_default_defense(&s) ==
           GAME_SPECIAL_DEFENSE_BLOCKED);

    s.special_event_resolved_id = 8;
    before_rng = s.ai_rng_state;
    assert(rich4_special_event_default_defense(&s) ==
           GAME_SPECIAL_DEFENSE_BLOCKED);
    assert(s.ai_rng_state == before_rng);
}

static void original_eligibility_filters(void)
{
    static GameState s;
    GameEventQueue q;
    {
        static const int order[] = {
            0, 2, 4, 7, 8, 10, 16, 28, 29, 35, 24
        };
        base_state(&s, 312);
        s.players[0].traffic_method = 2;
        s.players[1].traffic_method = 2;
        front_ids(s.news_event_order, GAME_NEWS_EVENT_COUNT,
                  order, (int)(sizeof(order) / sizeof(order[0])));
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s, 0, &q) ==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_logical_id == 24 &&
               s.news_event_cursor ==
                   (int)(sizeof(order) / sizeof(order[0])));
    }
    {
        static const int order[] = {
            0, 1, 5, 8, 9, 10, 12, 14, 33, 2
        };
        base_state(&s, 313);
        s.stage = 1;
        s.players[0].traffic_method = 3;
        front_ids(s.fortune_event_order, GAME_FORTUNE_EVENT_COUNT,
                  order, (int)(sizeof(order) / sizeof(order[0])));
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_fortune(&s, 0, &q) ==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_logical_id == 2 &&
               s.fortune_event_cursor ==
                   (int)(sizeof(order) / sizeof(order[0])));
    }
}

static void news15_requires_built_land(void)
{
    static GameState s;
    static const int order[] = {15, 24};
    GameEventQueue q;
    GameDomainRef target;
    Rich4Rng expected;
    uint32_t before_rng;
    int land_count, i;

    /* Known original bug: facility-only eligibility reaches rand()%0.
     * SDL skips news 15 and continues with the next eligible news item. */
    for (land_count = 0; land_count <= 1; land_count++) {
        base_state(&s, 93);
        s.land_count = land_count;
        s.land_owner[0] = -1;
        s.facility_count = 2;
        s.facility_level[0] = 1;
        s.facility_level[1] = 5;
        front_ids(s.news_event_order, GAME_NEWS_EVENT_COUNT, order, 2);
        before_rng = s.ai_rng_state;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s, 0, &q) ==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_logical_id == 24 && s.news_event_cursor == 2);
        assert(s.ai_rng_state == before_rng);
        assert(s.facility_level[0] == 1 && s.facility_level[1] == 5);
    }

    base_state(&s, 94);
    s.land_count = 3;
    s.land_level[1] = 3;
    s.land_level[2] = 1;
    for (i = 0; i < 3; i++) {
        s.land_owner[i] = -1;
        s.land_price[i] = 1000;
        s.land_price_per_level[i] = 500;
        s.land_value[i] = 1000 + s.land_level[i] * 500;
    }
    s.facility_count = 2;
    s.facility_level[0] = 4;
    s.facility_level[1] = 5;
    s.ai_rng_state = expected.state = 3;
    assert(rich4_rng_original_state_range(&expected, 2) == 1);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 15);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 15 && s.ai_rng_state == expected.state);
    assert(rich4_special_event_decode_property_target(
        &s, s.special_event_target_mask, &target));
    assert(target.kind == GAME_DOMAIN_LAND && target.id == 2);
    assert(rich4_special_event_commit(&s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_level[0] == 0 && s.land_level[1] == 3 && s.land_level[2] == 0);
    assert(s.facility_level[0] == 4 && s.facility_level[1] == 5);
}

static void news_two_phase_and_rollback(void)
{
    static GameState s,before;
    GameEventQueue q;
    GameEvent e;
    int i;
    base_state(&s,1); first(s.news_event_order,GAME_NEWS_EVENT_COUNT,24);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    for (i=0;i<GAME_STOCK_COUNT;i++) assert(s.stocks[i].f7==1);
    assert(s.special_event_resource_id==465 && s.special_event_panel_id==66);
    assert(q.count==3 && rich4_special_event_cancel(&s,&q)==GAME_SPECIAL_EVENT_NOT_CANCELABLE);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.special_event_phase==GAME_SPECIAL_PHASE_IDLE);

    base_state(&s,1); first(s.news_event_order,GAME_NEWS_EVENT_COUNT,25); before=s;
    rich4_game_event_queue_reset(&q);
    e = rich4_game_event_text_requested_from_fields(
        (Rich4GameEventFields){
            0, 0, 0, 0, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
        });
    for (i=0;i<30;i++) assert(rich4_game_event_queue_push(&q,&e));
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_EVENT_CAPACITY);
    assert(memcmp(&s,&before,sizeof(s))==0);
}

static void fortune_hospital_and_map_prison(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s,2); s.players[0].traffic_method=0;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,12);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_resolved_id==12 && s.special_event_resource_id==0x1e9 &&
           s.special_event_prepared_value==3);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].days_in_hospital==3 && s.players[0].node==80);

    base_state(&s,3); s.map=3;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,33);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id==33 && s.special_event_resolved_id==45 &&
           s.special_event_resource_id==0x202 && s.special_event_prepared_value==3);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_DOUBLED,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].days_in_prison==6 && s.players[0].node==70);
}

static void original_status_transitions(void)
{
    static GameState s;
    GameEventQueue q;

    base_state(&s,52);
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,6);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(
               &s,GAME_SPECIAL_DEFENSE_NONE,&q)==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].days_disappearing==3);
    assert(s.players[0].total_confinement_days==3);

    base_state(&s,53);
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,7);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(
               &s,GAME_SPECIAL_DEFENSE_DOUBLED,&q)==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].days_disappearing==
           (GAME_DISAPPEARING_ALIEN_BIT|6));
    assert(s.players[0].total_confinement_days==6);

    base_state(&s,54);
    s.players[0].traffic_method=1;
    s.players[0].ndices=2;
    s.tool_amount[4]=4;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,10);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(
               &s,GAME_SPECIAL_DEFENSE_NONE,&q)==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].traffic_method==0 &&
           s.players[0].ndices==1 && s.tool_amount[4]==5);

    base_state(&s,55);
    s.players[0].traffic_method=1;
    s.players[0].ndices=2;
    s.tool_amount[4]=7;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,13);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(
               &s,GAME_SPECIAL_DEFENSE_NONE,&q)==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].traffic_method==0 &&
           s.players[0].ndices==1 && s.tool_amount[4]==8);
    assert(s.players[0].days_in_hospital==3 &&
           s.players[0].node==s.hospital_node);
}

static void news_company_operator_goes_to_prison(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s,56);
    s.company_count=1;
    s.company_stock_index[0]=2;
    s.stock_company_auxiliary[2]=0;
    s.stock_company_owner[2]=1;
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,29);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(s.stock_company_auxiliary[2]==0);
    assert((s.special_event_target_mask&(1u<<1))!=0);
    assert(rich4_special_event_commit(
               &s,GAME_SPECIAL_DEFENSE_NONE,&q)==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.stock_company_auxiliary[2]==0);
    assert(s.players[1].days_in_prison==5 &&
           s.players[1].node==s.prison_node);
}

static void fortune_bankruptcy_and_news_callback(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s,4); s.price_index=2; s.players[0].cash=100; s.players[0].bank=0;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,23);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_resource_id==0x1f2 && s.special_event_prepared_value==2000);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_BANKRUPTCY);
    assert((s.players[0].play_state&0xff)==0 && s.players[0].cash==0);

    base_state(&s,5); first(s.news_event_order,GAME_NEWS_EVENT_COUNT,6);
    s.land_count=1; s.land_price[0]=1000; s.land_value[0]=1000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_phase==GAME_SPECIAL_PHASE_PREPARED &&
           s.special_event_logical_id==6 && s.special_event_resource_id==447 &&
           s.land_price[0]==1000 && q.count==2);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_price[0] == 1300);
}

static void fortune_insurance_claims(void)
{
    static GameState s;
    GameEventQueue q;
    GameEvent event;
    int saw_claim;

    base_state(&s, 57);
    s.company_count = 1;
    s.company_type[0] = 4;
    s.company_stock_index[0] = 5;
    s.company_period_business[0] = 50000;
    s.company_total_business[0] = 50000;
    s.players[0].days_assurance = 5;
    first(s.fortune_event_order, GAME_FORTUNE_EVENT_COUNT, 2);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(
               &s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].loan == 10000 && s.players[0].cash == 11000 &&
           s.players[0].days_assurance == 5 &&
           s.company_period_business[0] == 40000u &&
           s.company_total_business[0] == 40000u);
    saw_claim = 0;
    while (rich4_game_event_queue_pop(&q, &event))
        if (event.type ==
                GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED &&
            rich4_game_event_value(&event) == 10000)
            saw_claim = 1;
    assert(saw_claim);

    base_state(&s, 58);
    s.players[0].cash = 10000;
    s.players[0].bank = 0;
    s.company_count = 1;
    s.company_type[0] = 4;
    s.company_stock_index[0] = 5;
    s.company_period_business[0] = 50000;
    s.company_total_business[0] = 50000;
    s.players[0].days_assurance = 5;
    first(s.fortune_event_order, GAME_FORTUNE_EVENT_COUNT, 23);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(
               &s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].cash == 10000 &&
           s.company_period_business[0] == 49000u &&
           s.company_total_business[0] == 49000u);
}

static void news_reaction_owner_snapshot(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s, 51);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 5);
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_level[0] = 2;
    s.land_price[0] = 1000;
    s.land_price_per_level[0] = 500;
    s.land_value[0] = 2000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(((s.special_event_target_mask >> 24) & 0x0f) == 2);
    assert(s.land_owner[0] == 1);
    assert(rich4_special_event_commit(
               &s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_owner[0] == -1);
}

static void news_tornado_matches_original_target_pool(void)
{
    static GameState s;
    GameEventQueue q;
    GameDomainRef target;

    base_state(&s, 93);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 21);
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_level[0] = 2;
    s.land_price[0] = 1000;
    s.land_price_per_level[0] = 500;
    s.land_value[0] = 2000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 21);
    assert(rich4_special_event_decode_property_target(
               &s, s.special_event_target_mask, &target));
    assert(target.kind == GAME_DOMAIN_LAND && target.id == 0);
    assert(((s.special_event_target_mask >> 24) & 0x0fu) == 2u);
    assert(rich4_special_event_commit(
               &s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_level[0] == 1);

    base_state(&s, 94);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 21);
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_level[0] = 0;
    s.land_price[0] = 1000;
    s.land_price_per_level[0] = 500;
    s.land_value[0] = 1000;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id == 21);
    assert(rich4_special_event_decode_property_target(
               &s, s.special_event_target_mask, &target));
    assert(target.kind == GAME_DOMAIN_LAND && target.id == 0);
    assert(((s.special_event_target_mask >> 24) & 0x0fu) == 2u);
    assert(rich4_special_event_commit(
               &s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_level[0] == 0 && s.land_owner[0] == 1);
}

static void news_alien_attack_targets_nearby_players(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s, 71);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 4);
    s.tool_route_node_count = 3;
    s.players[0].node = 1;
    s.players[1].node = 2;
    s.tool_route_x[1] = 20;
    s.tool_route_y[1] = 30;
    s.tool_route_x[2] = 400;
    s.tool_route_y[2] = 400;
    s.special_player_node[0] = 1;
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_level[0] = 2;
    s.land_value[0] = 2000;
    s.fixed_asset_value[1] = 2000;
    s.tool_land_x[0] = 0;
    s.tool_land_y[0] = 0;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    {
        const GameNewsAreaAnchor anchors[] = {
            {2001, 220, 220}, {0x8011, 240, 250}
        };
        assert(s.special_event_target_mask == 0);
        assert(rich4_special_event_prepare_news_phase1(&s));
        assert(rich4_special_event_apply_news_area(&s, anchors, 2));
    }
    assert(((s.special_event_target_mask >> 16) & 0xffu) == 0x11u);
    assert(s.land_owner[0] == -1 && s.players[0].days_in_hospital == 0);
    assert(rich4_special_event_commit(
               &s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_owner[0] == -1);
    assert(s.players[0].days_in_hospital == 3);
    assert(s.players[1].days_in_hospital == 0);
    assert(s.special_player_confinement[0] == GAME_CONFINEMENT_HOSPITAL);
}

static void news_callback_targets_and_portrait_masks(void)
{
    static const int property_news[] = {6, 14, 18, 20};
    static const int reward_news[] = {8, 9, 10};
    static GameState s;
    GameEventQueue q;
    GameDomainRef target;
    int index;

    for (index = 0;
         index < (int)(sizeof(property_news) / sizeof(property_news[0]));
         index++) {
        base_state(&s, (unsigned int)(80 + index));
        first(s.news_event_order, GAME_NEWS_EVENT_COUNT,
              property_news[index]);
        s.land_count = 1;
        s.land_owner[0] = 1;
        s.land_level[0] = 2;
        s.land_price[0] = 1000;
        s.land_price_per_level[0] = 500;
        s.land_value[0] = 2000;
        s.tool_land_x[0] = 321;
        s.tool_land_y[0] = 654;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s, 0, &q) ==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(rich4_special_event_decode_property_target(
            &s, s.special_event_target_mask, &target));
        assert(target.kind == GAME_DOMAIN_LAND && target.id == 0);
    }

    for (index = 0;
         index < (int)(sizeof(reward_news) / sizeof(reward_news[0]));
         index++) {
        base_state(&s, (unsigned int)(86 + index));
        first(s.news_event_order, GAME_NEWS_EVENT_COUNT,
              reward_news[index]);
        if (reward_news[index] == 8) {
            s.land_count = 1;
            s.land_owner[0] = 1;
        } else if (reward_news[index] == 9) {
            s.land_count = 1;
            s.land_owner[0] = 0;
        } else {
            s.player_stock_amount[1][0] = 1;
        }
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s, 0, &q) ==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(((s.special_event_target_mask >> 24) & 0x0fu) == 2u);
    }

    base_state(&s, 90);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 11);
    s.players[0].cash = 19;
    s.players[1].cash = 20;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(((s.special_event_target_mask >> 16) & 0xffu) == 0x02u);
    assert(s.special_event_player_amount[0] == 0 &&
           s.special_event_player_amount[1] == 1);

    base_state(&s, 91);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 12);
    s.fixed_asset_value[0] = 0;
    s.fixed_asset_value[1] = 20;
    s.land_price[0] = 20;
    s.land_count = 1;
    s.land_owner[0] = 1;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(((s.special_event_target_mask >> 16) & 0xffu) == 0x02u);
    assert(s.special_event_player_amount[0] == 0 &&
           s.special_event_player_amount[1] == 1);

    base_state(&s, 92);
    first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 13);
    s.player_stock_amount[1][0] = 20;
    s.stocks[0].f20 = 1.0f;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s, 0, &q) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(((s.special_event_target_mask >> 16) & 0xffu) == 0x02u);
    assert(s.special_event_player_amount[0] == 0 &&
           s.special_event_player_amount[1] == 1);
}

static void magic_cancel_and_commit(void)
{
    static GameState s;
    GameEventQueue q;
    base_state(&s,6); s.players[0].bank=700; s.players[1].bank=700;
    assert(rich4_magic_house_action4_eligible(&s)==3u);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_magic_action4(&s,0,2u,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_panel_id==18 && s.special_event_cancelable==1);
    assert(rich4_special_event_cancel(&s,&q)==GAME_SPECIAL_EVENT_CANCELED);
    assert(s.players[1].cash==200 && s.players[1].bank==700);
    assert(rich4_special_event_begin_magic_action4(&s,0,2u,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].cash==0 && s.players[1].bank==900);
}

static GameSpecialEventResult apply_magic(
    GameState *s, int action, GameEventQueue *q)
{
    rich4_game_event_queue_reset(q);
    s->players[0].sex = 0;
    s->players[1].sex = 1;
    return rich4_magic_house_apply_action(
        s, 0, 10, action, 2u, q);
}

static void magic_original_targets_and_actions(void)
{
    static GameState s;
    GameEventQueue q;
    unsigned int mask;

    base_state(&s, 60);
    s.players[0].sex = 0;
    s.players[1].sex = 1;
    s.players[0].points = 9;
    s.players[1].points = 9;
    s.players[0].traffic_method = 0;
    s.players[1].traffic_method = 2;
    s.players[1].god_info = 3;
    assert(rich4_magic_house_target_mask(&s, 5) == 3u);
    assert(rich4_magic_house_target_mask(&s, 6) == 1u);
    assert(rich4_magic_house_target_mask(&s, 8) == 2u);
    assert(rich4_magic_house_target_mask(&s, 9) == 2u);
    assert(rich4_magic_house_target_mask(&s, 10) == 2u);
    assert(rich4_magic_house_target_mask(&s, 11) == 1u);
    assert(rich4_magic_house_choose_condition(&s, &s.special_event_logical_id,
                                               &mask) ==
           GAME_SPECIAL_EVENT_APPLIED);
    assert(mask != 0);

    base_state(&s, 61);
    s.players[1].card_inventory[0] = 1;
    s.card_amount[0] = 0;
    s.players[1].points = 20;
    assert(apply_magic(&s, 0, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].card_inventory[0] == 0 &&
           s.card_amount[0] == 1 &&
           s.players[1].points ==
               (uint16_t)(20 + rich4_card_catalog[0].shop_price));

    base_state(&s, 62);
    s.price_index = 2;
    s.players[1].days_in_prison = 2;
    assert(apply_magic(&s, 2, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert((s.players[1].days_in_prison & 0x7f) == 5 &&
           s.players[1].relation_balance[0] == 180);

    base_state(&s, 72);
    s.players[1].card_inventory[0] = 21;
    assert(apply_magic(&s, 2, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].days_in_prison == 0 &&
           s.players[1].card_inventory[0] == 0 &&
           s.card_amount[20] == 1);

    base_state(&s, 63);
    s.players[1].days_stopping = 2;
    assert(apply_magic(&s, 3, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].days_stopping == 3);

    base_state(&s, 64);
    assert(apply_magic(&s, 4, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].cash == 0 && s.players[1].bank == 700);

    base_state(&s, 65);
    s.tool_route_node_count = 3;
    s.players[1].node = 1;
    s.route_object_raw[1] = 0x7d1;
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_level[0] = 2;
    s.land_price[0] = 1000;
    s.land_price_per_level[0] = 500;
    s.land_value[0] = 2000;
    s.fixed_asset_value[1] = 2000;
    assert(apply_magic(&s, 5, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_level[0] == 3 && s.land_value[0] == 2500 &&
           s.fixed_asset_value[1] == 2500);

    base_state(&s, 73);
    s.players[1].play_state = 1;
    s.tool_route_node_count = 3;
    s.players[1].node = 1;
    s.route_object_raw[1] = 0xfa1;
    s.facility_count = 1;
    s.facility_owner[0] = 1;
    s.facility_type[0] = 3;
    s.facility_level[0] = 0;
    s.facility_price[0] = 1000;
    s.facility_price_per_level[0] = 500;
    s.facility_value[0] = 1000;
    assert(apply_magic(&s, 5, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.facility_type[0] == 3 &&
           s.facility_level[0] == 1 &&
           s.facility_value[0] == 1500);

    base_state(&s, 66);
    s.card_amount[7] = 1;
    assert(apply_magic(&s, 6, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].card_inventory[0] == 8 &&
           s.card_amount[7] == 0);

    base_state(&s, 67);
    s.tool_route_node_count = 5;
    s.players[1].node = 1;
    s.players[1].next_node = 2;
    s.tool_route_next[1][0] = 2;
    s.tool_route_next[1][1] = 3;
    s.tool_route_next[1][2] = 4;
    s.tool_route_edge_blocked[1] = 1u << 2;
    assert(apply_magic(&s, 7, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].next_node == 3);

    base_state(&s, 68);
    s.players[1].traffic_method = 1;
    s.players[1].ndices = 2;
    s.players[1].tool_inventory[0] = 2;
    s.players[1].points = 10;
    assert(apply_magic(&s, 8, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].traffic_method == 0 &&
           s.players[1].ndices == 1 &&
           s.players[1].tool_inventory[0] == 0 &&
           s.players[1].tool_inventory[4] == 0 &&
           s.tool_amount[0] == 2 && s.tool_amount[4] == 1);

    base_state(&s, 69);
    s.tool_route_node_count = 3;
    s.players[1].node = 1;
    s.route_object_raw[1] = 0x7d1;
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_level[0] = 3;
    s.land_price[0] = 1000;
    s.land_price_per_level[0] = 500;
    s.land_value[0] = 2500;
    s.fixed_asset_value[1] = 2500;
    assert(apply_magic(&s, 9, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_level[0] == 2 && s.land_value[0] == 2000 &&
           s.fixed_asset_value[1] == 2000);

    base_state(&s, 74);
    s.tool_route_node_count = 3;
    s.players[1].node = 1;
    s.route_object_raw[1] = 0xfa1;
    s.facility_count = 1;
    s.facility_owner[0] = 1;
    s.facility_type[0] = 2;
    s.facility_level[0] = 2;
    s.facility_price[0] = 1000;
    s.facility_price_per_level[0] = 500;
    s.facility_value[0] = 2000;
    assert(apply_magic(&s, 9, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.facility_type[0] == 2 &&
           s.facility_level[0] == 1 &&
           s.facility_value[0] == 1500);

    base_state(&s, 70);
    s.price_index = 3;
    s.players[1].days_in_hospital = 1;
    assert(apply_magic(&s, 10, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert((s.players[1].days_in_hospital & 0x7f) == 4 &&
           s.players[1].relation_balance[0] == 270);

    base_state(&s, 71);
    s.players[0].cash = 5000;
    s.players[0].bank = 0;
    s.tool_route_node_count = 3;
    s.players[1].node = 1;
    s.route_object_raw[1] = 0x7d1;
    s.land_count = 1;
    s.land_owner[0] = 1;
    s.land_price[0] = 1000;
    s.land_value[0] = 1000;
    s.fixed_asset_value[1] = 1000;
    assert(apply_magic(&s, 11, &q) == GAME_SPECIAL_EVENT_APPLIED);
    assert(s.land_owner[0] == 1 && s.players[0].cash == 5000 &&
           s.players[1].bank == 500);
    assert(q.count == 2 && q.events[0].type == GAME_EVENT_AUCTION_REQUESTED);
    assert(rich4_game_event_auction_property(&q.events[0]).kind ==
               GAME_DOMAIN_LAND &&
           rich4_game_event_auction_seller(&q.events[0]) == 1 &&
           rich4_game_event_auction_trigger(&q.events[0]) ==
               GAME_AUCTION_TRIGGER_MAGIC_HOUSE);
}

static void snapshot_restore(void)
{
    static GameState s,loaded;
    GameSpecialEventSnapshot snap,bad;
    GameEventQueue q;
    base_state(&s,7); s.players[0].traffic_method=0;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,12);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_snapshot_export(&s,&snap));
    base_state(&loaded,99);
    assert(rich4_special_event_snapshot_import(&loaded,&snap));
    assert(loaded.special_event_resolved_id==12 && loaded.fortune_event_cursor==1 &&
           loaded.ai_rng_state==s.ai_rng_state);
    assert(rich4_special_event_commit(&loaded,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(loaded.players[0].days_in_hospital==3);
    bad=snap; bad.news_order[1]=bad.news_order[0];
    assert(!rich4_special_event_snapshot_import(&loaded,&bad));

    base_state(&s,8); first(s.news_event_order,GAME_NEWS_EVENT_COUNT,25);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_snapshot_export(&s,&snap));
    base_state(&loaded,9);
    assert(rich4_special_event_snapshot_import(&loaded,&snap));
    assert(loaded.special_event_logical_id==25 && loaded.stocks[0].f7==16 &&
           loaded.stocks[GAME_STOCK_COUNT-1].f7==16);

    /* News presentation targets use the high bytes for the property owner
     * and nearby actors; snapshot validation must preserve that encoding. */
    base_state(&s,10);
    first(s.news_event_order,GAME_NEWS_EVENT_COUNT,4);
    s.tool_route_node_count=3;
    s.players[0].node=1; s.players[1].node=2;
    s.tool_route_x[1]=20; s.tool_route_y[1]=30;
    s.tool_route_x[2]=400; s.tool_route_y[2]=400;
    s.special_player_node[0]=1;
    s.land_count=1; s.land_owner[0]=1; s.land_level[0]=2;
    s.land_value[0]=2000; s.fixed_asset_value[1]=2000;
    s.tool_land_x[0]=0; s.tool_land_y[0]=0;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_news(&s,0,&q)==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_target_mask == 0);
    assert(rich4_special_event_prepare_news_phase1(&s));
    {
        const GameNewsAreaAnchor anchors[] = {{2001,220,220},{0x8011,240,250}};
        assert(rich4_special_event_apply_news_area(&s,anchors,2));
    }
    assert((s.special_event_target_mask & 0x00ff0000u)!=0);
    assert((s.special_event_target_mask & 0x0f000000u)!=0);
    assert(rich4_special_event_snapshot_export(&s,&snap));
    base_state(&loaded,11);
    assert(rich4_special_event_snapshot_import(&loaded,&snap));
    assert(loaded.special_event_target_mask==s.special_event_target_mask);
}

static void complete_news_catalog(void)
{
    int id;
    for (id=0;id<GAME_NEWS_EVENT_COUNT;id++) {
        static GameState s;
        GameEventQueue q;
        int i;
        base_state(&s,(unsigned int)(100+id));
        s.players[0].cash=s.players[0].bank=1000000;
        s.players[1].cash=s.players[1].bank=1000000;
        s.players[0].days_in_prison=2;
        s.players[1].days_in_hospital=2;
        s.players[1].traffic_method=2;
        s.land_count=2;
        s.land_owner[0]=0; s.land_level[0]=2;
        s.land_price[0]=1000; s.land_price_per_level[0]=500;
        s.land_value[0]=2000;
        s.land_owner[1]=-1; s.land_price[1]=1000;
        s.land_value[1]=1000;
        s.fixed_asset_value[0]=2000;
        s.player_stock_amount[0][0]=100;
        for (i=0;i<GAME_STOCK_COUNT;i++) {
            s.stocks[i].f20=100.0f;
            s.stock_company_auxiliary[i]=-1;
        }
        s.stocks[0].f6=2;
        s.stock_company_auxiliary[0]=0;
        s.company_count=1;
        s.company_stock_index[0]=0;
        s.stock_company_owner[0]=0;
        if (id==29) s.players[0].days_in_prison=0;
        s.stock_company_value[0]=30000;
        s.stock_company_available[0]=30000;
        s.company_period_business[0]=30000;
        first(s.news_event_order,GAME_NEWS_EVENT_COUNT,id);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s,0,&q)==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_logical_id==id &&
               s.special_event_resource_id==441+id);
        if (id == 4 || id == 20) {
            const GameNewsAreaAnchor anchors[] = {{2001,220,220}};
            assert(rich4_special_event_prepare_news_phase1(&s));
            assert(rich4_special_event_apply_news_area(&s,anchors,1));
        }
        assert(rich4_special_event_commit(
                   &s,GAME_SPECIAL_DEFENSE_NONE,&q)!=
               GAME_SPECIAL_EVENT_INVALID);
    }
}

static void complete_fortune_catalog(void)
{
    int logical;
    for (logical=0;logical<GAME_FORTUNE_EVENT_COUNT;logical++) {
        static GameState s;
        GameEventQueue q;
        int i;
        base_state(&s,(unsigned int)(200+logical));
        s.players[0].cash=s.players[0].bank=1000000;
        s.players[1].cash=s.players[1].bank=1000000;
        s.land_count=2;
        s.land_owner[0]=0; s.land_level[0]=2;
        s.land_price[0]=1000; s.land_price_per_level[0]=500;
        s.land_value[0]=2000;
        s.land_owner[1]=0; s.land_level[1]=0;
        s.land_price[1]=1000; s.land_price_per_level[1]=500;
        s.land_value[1]=1000;
        s.fixed_asset_value[0]=3000;
        s.players[1].card_inventory[0]=1;
        s.player_stock_amount[0][0]=100;
        s.stocks[0].f20=100.0f;
        for (i=0;i<GAME_CARD_COUNT;i++) s.card_amount[i]=1;
        if (logical==10 || logical==11 || logical==13 ||
            logical==15) s.players[0].traffic_method=1;
        else s.players[0].traffic_method=0;
        first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,logical);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_fortune(&s,0,&q)==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_logical_id==logical);
        assert(rich4_special_event_commit(
                   &s,GAME_SPECIAL_DEFENSE_NONE,&q)!=
               GAME_SPECIAL_EVENT_INVALID);
    }
}

static void fortune_random_card_inventory(void)
{
    GameState s;
    GameEventQueue q;
    int full, i;
    for (full=0;full<2;full++) {
        base_state(&s,1);
        s.players[0].is_ai=1;
        s.players[0].play_state=2;
        s.players[1].card_inventory[0]=2;
        s.players[1].card_inventory[1]=3;
        s.players[1].card_inventory[2]=2;
        if (full)
            for (i=0;i<GAME_PLAYER_CARD_SLOTS;i++)
                s.players[0].card_inventory[i]=30;
        s.card_amount[29]=10;
        s.card_amount[1]=10;
        first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,5);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_fortune(&s,0,&q)==
            GAME_SPECIAL_EVENT_PREPARED);
        s.ai_rng_state=1; /* Watcom rand() first draw: 16838. */
        assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==
            GAME_SPECIAL_EVENT_APPLIED);
        assert(s.players[1].card_inventory[0]==3 &&
               s.players[1].card_inventory[1]==2 &&
               s.players[1].card_inventory[2]==0);
        assert(s.players[0].card_inventory[full ? GAME_PLAYER_CARD_SLOTS-1 : 0]==2);
        assert(rich4_card_inventory_count(&s,0)==(full ? GAME_PLAYER_CARD_SLOTS : 1));
        assert(s.card_amount[29]==10+full && s.card_amount[1]==10);
    }
}

/* 0x44BB4B transport rewrite matrix, including 0x44BDE0 (15 -> 14). */
static void fortune_transport_matrix(void)
{
    static const int expected[4][7] = {
        {-1,-1,12,12,14,14,14}, {10,10,13,13,15,15,15},
        {11,11,-1,-1,16,16,16}, {-1,-1,-1,-1,-1,-1,-1}
    };
    GameState s;
    GameEventQueue q;
    int method, logical;
    for (method=0;method<4;method++) for (logical=10;logical<=16;logical++) {
        int order[2]={logical,2};
        uint8_t deck[GAME_FORTUNE_EVENT_COUNT];
        uint32_t rng;
        base_state(&s,1);
        s.players[0].traffic_method=method;
        front_ids(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,order,2);
        memcpy(deck,s.fortune_event_order,sizeof(deck)); rng=s.ai_rng_state;
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_logical_id==(expected[method][logical-10]<0 ? 2 : logical));
        assert(s.special_event_resolved_id==(expected[method][logical-10]<0 ? 2 : expected[method][logical-10]));
        assert(s.fortune_event_cursor==(expected[method][logical-10]<0 ? 2 : 1));
        assert(s.ai_rng_state==rng && !memcmp(deck,s.fortune_event_order,sizeof(deck)));
    }
}

static void fortune_target_chains(void)
{
    static const int ids[]={6,7,12,13,33,34,35,36};
    GameState s;
    GameEventQueue q;
    int k, mode;
    for (k=0;k<(int)(sizeof(ids)/sizeof(ids[0]));k++) for (mode=0;mode<4;mode++) {
        uint32_t rng;
        base_state(&s,1); s.players[0].traffic_method=ids[k]==13 ? 1 : 0;
        s.players[0].play_state=mode==3 ? 2 : 1;
        s.players[0].card_inventory[0]=19;
        if (mode==0) s.players[0].card_inventory[1]=21;
        s.players[1].card_inventory[0]=21; /* No recursive absolution. */
        s.players[1].card_inventory[1]=19;
        s.players[0].relation_balance[1]=10;
        first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,ids[k]);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        rng=s.ai_rng_state;
        assert(rich4_special_event_commit_with_target(&s,GAME_SPECIAL_DEFENSE_DOUBLED,
            mode==1 ? 1 : -1,&q,0)==GAME_SPECIAL_EVENT_APPLIED);
        assert(s.ai_rng_state==rng); /* Relationship choice / human choice / immunity. */
        assert(s.players[1].card_inventory[0]==21 && s.players[1].card_inventory[1]==19);
        if (mode==0) {
            assert(!s.players[0].days_disappearing && !s.players[0].days_in_hospital &&
                   !s.players[0].days_in_prison && s.players[0].card_inventory[0]==19);
            assert(!s.players[1].days_disappearing && !s.players[1].days_in_hospital &&
                   !s.players[1].days_in_prison);
        } else {
            int victim=mode==2 ? 0 : 1;
            int days=ids[k]>=33 ? 2*(3+2*(ids[k]-33)) : 6;
            assert((ids[k]==6 || ids[k]==7) ?
                (s.players[victim].days_disappearing & 63)==days :
                (ids[k]==12 || ids[k]==13) ? s.players[victim].days_in_hospital==days :
                s.players[victim].days_in_prison==days);
            assert(rich4_card_inventory_has(&s,0,18)==(mode==2));
        }
    }
    /* AI relation ties retain the lower index, even when confined. */
    base_state(&s,1); s.player_count=4; s.players[0].play_state=2;
    for (k=1;k<4;k++) s.players[k].play_state=2;
    s.players[0].card_inventory[0]=19;
    s.players[0].relation_balance[1]=s.players[0].relation_balance[2]=5;
    s.players[1].days_in_hospital=2;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,6);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].days_disappearing==3 && !s.players[2].days_disappearing);
    /* No relationship: exclude each of +50..+53, sample only remaining players. */
    for (mode=0;mode<4;mode++) {
        Rich4Rng rng={1};
        uint32_t expected_rng;
        (void)rich4_rng_original_state_range(&rng,1); expected_rng=rng.state;
        base_state(&s,1); s.player_count=3; s.players[0].play_state=2;
        s.players[2].play_state=2; s.players[0].card_inventory[0]=19;
        if (mode==0) s.players[1].delayed_status_block=1;
        if (mode==1) s.players[1].days_disappearing=1;
        if (mode==2) s.players[1].days_in_prison=1;
        if (mode==3) s.players[1].days_in_hospital=1;
        first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,6);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        s.ai_rng_state=1;
        assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
        assert(s.players[2].days_disappearing==3 && s.ai_rng_state==expected_rng);
    }
}

static void fortune_human_card_selection(void)
{
    GameState s,before;
    GameEventQueue q;
    uint32_t rng;
    int i;
    base_state(&s,1);
    s.players[1].card_inventory[0]=2; s.players[1].card_inventory[1]=3;
    s.players[1].card_inventory[2]=2;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,5);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    before=s; rng=s.ai_rng_state;
    assert(!rich4_special_event_fortune_take_card(&s,1,4));
    assert(!memcmp(&before,&s,sizeof(s)));
    assert(rich4_special_event_fortune_take_card(&s,1,-1));
    assert(!memcmp(&before,&s,sizeof(s)));
    for (i=0;i<GAME_PLAYER_CARD_SLOTS;i++) s.players[0].card_inventory[i]=30;
    assert(rich4_special_event_fortune_take_card(&s,1,2));
    assert(s.players[1].card_inventory[0]==3 && s.players[1].card_inventory[1]==2);
    assert(s.players[0].card_inventory[14]==2 && s.card_amount[29]==1);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].card_inventory[0]==3 && s.players[1].card_inventory[1]==2);
    assert(s.ai_rng_state==rng);
}

static void fortune_bank_rejection_and_theft(void)
{
    GameState s;
    GameEventQueue q;
    base_state(&s,1); s.players[0].days_rejected_by_bank=240;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,3);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_DOUBLED,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].days_rejected_by_bank==14);
    base_state(&s,1); s.player_count=4;
    s.players[1].bank=1000000019; s.players[1].days_in_hospital=5;
    s.players[2].play_state=0; s.players[2].bank=1000;
    s.players[3].play_state=1; s.players[3].bank=9;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,4);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[1].bank==900000016 && s.players[2].bank==1000 && s.players[3].bank==9);
    assert(s.players[0].bank==100001003 && s.players[0].cash==1000);
    assert(s.players[1].monthly_loss==100000003 && s.players[0].monthly_income==100000003);
}

static void fortune_modifier_boundaries(void)
{
    static const int modifiers[]={-1,0,50,51,100,101};
    GameState s;
    GameEventQueue q;
    int id,k;
    for (id=0;id<49;id++) for (k=0;k<6;k++) {
        Rich4Rng expected={3};
        int r=modifiers[k],flip=0;
        int income=id==20 || id==21 || id==22 || id==25 || id==27 || id==28 || id==29 || id==31;
        int ignored=id==0 || id==1 || id==4 || id==5;
        int land=id==6 || id==7 || (id>=10 && id<=13) || id==32 || id>=33;
        GameSpecialDefenseOutcome outcome=GAME_SPECIAL_DEFENSE_NONE;
        base_state(&s,1);
        s.special_event_family=GAME_SPECIAL_EVENT_FORTUNE;
        s.special_event_phase=GAME_SPECIAL_PHASE_PREPARED;
        s.special_event_actor=0; s.special_event_resolved_id=id;
        s.ai_rng_state=3;
        s.players[0].god_land_modifier=land ? r : -r;
        s.players[0].god_investment_modifier=land ? -r : r;
        if (!ignored) {
            if (r>50 && r<=100) flip=rich4_rng_original_state_range(&expected,2);
            if (r>100 || flip) outcome=income ? GAME_SPECIAL_DEFENSE_DOUBLED : GAME_SPECIAL_DEFENSE_BLOCKED;
            if (r<0) outcome=income ? GAME_SPECIAL_DEFENSE_BLOCKED : GAME_SPECIAL_DEFENSE_DOUBLED;
        }
        assert(rich4_special_event_default_defense(&s)==outcome);
        assert(s.ai_rng_state==expected.state);
    }
    /* 0x44C7EF ignores return 2: losses stay 10%, with float32 ratio. */
    base_state(&s,1); s.players[0].god_investment_modifier=-60;
    s.player_stock_amount[0][0]=1000000019;
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,8);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,rich4_special_event_default_defense(&s),&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.player_stock_amount[0][0]==900000016);
}

static void fortune_properties_and_shared_deck(void)
{
    GameState s;
    GameEventQueue q;
    int id,i,map;
    for (id=0;id<2;id++) {
        base_state(&s,1); s.land_count=5; s.facility_count=1;
        for (i=0;i<5;i++) {
            s.land_owner[i]=i==1 || i==4 ? 0 : 1;
            s.land_level[i]=id==0 ? 3 : 0;
            s.land_price[i]=0; s.land_price_per_level[i]=0;
        }
        s.facility_owner[0]=0; s.facility_level[0]=3;
        s.land_runtime_value[1]=123;
        first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,id);
        rich4_game_event_queue_reset(&q); s.ai_rng_state=1;
        assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_target_mask==2 && s.special_event_prepared_value==0);
        s.ai_rng_state=3; /* phase 1 must use the saved phase-0 land. */
        assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
        if (id==0) assert(s.land_level[1]==0 && s.land_level[4]==3);
        else assert(s.land_owner[1]==-1 && s.land_runtime_value[1]==0 && s.land_owner[4]==0);
        assert(s.facility_level[0]==3 && s.ai_rng_state==3);
    }
    for (map=0;map<4;map++) for (id=33;id<37;id++) {
        base_state(&s,1); s.map=map;
        first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,id);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_resolved_id==id+map*4);
        assert(s.special_event_prepared_value==3+2*(id-33));
    }
    base_state(&s,1);
    for (i=0;i<GAME_FORTUNE_EVENT_COUNT;i++) s.fortune_event_order[i]=(uint8_t)i;
    s.fortune_event_cursor=36; s.stage=1;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,1,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id==2 && s.fortune_event_cursor==3);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(s.special_event_logical_id==3 && s.fortune_event_cursor==4);
    for (i=0;i<GAME_FORTUNE_EVENT_COUNT;i++) assert(s.fortune_event_order[i]==i);
}

static void first_news_can_auction_unowned_property(void)
{
    static GameState s;
    GameEventQueue q;
    int facility, found, i;
    for (facility = 0; facility < 2; facility++) {
        base_state(&s, 1);
        s.players[1].play_state = 0; /* Original pending first landing. */
        s.land_count = facility ? 0 : 1;
        s.facility_count = facility ? 1 : 0;
        s.land_owner[0] = s.facility_owner[0] = -1;
        first(s.news_event_order, GAME_NEWS_EVENT_COUNT, 7);
        rich4_game_event_queue_reset(&q);
        assert(rich4_special_event_begin_news(&s, 0, &q) ==
               GAME_SPECIAL_EVENT_PREPARED);
        assert(s.special_event_logical_id == 7);
        assert(rich4_special_event_commit(&s, GAME_SPECIAL_DEFENSE_NONE, &q) ==
               GAME_SPECIAL_EVENT_APPLIED);
        found = 0;
        for (i = 0; i < q.count; i++) {
            if (q.events[i].type != GAME_EVENT_AUCTION_REQUESTED) continue;
            assert(rich4_game_event_auction_seller(&q.events[i]) == -1);
            assert(rich4_game_event_auction_property(&q.events[i]).kind ==
                   (facility ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND));
            assert(rich4_game_event_auction_property(&q.events[i]).id == 0);
            found++;
        }
        assert(found == 1);
    }
}

static void fortune_stock_return_accounting(void)
{
    GameState s, before;
    GameEventQueue q;
    int i;
    base_state(&s,401);
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,8);
    s.lottery_pool=100;
    for (i=0;i<2;i++) {
        s.player_stock_amount[0][i]=10+i;
        s.player_stock_average_cost[0][i]=9.0f;
        s.stocks[i].f20=i ? 2.25f : 1.5f;
        s.stocks[i].f8=65535; s.stocks[i].f10=100;
    }
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.lottery_pool==103 && s.players[0].bank==1000);
    for (i=0;i<2;i++) {
        assert(s.player_stock_amount[0][i]==9+i);
        assert(s.stocks[i].f8==0 && s.stocks[i].f10==101);
        assert(s.player_stock_average_cost[0][i]==9.0f);
    }
    base_state(&s,402);
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,9);
    for (i=0;i<2;i++) {
        s.player_stock_amount[0][i]=1;
        s.player_stock_average_cost[0][i]=9.0f;
        s.stocks[i].f20=1.5f; s.stocks[i].f8=10; s.stocks[i].f10=20;
    }
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    before=s;
    q.count=GAME_EVENT_QUEUE_CAPACITY;
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_EVENT_CAPACITY);
    assert(memcmp(&s,&before,sizeof(s))==0);
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].bank==1002 && s.lottery_pool==0); /* trunc(1.5)+trunc(1.5) */
    for (i=0;i<2;i++) {
        assert(s.player_stock_amount[0][i]==0 && s.player_stock_average_cost[0][i]==0.0f);
        assert(s.stocks[i].f8==11 && s.stocks[i].f10==21);
    }
    base_state(&s,403);
    first(s.fortune_event_order,GAME_FORTUNE_EVENT_COUNT,9);
    s.player_stock_amount[0][0]=16777217;
    s.stocks[0].f20=1.0f;
    rich4_game_event_queue_reset(&q);
    assert(rich4_special_event_begin_fortune(&s,0,&q)==GAME_SPECIAL_EVENT_PREPARED);
    assert(rich4_special_event_commit(&s,GAME_SPECIAL_DEFENSE_NONE,&q)==GAME_SPECIAL_EVENT_APPLIED);
    assert(s.players[0].bank==16778217); /* integer shares must not round through float32 */
}

#include "news_selection_cases.inc"

int main(void)
{
    fortune_stock_return_accounting();
    news_selection_regressions();
    deterministic_sequences();
    automatic_god_defense();
    original_eligibility_filters();
    news15_requires_built_land();
    first_news_can_auction_unowned_property();
    news_two_phase_and_rollback();
    fortune_hospital_and_map_prison();
    original_status_transitions();
    news_company_operator_goes_to_prison();
    fortune_bankruptcy_and_news_callback();
    fortune_insurance_claims();
    news_reaction_owner_snapshot();
    news_tornado_matches_original_target_pool();
    news_alien_attack_targets_nearby_players();
    news_callback_targets_and_portrait_masks();
    magic_cancel_and_commit();
    magic_original_targets_and_actions();
    snapshot_restore();
    complete_news_catalog();
    complete_fortune_catalog();
    fortune_random_card_inventory();
    fortune_transport_matrix();
    fortune_target_chains();
    fortune_human_card_selection();
    fortune_bank_rejection_and_theft();
    fortune_modifier_boundaries();
    fortune_properties_and_shared_deck();
    puts("unit special event: ok");
    return 0;
}
