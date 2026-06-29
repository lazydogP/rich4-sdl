#include "app/property_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/god.h"
#include "app/monthly_finance_flow.h"

#include <string.h>

#include "app/dynamic_object_flow.h"
#include "app/game_speech_flow.h"
#include "app/confinement_flow.h"
#include "app/hotel_flow.h"
#include "app/route_geometry_flow.h"
#include "app/turn_status_flow.h"
#include "core/confinement.h"
#include "core/game_constants.h"
#include "core/game_event.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/status_countdown.h"
#include "core/stock.h"
#include "core/turn_rotation.h"
#include "data/character_speech.h"

static int turn_player_index(int value, int count)
{
    if (count <= 0) return -1;
    value %= count;
    return value < 0 ? value + count : value;
}

static void begin_confinement_exit(GameSession *session, const MapData *map,
                                   int player_index, int kind)
{
    GamePlayerState *player;
    GameActorPresentationState *actor;
    int from_x;
    int from_y;
    int to_x;
    int to_y;
    int dx;
    int dy;
    float duration;

    if (session == NULL || map == NULL || player_index < 0 ||
        player_index >= session->state.player_count)
        return;
    player = &session->state.players[player_index];
    if (player->node <= 0 || (size_t)player->node >= map->route_count ||
        !(kind == GAME_CONFINEMENT_HOTEL
            ? rich4_hotel_anchor(&session->state, map, player_index, &from_x, &from_y)
            : rich4_confinement_anchor_xy(map, kind, &from_x, &from_y)))
        return;
    /* 0x40D6BE keeps +0x32 until the return move passes halfway. */
    if (kind == GAME_CONFINEMENT_HOTEL)
        player->delayed_status_block = GAME_STATUS_PENDING_BIT;
    to_x = map->routes[player->node].x;
    to_y = map->routes[player->node].y;
    actor = &session->presentation.actors[player_index];
    if (kind == GAME_CONFINEMENT_HOTEL) {
        session->presentation.hotel_entry.saved_direction = actor->direction;
        session->presentation.hotel_entry.wait_ticks = 0;
    }
    dx = to_x - from_x;
    dy = to_y - from_y;
    duration = rich4_original_fixed_move_duration(dx, dy);
    actor->visual_x = from_x;
    actor->visual_y = from_y;
    actor->move_active = 1;
    actor->move_from_node = player->node;
    actor->move_to_node = player->node;
    actor->move_from_x = from_x;
    actor->move_from_y = from_y;
    actor->move_to_x = to_x;
    actor->move_to_y = to_y;
    actor->move_progress = 0;
    actor->move_distance = (int)duration;
    if (actor->move_distance <= 0) actor->move_distance = 1;
    actor->direction = rich4_game_actor_direction_from_delta(dx, dy);
    actor->walk_frame = 0;
    session->presentation.confinement_exit.active = 1;
    session->presentation.confinement_exit.player = player_index;
    session->presentation.confinement_exit.kind = kind;
    session->presentation.confinement_exit.last_step_tick = 0;
    session->presentation.has_view_override = 0;
}

static void consume_turn_events(GameSession *session, const MapData *map,
                                uint32_t now, GameEventQueue *events)
{
    GameEvent event;

    while (rich4_game_event_queue_pop(events, &event)) {
        if ((session->presentation.periodic_finance.active ||
             session->presentation.bank_settlement.active) &&
            (event.type == GAME_EVENT_STATUS_TICK_REQUESTED ||
             event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED ||
             event.type == GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED)) {
            (void)rich4_game_event_queue_push(&session->pending_turn_events, &event);
            continue;
        }
        if (event.type == GAME_EVENT_FULL_ROUND_COMPLETED) {
            rich4_app_update_win_candidate(session);
        } else if (event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED &&
                   event.player >= 0 &&
                   event.player < session->state.player_count) {
            session->presentation.actors[event.player].dirty_flags |=
                GAME_PLAYER_DIRTY_TURN;
        } else if (event.type == GAME_EVENT_STATUS_TICK_REQUESTED &&
                   event.player >= 0 &&
                   event.player < session->state.player_count) {
            GamePlayerState *player = &session->state.players[event.player];
            int return_direction = session->presentation.actors[event.player].direction;
            int confinement_kind =
                (player->delayed_status_block & GAME_STATUS_PENDING_BIT) != 0
                    ? GAME_CONFINEMENT_HOTEL
                : (player->days_in_prison & GAME_STATUS_PENDING_BIT) != 0
                    ? GAME_CONFINEMENT_PRISON
                : (player->days_in_hospital & GAME_STATUS_PENDING_BIT) != 0
                    ? GAME_CONFINEMENT_HOSPITAL
                    : GAME_CONFINEMENT_NONE;
            if (rich4_status_player_has_pending(player))
                rich4_app_process_pending_status_events(session, event.player);
            if (confinement_kind != GAME_CONFINEMENT_NONE)
                begin_confinement_exit(session, map, event.player,
                                       confinement_kind);
            if (confinement_kind == GAME_CONFINEMENT_HOTEL)
                session->presentation.hotel_entry.saved_direction = return_direction;
            rich4_app_update_player_status_countdowns(session, event.player);
            /* 0x41CC6F..0x41CC9B: tick the selected player's attached god
             * even during confinement; expire immediately at zero. */
            if (player->god_info > 0 && player->god_info <= GAME_MAP_DYNAMIC_OBJECT_COUNT &&
                (player->play_state & GAME_PLAYER_PLAY_STATE_MASK) != 0) {
                GameMapDynamicObjectState *god = &session->presentation.dynamic_objects[player->god_info - 1];
                if (god->attached_player == event.player + 1 && god->countdown > 0) {
                    if (god->countdown == 1) {
                        GameEventQueue ignored;
                        rich4_game_event_queue_reset(&ignored);
                        if (rich4_god_expire(&session->state, event.player, &ignored) == GAME_GOD_APPLIED)
                            (void)rich4_app_begin_dynamic_god_detach(
                                &session->presentation, &session->state, event.player, now);
                    } else --god->countdown;
                }
            }
        } else if (event.type == GAME_EVENT_PERIODIC_FINANCE_REQUESTED) {
            rich4_periodic_finance_begin(session, now);
        } else if (event.type == GAME_EVENT_BANK_SETTLEMENT_APPLIED &&
                   event.player >= 0 && event.player < GAME_MAX_PLAYERS) {
            unsigned int bit = 1u << event.player;
            GameBankSettlementPresentationState *settlement =
                &session->presentation.bank_settlement;
            if (!settlement->active) {
                settlement->active = 1;
                settlement->player_mask = 0;
                settlement->loan_mask = 0;
                memset(settlement->interest, 0,
                       sizeof(settlement->interest));
                memset(settlement->balance, 0,
                       sizeof(settlement->balance));
                settlement->frame = 0;
                settlement->tick = now;
                /* sub_437E61 reveals the two Panel25 stages on successive
                 * 100 ms timer ticks, then keeps the completed statement for
                 * 30 further ticks when speech playback is disabled. */
                settlement->until = now + 3200;
            }
            settlement->player_mask |= bit;
            if (rich4_game_event_bank_settlement_loan_outstanding(&event) != 0)
                settlement->loan_mask |= bit;
            settlement->interest[event.player] =
                rich4_game_event_bank_settlement_interest(&event);
            settlement->balance[event.player] =
                rich4_game_event_bank_settlement_balance(&event);
        } else if (event.type ==
                   GAME_EVENT_BANK_LOAN_DUE_PRESENTATION_REQUESTED &&
                   event.player >= 0 && event.player < GAME_MAX_PLAYERS) {
            session->presentation.bank_loan_due.active = 1;
            session->presentation.bank_loan_due.player = event.player;
            session->presentation.bank_loan_due.days =
                rich4_game_event_bank_loan_days(&event);
            session->presentation.bank_loan_due.principal =
                rich4_game_event_bank_loan_principal(&event);
            session->presentation.bank_loan_due.forced =
                rich4_game_event_bank_loan_forced(&event) != 0;
            session->presentation.bank_loan_due.payment =
                rich4_game_event_bank_loan_payment(&event);
            session->presentation.bank_loan_due.until =
                session->presentation.bank_settlement.active
                    ? session->presentation.bank_settlement.until + 1500
                    : now + 1500;
        }
    }
    if (session->presentation.bank_settlement.active &&
        session->presentation.bank_settlement.stage == 0)
        rich4_monthly_finance_begin(session);
}

int rich4_app_resume_turn_events(GameSession *session, const MapData *map,
                                 uint32_t now)
{
    if (session == NULL || session->pending_turn_events.count == 0 ||
        session->presentation.periodic_finance.active ||
        session->presentation.bank_settlement.active ||
        session->presentation.bankruptcy.active ||
        session->presentation.auction.active) return 0;
    if (session->state.game_over)
        rich4_game_event_queue_reset(&session->pending_turn_events);
    else
        consume_turn_events(session, map, now, &session->pending_turn_events);
    return 1;
}

int rich4_app_advance_turn_rotation(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result)
{
    int guard;
    int previous_elapsed_days;

    if (out_result != NULL) memset(out_result, 0, sizeof(*out_result));
    if (session == NULL || !session->state.active || session->state.game_over ||
        session->state.player_count <= 0 || session->pending_turn_events.count > 0)
        return 0;
    previous_elapsed_days = session->state.elapsed_days;
    for (guard = 0; guard < GAME_MAX_PLAYERS + 5; guard++) {
        TurnAdvanceResult result;
        GameEventQueue events;
        int previous_months = rich4_stock_elapsed_months(&session->state);

        /* 0x418F2C..0x418F87 returns the same ordinary actor. The god
         * handler may present a modal before 0x448A7E checks the new owner. */
        if (session->state.turn_rotation_index >= 0 &&
            session->state.turn_rotation_index < session->state.player_count &&
            (session->state.players[session->state.turn_rotation_index].play_state &
             0x10) != 0) {
            int player = session->state.turn_rotation_index;
            (void)rich4_property_begin_god_landing(
                session, player, session->state.players[player].node, now);
            session->presentation.engineering_attack.pending_return = 1;
        }
        rich4_game_event_queue_reset(&events);
        if (!rich4_turn_advance_once_events_at_tick(&session->state, &result,
                &events, now + session->rng_clock_offset))
            return 0;
        if (session->state.elapsed_days != previous_elapsed_days)
            session->presentation.auto_save_pending = 1;
        if (rich4_stock_elapsed_months(&session->state) > previous_months)
            (void)rich4_app_reset_monthly_dynamic_objects_with_rng(
                &session->presentation, map, &session->state.ai_rng_state);
        consume_turn_events(session, map, now, &events);
        if (result.kind == TURN_SELECTION_SPECIAL) {
            int slot = result.selected_index - GAME_MAX_PLAYERS;
            TurnSpecialAutoActionResult special_action;
            rich4_app_update_special_player_status_countdowns(session, slot);
            rich4_turn_run_special_auto_action(
                &session->state, map, slot, &special_action);
            if (special_action.kind == TURN_SPECIAL_AUTO_MOVED) {
                if (out_result != NULL) {
                    out_result->special_action_pending = 1;
                    out_result->special_action = special_action;
                }
                return 1;
            }
            continue;
        }
        if (result.kind == TURN_SELECTION_NORMAL) {
            int player_index = result.selected_index;
            const GamePlayerState *player =
                &session->state.players[player_index];
            if ((player->play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0)
                continue;
            /* The original clears the pending camera target before the
             * turn-start repaint.  A confined ordinary player is then
             * centered from its already-stored facility anchor by the
             * ViewModel, before the countdown prompt is shown. */
            session->presentation.has_view_override = 0;
            return 1;
        }
    }
    return 0;
}

static void retain_return_presentation(GameSession *session)
{
    if (!session->presentation.engineering_attack.pending_return) return;
    session->flow.turn_flow_active = 1;
    session->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
    session->flow.turn_wait = 0;
    session->flow.route_events.state.execution_started = 0;
}

int rich4_app_complete_route_event_turn(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result)
{
    int next;

    if (session == NULL || session->state.player_count <= 0) return 0;
    if (!rich4_app_advance_turn_rotation(session, map, now, out_result))
        return 0;
    session->flow.turn_flow_active = 0;
    session->flow.turn_state = GAME_TURN_STATE_IDLE;
    session->flow.turn_state_frame = 0;
    session->flow.steps_remaining = 0;
    session->presentation.dice.popup_active = 0;
    session->presentation.dice.status_visible = 0;
    session->presentation.dice.status_dragging = 0;
    session->presentation.movement_tick = 0;
    next = turn_player_index(session->state.current_player,
                             session->state.player_count);
    if (next >= 0)
        session->presentation.actors[next].dirty_flags |=
            GAME_PLAYER_DIRTY_TURN;
    retain_return_presentation(session);
    return 1;
}

int rich4_app_finish_control_locked_turn(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result)
{
    int next;

    if (session == NULL) return 0;
    session->flow.turn_flow_active = 0;
    session->flow.turn_state = GAME_TURN_STATE_IDLE;
    session->flow.turn_state_frame = 0;
    session->flow.turn_state_tick = now;
    session->flow.steps_remaining = 0;
    session->presentation.dice.popup_active = 0;
    session->presentation.dice.status_visible = 0;
    session->presentation.dice.status_dragging = 0;
    session->presentation.dice.action_hint_active = 0;
    session->presentation.dice.input_refresh_active = 0;
    session->presentation.confinement_status.active = 0;
    session->presentation.confinement_status.until = 0;
    if (!rich4_app_advance_turn_rotation(session, map, now, out_result))
        return 0;
    next = turn_player_index(session->state.current_player,
                             session->state.player_count);
    if (next >= 0)
        session->presentation.actors[next].dirty_flags |=
            GAME_PLAYER_DIRTY_TURN;
    retain_return_presentation(session);
    return 1;
}

int rich4_app_update_confinement_exit(
    GameSession *session, uint32_t now, int frame_ms)
{
    GameConfinementExitPresentationState *exit_flow;
    GameActorPresentationState *actor;

    if (session == NULL || !session->presentation.confinement_exit.active)
        return 0;
    exit_flow = &session->presentation.confinement_exit;
    if (exit_flow->player < 0 ||
        exit_flow->player >= session->state.player_count) {
        memset(exit_flow, 0, sizeof(*exit_flow));
        return 0;
    }
    actor = &session->presentation.actors[exit_flow->player];
    if (!rich4_consume_animation_pulse(
            &exit_flow->last_step_tick, now, frame_ms))
        return 1;
    if (exit_flow->kind == GAME_CONFINEMENT_HOTEL && !actor->move_active) {
        if (--session->presentation.hotel_entry.wait_ticks > 0) return 1;
        actor->direction = session->presentation.hotel_entry.saved_direction;
        memset(exit_flow, 0, sizeof(*exit_flow));
        /* 0x418F2C..0x418F87 restores the same actor without a new day/tick. */
        (void)rich4_app_advance_turn_rotation(session, NULL, now, NULL);
        retain_return_presentation(session);
        return 0;
    }
    actor->move_progress++;
    actor->walk_frame++;
    if (exit_flow->kind == GAME_CONFINEMENT_HOTEL &&
        actor->move_distance - actor->move_progress < (actor->move_distance >> 1)) {
        GamePlayerState *player = &session->state.players[exit_flow->player];
        player->delayed_status_block = 0;
        player->days_disappearing = player->days_in_prison = player->days_in_hospital = 0;
    }
    if (actor->move_progress >= actor->move_distance) {
        actor->visual_x = actor->move_to_x;
        actor->visual_y = actor->move_to_y;
        actor->move_progress = actor->move_distance;
        actor->move_active = 0;
        if (exit_flow->kind == GAME_CONFINEMENT_HOTEL) {
            session->presentation.hotel_entry.wait_ticks = 9;
            return 1;
        }
        memset(exit_flow, 0, sizeof(*exit_flow));
        return 0;
    }
    {
        float duration = rich4_original_fixed_move_duration(
            actor->move_to_x - actor->move_from_x,
            actor->move_to_y - actor->move_from_y);
        actor->visual_x = rich4_original_interpolated_axis(
            actor->move_from_x, actor->move_to_x,
            actor->move_progress, duration);
        actor->visual_y = rich4_original_interpolated_axis(
            actor->move_from_y, actor->move_to_y,
            actor->move_progress, duration);
    }
    return 1;
}
