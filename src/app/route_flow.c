#include "app/route_flow.h"

#include <string.h>

#include "core/facility.h"
#include "core/game_constants.h"
#include "core/route_event_executor.h"
#include "core/route_event_queue.h"

/* App-owned route dispatch lifecycle. */

/* RouteEvent is the authority for the current object and property facts.
 * Keep only the dispatch cursor in RouteEventDispatchState; callers that need
 * the current fact resolve it from the collection instead of reading a
 * second, stale copy. */
static const RouteEvent *rich4_app_route_current_event(
    const GameSession *session)
{
    int i;
    if (session == NULL) return NULL;
    if (session->flow.route_events.state.dispatch.phase ==
        GAME_ROUTE_EVENT_DISPATCH_PASS)
        return rich4_route_event_collection_at(
            &session->flow.route_facts,
            session->flow.route_events.state.dispatch.index);
    if (session->flow.route_events.state.dispatch.phase !=
        GAME_ROUTE_EVENT_DISPATCH_STOP)
        return NULL;
    for (i = 0; i < session->flow.route_facts.count; i++) {
        const RouteEvent *event = rich4_route_event_collection_at(
            &session->flow.route_facts, i);
        if (event != NULL && event->phase == ROUTE_EVENT_PHASE_STOP)
            return event;
    }
    return NULL;
}

static int rich4_app_route_next_dispatchable_pass_event(const GameSession *session,
                                                     int start_index)
{
    int i;

    if (session == NULL) {
        return -1;
    }
    if (start_index < 0) {
        start_index = 0;
    }
    for (i = start_index; i < session->flow.route_facts.count; i++) {
        const RouteEvent *event = rich4_route_event_collection_at(
            &session->flow.route_facts, i);
        if (event != NULL && event->phase == ROUTE_EVENT_PHASE_PASS &&
            event->snapshot.event_kind != GAME_ROUTE_EVENT_NONE) {
            return i;
        }
    }
    return -1;
}

static void rich4_app_route_update_dispatch_next(GameSession *session)
{
    int i;
    int next_pass_event;

    if (session == NULL) {
        return;
    }

    session->flow.route_events.state.dispatch.current_ordinal = 0;
    session->flow.route_events.state.dispatch.next_phase = GAME_ROUTE_EVENT_DISPATCH_DONE;
    session->flow.route_events.state.dispatch.next_index = -1;

    if (session->flow.route_events.state.dispatch.phase == GAME_ROUTE_EVENT_DISPATCH_PASS) {
        if (session->flow.route_events.state.dispatch.index < 0 ||
            session->flow.route_events.state.dispatch.index >=
                session->flow.route_facts.count) {
            return;
        }
        for (i = 0; i <= session->flow.route_events.state.dispatch.index; i++) {
            const RouteEvent *event = rich4_route_event_collection_at(
                &session->flow.route_facts, i);
            if (event != NULL && event->phase == ROUTE_EVENT_PHASE_PASS &&
                event->snapshot.event_kind != GAME_ROUTE_EVENT_NONE) {
                session->flow.route_events.state.dispatch.current_ordinal++;
            }
        }
        next_pass_event =
            rich4_app_route_next_dispatchable_pass_event(
                session,
                session->flow.route_events.state.dispatch.index + 1);
        if (next_pass_event >= 0) {
            session->flow.route_events.state.dispatch.next_phase =
                GAME_ROUTE_EVENT_DISPATCH_PASS;
            session->flow.route_events.state.dispatch.next_index = next_pass_event;
        } else if (rich4_route_event_collection_count_phase(
                       &session->flow.route_facts,
                       ROUTE_EVENT_PHASE_STOP) > 0) {
            session->flow.route_events.state.dispatch.next_phase =
                GAME_ROUTE_EVENT_DISPATCH_STOP;
            session->flow.route_events.state.dispatch.next_index = -1;
        }
    } else if (session->flow.route_events.state.dispatch.phase ==
               GAME_ROUTE_EVENT_DISPATCH_STOP) {
        session->flow.route_events.state.dispatch.current_ordinal =
            session->flow.route_events.state.dispatch.pass_count + 1;
    }
}

int rich4_app_route_advance_dispatch(GameSession *session)
{
    if (session == NULL) {
        return 0;
    }
    if (session->flow.route_events.state.dispatch.phase == GAME_ROUTE_EVENT_DISPATCH_NONE ||
        session->flow.route_events.state.dispatch.phase == GAME_ROUTE_EVENT_DISPATCH_DONE) {
        rich4_app_route_update_dispatch_next(session);
        return 0;
    }

    session->flow.route_events.state.dispatch.phase =
        session->flow.route_events.state.dispatch.next_phase;
    session->flow.route_events.state.dispatch.index =
        session->flow.route_events.state.dispatch.next_index;
    rich4_app_route_update_dispatch_next(session);
    return session->flow.route_events.state.dispatch.phase != GAME_ROUTE_EVENT_DISPATCH_DONE;
}

static int game_route_event_handler_family_for_kind(int event_kind)
{
    switch (event_kind) {
    case GAME_ROUTE_EVENT_NONE:
        return GAME_ROUTE_EVENT_HANDLER_NONE;
    case GAME_ROUTE_EVENT_ROAD_NODE:
        return GAME_ROUTE_EVENT_HANDLER_ROAD;
    case GAME_ROUTE_EVENT_LAND:
    case GAME_ROUTE_EVENT_FACILITY:
        return GAME_ROUTE_EVENT_HANDLER_PROPERTY;
    case GAME_ROUTE_EVENT_STOCK_SPECIAL:
        return GAME_ROUTE_EVENT_HANDLER_STOCK_SPECIAL;
    case GAME_ROUTE_EVENT_SECOND_SPECIAL:
        return GAME_ROUTE_EVENT_HANDLER_SECOND_SPECIAL;
    default:
        break;
    }
    return GAME_ROUTE_EVENT_HANDLER_UNKNOWN;
}

int rich4_app_route_dispatch_current_handler(GameSession *session)
{
    const RouteEvent *event = rich4_app_route_current_event(session);
    int index;
    int handler_family;

    if (session == NULL ||
        session->flow.route_events.state.dispatch.phase == GAME_ROUTE_EVENT_DISPATCH_NONE ||
        session->flow.route_events.state.dispatch.phase == GAME_ROUTE_EVENT_DISPATCH_DONE ||
        event == NULL || event->snapshot.event_kind == GAME_ROUTE_EVENT_NONE) {
        return 0;
    }
    if (session->flow.route_events.state.dispatch.handled_count >= GAME_MAX_TURN_ROUTE_EVENTS) {
        return 0;
    }

    handler_family = game_route_event_handler_family_for_kind(
        event->snapshot.event_kind);
    index = session->flow.route_events.state.dispatch.handled_count++;
    session->debug.route_event_last_handler_family = handler_family;
    session->debug.route_event_last_handler_phase = session->flow.route_events.state.dispatch.phase;
    session->debug.route_event_last_handler_node = event->node;
    session->debug.route_event_handled_families[index] = handler_family;
    session->debug.route_event_handled_phases[index] =
        session->flow.route_events.state.dispatch.phase;
    session->debug.route_event_handled_ordinals[index] =
        session->flow.route_events.state.dispatch.current_ordinal;
    session->debug.route_event_handled_nodes[index] =
        event->node;
    session->debug.route_event_handled_event_kinds[index] =
        event->snapshot.event_kind;
    session->debug.route_event_handled_property_branches[index] =
        event->snapshot.property_branch;
    return 1;
}

int rich4_app_route_defer_current_event(GameSession *session)
{
    const RouteEvent *event = rich4_app_route_current_event(session);
    int index;

    if (session == NULL ||
        session->flow.route_events.state.dispatch.phase == GAME_ROUTE_EVENT_DISPATCH_NONE ||
        session->flow.route_events.state.dispatch.phase == GAME_ROUTE_EVENT_DISPATCH_DONE ||
        event == NULL || event->snapshot.event_kind == GAME_ROUTE_EVENT_NONE) {
        return 0;
    }
    if (session->debug.route_event_deferred_count >= GAME_MAX_TURN_ROUTE_EVENTS) {
        return 0;
    }
    index = session->debug.route_event_deferred_count++;
    session->debug.route_event_last_deferred_phase =
        session->flow.route_events.state.dispatch.phase;
    session->debug.route_event_last_deferred_kind =
        event->snapshot.event_kind;
    session->debug.route_event_last_deferred_node =
        event->node;
    session->debug.route_event_deferred_phases[index] =
        session->flow.route_events.state.dispatch.phase;
    session->debug.route_event_deferred_ordinals[index] =
        session->flow.route_events.state.dispatch.current_ordinal;
    session->debug.route_event_deferred_nodes[index] =
        event->node;
    session->debug.route_event_deferred_object_ids[index] =
        event->object_ref.raw_object_id;
    session->debug.route_event_deferred_object_types[index] =
        rich4_route_event_object_type_from_raw_id(
            event->object_ref.raw_object_id);
    session->debug.route_event_deferred_object_indices[index] =
        rich4_route_event_object_index_from_raw_id(
            event->object_ref.raw_object_id);
    session->debug.route_event_deferred_event_kinds[index] =
        event->snapshot.event_kind;
    session->debug.route_event_deferred_property_branches[index] =
        event->snapshot.property_branch;
    session->debug.route_event_deferred_property_owners[index] =
        event->snapshot.property_owner;
    session->debug.route_event_deferred_property_values[index] =
        event->snapshot.property_value;
    return 1;
}

void rich4_app_route_prepare_dispatch(GameSession *session,
                                               int player_index)
{
    int i;
    int first_pass_event;

    rich4_app_route_reset_dispatch(session);
    if (session == NULL) {
        return;
    }

    rich4_route_event_executor_reset(&session->flow.route_events);
    for (i = 0; i < session->flow.route_facts.count; i++) {
        const RouteEvent *fact = rich4_route_event_collection_at(
            &session->flow.route_facts, i);
        if (fact != NULL)
            rich4_route_event_queue_push(&session->flow.route_events.queue, fact);
    }
    session->flow.route_event_turn_token++;
    if (session->flow.route_event_turn_token == 0)
        session->flow.route_event_turn_token++;
    rich4_route_event_executor_begin(&session->flow.route_events,
                                     player_index,
                                     session->flow.route_event_turn_token);
    session->flow.route_events.state.dispatch.pass_count =
        rich4_route_event_collection_count_phase(
            &session->flow.route_facts, ROUTE_EVENT_PHASE_PASS);
    session->flow.route_events.state.dispatch.total_count =
        session->flow.route_events.state.dispatch.pass_count +
        rich4_route_event_collection_count_phase(
            &session->flow.route_facts, ROUTE_EVENT_PHASE_STOP);

    first_pass_event = rich4_app_route_next_dispatchable_pass_event(session, 0);
    if (first_pass_event >= 0) {
        session->flow.route_events.state.dispatch.phase = GAME_ROUTE_EVENT_DISPATCH_PASS;
        session->flow.route_events.state.dispatch.index = first_pass_event;
    } else if (rich4_route_event_collection_count_phase(
                   &session->flow.route_facts,
                   ROUTE_EVENT_PHASE_STOP) > 0) {
        session->flow.route_events.state.dispatch.phase = GAME_ROUTE_EVENT_DISPATCH_STOP;
        session->flow.route_events.state.dispatch.index = -1;
    } else {
        session->flow.route_events.state.dispatch.phase = GAME_ROUTE_EVENT_DISPATCH_DONE;
        session->flow.route_events.state.dispatch.index = -1;
    }
    rich4_app_route_update_dispatch_next(session);
}



void rich4_app_route_reset_facts(GameSession *session)
{
    if (session == NULL) {
        return;
    }
    rich4_route_event_collection_reset(&session->flow.route_facts);
    rich4_route_event_executor_reset(&session->flow.route_events);
}

void rich4_app_route_set_hover(GameSession *session, int hover)
{
    if (session != NULL) session->presentation.route_interaction.hover = hover;
}

void rich4_app_route_set_pressed(GameSession *session, int pressed)
{
    if (session != NULL)
        session->presentation.route_interaction.pressed = pressed;
}

int rich4_app_route_interaction_is_mandatory_facility_build(
    const GameSession *session)
{
    const GameCommand *accept;
    int id;
    if (session == NULL || !session->presentation.route_interaction.active)
        return 0;
    accept = &session->presentation.route_interaction.accept;
    id = accept->subject.id;
    return accept->type == GAME_COMMAND_UPGRADE_PROPERTY &&
           accept->subject.kind == GAME_DOMAIN_FACILITY && id >= 0 &&
           id < session->state.facility_count &&
           session->state.facility_level[id] == 0;
}

int rich4_app_route_interaction_is_facility_research(
    const GameSession *session)
{
    const GameCommand *accept;
    GameFacilityAction action;
    int id;
    if (session == NULL || !session->presentation.route_interaction.active)
        return 0;
    accept = &session->presentation.route_interaction.accept;
    id = accept->subject.id;
    if (accept->type != GAME_COMMAND_UPGRADE_PROPERTY ||
        accept->subject.kind != GAME_DOMAIN_FACILITY || id < 0 ||
        id >= session->state.facility_count ||
        session->state.facility_level[id] <= 0)
        return 0;
    return rich4_facility_inspect(
               &session->state, accept->player, id, &action) &&
           action.kind == GAME_FACILITY_ACTION_RESEARCH;
}

int rich4_app_route_interaction_is_facility_choice(
    const GameSession *session)
{
    return rich4_app_route_interaction_is_mandatory_facility_build(session) ||
           rich4_app_route_interaction_is_facility_research(session);
}

int rich4_app_route_interaction_uses_confirm_dialog(
    const GameSession *session)
{
    const GameCommand *accept;
    if (session == NULL || !session->presentation.route_interaction.active)
        return 0;
    accept = &session->presentation.route_interaction.accept;
    return accept->type != GAME_COMMAND_BANK_WITHDRAW &&
           accept->type != GAME_COMMAND_BANK_APPLY_LOAN &&
           !rich4_app_route_interaction_is_facility_choice(session);
}

int rich4_app_route_facility_choice_at(int x, int y)
{
    if (x < 50 || x >= 390 || y < 286 || y >= 354) return -1;
    return (x - 50) / 68;
}

int rich4_app_route_facility_research_choice_at(int x, int y)
{
    /* 0x4402D7: Data[517] research strip, five 76-pixel cells. */
    if (x < 35 || x > 406 || y < 297 || y > 351) return -1;
    return (x - 35) / 76;
}

GameCommand rich4_app_route_facility_build_command(
    const GameSession *session, int choice)
{
    const GameCommand *accept;
    if (session == NULL || !rich4_app_route_interaction_is_mandatory_facility_build(
            session) || choice < 0 || choice >= 5)
        return rich4_game_command(
            GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, -1, 0);
    accept = &session->presentation.route_interaction.accept;
    return rich4_game_command_upgrade_property(
        accept->source, accept->player, accept->subject, choice);
}

GameCommand rich4_app_route_facility_research_command(
    const GameSession *session, int choice)
{
    const GameCommand *accept;
    int id;
    if (session == NULL ||
        !rich4_app_route_interaction_is_facility_research(session))
        return rich4_game_command(
            GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, -1, 0);
    accept = &session->presentation.route_interaction.accept;
    id = accept->subject.id;
    if (choice < 0 || choice >= session->state.facility_level[id])
        return rich4_game_command(
            GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, -1, 0);
    return rich4_game_command_upgrade_property(
        accept->source, accept->player, accept->subject, choice);
}

int rich4_app_route_interaction_command_for_choice(
    const GameSession *session, Rich4AppRouteInteractionChoice choice,
    GameCommand *out_command)
{
    if (out_command != NULL)
        *out_command = rich4_game_command(
            GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, -1, 0);
    if (session == NULL || out_command == NULL ||
        !session->presentation.route_interaction.active ||
        !rich4_app_route_interaction_uses_confirm_dialog(session) ||
        (choice != RICH4_APP_ROUTE_INTERACTION_ACCEPT &&
         choice != RICH4_APP_ROUTE_INTERACTION_DECLINE))
        return 0;
    *out_command = choice == RICH4_APP_ROUTE_INTERACTION_DECLINE
        ? session->presentation.route_interaction.decline
        : session->presentation.route_interaction.accept;
    return out_command->type != GAME_COMMAND_NONE;
}

void rich4_app_route_reset_dispatch(GameSession *session)
{
    if (session == NULL) {
        return;
    }
    rich4_route_event_executor_reset_dispatch(&session->flow.route_events);
    session->presentation.route_interaction.active = 0;
    session->presentation.route_interaction.hover = 0;
    session->presentation.route_interaction.pressed = -1;
    memset(&session->presentation.route_interaction.accept, 0,
           sizeof(session->presentation.route_interaction.accept));
    memset(&session->presentation.route_interaction.decline, 0,
           sizeof(session->presentation.route_interaction.decline));
    session->debug.route_event_last_handler_family = GAME_ROUTE_EVENT_HANDLER_NONE;
    session->debug.route_event_last_handler_phase = GAME_ROUTE_EVENT_DISPATCH_NONE;
    session->debug.route_event_last_handler_node = 0;
    memset(session->debug.route_event_handled_families, 0, sizeof(session->debug.route_event_handled_families));
    memset(session->debug.route_event_handled_phases, 0, sizeof(session->debug.route_event_handled_phases));
    memset(session->debug.route_event_handled_ordinals, 0, sizeof(session->debug.route_event_handled_ordinals));
    memset(session->debug.route_event_handled_nodes, 0, sizeof(session->debug.route_event_handled_nodes));
    memset(session->debug.route_event_handled_event_kinds, 0, sizeof(session->debug.route_event_handled_event_kinds));
    memset(session->debug.route_event_handled_property_branches, 0, sizeof(session->debug.route_event_handled_property_branches));
    session->debug.route_event_deferred_count = 0;
    session->debug.route_event_last_deferred_phase = GAME_ROUTE_EVENT_DISPATCH_NONE;
    session->debug.route_event_last_deferred_kind = GAME_ROUTE_EVENT_NONE;
    session->debug.route_event_last_deferred_node = 0;
    memset(session->debug.route_event_deferred_phases, 0, sizeof(session->debug.route_event_deferred_phases));
    memset(session->debug.route_event_deferred_ordinals, 0, sizeof(session->debug.route_event_deferred_ordinals));
    memset(session->debug.route_event_deferred_nodes, 0, sizeof(session->debug.route_event_deferred_nodes));
    memset(session->debug.route_event_deferred_object_ids, 0, sizeof(session->debug.route_event_deferred_object_ids));
    memset(session->debug.route_event_deferred_object_types, 0, sizeof(session->debug.route_event_deferred_object_types));
    memset(session->debug.route_event_deferred_object_indices, 0xff, sizeof(session->debug.route_event_deferred_object_indices));
    memset(session->debug.route_event_deferred_event_kinds, 0, sizeof(session->debug.route_event_deferred_event_kinds));
    memset(session->debug.route_event_deferred_property_branches, 0, sizeof(session->debug.route_event_deferred_property_branches));
    memset(session->debug.route_event_deferred_property_owners, 0xff, sizeof(session->debug.route_event_deferred_property_owners));
    memset(session->debug.route_event_deferred_property_values, 0, sizeof(session->debug.route_event_deferred_property_values));
}
