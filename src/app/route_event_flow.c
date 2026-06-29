#include "app/route_event_flow.h"
#include "app/special_place_notice_flow.h"

#include "app/auction_flow.h"
#include "app/engineering_attack_flow.h"
#include "app/bankruptcy_flow.h"
#include "app/bank_flow.h"
#include "app/confinement_flow.h"
#include "app/game_speech_flow.h"
#include "app/insurance_flow.h"
#include "app/hotel_flow.h"
#include "app/magic_house_flow.h"
#include "app/point_minigame_flow.h"
#include "app/property_flow.h"
#include "app/roulette_flow.h"
#include "app/route_flow.h"
#include "app/shop_flow.h"
#include "app/special_presentation_flow.h"
#include "app/turn_status_flow.h"
#include "app/turn_movement_flow.h"
#include "core/god.h"
#include "core/character_speech_rules.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/property.h"
#include "core/route_event_executor.h"
#include "core/turn_flow.h"

static int route_event_is_bank_interaction(const GameEvent *event)
{
    int accept;
    int decline;
    if (event == NULL) return 0;
    accept = rich4_game_event_interaction_accept_type(event);
    decline = rich4_game_event_interaction_decline_type(event);
    return (accept == GAME_COMMAND_BANK_WITHDRAW &&
            decline == GAME_COMMAND_BANK_DEPOSIT) ||
           (accept == GAME_COMMAND_BANK_APPLY_LOAN &&
            decline == GAME_COMMAND_BANK_REPAY_LOAN);
}

int rich4_app_route_event_presentation(
    const GameEvent *event, Rich4AppRoutePresentation *out_presentation)
{
    if (out_presentation == NULL) return 0;
    out_presentation->kind = RICH4_ROUTE_PRESENTATION_NONE;
    if (event == NULL) return 0;

    switch (event->type) {
    case GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED:
        out_presentation->kind = RICH4_ROUTE_PRESENTATION_SPECIAL_EVENT;
        break;
    case GAME_EVENT_TOOL_PRESENTATION_REQUESTED:
        if (rich4_game_event_tool_presentation_requested_fields(event).subject_ref.kind != GAME_DOMAIN_GOD)
            return 0;
        out_presentation->kind = RICH4_ROUTE_PRESENTATION_GOD_ANIMATION;
        break;
    case GAME_EVENT_PROPERTY_LEVEL_CHANGED:
        out_presentation->kind = RICH4_ROUTE_PRESENTATION_PROPERTY_LEVEL;
        break;
    case GAME_EVENT_STATUS_CHANGED:
        out_presentation->kind = RICH4_ROUTE_PRESENTATION_STATUS;
        break;
    case GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED:
        out_presentation->kind = RICH4_ROUTE_PRESENTATION_SPECIAL_PLACE;
        break;
    case GAME_EVENT_LOTTERY_VISIT_REQUESTED:
        out_presentation->kind = RICH4_ROUTE_PRESENTATION_LOTTERY_VISIT;
        break;
    default:
        return 0;
    }
    out_presentation->event = *event;
    return 1;
}

int rich4_app_route_event_apply(
    GameSession *session, const GameEvent *event,
    const RouteEvent *current_route_event, uint32_t now,
    int animation_enabled, Rich4AudioEventQueue *audio_events)
{
    GameCommandSource source;
    if (session == NULL || event == NULL) return 0;

    switch (event->type) {
    case GAME_EVENT_SPECIAL_PLACE_NOTICE_REQUESTED:
        return rich4_special_place_notice_enqueue(session, event, now);
    case GAME_EVENT_SPECIAL_EVENT_PRESENTATION_REQUESTED:
        return 0;
    case GAME_EVENT_INTERACTION_REQUESTED:
        if (session->presentation.special_event.active &&
            rich4_game_event_interaction_accept_type(event) ==
                GAME_COMMAND_CONFIRM)
            return 1;
        source = session->state.players[event->player].is_ai
            ? GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN;
        session->presentation.route_interaction.accept =
            rich4_game_command_with_turn_token(
                rich4_game_domain_command(
                    (GameCommandType)rich4_game_event_interaction_accept_type(event),
                    source, event->player,
                    rich4_game_event_interaction_subject(event),
                    rich4_game_event_interaction_target(event), 0),
                rich4_game_event_interaction_turn_token(event));
        session->presentation.route_interaction.decline =
            rich4_game_command_with_turn_token(
                rich4_game_domain_command(
                    (GameCommandType)rich4_game_event_interaction_decline_type(event),
                    source, event->player,
                    rich4_game_event_interaction_subject(event),
                    rich4_game_event_interaction_target(event), 0),
                rich4_game_event_interaction_turn_token(event));
        session->presentation.route_interaction.active = 1;
        session->presentation.route_interaction.pressed = -1;
        if (route_event_is_bank_interaction(event))
            rich4_bank_flow_begin_interaction(
                session,
                rich4_game_event_interaction_accept_type(event),
                current_route_event, now, animation_enabled, audio_events);
        return 1;
    case GAME_EVENT_ROUTE_EVENT_RESOLVED:
        session->presentation.route_interaction.active = 0;
        if (rich4_game_event_route_event_result(event) ==
                ROUTE_EVENT_UNIMPLEMENTED_CONFIRMED ||
            rich4_game_event_route_event_result(event) ==
                ROUTE_EVENT_UNKNOWN_SKIPPED)
            rich4_app_route_defer_current_event(session);
        return 1;
    case GAME_EVENT_PROPERTY_OWNER_CHANGED:
        /* 0x40F381's takeover is not a purchase. */
        if (rich4_game_event_property_owner_changed_fields(event).target_ref.kind ==
            GAME_DOMAIN_GOD)
            return 1;
        if (rich4_game_event_property_owner(event) < 0 ||
            rich4_game_event_property_value(event) < 0)
            return 1;
        session->presentation.property.sound_pending = 1;
        session->presentation.property.purchase_delay_pending = 1;
        if (rich4_game_event_property_ref(event).kind != GAME_DOMAIN_LAND ||
            rich4_property_same_group_owned_count(
                &session->state, event->player,
                rich4_game_event_property_ref(event).id) < 3 ||
            !game_start_property_speech(
                session, event->player, RICH4_PROPERTY_SPEECH_PURCHASE_THREE,
                0, GAME_PROPERTY_PRESENT_NONE, now)) {
            session->presentation.property.purchase_delay_pending = 0;
            rich4_turn_flow_start_property_end_delay(&session->flow, now);
        }
        return 1;
    case GAME_EVENT_PROPERTY_LEVEL_CHANGED:
    case GAME_EVENT_SPECIAL_PLACE_PRESENTATION_REQUESTED:
    case GAME_EVENT_LOTTERY_VISIT_REQUESTED:
        return 0;
    case GAME_EVENT_STATUS_CHANGED:
        return rich4_hotel_capture_entry(session, event);
    case GAME_EVENT_INSURANCE_FEE_PRESENTATION_REQUESTED:
    case GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED:
        rich4_insurance_flow_enqueue_notice(session, event, now);
        return 1;
    case GAME_EVENT_PROPERTY_CHARGE_PRESENTATION_REQUESTED:
        rich4_property_begin_charge_presentation(session, event, now);
        return 1;
    case GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED:
        rich4_property_begin_beggar_presentation(session, event, now);
        return 1;
    case GAME_EVENT_GOD_TRADE_REJECTION_PRESENTATION_REQUESTED:
        rich4_property_begin_god_trade_rejection(session, event, now);
        return 1;
    case GAME_EVENT_BANK_REJECTION_PRESENTATION_REQUESTED:
        rich4_bank_rejection_begin(session, event, now, audio_events);
        return 1;
    case GAME_EVENT_CONFINEMENT_VISIT_REQUESTED:
        rich4_confinement_begin(session, event, now, audio_events);
        return 1;
    case GAME_EVENT_POINT_MINIGAME_REQUESTED:
    case GAME_EVENT_POINT_MINIGAME_AUTOMATIC_RESOLVED:
        rich4_point_minigame_begin(
            session, event, now, animation_enabled, audio_events);
        return 1;
    case GAME_EVENT_SHOP_VISIT_REQUESTED:
        rich4_shop_begin(
            session, event, now, animation_enabled, audio_events);
        return 1;
    case GAME_EVENT_FACILITY_ROULETTE_REQUESTED:
        rich4_roulette_begin(session, event, now, audio_events);
        return 1;
    case GAME_EVENT_MARKET_CHANGED:
        return rich4_company_issue_control_notice(session, event);
    case GAME_EVENT_COMPANY_STOCK_ISSUE_REQUESTED:
        rich4_company_issue_begin(session, event);
        return 1;
    case GAME_EVENT_MAGIC_HOUSE_REQUESTED:
        rich4_magic_house_begin(
            session, event, now, animation_enabled, audio_events);
        return 1;
    case GAME_EVENT_GAME_ENDED:
        if (rich4_bankruptcy_capture_event(session, event, now))
            return 1;
        rich4_app_capture_game_ended(session, event);
        return 1;
    case GAME_EVENT_AUCTION_REQUESTED:
        if (rich4_game_event_auction_trigger(event) ==
            GAME_AUCTION_TRIGGER_BANKRUPTCY)
            return rich4_bankruptcy_capture_event(session, event, now);
        return rich4_auction_flow_enqueue(session, event, audio_events);
    case GAME_EVENT_PLAYER_ELIMINATED:
        if (rich4_bankruptcy_capture_event(session, event, now))
            return 1;
        if (event->player >= 0 && event->player < GAME_MAX_PLAYERS) {
            session->presentation.actors[event->player].move_active = 0;
            session->presentation.actors[event->player].dirty_flags |=
                GAME_PLAYER_DIRTY_TURN;
        }
        return 1;
    default:
        return 0;
    }
}

RouteEventExecutionResult rich4_app_route_event_step(
    GameSession *session, uint32_t now, int animation_enabled,
    GameEventQueue *presentation_events,
    Rich4AudioEventQueue *audio_events)
{
    const RouteEvent *current;
    RouteEvent current_copy;
    const RouteEvent *current_for_apply;
    GameEventQueue events;
    GameEvent resolved;
    RouteEventExecutionResult result;
    int entry_action = 0;

    if (session == NULL || presentation_events == NULL || audio_events == NULL)
        return ROUTE_EVENT_EXECUTION_INVALID;

    rich4_game_event_queue_reset(presentation_events);
    rich4_audio_event_queue_reset(audio_events);
    current = rich4_route_event_executor_current(&session->flow.route_events);
    current_for_apply = NULL;
    if (current != NULL) {
        current_copy = *current;
        current_for_apply = &current_copy;
        if (!rich4_route_event_executor_is_waiting(
                &session->flow.route_events)) {
            rich4_app_route_dispatch_current_handler(session);
            /* 0x419884 plays before special dispatch for every actor.
             * Bank's second page and pass entry do not replay this sound. */
            if (current->phase == ROUTE_EVENT_PHASE_STOP &&
                current->snapshot.route_action >= 2 &&
                current->snapshot.route_action <= 16 &&
                session->flow.route_events.state.bank_stop_stage == 0 &&
                session->flow.route_events.state.execution_actor >= 0 &&
                session->flow.route_events.state.execution_actor < session->state.player_count) {
                const GamePlayerState *player = &session->state.players[
                    session->flow.route_events.state.execution_actor];
                if ((player->play_state & 0xff) != 0 && !player->days_sleep_walking)
                    entry_action = current->snapshot.route_action;
            }
        }
    }
    rich4_game_event_queue_reset(&events);
    result = rich4_route_event_executor_step(
        &session->flow.route_events, &session->state, &events);
    /* A beggar's arrival modal precedes 0x41982D; the square has not been
     * dispatched yet. Its returning executor step emits the entry once. */
    if (entry_action != 0 && result != ROUTE_EVENT_EXECUTION_INVALID &&
        !(events.count > 0 && events.events[events.head].type ==
            GAME_EVENT_BEGGAR_DONATION_PRESENTATION_REQUESTED))
        rich4_audio_event_emit(audio_events, RICH4_AUDIO_EVENT_SPECIAL_PLACE_ENTRY,
                              1, entry_action);
    while (rich4_game_event_queue_pop(&events, &resolved)) {
        if (!rich4_app_route_event_apply(
                session, &resolved, current_for_apply, now,
                animation_enabled, audio_events) &&
            !rich4_game_event_queue_push(presentation_events, &resolved))
            return ROUTE_EVENT_EXECUTION_INVALID;
    }
    return result;
}

int rich4_app_route_event_finish(GameSession *session, uint32_t now)
{
    if (session == NULL) return 0;
    session->flow.route_events.state.dispatch.phase =
        GAME_ROUTE_EVENT_DISPATCH_DONE;
    if (!session->flow.route_events.state.dispatch.resume_movement)
        return 0;
    /* sub_41982D resumes the remaining route only after the pass-event modal
     * handler has completed.  The state transition is app-owned; SDL only
     * supplies the frame clock and later renders the resumed movement. */
    session->flow.route_events.state.dispatch.resume_movement = 0;
    rich4_app_route_reset_facts(session);
    rich4_app_route_reset_dispatch(session);
    session->flow.turn_state = GAME_TURN_STATE_MOVE;
    session->flow.turn_state_frame = 0;
    session->flow.turn_state_tick = now;
    return 1;
}

static void complete_route_turn(GameSession *session, const MapData *map,
                                 uint32_t now,
                                 const Rich4AppRouteEventServices *services)
{
    Rich4AppTurnRotationResult rotation = { 0 };
    session->flow.turn_wait = 0;
    if (rich4_app_complete_route_event_turn(session, map, now, &rotation) &&
        rotation.special_action_pending && services->begin_special_actor != NULL)
        services->begin_special_actor(services->actor_context, &rotation);
}

static int stopped_common_tail_node(const GameSession *session)
{
    int i;
    for (i = 0; i < session->flow.route_facts.count; ++i) {
        const RouteEvent *event = rich4_route_event_collection_at(
            &session->flow.route_facts, i);
        if (event != NULL && event->phase == ROUTE_EVENT_PHASE_STOP &&
            event->snapshot.route_action == 0 &&
            event->object_ref.raw_object_id != 0)
            return event->node;
    }
    return -1;
}

int rich4_app_route_event_update(
    GameSession *session, const MapData *map, uint32_t now,
    int animation_enabled, const Rich4AppRouteEventServices *services)
{
    RouteEventExecutionResult execution_result;
    GameEventQueue events;
    GameEvent resolved;

    if (session == NULL || services == NULL ||
        session->flow.turn_state != GAME_TURN_STATE_ROUTE_EVENTS)
        return 0;
    if (session->presentation.engineering_attack.active) return 1;
    if (session->presentation.special_place_notice.queue.count > 0) return 1;
    if (services->update_property_charge != NULL &&
        services->update_property_charge(services->property_context))
        return 1;
    if (session->presentation.hotel_entry.active) {
        int result = rich4_hotel_update_entry(session, map, now,
            (int)services->property_end_delay_ms);
        if (result == 2) complete_route_turn(session, map, now, services);
        return 1;
    }
    if (session->presentation.property.max_level_pending &&
        !session->presentation.speech.active &&
        services->start_property_max_level != NULL)
        (void)services->start_property_max_level(services->property_context);
    if (session->presentation.property.max_level_active &&
        services->update_property_max_level != NULL &&
        services->update_property_max_level(services->property_context))
        return 1;
    /* 0x418F59/0x418F78: finish the returning actor's god presentations
     * before checking demolition; this path has no new EA5=0x88 delay. */
    if (session->presentation.engineering_attack.pending_return &&
        session->deferred_events.count == 0) {
        int player = session->state.current_player;
        session->presentation.engineering_attack.pending_return = 0;
        (void)rich4_engineering_attack_begin(
            session, player, session->state.players[player].node, now);
        session->flow.turn_flow_active = 0;
        session->flow.turn_state = GAME_TURN_STATE_IDLE;
        return 1;
    }
    if (session->flow.turn_wait == RICH4_MOVE_BEGGAR) {
        if (session->flow.route_events.state.waiting_for_presentation ||
            session->flow.route_events.state.waiting_for_liquidation ||
            session->presentation.bankruptcy.active ||
            session->presentation.auction.active)
            return 1;
        rich4_app_route_reset_facts(session);
        rich4_app_route_reset_dispatch(session);
        session->flow.turn_state = GAME_TURN_STATE_MOVE;
        session->flow.turn_wait = RICH4_MOVE_AFTER_BEGGAR;
        return 1;
    }
    if (session->flow.turn_wait == RICH4_ROUTE_END_COUNTDOWN) {
        if (!rich4_turn_flow_update_property_end_delay(
                &session->flow, now, services->property_end_delay_ms))
            complete_route_turn(session, map, now, services);
        return 1;
    }

    /* Direct surrender is initiated from the options panel, where no route
     * executor is running.  Its bankruptcy animation still releases auction
     * and ending events through the same transient queue, so drain that queue
     * without asking the executor to advance a nonexistent route event. */
    if (!session->flow.route_events.state.execution_started &&
        session->deferred_events.count > 0) {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(session);
        Rich4AudioEvent audio_event;
        GameEventQueue pending;
        GameEvent deferred;
        int deferred_ok = 1;
        rich4_game_event_queue_reset(&pending);
        while (rich4_game_event_queue_pop(
                &session->deferred_events, &deferred)) {
            if (!rich4_app_route_event_apply(
                session, &deferred, NULL, now, animation_enabled,
                    audio_events) &&
                !rich4_game_event_queue_push(&pending, &deferred)) {
                deferred_ok = 0;
                break;
            }
        }
        audio_events = rich4_app_session_audio_release(session);
        while (rich4_audio_event_pop(audio_events, &audio_event))
            if (services->consume_audio_event != NULL)
                services->consume_audio_event(
                    services->audio_context, &audio_event);
        while (rich4_game_event_queue_pop(&pending, &deferred)) {
            Rich4AppRoutePresentation presentation;
            if (rich4_app_route_event_presentation(
                    &deferred, &presentation) && services->present != NULL)
                services->present(services->presentation_context, &presentation);
        }
        if (!deferred_ok) return ROUTE_EVENT_EXECUTION_INVALID;
        return 1;
    }

    {
        Rich4AudioEventQueue *audio_events =
            rich4_app_session_audio_borrow(session);
        Rich4AudioEvent audio_event;
        int deferred_ok = 1;

        if (session->flow.route_events.state.execution_started) {
            execution_result = rich4_app_route_event_step(
                session, now, animation_enabled, &events, audio_events);
        } else {
            /* An empty road (raw object 0) has no executor queue, but
             * 0x41982D still returns 0x80 for next-tick turn rotation. */
            rich4_game_event_queue_reset(&events);
            execution_result = ROUTE_EVENT_EXECUTION_FINISHED;
        }
        while (rich4_game_event_queue_pop(
                &session->deferred_events, &resolved)) {
            if (!rich4_app_route_event_apply(
                session, &resolved, NULL, now, animation_enabled,
                    audio_events) &&
                !rich4_game_event_queue_push(
                    &events, &resolved)) {
                deferred_ok = 0;
                break;
            }
        }
        audio_events = rich4_app_session_audio_release(session);
        while (rich4_audio_event_pop(audio_events, &audio_event))
            if (services->consume_audio_event != NULL)
                services->consume_audio_event(
                    services->audio_context, &audio_event);
        if (!deferred_ok) return ROUTE_EVENT_EXECUTION_INVALID;
    }
    if (session->presentation.bankruptcy.active ||
        session->presentation.auction.active)
        return 1;
    while (rich4_game_event_queue_pop(&events, &resolved)) {
        Rich4AppRoutePresentation presentation;
        if (!rich4_app_route_event_presentation(
                &resolved, &presentation))
            continue;
        if (services->present != NULL)
            services->present(services->presentation_context, &presentation);
    }
    if (session->presentation.property.phase != GAME_PROPERTY_PRESENT_NONE) return 1;
    if (session->presentation.insurance.count > 0) return 1;
    if (execution_result == ROUTE_EVENT_EXECUTION_PROGRESSED ||
        execution_result == ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND ||
        execution_result == ROUTE_EVENT_EXECUTION_INVALID)
        return 1;
    if (rich4_app_route_event_finish(session, now)) return 1;
    /* 0x41982D's shared tail calls 0x40F381 after the property branch.
     * No EA5 end ticks elapse during its presentations. */
    if (session->flow.turn_wait != RICH4_ROUTE_END_EFFECTS) {
        int node = stopped_common_tail_node(session);
        session->flow.turn_wait = RICH4_ROUTE_END_EFFECTS;
        if (node >= 0) {
            if (rich4_property_begin_god_landing(
                    session, session->state.current_player, node, now)) return 1;
        }
    }
    session->flow.property_turn_end_ticks =
        stopped_common_tail_node(session) >= 0 ? 8 : 1;
    /* 0x41B09D: after 0x40F381, before EA5's eight idle ticks. */
    (void)rich4_engineering_attack_begin(
        session, session->state.current_player,
        stopped_common_tail_node(session), now);
    if (session->presentation.hotel_entry.pending &&
        rich4_hotel_update_entry(session, map, now,
            (int)services->property_end_delay_ms))
        return 1;
    /* Modal waits leave one timer pulse pending (0x401F98); keep the
     * movement epoch instead of restarting a full tick after the dialog. */
    session->flow.property_turn_end_tick = session->presentation.movement_tick != 0
        ? session->presentation.movement_tick : now;
    session->flow.turn_wait = RICH4_ROUTE_END_COUNTDOWN;
    return 1;
}
