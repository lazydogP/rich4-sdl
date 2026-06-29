#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/hotel_flow.h"
#include "app/audio_event.h"
#include "app/insurance_flow.h"
#include "app/route_event_flow.h"
#include "app/turn_rotation_flow.h"
#include "app/turn_start_flow.h"
#include "app/turn_status_flow.h"
#include "core/facility.h"
#include "core/game_event_payload.h"
#include "view/game_view_model.h"

static GameSession session;
static MapData map;
static MapRouteNode routes[3];
static MapFacilityRecord facilities[2];

static void setup(void)
{
    int i;
    memset(&session, 0, sizeof(session));
    memset(&map, 0, sizeof(map));
    memset(routes, 0, sizeof(routes));
    memset(facilities, 0, sizeof(facilities));
    map.routes = routes; map.route_count = 3;
    map.facilities = facilities; map.facility_count = 2;
    routes[1].x = 96; routes[1].y = 96;
    routes[1].next[0] = 2;
    routes[1].object = rich4_map_object_ref(4001);
    routes[2].x = 96; routes[2].y = 128;
    routes[2].next[0] = 1;
    facilities[1].x = 176; facilities[1].y = 96;
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.price_index = 2;
    session.state.current_player = session.state.turn_rotation_index = 0;
    session.state.facility_count = 1;
    session.state.facility_owner[0] = 1;
    session.state.facility_level[0] = 2;
    session.state.facility_type[0] = 1;
    session.state.facility_charge_by_level[0][2] = 300;
    session.state.tool_route_node_count = 3;
    session.state.tool_route_x[1] = 96; session.state.tool_route_y[1] = 96;
    session.state.tool_route_x[2] = 96; session.state.tool_route_y[2] = 128;
    session.state.route_object_raw[1] = 4001;
    for (i = 0; i < 2; i++) {
        session.state.players[i].play_state = 1;
        session.state.players[i].linked_player = -1;
        session.state.players[i].cash = 100000;
        session.state.players[i].node = 1;
        session.state.players[i].next_node = 2;
        session.presentation.actors[i].visual_x = 96;
        session.presentation.actors[i].visual_y = 96;
        session.presentation.actors[i].intro_visible = 1;
    }
    session.state.players[0].traffic_method = 2;
    session.state.players[0].ndices = 3;
}

static void book(int days)
{
    GameEventQueue events;
    GameEvent event;
    GameFacilityLandingResult result;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&session.state, 0, 0, days, 8,
        0, -1, &result, &events) == GAME_PAYMENT_COMMITTED);
    while (rich4_game_event_queue_pop(&events, &event))
        (void)rich4_hotel_capture_entry(&session, &event);
    assert(session.presentation.hotel_entry.pending);
}

static void hotel_payment_rules(void)
{
    GameEventQueue events;
    GameFacilityLandingResult result;
    setup();
    session.state.players[0].days_in_prison = 4;
    session.state.players[0].confinement_actor_type = GAME_CONFINEMENT_PRISON;
    session.state.players[0].card_inventory[0] = 20;
    session.state.facility_price_status[0] = 1;
    book(4);
    assert(session.state.players[0].cash == 95200);
    assert(session.state.players[1].bank == 4800);
    assert(session.state.players[0].card_inventory[0] == 20);
    assert(session.state.players[0].days_in_prison == 0);
    assert(session.state.players[0].confinement_actor_type == GAME_CONFINEMENT_NONE);
    assert(session.state.players[0].traffic_method == 2 && session.state.players[0].ndices == 3);
    assert(session.state.players[0].delayed_status_block == 3);
    assert(session.state.players[0].total_confinement_days == 4);
    setup();
    session.state.players[0].god_info = session.state.players[0].god_type = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&session.state, 0, 0, 4, 8,
        0, -1, &result, &events) == GAME_PAYMENT_COMMITTED);
    assert(result.kind == GAME_LANDING_NONE && events.count == 0);
    assert(!session.state.players[0].delayed_status_block);
    setup();
    session.state.facility_type[0] = 3;
    session.state.facility_price_status[0] = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&session.state, 0, 0, 0, 8,
        0, -1, &result, &events) == GAME_PAYMENT_COMMITTED);
    assert(result.amount == 16000 && !session.state.players[0].delayed_status_block);
    setup();
    session.state.facility_type[0] = 3;
    session.state.players[0].traffic_method = 1;
    session.state.players[0].card_inventory[0] = 20;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&session.state, 0, 0, 0, 1,
        1, -1, &result, &events) == GAME_PAYMENT_COMMITTED);
    assert(result.amount == 1000 && session.state.players[0].card_inventory[0] == 20);
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&session.state, 0, 0, 0, 4,
        1, -1, &result, &events) == GAME_PAYMENT_COMMITTED);
    assert(result.amount == 0 && session.state.players[0].card_inventory[0] == 0);
    setup();
    session.state.players[1].days_sleeping = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&session.state, 0, 0, 4, 8,
        0, -1, &result, &events) == GAME_PAYMENT_COMMITTED);
    assert(result.kind == GAME_LANDING_NONE && events.count == 0);
}

static void entry_and_substitute(void)
{
    int i;
    setup(); book(3);
    assert(rich4_hotel_update_entry(&session, &map, 1000, 40) == 1);
    for (i = 1; i <= 5; i++)
        assert(rich4_hotel_update_entry(&session, &map, 1000 + 40*i, 40) == 1);
    assert(session.presentation.actors[0].visual_x == 136);
    assert(session.state.players[0].play_state & 0x20);
    assert(rich4_hotel_update_entry(&session, &map, 1240, 40) == 1);
    assert(!(session.state.players[0].play_state & 0x20));
    assert(session.state.players[0].delayed_status_block == 2);
    /* A late frame exposes one pulse, not a backlog of movement steps. */
    assert(rich4_hotel_update_entry(&session, &map, 2000, 40) == 1);
    assert(session.presentation.actors[0].move_progress == 7);
    for (i = 1; i <= 3; i++)
        assert(rich4_hotel_update_entry(&session, &map, 2000 + 40*i, 40) == 1);
    assert(session.presentation.actors[0].visual_x == 176);
    assert(!session.presentation.actors[0].move_active);
    assert(rich4_hotel_update_entry(&session, &map, 2160, 40) == 1);
    for (i = 1; i <= 7; i++)
        assert(rich4_hotel_update_entry(&session, &map, 2160 + 40*i, 40) == 1);
    assert(rich4_hotel_update_entry(&session, &map, 2480, 40) == 2);
    assert(!session.presentation.hotel_entry.active);
    setup();
    session.state.players[1].god_info = session.state.players[1].god_type = 15;
    /* A dead god owner suppresses its business; use treasure for substitution. */
    session.state.players[1].god_info = session.state.players[1].god_type = 14;
    session.state.players[1].node = 2;
    book(1);
    assert(session.state.players[1].delayed_status_block == 0x80);
    assert(session.state.players[1].node == 1);
    assert(rich4_hotel_update_entry(&session, &map, 1000, 40) == 0);
    assert(session.presentation.actors[1].visual_x == 176);
    assert(!session.presentation.actors[1].move_active);
}

static void exit_and_camera(void)
{
    Rich4AppTurnRotationResult rotation;
    GameViewModelInput input;
    GameViewportModel viewport;
    int i;
    setup();
    session.state.turn_rotation_index = 1;
    session.state.players[0].delayed_status_block = 0x80;
    session.presentation.actors[0].visual_x = 176;
    session.presentation.actors[0].direction = 6;
    memset(&input, 0, sizeof(input));
    rich4_game_viewport_build(&session.state, &input, &map, &viewport);
    assert(viewport.center_world_x == 176);
    assert(rich4_app_advance_turn_rotation(&session, &map, 1000, &rotation));
    assert(session.presentation.confinement_exit.active);
    assert(session.state.players[0].delayed_status_block == 0x80);
    assert(rich4_app_update_confinement_exit(&session, 1000, 40));
    for (i = 1; i <= 5; i++)
        assert(rich4_app_update_confinement_exit(&session, 1000 + 40*i, 40));
    assert(session.state.players[0].delayed_status_block == 0x80);
    assert(rich4_app_update_confinement_exit(&session, 1240, 40));
    assert(session.state.players[0].delayed_status_block == 0);
    for (i = 7; i <= 9; i++)
        assert(rich4_app_update_confinement_exit(&session, 1000 + 40*i, 40));
    assert(rich4_app_update_confinement_exit(&session, 1400, 40));
    assert(session.presentation.actors[0].visual_x == 96);
    for (i = 11; i <= 18; i++)
        assert(rich4_app_update_confinement_exit(&session, 1000 + 40*i, 40));
    assert(!rich4_app_update_confinement_exit(&session, 1760, 40));
    assert(session.state.current_player == 0);
    assert((session.state.players[0].play_state & 0x30) == 0);
    assert(session.presentation.actors[0].direction == 6);
    assert(session.state.players[0].traffic_method == 2 && session.state.players[0].ndices == 3);
}

static void insured_entry_order(void)
{
    GameEventQueue events;
    GameEvent event;
    GameFacilityLandingResult result;
    setup();
    session.state.players[0].days_assurance = 5;
    session.state.company_count = 1;
    session.state.company_type[0] = 4;
    session.state.company_stock_index[0] = 0;
    rich4_game_event_queue_reset(&events);
    assert(rich4_facility_resolve_landing(&session.state, 0, 0, 3, 8,
        0, -1, &result, &events) == GAME_PAYMENT_COMMITTED);
    while (rich4_game_event_queue_pop(&events, &event)) {
        (void)rich4_hotel_capture_entry(&session, &event);
        if (event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED)
            rich4_insurance_flow_enqueue_notice(&session, &event, 1000);
    }
    assert(session.state.players[0].cash == 98200);
    assert(session.presentation.hotel_entry.pending);
    assert(session.state.players[0].total_confinement_days == 0);
    assert(rich4_insurance_flow_update(&session, 1000));
    assert(rich4_insurance_flow_update(&session, 2999));
    assert(session.state.players[0].cash == 98200);
    assert(!rich4_insurance_flow_update(&session, 3000));
    assert(session.state.players[0].cash == 110200);
    assert(session.state.players[0].total_confinement_days == 3);
    assert(!rich4_insurance_flow_update(&session, 3001));
    assert(session.state.players[0].cash == 110200);
    assert(rich4_hotel_update_entry(&session, &map, 3001, 40));
    assert(session.presentation.actors[0].move_active);
}

static void route_entry_and_countdown(void)
{
    Rich4AppRouteEventServices services;
    RouteEvent stop;
    int i;
    setup(); book(3);
    memset(&services, 0, sizeof(services));
    services.property_end_delay_ms = 40;
    memset(&stop, 0, sizeof(stop));
    stop.phase = ROUTE_EVENT_PHASE_STOP;
    stop.node = 1;
    stop.object_ref = rich4_map_object_ref(4001);
    rich4_route_event_collection_push(&session.flow.route_facts, &stop);
    session.flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    session.flow.turn_flow_active = 1;
    assert(rich4_app_route_event_update(&session, &map, 1000, 1, &services));
    assert(session.presentation.hotel_entry.active);
    for (i = 1; i <= 19; i++)
        assert(rich4_app_route_event_update(&session, &map, 1000 + 40*i, 1, &services));
    assert(!session.presentation.hotel_entry.active);
    assert(session.state.current_player == 1);
    assert(!session.flow.turn_flow_active);
    /* On the next hotel turn the remaining count is displayed for 1500ms. */
    session.state.current_player = 0;
    session.presentation.actors[0].dirty_flags = GAME_PLAYER_DIRTY_TURN;
    assert(rich4_turn_start_prepare(&session, 2000) == RICH4_TURN_START_CONFINEMENT_STATUS);
    assert(rich4_app_begin_confinement_status_presentation(&session, 0, 50, 2000));
    assert(session.presentation.confinement_status.days == 3);
    assert(!session.presentation.speech.active);
    assert(rich4_app_update_confinement_status_presentation(&session, 2000, 0) == 1);
    assert(rich4_app_update_confinement_status_presentation(&session, 3499, 0) == 1);
    assert(rich4_app_update_confinement_status_presentation(&session, 3500, 0) == 2);
}

int main(void)
{
    hotel_payment_rules();
    entry_and_substitute();
    exit_and_camera();
    insured_entry_order();
    route_entry_and_countdown();
    puts("hotel/gas rules and animation: ok");
    return 0;
}
