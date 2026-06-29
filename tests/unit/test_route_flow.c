#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/route_flow.h"
#include "core/game_constants.h"

static RouteEvent make_event(RouteEventPhase phase, int node, int raw_id)
{
    RouteEvent event;
    memset(&event, 0, sizeof(event));
    event.phase = phase;
    event.node = node;
    event.object_ref = rich4_map_object_ref((unsigned short)raw_id);
    event.snapshot.player = 0;
    event.snapshot.event_kind = GAME_ROUTE_EVENT_ROAD_NODE;
    return event;
}

int main(void)
{
    GameSession session;
    RouteEvent pass;
    RouteEvent stop;

    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.state.current_player = 0;
    pass = make_event(ROUTE_EVENT_PHASE_PASS, 3, 3);
    stop = make_event(ROUTE_EVENT_PHASE_STOP, 4, 4);
    assert(rich4_route_event_collection_push(&session.flow.route_facts, &pass));
    assert(rich4_route_event_collection_push(&session.flow.route_facts, &stop));

    rich4_app_route_prepare_dispatch(&session, 0);
    assert(session.flow.route_events.state.dispatch.phase ==
           GAME_ROUTE_EVENT_DISPATCH_PASS);
    assert(session.flow.route_events.state.dispatch.next_phase ==
           GAME_ROUTE_EVENT_DISPATCH_STOP);
    assert(rich4_app_route_advance_dispatch(&session));
    assert(session.flow.route_events.state.dispatch.phase ==
           GAME_ROUTE_EVENT_DISPATCH_STOP);

    rich4_app_route_reset_dispatch(&session);
    assert(session.flow.route_events.state.dispatch.phase ==
           GAME_ROUTE_EVENT_DISPATCH_NONE);
    assert(session.presentation.route_interaction.active == 0);
    rich4_app_route_reset_facts(&session);
    assert(session.flow.route_facts.count == 0);
    assert(session.flow.route_events.queue.count == 0);

    session.state.facility_count = 1;
    session.state.facility_level[0] = 0;
    session.presentation.route_interaction.active = 1;
    session.presentation.route_interaction.accept =
        rich4_game_command_upgrade_property(
            GAME_COMMAND_SOURCE_HUMAN, 0,
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0), 0);
    assert(rich4_app_route_interaction_is_mandatory_facility_build(
        &session));
    assert(!rich4_app_route_interaction_uses_confirm_dialog(&session));
    assert(rich4_app_route_facility_choice_at(50, 286) == 0);
    assert(rich4_app_route_facility_choice_at(390, 286) == -1);
    assert(rich4_app_route_facility_research_choice_at(35, 297) == 0);
    assert(rich4_app_route_facility_research_choice_at(406, 351) == 4);
    assert(rich4_app_route_facility_research_choice_at(34, 297) == -1);
    assert(rich4_app_route_facility_research_choice_at(407, 351) == -1);
    rich4_app_route_set_pressed(&session, 2);
    assert(session.presentation.route_interaction.pressed == 2);
    rich4_app_route_set_pressed(&session, -1);
    assert(session.presentation.route_interaction.pressed == -1);
    {
        GameCommand command = rich4_app_route_facility_build_command(
            &session, 3);
        assert(command.type == GAME_COMMAND_UPGRADE_PROPERTY);
        assert(command.subject.kind == GAME_DOMAIN_FACILITY);
        assert(command.value == 3);
    }
    session.state.facility_level[0] = 1;
    assert(!rich4_app_route_interaction_is_mandatory_facility_build(
        &session));
    session.state.players[0].play_state = 1;
    session.state.price_index = 1;
    session.state.facility_owner[0] = 0;
    session.state.facility_type[0] = 4;
    session.state.facility_price[0] = 100;
    session.state.facility_price_per_level[0] = 100;
    assert(rich4_app_route_interaction_is_facility_research(&session));
    assert(rich4_app_route_interaction_is_facility_choice(&session));
    assert(!rich4_app_route_interaction_uses_confirm_dialog(&session));
    {
        GameCommand command = rich4_app_route_facility_research_command(
            &session, 0);
        assert(command.type == GAME_COMMAND_UPGRADE_PROPERTY &&
               command.subject.kind == GAME_DOMAIN_FACILITY &&
               command.value == 0);
    }
    session.state.facility_type[0] = 1;
    assert(!rich4_app_route_interaction_is_facility_research(&session));
    assert(rich4_app_route_interaction_uses_confirm_dialog(&session));
    session.presentation.route_interaction.accept.type =
        GAME_COMMAND_BANK_WITHDRAW;
    assert(!rich4_app_route_interaction_uses_confirm_dialog(&session));
    session.presentation.route_interaction.accept.type =
        GAME_COMMAND_PURCHASE_PROPERTY;
    session.presentation.route_interaction.decline =
        rich4_game_command_purchase_property(
            GAME_COMMAND_SOURCE_HUMAN, 0,
            rich4_game_domain_ref(GAME_DOMAIN_LAND, 1));
    {
        GameCommand command;
        assert(rich4_app_route_interaction_command_for_choice(
            &session, RICH4_APP_ROUTE_INTERACTION_ACCEPT, &command));
        assert(command.type == GAME_COMMAND_PURCHASE_PROPERTY);
        assert(rich4_app_route_interaction_command_for_choice(
            &session, RICH4_APP_ROUTE_INTERACTION_DECLINE, &command));
        assert(command.type == GAME_COMMAND_PURCHASE_PROPERTY);
    }
    puts("app route flow: ok");
    return 0;
}
