#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/game_session.h"
#include "app/turn_rotation_flow.h"
#include "core/auction.h"
#include "core/card_internal.h"
#include "core/facility.h"
#include "core/game_event_payload.h"
#include "core/god.h"
#include "core/property.h"
#include "core/tool_internal.h"
#include "core/turn_rotation.h"

static GameSession session;
static GameState before;

static uint32_t date(int year, int month, int day)
{
    return ((uint32_t)year << 16) | ((uint32_t)month << 8) | (uint32_t)day;
}

static GameDomainRef plot(int facility, int id)
{
    return rich4_game_domain_ref(facility ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND, id);
}

static GameState *setup(void)
{
    GameState *s = &session.state;
    int i;
    memset(&session, 0, sizeof(session));
    s->active = 1;
    s->player_count = 2;
    s->price_index = 1;
    s->calendar_start_year = 2000;
    s->calendar_start_month = 11;
    s->calendar_start_day = 30;
    s->land_period_index = 5;
    s->land_count = s->facility_count = 2;
    s->tool_route_node_count = 3;
    s->route_object_raw[1] = 2001;
    s->route_object_raw[2] = 4001;
    for (i = 0; i < GAME_MAX_MAP_PROPERTIES; i++)
        s->land_owner[i] = s->facility_owner[i] = -1;
    for (i = 0; i < 2; i++) {
        s->players[i].play_state = 1;
        s->players[i].linked_player = -1;
        s->players[i].cash = 100000;
        s->players[i].node = 1;
        s->land_group[i] = i;
        s->land_price[i] = s->land_value[i] = 1000;
        s->land_price_per_level[i] = 100;
        s->facility_price[i] = s->facility_value[i] = 2000;
        s->facility_price_per_level[i] = 200;
    }
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) s->special_player_f10[i] = 1;
    return s;
}

static void purchase_settings_and_renewal(void)
{
    static const uint32_t expected[] = {
        0, 0x07d20b1eu, 0x07d10b1eu, 0x07d1051eu, 0x07d1021eu, 0x07d00c1eu
    };
    GameEventQueue q;
    GameCommand command;
    int setting, facility;
    for (setting = 0; setting < 6; setting++) {
        GameState *s = setup();
        s->land_period_index = setting;
        assert(rich4_property_lease_deadline(s) == expected[setting]);
        for (facility = 0; facility < 2; facility++) {
            command = rich4_game_domain_command(GAME_COMMAND_PURCHASE_PROPERTY,
                GAME_COMMAND_SOURCE_HUMAN, 0, plot(facility, 0),
                rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
            rich4_game_event_queue_reset(&q);
            assert(facility ? rich4_facility_apply_command(s, &command, &q)
                            : rich4_land_apply_command(s, &command, &q));
            assert((facility ? s->facility_last_change[0] : s->land_lease_date[0]) == expected[setting]);
            s->elapsed_days = 1;
            command.type = GAME_COMMAND_UPGRADE_PROPERTY;
            rich4_game_event_queue_reset(&q);
            assert(facility ? rich4_facility_apply_command(s, &command, &q)
                            : rich4_land_apply_command(s, &command, &q));
            assert((facility ? s->facility_last_change[0] : s->land_lease_date[0]) == expected[setting]);
            s->elapsed_days = 0;
        }
    }
    {
        GameState *s = setup();
        GameCommand command = rich4_game_domain_command(GAME_COMMAND_PURCHASE_PROPERTY,
            GAME_COMMAND_SOURCE_HUMAN, 0, plot(0, 0),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
        s->players[0].cash = 0;
        before = *s;
        rich4_game_event_queue_reset(&q);
        assert(!rich4_land_apply_command(s, &command, &q));
        assert(memcmp(&before, s, sizeof(before)) == 0);
    }
    {
        GameState *s = setup();
        s->calendar_start_month = 2;
        s->calendar_start_day = 29;
        s->land_period_index = 2;
        assert(rich4_property_lease_deadline(s) == date(2001, 2, 29));
        s->land_period_index = 5;
        s->calendar_start_month = 12;
        s->calendar_start_day = 31;
        assert(rich4_property_lease_deadline(s) == date(2001, 1, 31));
        s->land_period_index = 0;
        s->land_lease_date[0] = date(2001, 2, 3);
        rich4_property_begin_lease(s, plot(0, 0));
        assert(s->land_lease_date[0] == date(2001, 2, 3));
    }
}

static void transfers_and_purchase_card(void)
{
    GameAuctionState auction;
    GameCommand card;
    GameEventQueue q;
    int facility, owned;
    const uint32_t old = 0x07d10a0fu;
    for (facility = 0; facility < 2; facility++) {
        for (owned = 0; owned < 2; owned++) {
            GameState *s = setup();
            if (facility) {
                s->facility_owner[0] = owned ? 1 : -1;
                s->facility_last_change[0] = old;
            } else {
                s->land_owner[0] = owned ? 1 : -1;
                s->land_lease_date[0] = old;
            }
            s->fixed_asset_value[1] = owned ? (facility ? 2000 : 1000) : 0;
            memset(&auction, 0, sizeof(auction));
            auction.active = 1;
            auction.phase = GAME_AUCTION_SOLD;
            auction.target = plot(facility, 0);
            auction.seller = owned ? 1 : -1;
            auction.leader = 0;
            auction.current_price = 1000;
            rich4_game_event_queue_reset(&q);
            assert(rich4_auction_settle(&auction, s, 0, &q) == GAME_AUCTION_SETTLEMENT_SOLD);
            assert((facility ? s->facility_last_change[0] : s->land_lease_date[0]) ==
                   (owned ? old : date(2000, 12, 30)));
        }
        {
            GameState *s = setup();
            s->players[0].god_info = 12;
            s->players[0].god_type = 12;
            rich4_game_event_queue_reset(&q);
            assert(rich4_god_apply_landing_effect(s, 0, facility ? 2 : 1, &q));
            assert((facility ? s->facility_last_change[0] : s->land_lease_date[0]) == date(2000, 12, 30));
            if (facility) s->facility_owner[0] = 1;
            else s->land_owner[0] = 1;
            s->fixed_asset_value[1] = facility ? 2000 : 1000;
            s->elapsed_days = 1;
            rich4_game_event_queue_reset(&q);
            assert(rich4_god_apply_landing_effect(s, 0, facility ? 2 : 1, &q));
            assert((facility ? s->facility_last_change[0] : s->land_lease_date[0]) == date(2000, 12, 30));
        }
        {
            GameState *s = setup();
            s->land_owner[0] = s->facility_owner[0] = 1;
            s->land_lease_date[0] = s->facility_last_change[0] = old;
            card = rich4_game_domain_command(GAME_COMMAND_USE_CARD, GAME_COMMAND_SOURCE_HUMAN,
                0, rich4_game_domain_ref(GAME_DOMAIN_CARD, 2), plot(facility, 0), 0);
            rich4_game_event_queue_reset(&q);
            assert(rich4_card_apply_property_card(s, &card, 2, &q));
            assert((facility ? s->facility_last_change[0] : s->land_lease_date[0]) == date(2000, 12, 30));
        }
    }
    {
        GameState *s = setup();
        s->land_owner[0] = 0;
        s->land_level[0] = 2;
        s->land_lease_date[0] = old;
        rich4_game_event_queue_reset(&q);
        assert(rich4_tool_transfer_effect(s, 0, plot(0, 0), plot(0, 1), &q));
        assert(s->land_lease_date[0] == 0 && s->land_lease_date[1] == old);
    }
}

static void expiry_and_atomicity(void)
{
    GameState *s = setup();
    GameEventQueue q;
    GameEvent filler = rich4_game_event_scene_refresh_requested_from_fields((Rich4GameEventFields){0});
    int i;
    s->land_count = 40;
    for (i = 0; i < 40; i++) {
        s->land_owner[i] = 0;
        s->land_level[i] = 3;
        s->land_value[i] = 10;
        s->land_lease_date[i] = date(2000, 11, 30);
    }
    s->land_lease_date[0] = 0;
    s->land_lease_date[1] = date(2000, 11, 29); /* no <= catch-up */
    s->land_lease_date[2] = date(2000, 12, 1);
    s->facility_owner[0] = 1;
    s->facility_type[0] = 4;
    s->facility_level[0] = 3;
    s->facility_runtime_value[0] = 991;
    s->facility_last_change[0] = date(2000, 11, 30);
    s->land_price_status[3] = 0x50;
    before = *s;
    rich4_game_event_queue_reset(&q);
    while (q.count < GAME_EVENT_QUEUE_CAPACITY) assert(rich4_game_event_queue_push(&q, &filler));
    assert(!rich4_turn_finish_property_day(s, &q));
    assert(memcmp(&before, s, sizeof(before)) == 0);
    rich4_game_event_queue_reset(&q);
    assert(rich4_turn_finish_property_day(s, &q));
    assert(q.count == 1 && q.events[0].type == GAME_EVENT_SCENE_REFRESH_REQUESTED);
    for (i = 0; i < 40; i++) {
        assert(s->land_owner[i] == (i < 3 ? 0 : -1));
        assert(s->land_level[i] == 3 && s->land_value[i] == 10);
    }
    assert(s->land_price_status[3] == 0x40);
    assert(s->fixed_asset_value[0] == 30 && s->fixed_asset_value[1] == 0);
    assert(s->facility_owner[0] == -1 && s->facility_last_change[0] == 0);
    assert(s->facility_level[0] == 3 && s->facility_type[0] == 4 && s->facility_runtime_value[0] == 991);
}

static void daily_and_modal_order(void)
{
    GameEventQueue q;
    TurnAdvanceResult turn;
    Rich4AppTurnRotationResult app_turn;
    GameState *s = setup();
    s->calendar_start_month = 7;
    s->calendar_start_day = 10;
    s->land_owner[0] = 0;
    s->land_lease_date[0] = date(2000, 7, 11);
    s->turn_rotation_index = 1;
    rich4_game_event_queue_reset(&q);
    assert(rich4_turn_advance_once_events(s, &turn, &q));
    assert(s->elapsed_days == 1 && s->land_owner[0] == -1 && s->land_lease_date[0] == 0);

    s = setup();
    s->calendar_start_month = 7;
    s->calendar_start_day = 10;
    s->land_owner[0] = 0;
    s->land_lease_date[0] = date(2000, 7, 11);
    s->turn_rotation_index = 1;
    s->win_target = 1;
    rich4_game_event_queue_reset(&q);
    assert(rich4_turn_advance_once_events(s, &turn, &q));
    assert(s->elapsed_days == 1 && s->land_owner[0] == 0); /* victory precedes expiry */

    for (int monthly = 0; monthly < 2; monthly++) {
        s = setup();
        s->calendar_start_month = 7;
        s->calendar_start_day = monthly ? 31 : 14;
        s->land_owner[0] = 0;
        s->land_lease_date[0] = monthly ? date(2000, 8, 1) : date(2000, 7, 15);
        s->turn_rotation_index = 1;
        assert(rich4_app_advance_turn_rotation(&session, NULL, 100, &app_turn));
        assert(s->land_owner[0] == 0 && session.pending_turn_events.count > 0);
        assert(monthly ? session.presentation.bank_settlement.active : session.presentation.periodic_finance.active);
        /* Simulate the corresponding original blocking window returning. */
        session.presentation.bank_settlement.active = 0;
        session.presentation.periodic_finance.active = 0;
        assert(rich4_app_resume_turn_events(&session, NULL, 5000));
        assert(s->land_owner[0] == -1 && s->land_lease_date[0] == 0);
    }

    /* A calendar FLC also precedes property day-end in 0x41CF67. */
    s = setup();
    s->land_owner[0] = 0;
    s->land_lease_date[0] = date(2000, 11, 30);
    session.presentation.tool_animation_count = 1;
    rich4_game_event_queue_reset(&session.pending_turn_events);
    {
        GameEvent finish = rich4_game_event_property_day_end_requested_from_fields(
            (Rich4GameEventFields){.player = -1});
        assert(rich4_game_event_queue_push(&session.pending_turn_events, &finish));
    }
    assert(rich4_app_resume_turn_events(&session, NULL, 100));
    assert(s->land_owner[0] == 0 && session.pending_turn_events.count == 1);
    session.presentation.tool_animation_count = 0;
    assert(rich4_app_resume_turn_events(&session, NULL, 200));
    assert(s->land_owner[0] == -1 && session.pending_turn_events.count == 0);
}

int main(void)
{
    purchase_settings_and_renewal();
    transfers_and_purchase_card();
    expiry_and_atomicity();
    daily_and_modal_order();
    puts("property leases: setup, acquisition, transfer, day-end and modal order: ok");
    return 0;
}
