#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/bankruptcy_flow.h"
#include "app/property_flow.h"
#include "core/game_event_payload.h"
#include "core/route_event.h"
#include "core/route_event_executor.h"
#include "core/card.h"
#include "core/facility.h"

static void begin_insurance(GameSession *s, int control, int shares, int god, int outcome)
{
    RouteEvent stop;
    GameEventQueue events;
    GameEvent event;
    int i;
    memset(s, 0, sizeof(*s));
    s->state.active = 1; s->state.player_count = 3; s->state.price_index = 1;
    s->state.initial_fund = 100000;
    for (i = 0; i < 3; i++) {
        s->state.players[i].play_state = 1;
        s->state.players[i].cash = 100000;
        s->state.players[i].linked_player = -1;
        s->state.players[i].character = i;
    }
    s->state.players[0].play_state = control;
    s->state.players[0].is_ai = control == 2;
    s->state.players[0].god_info = god;
    s->state.players[0].days_assurance = 120;
    s->state.player_stock_amount[0][2] = shares;
    s->state.stock_company_owner[2] = 1;
    s->state.company_count = 1; s->state.company_type[0] = 4;
    s->state.company_stock_index[0] = 2; s->state.company_base_fee[0] = 1000;
    s->state.stock_company_value[2] = 1000000;
    memset(&stop, 0, sizeof(stop));
    stop.phase = ROUTE_EVENT_PHASE_STOP; stop.node = 20;
    stop.object_ref.raw_object_id = 6001;
    rich4_route_event_executor_reset(&s->flow.route_events);
    assert(rich4_route_event_queue_push(&s->flow.route_events.queue, &stop));
    assert(rich4_route_event_executor_begin(&s->flow.route_events, 0, 5));
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_step(&s->flow.route_events, &s->state, &events) ==
        ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_event_executor_resume_company_roulette(&s->flow.route_events,
        &s->state, outcome, &events) == ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND);
    while (rich4_game_event_queue_pop(&events, &event))
        if (event.type == GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED)
            rich4_property_begin_charge_presentation(s, &event, 100);
    assert(s->presentation.property.insurance_active);
    assert(s->state.players[0].days_assurance == ((120 + outcome) & 0x7f));
    assert(s->state.players[0].cash == 100000);
}

static void finish_insurance(GameSession *s)
{
    uint32_t now;
    for (now = 20000; s->presentation.property.insurance_active && now < 100000; now += 1500) {
        s->presentation.speech.active = 0;
        (void)rich4_property_update_charge_presentation(s, now);
    }
    assert(!s->presentation.property.insurance_active);
}

static void test_insurance_passive_flow(void)
{
    static GameSession s;
    int control, shares;
    for (control = 1; control <= 5; control += control == 1 ? 1 : 3) {
        for (shares = 0; shares <= 20; shares += 20) {
            begin_insurance(&s, control, shares, 0, 10);
            s.state.players[0].card_inventory[0] = 20;
            assert(rich4_property_update_charge_presentation(&s, 1599));
            assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_AMOUNT);
            assert(rich4_property_update_charge_presentation(&s, 1600));
            assert(rich4_property_update_charge_presentation(&s, 1601));
            if (control == 1) {
                assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_INSURANCE_FREE_CONFIRM);
                assert(rich4_property_insurance_choose(&s, 0, 1602));
            }
            assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_INSURANCE_CARD);
            assert(rich4_card_inventory_has(&s.state, 0, 19));
            assert(rich4_property_update_charge_presentation(&s, 1700));
            assert(s.state.players[0].cash == 100000);
            finish_insurance(&s);
            assert(!rich4_card_inventory_has(&s.state, 0, 19));
            assert(s.state.players[0].cash == 100000 && s.state.company_total_business[0] == 0);
            assert(s.state.players[0].days_assurance == 2);
        }
    }
    /* AI redirect card art and target notice both precede deduction. */
    begin_insurance(&s, 5, 0, 0, 10);
    s.state.players[0].card_inventory[0] = 19;
    s.state.players[0].relation_balance[2] = 50;
    assert(rich4_property_update_charge_presentation(&s, 1600));
    assert(rich4_property_update_charge_presentation(&s, 1601));
    assert(rich4_property_update_charge_presentation(&s, 1602));
    assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_INSURANCE_CARD);
    assert(rich4_property_update_charge_presentation(&s, 3102));
    assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_INSURANCE_REDIRECT_NOTICE);
    assert(s.presentation.property.insurance_target == 2 && s.state.players[2].cash == 100000);
    finish_insurance(&s);
    assert(s.state.players[0].cash == 100000 && s.state.players[2].cash == 90000);
    assert(s.state.company_total_business[0] == 10000 && s.state.players[0].days_assurance == 2);
    /* Human cancellation leaves the card; death's raw-slot rule overrides payer. */
    begin_insurance(&s, 1, 20, 0, 10);
    s.state.players[0].card_inventory[0] = 19;
    s.state.players[2].god_info = 14; s.state.players[2].god_type = 1;
    assert(rich4_property_update_charge_presentation(&s, 1600));
    assert(rich4_property_update_charge_presentation(&s, 1601));
    assert(rich4_property_update_charge_presentation(&s, 1602));
    assert(rich4_property_update_charge_presentation(&s, 3102));
    assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_INSURANCE_TARGET);
    assert(rich4_property_insurance_choose(&s, -1, 3103));
    assert(rich4_property_update_charge_presentation(&s, 3103));
    assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_INSURANCE_SUBSTITUTE_NOTICE);
    assert(rich4_property_update_charge_presentation(&s, 4602));
    assert(s.state.players[2].cash == 100000);
    finish_insurance(&s);
    assert(rich4_card_inventory_has(&s.state, 0, 18));
    assert(s.state.players[2].cash == 90000 && s.state.players[0].cash == 100000);
    /* Raw god byte 1 halves before payment and must not be applied twice. */
    begin_insurance(&s, 5, 0, 1, 10);
    s.state.players[0].god_type = 6;
    assert(s.presentation.property.amount == 5000 && s.presentation.property.god_type == 1);
    finish_insurance(&s);
    assert(s.state.players[0].cash == 95000 && s.state.company_total_business[0] == 5000);
    /* A bankrupt substitute pays only available money; the actor can continue. */
    begin_insurance(&s, 5, 0, 0, 10);
    s.state.players[2].god_info = 15;
    s.state.players[2].cash = 1000;
    finish_insurance(&s);
    assert(s.state.players[0].cash == 100000 && (s.state.players[0].play_state & 0xff));
    assert(!(s.state.players[2].play_state & 0xff));
    assert(s.state.company_total_business[0] == 1000 && s.state.players[0].days_assurance == 2);
    /* The issue is independent of exchange flags and rejects dreamwalking. */
    {
        int price, maximum;
        s.state.stock_company_available[2] = 500;
        s.state.stock_company_auxiliary[2] = -1;
        assert(rich4_company_max_issue_purchase(&s.state, 0, 0, &price, &maximum));
        assert(price == 100 && maximum == 500);
        s.state.players[0].days_sleep_walking = 1;
        assert(!rich4_company_max_issue_purchase(&s.state, 0, 0, &price, &maximum));
    }
}

static GameEvent charge_event(void)
{
    Rich4GameEventFields fields = {
        0, 0, 100, 0, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 0)
    };
    return rich4_game_event_property_charge_presentation_requested_from_fields(
        fields);
}

static void setup_resident(GameSession *s, int god, int ai)
{
    memset(s, 0, sizeof(*s));
    s->state.active = 1;
    s->state.player_count = 2;
    s->state.price_index = 1;
    s->state.players[0].play_state = ai ? 2 : 1;
    s->state.players[0].is_ai = ai;
    s->state.players[1].play_state = 1;
    s->state.players[0].linked_player = -1;
    s->state.players[1].linked_player = -1;
    s->state.players[0].god_info = god;
    s->state.players[0].god_type = god;
    s->state.land_count = 1;
    s->state.route_object_raw[3] = 2001;
    s->state.land_owner[0] = 1;
    s->state.land_level[0] = 4;
    s->state.land_price[0] = 100;
    s->state.land_price_per_level[0] = 10;
    s->state.land_value[0] = 140;
    s->state.fixed_asset_value[1] = 140;
    s->state.land_toll_by_level[0][4] = 101;
}

static void test_resident_notices(void)
{
    static GameSession s;
    int ai, i;
    const int gods[4] = {1,2,5,6};
    const int fees[4] = {50,0,151,202};
    for (ai = 0; ai < 2; ai++) {
        for (i = 0; i < 4; i++) {
            Rich4GameEventFields f = {0};
            GameEvent e;
            setup_resident(&s, gods[i], ai);
            f.player = 0; f.value = fees[i];
            f.subject_ref = rich4_game_domain_ref(GAME_DOMAIN_LAND, 0);
            f.target_ref = rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1);
            e = rich4_game_event_property_charge_presentation_requested_from_fields(f);
            rich4_property_begin_charge_presentation(&s, &e, 100);
            assert(s.presentation.property.god_original_amount == 101);
            assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_AMOUNT);
            assert(rich4_property_update_charge_presentation(&s, 1599));
            assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_AMOUNT);
            assert(rich4_property_update_charge_presentation(&s, 1600));
            assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_GOD_FEE);
            assert(!s.presentation.speech.active);
            assert(rich4_property_skip_wait(&s, 1700));
            assert(rich4_property_update_charge_presentation(&s, 1700));
            assert(s.presentation.speech.active == (gods[i] == 2));
            assert(s.presentation.property.phase == (gods[i] == 2 ?
                GAME_PROPERTY_PRESENT_COMMIT : GAME_PROPERTY_PRESENT_PAYER));
        }
        setup_resident(&s, 9, ai);
        assert(rich4_property_begin_god_landing(&s, 0, 3, 100));
        assert(s.state.land_level[0] == 5 && !s.presentation.speech.active);
        assert(!s.presentation.property.sound_pending && !s.deferred_events.count);
        assert(!rich4_property_update_charge_presentation(&s, 1600));
        assert(s.presentation.property.sound_pending == 2);
        assert(!s.presentation.speech.active && !s.presentation.property.max_level_pending);
        assert(s.deferred_events.count == 1);
        assert(rich4_game_event_tool_resource(&s.deferred_events.events[s.deferred_events.head]) == 523);
        setup_resident(&s, 10, ai);
        assert(rich4_property_begin_god_landing(&s, 0, 3, 100));
        assert(s.state.land_level[0] == 4);
        assert(!rich4_property_update_charge_presentation(&s, 1600));
        assert(s.state.land_level[0] == 3 && !s.presentation.speech.active);
        assert(rich4_game_event_tool_resource(&s.deferred_events.events[s.deferred_events.head]) == 526);
        setup_resident(&s, 12, ai);
        s.state.land_owner[0] = -1;
        s.state.land_level[0] = 0;
        s.state.land_value[0] = 100;
        assert(rich4_property_begin_god_landing(&s, 0, 3, 100));
        assert(s.state.land_owner[0] == 0 && !s.presentation.speech.active);
        assert(rich4_property_update_charge_presentation(&s, 1600));
        assert(s.presentation.speech.active && s.presentation.speech.type == 0);
        assert(!s.presentation.property.sound_pending);
    }
    /* 0x40B1EC: human chooses the kind even on another owner's empty facility. */
    setup_resident(&s, 9, 0);
    s.state.route_object_raw[3] = 4001;
    s.state.facility_count = 1;
    s.state.facility_owner[0] = 1;
    s.state.facility_price[0] = s.state.facility_value[0] = 100;
    s.state.facility_price_per_level[0] = 10;
    s.state.fixed_asset_value[1] = 100;
    assert(rich4_property_begin_god_landing(&s, 0, 3, 100));
    assert(rich4_property_update_charge_presentation(&s, 1600));
    assert(s.presentation.property.phase == GAME_PROPERTY_PRESENT_GOD_BUILD_CHOICE);
    assert(s.state.facility_level[0] == 0 && s.presentation.route_interaction.active);
    {
        GameCommand c = rich4_game_command_upgrade_property(GAME_COMMAND_SOURCE_HUMAN,
            0, rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0), 2);
        assert(rich4_property_choose_god_building(&s, &c));
    }
    assert(rich4_property_update_charge_presentation(&s, 1700));
    assert(s.state.facility_type[0] == 2 && s.state.facility_level[0] == 1);
    assert(s.state.players[0].cash == 0 && !s.presentation.speech.active);
}

int main(void)
{
    GameSession session;
    GameEvent event;

    test_insurance_passive_flow();

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.land_count = 1;
    session.state.price_index = 1;
    session.state.land_group[0] = 2;
    event = charge_event();
    rich4_property_begin_charge_presentation(&session, &event, 1000);
    assert(session.presentation.property.phase ==
           GAME_PROPERTY_PRESENT_AMOUNT);
    assert(session.presentation.property.amount == 100);
    assert(rich4_property_update_charge_presentation(&session, 2600));
    assert(session.presentation.property.phase == GAME_PROPERTY_PRESENT_PAYER);

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.facility_count = 1;
    {
        Rich4GameEventFields fields = {
            0, 0, 2500, 0, 0,
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1)
        };
        event = rich4_game_event_property_charge_presentation_requested_from_fields(
            fields);
    }
    rich4_property_begin_charge_presentation(&session, &event, 1000);
    assert(session.presentation.property.property_kind == GAME_DOMAIN_FACILITY &&
           session.presentation.property.phase == GAME_PROPERTY_PRESENT_AMOUNT &&
           session.presentation.property.amount == 2500);
    assert(rich4_property_update_charge_presentation(&session, 2600));
    assert(session.presentation.property.phase == GAME_PROPERTY_PRESENT_COMMIT);

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 3;
    session.state.land_count = 2;
    session.state.price_index = 1;
    session.state.players[0].play_state = 1;
    session.state.players[1].play_state = 1;
    session.state.players[2].play_state = 1;
    session.state.players[1].linked_player = 2;
    session.state.players[2].linked_player = 1;
    session.state.land_owner[0] = 1;
    session.state.land_owner[1] = 2;
    session.state.land_group[0] = session.state.land_group[1] = 4;
    session.state.land_toll_by_level[0][0] = 100;
    session.state.land_toll_by_level[1][0] = 50;
    {
        Rich4GameEventFields fields = {
            0, 0, 150, 0, 0,
            rich4_game_domain_ref(GAME_DOMAIN_LAND, 0),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1)
        };
        event = rich4_game_event_property_charge_presentation_requested_from_fields(
            fields);
    }
    rich4_property_begin_charge_presentation(&session, &event, 1000);
    assert(session.presentation.property.related_owner == 2 &&
           session.presentation.property.owner_amount == 100 &&
           session.presentation.property.related_amount == 50);
    assert(session.presentation.property.flash_count == 2 &&
           session.presentation.property.phase == GAME_PROPERTY_PRESENT_FLASH);
    /* Two chain stores on different streets still flash together. */
    session.state.land_type[0] = session.state.land_type[1] = 1;
    session.state.land_group[1] = 99;
    rich4_property_begin_charge_presentation(&session, &event, 1000);
    assert(session.presentation.property.flash_count == 2 &&
           session.presentation.property.phase == GAME_PROPERTY_PRESENT_FLASH);
    session.state.players[1].linked_player = -1;
    rich4_property_begin_charge_presentation(&session, &event, 1000);
    assert(session.presentation.property.flash_count == 1 &&
           session.presentation.property.phase == GAME_PROPERTY_PRESENT_AMOUNT);

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    {
        Rich4GameEventFields fields = {
            0, 0, 5, 0, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
        };
        event = rich4_game_event_property_level_changed_from_fields(fields);
    }
    rich4_property_begin_level_presentation(&session, &event, 3, 3000);
    assert(session.presentation.speech.active ||
           session.presentation.property.max_level_active);
    session.presentation.speech.active = 0;
    assert(rich4_property_start_max_level_animation(&session, 3, 3000));
    assert(rich4_property_update_max_level_animation(
        &session, 3, 10, 3010));

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.presentation.route_interaction.active = 1;
    event = rich4_game_event_god_trade_rejection(0, 7, 1500, 91);
    rich4_property_begin_god_trade_rejection(&session, &event, 4000);
    assert(session.presentation.property.phase ==
               GAME_PROPERTY_PRESENT_GOD_TRADE_REJECTION &&
           session.presentation.property.amount == 7 &&
           session.presentation.route_interaction.active == 0);
    assert(rich4_property_update_charge_presentation(&session, 5499));
    {
        RouteEvent stop;
        memset(&stop, 0, sizeof(stop));
        stop.phase = ROUTE_EVENT_PHASE_STOP;
        stop.node = 4;
        stop.object_ref.kind = MAP_OBJECT_LAND;
        stop.object_ref.index = 0;
        stop.object_ref.raw_object_id = 0x7d1;
        rich4_route_event_executor_reset(&session.flow.route_events);
        assert(rich4_route_event_queue_push(
            &session.flow.route_events.queue, &stop));
        assert(rich4_route_event_executor_begin(
            &session.flow.route_events, 0, 91));
        session.flow.route_events.state.waiting_for_command = 0;
        session.flow.route_events.state.waiting_for_presentation = 1;
        session.flow.route_events.state.presentation_action =
            GAME_ROUTE_EVENT_PRESENTATION_GOD_TRADE_REJECTION;
        assert(!rich4_property_update_charge_presentation(&session, 5500));
        assert(session.presentation.property.phase ==
                   GAME_PROPERTY_PRESENT_NONE &&
               session.flow.route_events.queue.count == 0);
    }

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 3;
    session.state.players[0].character = 0;
    session.state.players[1].play_state = 1;
    session.state.players[2].play_state = 1;
    {
        GameEventQueue events;
        GameEvent auction = rich4_game_event_auction_request(
            0, rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), 0, 1,
            GAME_AUCTION_TRIGGER_BANKRUPTCY, 0);
        GameEvent eliminated;
        rich4_game_event_queue_reset(&events);
        assert(rich4_game_event_queue_push(&events, &auction));
        eliminated = rich4_game_event_player_eliminated_from_fields(
            (Rich4GameEventFields){0, 0, 0, 0, 0,
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)});
        assert(rich4_game_event_queue_push(&events, &eliminated));
        rich4_bankruptcy_capture_events(&session, &events, 0);
        assert(session.presentation.bankruptcy.active &&
               session.bankruptcy_auction_events.count == 1);
        assert(rich4_bankruptcy_update(&session, 2, 100, 0));
        assert(rich4_bankruptcy_update(&session, 2, 100, 100));
        assert(rich4_bankruptcy_update(&session, 2, 100, 200));
        assert(rich4_bankruptcy_update(&session, 2, 100, 2200));
        assert(!session.presentation.bankruptcy.active &&
               session.presentation.bankruptcy.awaiting_auction &&
               session.deferred_events.count == 1);
        rich4_bankruptcy_finish_auctions(&session, 2200);
        assert(session.presentation.bankruptcy.active &&
               session.presentation.bankruptcy.phase ==
                   RICH4_BANKRUPTCY_PHASE_SPEECH &&
               session.presentation.speech.active);
        session.presentation.speech.active = 0;
        rich4_bankruptcy_finish_speech(&session);
        assert(!session.presentation.bankruptcy.active &&
               !session.presentation.speech.active);
        rich4_bankruptcy_finish_auctions(&session, 2300);
        assert(!session.presentation.bankruptcy.active &&
               !session.presentation.speech.active);
    }

    /* 0x419FF0 -> 0x44F354 -> 0x44EFBD: receiver speech focuses
     * the owner while the payer remains the current actor. */
    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.price_index = 1;
    session.state.players[0].node = 1;
    session.state.players[1].node = 2;
    session.state.players[1].character = 1;
    session.state.tool_route_node_count = 3;
    session.state.tool_route_x[1] = 300; session.state.tool_route_y[1] = 400;
    session.state.tool_route_x[2] = 700; session.state.tool_route_y[2] = 800;
    session.presentation.property.phase = GAME_PROPERTY_PRESENT_RECEIVER;
    session.presentation.property.actor = 0;
    session.presentation.property.owner = 1;
    session.presentation.property.related_owner = -1;
    session.presentation.property.amount = 10000;
    assert(rich4_property_update_charge_presentation(&session, 100));
    assert(session.presentation.speech.active && session.presentation.speech.player == 1);
    assert(session.presentation.has_view_override);
    assert(session.presentation.view_world_x == 700 && session.presentation.view_world_y == 800);
    assert(session.state.current_player == 0);

    test_resident_notices();
    puts("app property flow: ok");
    return 0;
}
