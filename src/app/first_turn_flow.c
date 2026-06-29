#include "app/first_turn_flow.h"

#include "core/game_constants.h"
#include "core/game_event.h"
#include "core/player_status.h"
#include "debug/trace.h"

static int player_has_delayed_status_block(const GamePlayerState *player)
{
    if (player == NULL) {
        return 0;
    }
    return player->delayed_status_block != 0 ||
           player->days_disappearing != 0 ||
           player->days_in_prison != 0 ||
           player->days_in_hospital != 0;
}

static int player_should_remark_after_automatic_preaction(
    const GamePlayerState *player)
{
    if (player == NULL) {
        return 0;
    }
    return player_has_delayed_status_block(player) ||
           player->days_sleep_walking != 0 ||
           player->days_sleeping != 0;
}

FirstTurnDispatchPlan rich4_app_prepare_first_turn_dispatch(
    const GameSession *session, int current_player, int dispatch_result)
{
    if (session == NULL || current_player < 0 ||
        current_player >= session->state.player_count) {
        return rich4_first_turn_dispatch_plan(dispatch_result, 0);
    }
    return rich4_first_turn_dispatch_plan(
        dispatch_result,
        player_should_remark_after_automatic_preaction(
            &session->state.players[current_player]));
}

static void record_automatic_preaction(GameSession *session,
                                       int player_index,
                                       int dispatch_result,
                                       int status_remark_dirty)
{
    int i;
    int card_total = 0;
    int tool_total = 0;
    int player_card_total = 0;
    int player_tool_total = 0;

    if (session == NULL) {
        return;
    }
    for (i = 0; i < GAME_CARD_COUNT; i++) {
        card_total += session->state.card_amount[i];
    }
    for (i = 0; i < GAME_TOOL_COUNT; i++) {
        tool_total += session->state.tool_amount[i];
    }
    if (player_index >= 0 && player_index < session->state.player_count) {
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            if (session->state.players[player_index].card_inventory[i] != 0) {
                player_card_total++;
            }
        }
        for (i = 0; i < GAME_TOOL_COUNT; i++) {
            player_tool_total +=
                session->state.players[player_index].tool_inventory[i];
        }
    }
    session->debug.automatic_preaction_player = player_index;
    session->debug.automatic_preaction_dispatch_result = dispatch_result;
    session->debug.automatic_preaction_sale_state =
        GAME_AUTOMATIC_PREACTION_DEFERRED;
    session->debug.automatic_preaction_card_state =
        GAME_AUTOMATIC_PREACTION_DEFERRED;
    session->debug.automatic_preaction_tool_state =
        GAME_AUTOMATIC_PREACTION_DEFERRED;
    session->debug.automatic_preaction_status_checked = 1;
    session->debug.automatic_preaction_status_remark_dirty =
        status_remark_dirty;
    session->debug.automatic_preaction_card_pool_total = card_total;
    session->debug.automatic_preaction_tool_pool_total = tool_total;
    session->debug.automatic_preaction_player_card_total = player_card_total;
    session->debug.automatic_preaction_player_tool_total = player_tool_total;
}

static void reset_turn_dispatch_state(GameSession *session, uint32_t now)
{
    session->flow.turn_flow_active = 0;
    session->flow.turn_state = 0;
    session->flow.turn_state_frame = 0;
    session->flow.turn_state_tick = now;
}

Rich4FirstTurnPresentationRequest rich4_app_apply_first_turn_dispatch(
    GameSession *session,
    const FirstTurnDispatchPlan *dispatch_plan,
    int current_player,
    int landed_player,
    uint32_t now)
{
    const GamePlayerState *landed;

    if (session == NULL || dispatch_plan == NULL ||
        current_player < 0 || current_player >= session->state.player_count ||
        landed_player < 0 || landed_player >= session->state.player_count) {
        return RICH4_FIRST_TURN_PRESENTATION_NONE;
    }
    landed = &session->state.players[landed_player];

    if (dispatch_plan->result == 0 &&
        session->flow.player_status_decision.status_slot >= 50 &&
        session->flow.player_status_decision.status_slot <= 54) {
        return RICH4_FIRST_TURN_PRESENTATION_CONTROL_LOCKED;
    }
    if (dispatch_plan->result == -1 &&
        session->flow.player_status_decision.status_slot == 55) {
        session->flow.turn_state = GAME_TURN_STATE_PREROLL;
        session->flow.turn_state_frame = 0;
        session->flow.turn_state_tick = now;
        session->presentation.dice.popup_active = 0;
        session->flow.steps_remaining = 0;
        return RICH4_FIRST_TURN_PRESENTATION_NONE;
    }
    if (dispatch_plan->show_dice_panel) {
        reset_turn_dispatch_state(session, now);
        session->presentation.dice.popup_active = 0;
        session->flow.steps_remaining = 0;
        if (landed->is_ai) {
            session->presentation.actors[landed_player].dirty_flags |=
                GAME_PLAYER_DIRTY_TURN;
            session->presentation.dice.status_visible = 0;
            return RICH4_FIRST_TURN_PRESENTATION_AI_DICE;
        }
        return RICH4_FIRST_TURN_PRESENTATION_HUMAN_DICE;
    }
    if (dispatch_plan->automatic_preaction) {
        record_automatic_preaction(session, current_player,
                                   dispatch_plan->result,
                                   dispatch_plan->remark_dirty);
        if (dispatch_plan->remark_dirty) {
            session->presentation.actors[current_player].dirty_flags |=
                GAME_PLAYER_DIRTY_TURN;
            reset_turn_dispatch_state(session, now);
            session->presentation.dice.popup_active = 0;
            session->flow.steps_remaining = 0;
        } else {
            session->flow.turn_state = 2;
            session->flow.turn_state_frame = 0;
            session->flow.turn_state_tick = now;
        }
        return RICH4_FIRST_TURN_PRESENTATION_NONE;
    }
    reset_turn_dispatch_state(session, now);
    return RICH4_FIRST_TURN_PRESENTATION_NONE;
}

int rich4_app_update_first_landing(
    GameSession *session,
    int current_player,
    int landed_player,
    int view_x,
    int view_y,
    int landed_x,
    int landed_y,
    int landed_direction,
    int landing_frames,
    int landing_delay_ms,
    uint32_t now,
    Rich4FirstTurnPresentationRequest *out_request)
{
    GamePlayerState *landed;
    int dispatch_result;
    FirstTurnDispatchPlan dispatch_plan;

    if (out_request != NULL)
        *out_request = RICH4_FIRST_TURN_PRESENTATION_NONE;
    if (session == NULL || current_player < 0 ||
        current_player >= session->state.player_count ||
        session->flow.turn_state != GAME_TURN_STATE_LANDING)
        return 0;

    if (landed_player < 0 || landed_player >= session->state.player_count)
        landed_player = current_player;
    landed = &session->state.players[landed_player];
    session->presentation.view_world_x = view_x;
    session->presentation.view_world_y = view_y;
    session->presentation.has_view_override = 1;
    if (session->flow.turn_state_frame < landing_frames &&
        rich4_animation_clock_step(
            &session->presentation.animation_clock[RICH4_ANIMATION_LANDING],
            now, landing_delay_ms)) {
        session->presentation.actors[current_player].walk_frame++;
        session->flow.turn_state_frame++;
        session->flow.turn_state_tick = now;
    }
    if (session->flow.turn_state_frame < landing_frames)
        return 1;

    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_LANDING]);
    session->presentation.actors[landed_player].first_turn_landing_pending = 0;
    rich4_trace_emit(RICH4_TRACE_FIRST_LANDING, landed_player,
                     landed->node, landed->is_ai);
    session->presentation.actors[landed_player].intro_visible = 1;
    landed->play_state = landed->is_ai ? 2 : 1;
    session->presentation.actors[landed_player].visual_x = landed_x;
    session->presentation.actors[landed_player].visual_y = landed_y;
    session->presentation.actors[landed_player].direction = landed_direction;
    session->presentation.actors[landed_player].walk_frame = 0;
    session->flow.landing_pending_player = 0;
    session->presentation.has_view_override = 0;
    {
        GameEventQueue events;
        GameEvent event;
        rich4_game_event_queue_reset(&events);
        rich4_player_status_evaluate_events(
            &session->state, landed_player, 0,
            &session->flow.player_status_decision, &events);
        while (rich4_game_event_queue_pop(&events, &event)) {
            if (event.type == GAME_EVENT_ACTOR_REFRESH_REQUESTED)
                session->presentation.actors[landed_player].dirty_flags |=
                    GAME_PLAYER_DIRTY_TURN;
        }
    }
    dispatch_result = session->flow.player_status_decision.result;
    dispatch_plan = rich4_app_prepare_first_turn_dispatch(
        session, current_player, dispatch_result);
    rich4_trace_emit(RICH4_TRACE_FIRST_ACTION_DISPATCH, landed_player,
                     dispatch_result, landed->is_ai);
    session->debug.last_first_dispatch_result = dispatch_result;
    session->debug.last_first_dispatch_branch = dispatch_plan.branch;
    session->debug.first_dispatch_return_only = dispatch_plan.return_only;
    if (out_request != NULL) {
        *out_request = rich4_app_apply_first_turn_dispatch(
            session, &dispatch_plan, current_player, landed_player, now);
        if (*out_request == RICH4_FIRST_TURN_PRESENTATION_HUMAN_DICE) {
            session->presentation.dice.status_visible = 1;
            session->presentation.dice.status_tick = now;
            session->presentation.dice.action_hint_active =
                dispatch_plan.action_hint != 0;
            session->presentation.dice.input_refresh_active =
                dispatch_plan.input_refresh != 0;
        }
    }
    return 1;
}
