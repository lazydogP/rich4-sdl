#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/special_presentation_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/insurance_flow.h"
#include "core/insurance.h"
#include "core/game_event_payload.h"
#include "core/special_event.h"
#include "core/route_event_executor.h"

static void fortune_claim_after_callback_speech(void)
{
    static GameSession s;
    GameEvent presentation;
    GameCommand confirm = {0};
    RouteEvent route = {0};
    memset(&s, 0, sizeof(s));
    s.state.active = 1;
    s.state.player_count = 2;
    s.state.price_index = 1;
    s.state.players[0].play_state = s.state.players[1].play_state = 1;
    s.state.players[0].cash = 10000;
    s.state.players[0].days_assurance = 1;
    s.state.company_count = 1;
    s.state.company_type[0] = 4;
    s.state.company_stock_index[0] = -1;
    s.state.special_event_family = GAME_SPECIAL_EVENT_FORTUNE;
    s.state.special_event_actor = 0;
    s.state.special_event_logical_id = s.state.special_event_resolved_id = 23;
    s.state.special_event_prepared_value = 1000;
    s.state.special_event_phase = GAME_SPECIAL_PHASE_PREPARED;
    presentation = rich4_game_event_special_event_presentation(0, 498, 66, GAME_SPECIAL_EVENT_FORTUNE);
    assert(rich4_special_event_begin(&s, &presentation, 0, 0, 1000));
    route.phase = ROUTE_EVENT_PHASE_STOP;
    route.node = 2;
    assert(rich4_route_event_queue_push(&s.flow.route_events.queue, &route));
    s.flow.route_events.state.execution_started = 1;
    s.flow.route_events.state.execution_actor = 0;
    s.flow.route_events.state.waiting_for_command = 1;
    confirm.type = GAME_COMMAND_CONFIRM;
    s.flow.route_events.state.pending_accept = confirm;
    assert(rich4_special_event_submit_page_command(&s, &confirm, 3400) == ROUTE_EVENT_COMMAND_APPLIED);
    assert(s.state.players[0].cash == 9000);
    assert(s.presentation.insurance.count == 1);
    assert(s.presentation.insurance.queue[0].kind == GAME_INSURANCE_NOTICE_WAIT_SPECIAL);
    assert(!rich4_insurance_flow_update(&s, 100000));
    assert(s.state.players[0].cash == 9000);
    assert(rich4_special_event_finish_callback(&s, 100000));
    assert(rich4_insurance_flow_update(&s, 101999));
    assert(s.state.players[0].cash == 9000);
    assert(!rich4_insurance_flow_update(&s, 102000));
    assert(s.state.players[0].cash == 10000 && s.state.players[0].monthly_income == 1000);
    assert(rich4_special_event_finish_callback(&s, 102000));
    assert(s.presentation.special_event.phase == RICH4_SPECIAL_PRESENT_POST_HOLD);
}

static void deferred_confinement_callback(int family, int id, int kind)
{
    GameSession s;
    GameEvent presentation;
    GameCommand confirm = {0};
    RouteEvent route = {0};
    Rich4SpecialEventReactionResult reaction;
    int i;
    memset(&s, 0, sizeof(s));
    s.state.active = 1;
    s.state.player_count = 2;
    s.state.price_index = 1;
    s.state.prison_node = 5;
    s.state.hospital_node = 6;
    s.state.land_count = 1;
    s.state.land_level[0] = 1;
    for (i = 0; i < 2; i++) {
        s.state.players[i].node = 2 + i;
        s.state.players[i].play_state = 1;
        s.state.players[i].cash = 1000;
        s.state.players[i].traffic_method = 1;
    }
    s.state.special_event_family = family;
    s.state.special_event_actor = 0;
    s.state.special_event_logical_id = s.state.special_event_resolved_id = id;
    s.state.special_event_prepared_value = 3;
    s.state.special_event_phase = GAME_SPECIAL_PHASE_PREPARED;
    s.state.special_event_target_mask = id == 4 ? 1u | (3u << 16) : 1u;
    presentation = rich4_game_event_special_event_presentation(0, 445, 66, family);
    assert(rich4_special_event_begin(&s, &presentation, kind, 1, 1000));
    route.phase = ROUTE_EVENT_PHASE_STOP;
    route.node = 2;
    assert(rich4_route_event_queue_push(&s.flow.route_events.queue, &route));
    s.flow.route_events.state.execution_started = 1;
    s.flow.route_events.state.execution_actor = 0;
    s.flow.route_events.state.waiting_for_command = 1;
    confirm.type = GAME_COMMAND_CONFIRM;
    s.flow.route_events.state.pending_accept = confirm;
    if (family == GAME_SPECIAL_EVENT_NEWS && id == 4) {
        const GameNewsAreaAnchor anchors[] = {{0x8003,220,220}};
        assert(rich4_special_event_submit_page_command_with_area(
            &s,&confirm,3400,anchors,1,1,NULL) == ROUTE_EVENT_COMMAND_APPLIED);
    } else
        assert(rich4_special_event_submit_page_command(&s, &confirm, 3400) == ROUTE_EVENT_COMMAND_APPLIED);
    assert(s.presentation.tool_animation_count == 0); /* consequence runs first */
    for (i = 0; i < 2; i++) {
        assert(s.state.players[i].node == 2 + i);
        assert(s.state.players[i].days_in_prison == 0);
        assert(s.state.players[i].days_in_hospital == 0);
        assert(s.state.players[i].cash == 1000);
        assert(s.state.players[i].total_confinement_days == 0);
    }
    assert(s.presentation.special_event.pending_confinement_days[0] == (id == 29 ? 5 : 3));
    if (id == 4) assert(s.presentation.special_event.pending_confinement_days[1] == 3);
    assert(rich4_special_event_prepare_reaction(&s, 4000, &reaction));
    assert(s.presentation.special_event.reaction_stage == 5);
    assert(s.presentation.tool_animation_count == 1);
    assert(s.presentation.tool_animation[0].admission_stage == 1);
    assert(s.presentation.tool_animation[0].confinement_days == (id == 29 ? 5 : 3));
    assert(rich4_special_event_render_state(&s) == &s.state);
    if (id == 4) assert(s.presentation.special_event.pending_confinement_days[1] == 3);
}

static int pop_type(Rich4AudioEventQueue *queue)
{
    Rich4AudioEvent event;
    assert(rich4_audio_event_pop(queue, &event));
    return event.type;
}

static void company_control_notice_timeline(void)
{
    GameSession s;
    GameEvent event;
    GameEventQueue events;
    memset(&s, 0, sizeof(s));
    s.state.company_count = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_game_event_emit_typed_domain(&events,
        rich4_game_event_market_changed_from_fields, 2,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 4),
        rich4_game_domain_ref(GAME_DOMAIN_COMPANY, 0), -1, 2, 0));
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(rich4_company_issue_control_notice(&s, &event));
    assert(s.presentation.company_issue.phase == 2);
    assert(rich4_company_issue_update(&s, 100));
    assert(!rich4_company_issue_confirm(&s, 1, NULL));
    assert(!rich4_company_issue_finish(&s, 0));
    assert(rich4_company_issue_update(&s, 1599));
    assert(!rich4_company_issue_update(&s, 1600));
    assert(rich4_company_issue_control_notice(&s, &event));
    assert(rich4_company_issue_update(&s, 2000));
    assert(rich4_company_issue_skip_notice(&s, 2100));
    assert(!rich4_company_issue_update(&s, 2100));
}

static void road_gift_timeline(void)
{
    GameSession s;
    Rich4AudioEvent sound;
    memset(&s, 0, sizeof(s));
    s.state.active = 1;
    s.state.player_count = 1;
    s.state.players[0].play_state = 1;
    s.state.players[0].node = 2;
    s.presentation.dynamic_objects[12].type = 13;
    s.presentation.dynamic_objects[12].node = 3;
    s.state.tool_amount[5] = 2; /* car: high-price reaction */
    assert(!rich4_road_gift_begin(&s, 0, 1000));
    s.presentation.dynamic_objects[12].node = 2;
    assert(rich4_road_gift_begin(&s, 0, 1000));
    assert(s.state.players[0].tool_inventory[5] == 1);
    assert(s.state.tool_amount[5] == 1);
    assert(s.presentation.dynamic_objects[12].node == 0);
    assert(s.presentation.dynamic_objects[12].type == 13);
    assert(s.presentation.special_reward.action == 15);
    assert(s.presentation.special_reward.phase == 1);
    assert(s.presentation.special_reward.card == 5);
    assert(rich4_audio_event_pop(&s.audio_events, &sound));
    assert(sound.type == RICH4_AUDIO_EVENT_ROAD_GIFT);
    assert(!rich4_road_gift_begin(&s, 0, 1001));
    assert(rich4_special_reward_update(&s, 99, 10, 2499, NULL));
    assert(!s.presentation.speech.active);
    assert(rich4_special_reward_update(&s, 99, 10, 2500, NULL));
    assert(!s.presentation.special_reward.active && s.presentation.speech.active);
    assert(s.presentation.speech.current == rich4_character_shop_gift_speech(0, 150, 0));
    /* Original cap quirk: gift disappears and is announced even when full. */
    s.presentation.dynamic_objects[12].node = 2;
    s.state.players[0].tool_inventory[5] = 9;
    assert(rich4_road_gift_begin(&s, 0, 3000));
    assert(s.state.tool_amount[5] == 1 && s.state.players[0].tool_inventory[5] == 9);
    assert(s.presentation.dynamic_objects[12].node == 0);
    assert(rich4_road_reward_skip(&s, 3100));
    assert(rich4_special_reward_update(&s, 0, 0, 3100, NULL));
    assert(!s.presentation.special_reward.active);
    /* No public stock: keep the gift, with no pickup sound or presentation. */
    s.presentation.dynamic_objects[12].node = 2;
    s.state.tool_amount[5] = 0;
    rich4_audio_event_queue_reset(&s.audio_events);
    assert(!rich4_road_gift_begin(&s, 0, 4000));
    assert(s.presentation.dynamic_objects[12].node == 2);
    assert(!s.presentation.special_reward.active && s.audio_events.count == 0);
}

static void road_chest_timeline(void)
{
    GameSession s;
    MapData map = {0};
    Rich4AudioEvent sound;
    memset(&s, 0, sizeof(s));
    s.state.active = 1;
    s.state.player_count = 1;
    s.state.players[0].play_state = 1;
    s.state.players[0].node = 2;
    s.state.players[0].points = 100;
    s.presentation.dynamic_objects[13].type = 14;
    s.presentation.dynamic_objects[13].node = 3;
    assert(!rich4_road_chest_begin(&s, &map, 0, 1000));
    s.presentation.dynamic_objects[13].node = 2;
    assert(rich4_road_chest_begin(&s, &map, 0, 1000));
    assert(s.presentation.dynamic_objects[13].node == 0);
    assert(s.presentation.dynamic_objects[13].type == 14);
    assert(s.state.players[0].points == 100);
    assert(rich4_audio_event_pop(&s.audio_events, &sound));
    assert(sound.type == RICH4_AUDIO_EVENT_ROAD_CHEST);
    assert(!rich4_road_chest_begin(&s, &map, 0, 1001));
    assert(rich4_special_reward_update(&s, 0, 0, 2499, NULL));
    assert(s.state.players[0].points == 100 && !s.presentation.speech.active);
    assert(rich4_special_reward_update(&s, 0, 0, 2500, NULL));
    assert(s.state.players[0].points == 600);
    assert(!s.presentation.special_reward.active && s.presentation.speech.active);
    assert(!rich4_special_reward_update(&s, 0, 0, 2501, NULL));
    assert(s.state.players[0].points == 600);
    s.presentation.dynamic_objects[13].node = 2;
    s.presentation.speech.active = 0;
    assert(rich4_road_chest_begin(&s, &map, 0, 3000));
    assert(rich4_road_reward_skip(&s, 3100));
    assert(rich4_special_reward_update(&s, 0, 0, 3100, NULL));
    assert(s.state.players[0].points == 1100);
    assert(!rich4_road_reward_skip(&s, 3101));
}

static void setup_fortune_route(GameSession *s, int id)
{
    GameEvent page;
    RouteEvent route={0};
    int i;
    memset(s,0,sizeof(*s));
    s->state.active=1; s->state.player_count=3; s->state.price_index=1;
    s->state.ai_rng_state=1; s->state.prison_node=5; s->state.hospital_node=6;
    for (i=0;i<3;i++) {
        s->state.players[i].play_state=1; s->state.players[i].character=i;
        s->state.players[i].node=2+i; s->state.players[i].linked_player=-1;
    }
    s->state.special_event_family=GAME_SPECIAL_EVENT_FORTUNE;
    s->state.special_event_actor=0;
    s->state.special_event_logical_id=s->state.special_event_resolved_id=id;
    s->state.special_event_phase=GAME_SPECIAL_PHASE_PREPARED;
    s->state.special_event_prepared_value=id==5 ? 0 : 3;
    s->flow.route_events.state.execution_started=1;
    s->flow.route_events.state.execution_actor=0;
    s->flow.route_events.state.waiting_for_command=1;
    s->flow.route_events.state.pending_accept=rich4_game_command_confirm(GAME_COMMAND_SOURCE_HUMAN,0);
    route.phase=ROUTE_EVENT_PHASE_STOP; route.node=2;
    assert(rich4_route_event_queue_push(&s->flow.route_events.queue,&route));
    page=rich4_game_event_special_event_presentation(0,477+id,66,GAME_SPECIAL_EVENT_FORTUNE);
    assert(rich4_special_event_begin_from_event(s,&page,1000));
}

/* Complete each speech separately, like the platform's skip handler. */
static void finish_passive_speeches(GameSession *s, uint32_t now)
{
    Rich4SpecialEventUpdateInput input={0};
    Rich4SpecialEventUpdateResult result;
    int guard=4;
    while (s->presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH ||
           s->presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH) {
        assert(guard-->0);
        memset(&s->presentation.speech,0,sizeof(s->presentation.speech));
        (void)rich4_special_event_update(s,&input,now,&result);
    }
}

static void human_fortune_callbacks_wait_for_choices(void)
{
    static GameSession s;
    Rich4SpecialEventUpdateInput input={0};
    Rich4SpecialEventUpdateResult output;
    uint32_t rng;
    setup_fortune_route(&s,5);
    s.state.players[1].card_inventory[0]=2;
    s.state.players[1].card_inventory[1]=3;
    s.state.players[2].card_inventory[0]=4;
    /* Capture the same inventories in the presentation's phase-0 snapshot. */
    {
        GameEvent page=rich4_game_event_special_event_presentation(0,482,66,GAME_SPECIAL_EVENT_FORTUNE);
        assert(rich4_special_event_begin_from_event(&s,&page,1000));
    }
    rng=s.state.ai_rng_state;
    assert(rich4_special_event_update(&s,&input,2600,&output));
    assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET);
    assert(s.presentation.special_event.fortune_card_target_player==1);
    assert(rich4_special_event_update(&s,&input,100000,&output));
    assert(s.presentation.special_event.fortune_card_target_player==1);
    assert(s.state.players[0].card_inventory[0]==0 && s.state.ai_rng_state==rng);
    assert(!rich4_special_event_choice_select(&s,3,100000)); /* Empty slot stays open. */
    assert(rich4_special_event_choice_select(&s,1,100000));
    assert(s.state.players[0].card_inventory[0]==3 && s.state.players[1].card_inventory[0]==2);
    assert(s.presentation.special_event.fortune_card_target_player==2);
    assert(s.flow.route_events.state.waiting_for_command);
    assert(rich4_special_event_choice_select(&s,-1,100000));
    (void)rich4_special_event_update(&s,&input,100000,&output);
    assert(s.presentation.special_event.commit_seen);
    assert(s.state.special_event_phase==GAME_SPECIAL_PHASE_IDLE);
    assert(s.state.players[2].card_inventory[0]==4);
    assert(s.presentation.special_event.cards_gained==2); /* Includes canceled victim. */
    assert(!s.flow.route_events.state.waiting_for_command);

    setup_fortune_route(&s,6);
    s.state.players[0].card_inventory[0]=19;
    s.state.players[0].god_land_modifier=60;
    assert(rich4_special_event_update(&s,&input,2600,&output));
    rng=s.state.ai_rng_state;
    assert(rng==1103527590u); /* Modifier evaluated once, before the passive chain. */
    assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_CARD);
    assert(s.state.players[0].card_inventory[0]==19);
    assert(rich4_special_event_update(&s,&input,4100,&output));
    assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET);
    assert(rich4_special_event_update(&s,&input,100000,&output));
    assert(!s.state.players[0].days_disappearing && !s.state.players[2].days_disappearing);
    assert(rich4_special_event_choice_select(&s,2,100000));
    assert(!s.presentation.special_event.commit_seen);
    assert(s.presentation.speech.active && s.presentation.speech.player==0);
    assert(!s.state.players[2].days_disappearing);
    finish_passive_speeches(&s,100000);
    assert(s.presentation.special_event.commit_seen);
    assert(s.state.players[2].days_disappearing==3 && !s.state.players[0].days_disappearing);
    assert(s.presentation.special_event.reaction_player==2);
    assert(s.state.players[0].card_inventory[0]==0);
    /* The route did not evaluate 0x44B896 a second time. A day reaction of
     * three also consumes no random word. */
    assert(s.state.ai_rng_state==rng);
}

static void news29_human_passive_target_is_operator(void)
{
    static GameSession s;
    Rich4SpecialEventUpdateInput input = {0};
    Rich4SpecialEventUpdateResult result;
    GameEvent page;
    int immunity;
    for (immunity=0;immunity<2;immunity++) {
        setup_fortune_route(&s,6);
        s.state.special_event_family = GAME_SPECIAL_EVENT_NEWS;
        s.state.special_event_logical_id = s.state.special_event_resolved_id = 29;
        s.state.special_event_target_mask = (2u<<24) | (1u<<8) | 2u;
        s.state.players[1].card_inventory[0] = immunity ? 21 : 19;
        page = rich4_game_event_special_event_presentation(0,470,66,GAME_SPECIAL_EVENT_NEWS);
        assert(rich4_special_event_begin_from_event(&s,&page,1000));
        assert(rich4_special_event_update(&s,&input,3400,&result));
        assert(s.presentation.special_event.phase == RICH4_SPECIAL_PRESENT_PASSIVE_CARD);
        assert(s.presentation.special_event.reaction_player == 1);
        if (!immunity) {
            assert(rich4_special_event_update(&s,&input,4900,&result));
            assert(s.presentation.special_event.phase == RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET);
            assert(!rich4_special_event_choice_select(&s,1,4900));
            assert(rich4_special_event_choice_select(&s,0,4900));
        }
        (void)rich4_special_event_update(&s,&input,4900,&result);
        finish_passive_speeches(&s,4900);
        assert(s.presentation.special_event.commit_seen);
        assert(s.state.players[1].card_inventory[0] == 0);
        assert(s.presentation.tool_animation_count == (immunity ? 0 : 1));
        if (!immunity)
            assert(s.presentation.tool_animation[0].confinement_days == 5);
        assert(s.presentation.special_event.pending_confinement_days[1] == 0);
    }
}

static void news_area_requires_projected_anchors_after_page(void)
{
    static GameSession s;
    const GameNewsAreaAnchor anchors[] = {{2001,220,220},{0x8001,230,230}};
    Rich4SpecialEventUpdateInput input = {0};
    Rich4SpecialEventUpdateResult result;
    GameEvent page;
    uint32_t rng;
    setup_fortune_route(&s,6);
    s.state.special_event_family = GAME_SPECIAL_EVENT_NEWS;
    s.state.special_event_logical_id = s.state.special_event_resolved_id = 4;
    s.state.special_event_prepared_value = 0;
    s.state.special_event_target_mask = 0;
    s.state.land_count = 1;
    s.state.land_owner[0] = 1; s.state.land_level[0] = 2;
    s.state.tool_land_x[0] = 800; s.state.tool_land_y[0] = 900;
    page = rich4_game_event_special_event_presentation(0,445,66,GAME_SPECIAL_EVENT_NEWS);
    assert(rich4_special_event_begin_from_event(&s,&page,1000));
    rng = s.state.ai_rng_state;
    assert(!rich4_special_event_prepare_news_area_view(&s,3399,0));
    assert(s.state.ai_rng_state == rng);
    assert(rich4_special_event_prepare_news_area_view(&s,3400,0));
    assert(s.state.ai_rng_state != rng && s.presentation.view_world_x == 800);
    rng = s.state.ai_rng_state;
    assert(rich4_special_event_prepare_news_area_view(&s,3400,0));
    assert(s.state.ai_rng_state == rng);
    input.news_area_anchors = anchors;
    input.news_area_count = 2;
    input.news_area_available = 1;
    (void)rich4_special_event_update(&s,&input,3400,&result);
    assert(s.presentation.special_event.commit_seen);
    assert(s.state.land_owner[0] == -1);
    assert(s.presentation.special_event.target_mask & (1u<<16));
    assert(s.presentation.special_event.pending_confinement_days[0] == 0);
    /* The first pending victim has already entered the app admission queue. */
    assert(s.presentation.tool_animation_count == 1);
}

static void fortune_bank_theft_has_no_reaction_rng(void)
{
    static GameSession s;
    Rich4SpecialEventReactionResult reaction;
    Rich4Rng expected={1}, actual;
    setup_fortune_route(&s,4);
    s.presentation.special_event.pre_state.players[1].bank=1000;
    assert(!rich4_special_event_prepare_reaction(&s,3000,&reaction));
    assert(reaction.action==RICH4_SPECIAL_EVENT_REACTION_NONE);
    assert(!s.presentation.speech.active && s.state.ai_rng_state==1);
    actual.state=s.state.ai_rng_state;
    assert(rich4_rng_original_state_range(&actual,3)==
           rich4_rng_original_state_range(&expected,3));
}

static void fortune_money_reaction_rng(void)
{
    static GameSession s;
    const int amounts[]={0,1000,2000,4999,5000,8999,9000,10000};
    int kind, i;
    for (kind=0;kind<3;kind++) for (i=0;i<8;i++) {
        Rich4SpecialEventReactionResult reaction;
        setup_fortune_route(&s,kind ? 14 : 20);
        s.presentation.special_event.committed_amount=amounts[i];
        s.presentation.special_event.value=amounts[i];
        if (kind==2) s.presentation.special_event.defense=GAME_SPECIAL_DEFENSE_BLOCKED;
        (void)rich4_special_event_prepare_reaction(&s,3000,&reaction);
        assert(s.state.ai_rng_state==(amounts[i]>=5000 && amounts[i]<9000 ? 1103527590u : 1u));
    }
}

static void news_reward_reaction_rng(void)
{
    static GameSession s;
    Rich4SpecialEventReactionResult reaction;
    int id;
    for (id=8;id<=10;id++) {
        setup_fortune_route(&s,20);
        s.presentation.special_event.family=GAME_SPECIAL_EVENT_NEWS;
        s.presentation.special_event.resolved_id=id;
        s.presentation.special_event.player_amount[0]=id==9 ? 5000 : 10000;
        (void)rich4_special_event_prepare_reaction(&s,3000,&reaction);
        assert(s.state.ai_rng_state==(id==9 ? 1103527590u : 1u));
    }
    setup_fortune_route(&s,20);
    s.presentation.special_event.family=GAME_SPECIAL_EVENT_NEWS;
    s.presentation.special_event.resolved_id=9;
    s.presentation.special_event.player_amount[0]=-10000;
    assert(!rich4_special_event_prepare_reaction(&s,3000,&reaction));
    assert(!s.presentation.speech.active && s.state.ai_rng_state==1);

    setup_fortune_route(&s,20);
    s.presentation.special_event.family=GAME_SPECIAL_EVENT_NEWS;
    s.presentation.special_event.resolved_id=9;
    s.state.price_index=500000;
    s.presentation.special_event.player_amount[0]=-1794967296;
    /* Wrapped 5000*P is the middle tier; wrapped 9000*P is 205032704. */
    assert(rich4_special_event_prepare_reaction(&s,3000,&reaction));
    assert(s.presentation.speech.active && s.state.ai_rng_state==1103527590u);
}

static void magic_fortune_waits_for_callback(void)
{
    static GameSession s;
    Rich4SpecialEventUpdateInput input={0};
    Rich4SpecialEventUpdateResult result;
    GameEvent page;
    int i;
    setup_fortune_route(&s,5);
    s.state.players[1].card_inventory[0]=1;
    s.state.land_count=3;
    for (i=0;i<3;i++) { s.state.land_owner[i]=0; s.state.land_level[i]=1; }
    for (i=0;i<GAME_FORTUNE_EVENT_COUNT;i++) s.state.fortune_event_order[i]=(uint8_t)i;
    s.state.fortune_event_order[0]=5; s.state.fortune_event_order[5]=1;
    s.state.fortune_event_order[1]=0;
    s.state.fortune_event_cursor=1;
    s.flow.route_events.state.magic_fortune_sequence=1;
    s.flow.route_events.state.magic_fortune_target=0;
    s.flow.route_events.state.magic_fortunes_left=2;
    s.flow.route_events.state.magic_fortune_targets_left=1;
    page=rich4_game_event_special_event_presentation(0,482,66,GAME_SPECIAL_EVENT_FORTUNE);
    assert(rich4_special_event_begin_from_event(&s,&page,1000));
    assert(rich4_special_event_update(&s,&input,2600,&result));
    assert(rich4_special_event_choice_select(&s,-1,2600));
    assert(rich4_special_event_update(&s,&input,2600,&result));
    assert(s.state.fortune_event_cursor==1 && s.state.special_event_phase==GAME_SPECIAL_PHASE_IDLE);
    assert(s.state.ai_rng_state==1103527590u); /* birthday reward speech first */
    assert(s.flow.route_events.state.waiting_for_presentation);
    input.speech_active=1;
    assert(rich4_special_event_update(&s,&input,3000,&result));
    assert(s.state.fortune_event_cursor==1);
    input.speech_active=0; s.presentation.speech.active=0;
    assert(rich4_special_event_update(&s,&input,3000,&result));
    assert(s.presentation.special_event.until==3800);
    assert(rich4_special_event_update(&s,&input,3799,&result));
    assert(s.state.fortune_event_cursor==1 && s.state.ai_rng_state==1103527590u);
    assert(rich4_special_event_update(&s,&input,3800,&result));
    assert(result.action==RICH4_SPECIAL_EVENT_UPDATE_BEGIN);
    assert(s.state.fortune_event_cursor==2 && s.state.special_event_resolved_id==0);
    assert(s.state.ai_rng_state==2524885223u && s.state.special_event_target_mask==2u);
}

static void fortune_god_and_passive_callback_order(void)
{
    static GameSession s;
    Rich4SpecialEventUpdateInput input={0};
    Rich4SpecialEventUpdateResult result;
    int ai, immune;
    for (ai=0;ai<2;ai++) for (immune=0;immune<2;immune++) {
        uint32_t rng;
        int victim=ai ? 1 : 2;
        setup_fortune_route(&s,6);
        s.state.players[0].play_state=ai ? 2 : 1;
        s.state.players[0].is_ai=ai;
        s.state.players[0].god_land_modifier=-1;
        s.state.players[0].card_inventory[0]=immune ? 21 : 19;
        assert(rich4_special_event_update(&s,&input,2600,&result));
        assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_GOD_NOTICE);
        assert(s.state.ai_rng_state==1 && s.state.players[0].card_inventory[0]);
        assert(rich4_special_event_update(&s,&input,4099,&result));
        assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_GOD_NOTICE);
        assert(rich4_special_event_update(&s,&input,4100,&result));
        assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_CARD);
        rng=s.state.ai_rng_state;
        assert(rng==((ai && !immune) ? 1103527590u : 1u));
        assert(rich4_special_event_update(&s,&input,5599,&result));
        assert(s.state.players[0].card_inventory[0]);
        assert(rich4_special_event_update(&s,&input,5600,&result));
        if (!immune) {
            if (ai) {
                assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_REDIRECT_NOTICE);
                assert(s.presentation.special_event.fortune_redirect_target==victim);
                assert(rich4_special_event_update(&s,&input,7099,&result));
                assert(s.state.players[0].card_inventory[0]);
                assert(rich4_special_event_update(&s,&input,7100,&result));
            } else {
                assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_FORTUNE_REDIRECT_TARGET);
                assert(rich4_special_event_choice_select(&s,victim,5600));
            }
        }
        assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_ACTOR_SPEECH);
        assert(s.presentation.speech.active && s.presentation.speech.player==0);
        assert(s.presentation.speech.current==(immune ? rich4_character_absolution_speech(0) :
            rich4_character_insurance_card_speech(0,0)));
        assert(!s.state.players[victim].days_disappearing && !s.presentation.special_event.commit_seen);
        assert(s.state.players[0].card_inventory[0]==0 && s.state.ai_rng_state==rng);
        input.speech_active=1;
        assert(rich4_special_event_update(&s,&input,100000,&result));
        assert(!s.presentation.special_event.commit_seen);
        input.speech_active=0; s.presentation.speech.active=0;
        if (!immune) {
            assert(rich4_special_event_update(&s,&input,100000,&result));
            assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_PASSIVE_VICTIM_SPEECH);
            assert(s.presentation.speech.player==victim);
            assert(s.presentation.speech.current==rich4_character_insurance_card_speech(victim,2));
            assert(!s.state.players[victim].days_disappearing);
        }
        finish_passive_speeches(&s,100000);
        assert(s.presentation.special_event.commit_seen && !s.state.players[0].days_disappearing);
        assert(s.state.players[victim].days_disappearing==(immune ? 0 : 6));
        assert(s.state.ai_rng_state==(immune ? 1u : ai ? 2524885223u : 1103527590u));
    }
}

static void news4_dynamic_removal_is_ordered_and_atomic(void)
{
    static GameSession s;
    static GameState before;
    static GameMapDynamicObjectState objects_before[GAME_MAP_DYNAMIC_OBJECT_COUNT];
    MapRouteNode routes[7]={{0}};
    MapData map={0};
    GameCommand confirm, wrong;
    GameEvent page;
    const GameNewsAreaAnchor anchors[]={
        {0x8100,150,250},{0x8300,150,120},{0x9100,150,200},
        {0x8d00,150,210},{0x8e00,320,220}
    };
    int i;
    setup_fortune_route(&s,6);
    s.state.special_event_family=GAME_SPECIAL_EVENT_NEWS;
    s.state.special_event_logical_id=s.state.special_event_resolved_id=4;
    s.state.special_event_prepared_value=0; s.state.special_event_target_mask=1;
    s.state.land_count=1; s.state.land_level[0]=1;
    s.state.ai_rng_state=0;
    page=rich4_game_event_special_event_presentation(0,445,66,GAME_SPECIAL_EVENT_NEWS);
    assert(rich4_special_event_begin_from_event(&s,&page,1000));
    map.routes=routes; map.route_count=7;
    for (i=1;i<7;i++) { routes[i].x=(int16_t)(i*1000); routes[i].next[0]=1; }
    rich4_app_init_dynamic_object_pool(&s.presentation);
    s.presentation.dynamic_objects[0].node=1;
    s.presentation.dynamic_objects[2].node=2;
    s.presentation.dynamic_objects[16].node=3;
    s.presentation.dynamic_objects[12].node=4;
    s.presentation.dynamic_objects[13].node=5;
    s.state.tool_placed_objects[0].kind=16;
    s.state.tool_placed_objects[0].node=3;
    confirm=s.flow.route_events.state.pending_accept;
    before=s.state;
    memcpy(objects_before,s.presentation.dynamic_objects,sizeof(objects_before));
    wrong=confirm; wrong.player=1;
    assert(rich4_special_event_submit_page_command_with_area(
        &s,&wrong,3400,anchors,5,1,&map)==ROUTE_EVENT_COMMAND_UNEXPECTED);
    assert(memcmp(&s.state,&before,sizeof(before))==0);
    assert(memcmp(s.presentation.dynamic_objects,objects_before,sizeof(objects_before))==0);
    assert(rich4_special_event_submit_page_command_with_area(
        &s,&confirm,3400,anchors,5,1,NULL)==ROUTE_EVENT_COMMAND_INVALID);
    assert(memcmp(&s.state,&before,sizeof(before))==0);
    assert(memcmp(s.presentation.dynamic_objects,objects_before,sizeof(objects_before))==0);
    assert(rich4_special_event_submit_page_command_with_area(
        &s,&confirm,3400,anchors,5,1,&map)==ROUTE_EVENT_COMMAND_APPLIED);
    /* Row 120 relocates type 3 to type 4 at node 6 (only free node).
     * Row 200 clears the roadblock, row 210 clears the chest, then type 1
     * moves to type 2 at node 2 using the second rand (21468 % 3 == 0).
     * Array-order iteration would choose different destinations. */
    assert(s.presentation.dynamic_objects[2].node==0);
    assert(s.presentation.dynamic_objects[3].node==6);
    assert(s.presentation.dynamic_objects[0].node==0);
    assert(s.presentation.dynamic_objects[1].node==2);
    assert(s.presentation.dynamic_objects[12].node==0);
    assert(s.presentation.dynamic_objects[13].node==5);
    assert(s.presentation.dynamic_objects[16].node==0);
    assert(s.state.tool_placed_objects[0].kind==0 && s.state.tool_amount[1]==1);
    assert(s.state.ai_rng_state==3554416254u);
}

static void news_tax_other_bankrupt_player_preserves_route_actor(void)
{
    static GameSession s;
    GameEvent page;
    GameCommand command;
    setup_fortune_route(&s,6);
    s.state.special_event_family=GAME_SPECIAL_EVENT_NEWS;
    s.state.special_event_logical_id=s.state.special_event_resolved_id=12;
    s.state.special_event_prepared_value=0;
    s.state.special_event_player_amount[1]=100;
    s.state.special_event_player_amount[2]=10;
    s.state.players[1].cash=5; s.state.players[2].cash=100;
    s.state.players[1].attached_timed_bomb_slot=-1;
    page=rich4_game_event_special_event_presentation(0,453,66,GAME_SPECIAL_EVENT_NEWS);
    assert(rich4_special_event_begin_from_event(&s,&page,1000));
    command=s.flow.route_events.state.pending_accept;
    assert(rich4_special_event_submit_page_command(&s,&command,3400)==ROUTE_EVENT_COMMAND_APPLIED);
    assert(s.state.players[1].play_state==0 && s.state.players[0].play_state==1);
    assert(s.state.players[2].cash==90 && s.state.lottery_pool==15);
    assert(!s.flow.route_events.state.waiting_for_command);
}

static void fortune_passive_cancel_and_block(void)
{
    static GameSession s;
    Rich4SpecialEventUpdateInput input={0};
    Rich4SpecialEventUpdateResult result;
    setup_fortune_route(&s,6);
    s.state.players[0].card_inventory[0]=19;
    assert(rich4_special_event_update(&s,&input,2600,&result));
    assert(rich4_special_event_update(&s,&input,4100,&result));
    assert(rich4_special_event_choice_select(&s,-1,4100));
    assert(rich4_special_event_update(&s,&input,4100,&result));
    assert(s.state.players[0].days_disappearing==3 && s.state.players[0].card_inventory[0]==19);
    assert(s.state.ai_rng_state==1);
    setup_fortune_route(&s,6);
    s.state.players[0].play_state=2;
    s.state.players[0].card_inventory[0]=19;
    s.state.players[1].days_in_hospital=s.state.players[2].days_in_hospital=1;
    assert(rich4_special_event_update(&s,&input,2600,&result));
    assert(s.presentation.special_event.commit_seen && s.state.players[0].days_disappearing==3);
    assert(s.state.players[0].card_inventory[0]==19 && s.state.ai_rng_state==1);
    setup_fortune_route(&s,6);
    s.state.players[0].card_inventory[0]=21;
    s.state.players[0].god_land_modifier=101;
    assert(rich4_special_event_update(&s,&input,2600,&result));
    assert(s.presentation.special_event.phase==RICH4_SPECIAL_PRESENT_GOD_NOTICE);
    assert(rich4_special_event_update(&s,&input,4100,&result));
    assert(s.presentation.special_event.commit_seen && !s.state.players[0].days_disappearing);
    assert(s.state.players[0].card_inventory[0]==21 && s.state.ai_rng_state==1);
    setup_fortune_route(&s,8);
    s.state.special_event_prepared_value=10;
    s.state.players[0].god_investment_modifier=-1;
    s.state.player_stock_amount[0][0]=10; s.state.stocks[0].f20=1.0f;
    assert(rich4_special_event_update(&s,&input,2600,&result));
    assert(s.presentation.special_event.commit_seen && s.state.player_stock_amount[0][0]==9);
    assert(s.state.lottery_pool==1 && s.state.ai_rng_state==1);
}

int main(void)
{
    news4_dynamic_removal_is_ordered_and_atomic();
    news_tax_other_bankrupt_player_preserves_route_actor();
    fortune_passive_cancel_and_block();
    fortune_bank_theft_has_no_reaction_rng();
    fortune_money_reaction_rng();
    news_reward_reaction_rng();
    magic_fortune_waits_for_callback();
    fortune_god_and_passive_callback_order();
    news29_human_passive_target_is_operator();
    news_area_requires_projected_anchors_after_page();
    human_fortune_callbacks_wait_for_choices();
    fortune_claim_after_callback_speech();
    deferred_confinement_callback(GAME_SPECIAL_EVENT_NEWS, 29, GAME_CONFINEMENT_PRISON);
    deferred_confinement_callback(GAME_SPECIAL_EVENT_NEWS, 4, GAME_CONFINEMENT_HOSPITAL);
    deferred_confinement_callback(GAME_SPECIAL_EVENT_FORTUNE, 33, GAME_CONFINEMENT_PRISON);
    deferred_confinement_callback(GAME_SPECIAL_EVENT_FORTUNE, 12, GAME_CONFINEMENT_HOSPITAL);
    company_control_notice_timeline();
    GameSession session;
    Rich4AudioEventQueue audio_events;
    GameEvent event;

    road_gift_timeline();
    road_chest_timeline();
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    rich4_audio_event_queue_init(&audio_events);

    event = rich4_game_event_special_place_request(0, 10, 536, 100, 7);
    rich4_special_reward_begin(&session, &event, 2, 50, 1000, &audio_events);
    assert(session.presentation.special_reward.active);
    assert(session.presentation.special_reward.action == 10);
    assert(pop_type(&audio_events) == RICH4_AUDIO_EVENT_SPECIAL_REWARD_ANIMATION);
    assert(rich4_special_reward_update(
        &session, 2, 50, 1025, &audio_events));
    assert(session.presentation.special_reward.frame == 0);

    event = rich4_game_event_bank_rejection(0, 3, 250, 8);
    rich4_bank_rejection_begin(&session, &event, 2000, &audio_events);
    assert(session.presentation.bank_rejection.active);
    assert(session.presentation.bank_rejection.days == 3);
    assert(audio_events.count == 0);

    event = rich4_game_event_company_stock_issue_request(
        0, 2, rich4_game_domain_ref(GAME_DOMAIN_STOCK, 4), 20, 6, 9);
    rich4_company_issue_begin(&session, &event);
    assert(session.presentation.company_issue.active);
    assert(session.presentation.company_issue.company == 2);
    assert(session.presentation.company_issue.stock == 4);
    assert(session.presentation.company_issue.quantity == 6);

    session.presentation.special_event.active = 1;
    session.presentation.special_event.phase = RICH4_SPECIAL_PRESENT_PAGE;
    session.presentation.special_event.until = 500;
    rich4_special_event_skip_wait(&session, 700, &audio_events);
    assert(session.presentation.special_event.until == 700);
    assert(pop_type(&audio_events) == RICH4_AUDIO_EVENT_SPEECH_STOP);

    session.presentation.special_event.phase = RICH4_SPECIAL_PRESENT_REACTION;
    session.presentation.speech.active = 1;
    rich4_special_event_skip_wait(&session, 900, &audio_events);
    assert(session.presentation.speech.skip_requested);
    session.presentation.special_event.phase = RICH4_SPECIAL_PRESENT_CONSEQUENCE;
    session.presentation.special_event.until = 2000;
    rich4_special_event_skip_wait(&session, 1000, &audio_events);
    assert(session.presentation.special_event.until == 2000);
    assert(audio_events.count == 0);

    memset(&session.presentation.special_event, 0,
           sizeof(session.presentation.special_event));
    session.state.special_event_actor = 0;
    session.state.special_event_logical_id = 4;
    session.state.special_event_resolved_id = 4;
    session.state.special_event_prepared_value = 12;
    session.state.special_event_target_mask = (2u << 24) | (0x0au << 16);
    event = rich4_game_event_special_event_presentation(
        0, 445, 66, GAME_SPECIAL_EVENT_NEWS);
    assert(rich4_special_event_begin(&session, &event,
                                     GAME_CONFINEMENT_HOSPITAL, 1, 1200));
    assert(session.presentation.special_event.active);
    assert(session.presentation.special_event.family == GAME_SPECIAL_EVENT_NEWS);
    assert(session.presentation.special_event.resource == 445);
    assert(session.presentation.special_event.reaction_player == -1);
    assert(session.presentation.special_event.confinement ==
           GAME_CONFINEMENT_HOSPITAL);
    assert(session.presentation.special_event.first_confinement);
    assert(session.presentation.special_event.reaction_mask == 0x0au);
    assert(session.presentation.special_event.phase == RICH4_SPECIAL_PRESENT_PAGE);
    assert(session.presentation.special_event.tick == 1200);
    assert(session.presentation.special_event.until == 3600);
    assert(session.presentation.special_event.pre_state_valid);
    assert(session.presentation.special_event.original_players_valid);

    assert(rich4_special_event_confinement_kind(
               GAME_SPECIAL_EVENT_NEWS, 4) == GAME_CONFINEMENT_HOSPITAL);
    assert(rich4_special_event_confinement_kind(
               GAME_SPECIAL_EVENT_FORTUNE, 33) == GAME_CONFINEMENT_PRISON);
    session.state.players[0].days_in_hospital = 5;
    assert(rich4_special_event_confinement_days(
               &session, 0, GAME_CONFINEMENT_HOSPITAL) == 5);

    memset(&session.presentation.speech, 0,
           sizeof(session.presentation.speech));
    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_FORTUNE;
    session.presentation.special_event.resolved_id = 11;
    session.presentation.special_event.reaction_player = 0;
    session.presentation.special_event.confinement = 0;
    {
        Rich4SpecialEventReactionResult reaction;
        assert(rich4_special_event_prepare_reaction(
                   &session, 1500, &reaction));
        assert(reaction.action == RICH4_SPECIAL_EVENT_REACTION_SPEECH);
        assert(reaction.player == 0);
        assert(session.presentation.speech.active);
    }

    session.presentation.special_event.family = GAME_SPECIAL_EVENT_FORTUNE;
    session.presentation.special_event.resolved_id = 5;
    session.presentation.special_event.actor = 0;
    session.presentation.special_event.pre_state = session.state;
    session.presentation.special_event.pre_state_valid = 1;
    session.state.players[0].card_inventory[0] = 1;
    session.state.players[0].card_inventory[1] = 1;
    {
        Rich4GameEventFields fields;
        fields.player = 0;
        fields.subject = 5;
        fields.value = 37;
        fields.value2 = GAME_SPECIAL_DEFENSE_BLOCKED;
        fields.flags = GAME_SPECIAL_EVENT_FORTUNE;
        fields.subject_ref = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        fields.target_ref = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
        event = rich4_game_event_special_event_committed_from_fields(fields);
    }
    rich4_special_event_capture_commit(&session, &event);
    assert(session.presentation.special_event.commit_seen);
    assert(session.presentation.special_event.committed_amount == 37);
    assert(session.presentation.special_event.defense ==
           GAME_SPECIAL_DEFENSE_BLOCKED);
    assert(session.presentation.special_event.cards_gained == 0);

    memset(&session.presentation.special_event, 0,
           sizeof(session.presentation.special_event));
    session.state.player_count = 3;
    session.state.special_event_actor = 0;
    session.state.special_event_logical_id = 5;
    session.state.special_event_resolved_id = 5;
    session.state.players[0].play_state = 1;
    session.state.players[1].play_state = 2;
    session.state.players[2].play_state = 1;
    session.state.players[1].card_inventory[0] = 1;
    session.state.players[2].card_inventory[0] = 1;
    event = rich4_game_event_special_event_presentation(
        0, 445, 66, GAME_SPECIAL_EVENT_FORTUNE);
    assert(rich4_special_event_begin(&session, &event, 0, 0, 4000));
    assert(session.presentation.special_event.fortune_card_target_mask ==
           ((1u << 1) | (1u << 2)));
    assert(rich4_special_event_begin_fortune_card_targets(&session, 5000));
    assert(session.presentation.special_event.phase ==
           RICH4_SPECIAL_PRESENT_FORTUNE_CARD_TARGET);
    assert(session.presentation.special_event.fortune_card_target_player == 1);
    assert(session.presentation.special_event.until == 5000);
    assert(rich4_special_event_advance_fortune_card_target(&session, 6000));
    assert(session.presentation.special_event.fortune_card_target_player == 2);
    assert(!rich4_special_event_advance_fortune_card_target(&session, 7000));
    assert(session.presentation.special_event.fortune_card_target_player == -1);

    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_FORTUNE;
    session.presentation.special_event.resolved_id = 7;
    assert(rich4_special_event_begin_timed_phase(
        &session, RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION,
        4, 50, 2000, 2));
    assert(session.presentation.special_event.phase ==
           RICH4_SPECIAL_PRESENT_FORTUNE_ANIMATION);
    rich4_special_event_update_timeline(&session, 4, 50, 2000);
    rich4_special_event_update_timeline(&session, 4, 50, 2050);
    assert(session.presentation.special_event.frame == 1);
    assert(session.presentation.special_event.scene_refreshed);

    /* The original confinement FLCs switch the scene at one-based frames
     * 18 (prison) and 30 (hospital), not when the animation finishes. */
    memset(&session.presentation.special_event, 0,
           sizeof(session.presentation.special_event));
    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_NEWS;
    session.presentation.special_event.resolved_id = 4;
    session.presentation.special_event.reaction_player = 0;
    session.state.players[0].days_in_hospital = 5;
    session.presentation.special_event.pre_state = session.state;
    session.presentation.special_event.pre_state_valid = 1;
    session.state.players[0].days_in_hospital = 2;
    assert(rich4_special_event_begin_timed_phase(
        &session, RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION,
        40, 10, 10000, 18));
    assert(rich4_special_event_render_state(&session) ==
           &session.presentation.special_event.pre_state);
    for (int frame = 0; frame <= 16; frame++)
        rich4_special_event_update_timeline(&session, 40, 10, 10000 + frame * 10);
    rich4_special_event_update_timeline(&session, 40, 10, 10169);
    assert(session.presentation.special_event.frame == 16);
    assert(!session.presentation.special_event.scene_refreshed);
    rich4_special_event_update_timeline(&session, 40, 10, 10170);
    assert(session.presentation.special_event.frame == 17);
    assert(session.presentation.special_event.scene_refreshed);
    /* News id 4 has several victims: the pre-state container remains the
     * render source, but the refreshed victim is copied into it. */
    assert(rich4_special_event_render_state(&session) ==
           &session.presentation.special_event.pre_state);
    assert(session.presentation.special_event.pre_state.players[0]
               .days_in_hospital == 2);

    memset(&session.presentation.special_event, 0,
           sizeof(session.presentation.special_event));
    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_FORTUNE;
    session.presentation.special_event.resolved_id = 12;
    session.presentation.special_event.pre_state = session.state;
    session.presentation.special_event.pre_state_valid = 1;
    assert(rich4_special_event_begin_timed_phase(
        &session, RICH4_SPECIAL_PRESENT_CONFINEMENT_ANIMATION,
        40, 10, 11000, 30));
    for (int frame = 0; frame <= 28; frame++)
        rich4_special_event_update_timeline(&session, 40, 10, 11000 + frame * 10);
    rich4_special_event_update_timeline(&session, 40, 10, 11289);
    assert(session.presentation.special_event.frame == 28);
    assert(!session.presentation.special_event.scene_refreshed);
    rich4_special_event_update_timeline(&session, 40, 10, 11290);
    assert(session.presentation.special_event.frame == 29);
    assert(session.presentation.special_event.scene_refreshed);
    assert(rich4_special_event_render_state(&session) == &session.state);

    session.presentation.special_event.resolved_id = 0;
    assert(rich4_special_event_start_reaction_delay(&session, 3000));
    assert(session.presentation.special_event.phase ==
           RICH4_SPECIAL_PRESENT_REACTION_DELAY);
    assert(session.presentation.special_event.until == 3300);
    assert(rich4_special_event_finish_callback(&session, 4000));
    assert(session.presentation.special_event.phase ==
           RICH4_SPECIAL_PRESENT_POST_HOLD);
    assert(session.presentation.special_event.until == 4800);

    memset(&session.presentation.speech, 0,
           sizeof(session.presentation.speech));
    memset(&session.presentation.special_event, 0,
           sizeof(session.presentation.special_event));
    session.presentation.special_event.active = 1;
    session.presentation.special_event.phase =
        RICH4_SPECIAL_PRESENT_REACTION_DELAY;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_FORTUNE;
    session.presentation.special_event.resolved_id = 11;
    session.presentation.special_event.reaction_player = 0;
    session.presentation.special_event.until = 5000;
    {
        Rich4SpecialEventUpdateInput input = {0};
        Rich4SpecialEventUpdateResult update;
        assert(rich4_special_event_update(
                   &session, &input, 4999, &update));
        assert(update.action == RICH4_SPECIAL_EVENT_UPDATE_WAIT);
        assert(rich4_special_event_update(
                   &session, &input, 5000, &update));
        assert(update.action == RICH4_SPECIAL_EVENT_UPDATE_REACTION);
        assert(update.reaction.action ==
               RICH4_SPECIAL_EVENT_REACTION_SPEECH);
    }

    session.presentation.has_view_override = 1;
    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_NEWS;
    session.presentation.special_event.resolved_id = 4;
    assert(rich4_special_event_finish_callback(&session, 1000) == 0);
    assert(session.presentation.special_event.active == 0);
    assert(session.presentation.has_view_override == 0);

    session.presentation.has_view_override = 1;
    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_NEWS;
    session.presentation.special_event.resolved_id = 5;
    assert(rich4_special_event_finish_callback(&session, 1000) == 0);
    assert(session.presentation.special_event.active == 0);
    assert(session.presentation.has_view_override == 1);

    session.presentation.has_view_override = 1;
    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_NEWS;
    session.presentation.special_event.resolved_id = 4;
    rich4_special_event_deactivate(&session);
    assert(session.presentation.special_event.active == 0);
    assert(session.presentation.has_view_override == 0);

    /* News 5 first shows the property, then 0x44A5C3 focuses its owner. */
    memset(&session, 0, sizeof(session));
    session.state.player_count = 2;
    session.state.price_index = 1;
    session.state.players[0].node = 1;
    session.state.players[1].node = 2;
    session.state.players[1].character = 1;
    session.state.tool_route_node_count = 3;
    session.state.tool_route_x[1] = 300; session.state.tool_route_y[1] = 400;
    session.state.tool_route_x[2] = 700; session.state.tool_route_y[2] = 800;
    session.presentation.has_view_override = 1;
    session.presentation.view_world_x = 1000;
    session.presentation.view_world_y = 1100;
    session.presentation.special_event.active = 1;
    session.presentation.special_event.family = GAME_SPECIAL_EVENT_NEWS;
    session.presentation.special_event.resolved_id = 5;
    session.presentation.special_event.reaction_player = 1;
    {
        Rich4SpecialEventReactionResult reaction;
        assert(rich4_special_event_prepare_reaction(&session, 100, &reaction));
        assert(session.presentation.speech.active && session.presentation.speech.player == 1);
        assert(session.presentation.view_world_x == 700 && session.presentation.view_world_y == 800);
        assert(session.presentation.has_view_override && session.state.current_player == 0);
    }
    puts("app special-presentation flow: ok");
    return 0;
}
