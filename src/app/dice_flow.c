#include "app/dice_flow.h"

#include "app/game_command_flow.h"
#include "app/turn_start_flow.h"
#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_dispatch.h"
#include "core/game_event.h"
#include "core/game_event_payload.h"
#include "core/rng.h"
#include "core/route_event.h"
#include "core/route_movement.h"
#include "core/route_event_executor.h"
#include "debug/trace.h"
#include "view/view_constants.h"

enum {
    RICH4_DICE_DEFAULT_FRAME_MS = 20,
    RICH4_DICE_RESULT_HOLD_MS = 500
};

static int rich4_dice_status_mask_at(const unsigned char *mask_data,
                                     size_t mask_size, int rel_x, int rel_y)
{
    if (mask_data == NULL ||
        mask_size < (size_t)(GAME_DICE_STATUS_W * GAME_DICE_STATUS_H) ||
        rel_x < 0 || rel_x >= GAME_DICE_STATUS_W ||
        rel_y < 0 || rel_y >= GAME_DICE_STATUS_H)
        return -1;
    return mask_data[rel_y * GAME_DICE_STATUS_W + rel_x];
}

static int rich4_dice_apply_command(GameSession *session,
                                    const GameCommand *command, uint32_t now)
{
    GameEventQueue events;
    GameEvent event;
    int accepted;

    rich4_game_event_queue_reset(&events);
    accepted = rich4_app_apply_game_command(session, command, &events);
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_DICE_ROLL_REQUESTED)
            rich4_dice_begin_roll(session, now);
        else if (event.type == GAME_EVENT_TURN_FLOW_REQUESTED)
            (void)rich4_turn_start_begin(session, now);
        else if (event.type == GAME_EVENT_DICE_COUNT_CHANGED)
            session->presentation.dice.status_visible = 1;
    }
    return accepted;
}

static int rich4_dice_wrap_index(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    return value < 0 ? value + count : value;
}

static int rich4_dice_player_index(const GameSession *session)
{
    if (session == NULL || session->state.player_count <= 0) return -1;
    return rich4_dice_wrap_index(session->state.current_player,
                                 session->state.player_count);
}

static int rich4_dice_clamp(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

void rich4_dice_begin_roll(GameSession *session, uint32_t now)
{
    int i;
    int current;
    GamePlayerState *player;

    if (session == NULL || !session->state.active ||
        session->state.player_count <= 0) return;
    /* 0x40EAD7 is blocking: its money/card result precedes the next roll
     * and the time-machine snapshot. PREROLL retries after the modal. */
    if (session->presentation.god_acquisition.active) return;
    current = rich4_dice_player_index(session);
    player = &session->state.players[current];
    session->time_machine_state = session->state;
    session->time_machine_flow = session->flow;
    session->time_machine_presentation = session->presentation;
    session->time_machine_valid = rich4_presentation_rebind_speech(
        &session->time_machine_presentation);
    session->presentation.dice.count = player->remote_dice_value != 0
        ? 1 : player->ndices;
    if (session->presentation.dice.count < 1)
        session->presentation.dice.count = 1;
    if (session->presentation.dice.count > 3)
        session->presentation.dice.count = 3;
    session->flow.dice_value = 0;
    session->presentation.dice.cache_reset_pending = 1;
    for (i = 0; i < session->presentation.dice.count; i++) {
        session->presentation.dice.faces[i] = player->remote_dice_value != 0
            ? player->remote_dice_value : 1 + rich4_rng_range_at(&session->state.ai_rng_state, 6);
        session->flow.dice_value += session->presentation.dice.faces[i];
    }
    /* The original one-shot remote value is consumed before movement. */
    player->remote_dice_value = 0;
    rich4_trace_emit(RICH4_TRACE_DICE_ROLL, current,
                     session->flow.dice_value,
                     session->presentation.dice.count);
    if (player->is_ai)
        rich4_trace_emit(RICH4_TRACE_AI_AUTO_MOVE, current,
                         session->flow.dice_value,
                         session->presentation.dice.count);
    session->flow.steps_remaining = 0;
    session->presentation.dice.popup_active = 1;
    rich4_animation_clock_reset(&session->presentation.animation_clock[RICH4_ANIMATION_DICE]);
    session->presentation.dice.animation_frame = 0;
    session->presentation.dice.animation_tick = now;
    session->presentation.dice.result_visible = 0;
    session->presentation.dice.result_tick = 0;
    session->presentation.dice.sound_pending = 0;
    session->presentation.movement_tick = 0;
}

static void rich4_dice_begin_route_move(GameSession *session,
                                        const MapData *map)
{
    GamePlayerState *player;
    RouteMoveResult result;
    int current;

    current = rich4_dice_player_index(session);
    if (current < 0) return;
    player = &session->state.players[current];
    if (player->previous_node == 0) {
        result = rich4_route_move_begin_with_next_and_rng(
            &session->flow.route_move, map, player->node,
            player->previous_node, player->next_node,
            session->flow.steps_remaining, &session->state.ai_rng_state);
    } else {
        result = rich4_route_move_begin_with_rng(
            &session->flow.route_move, map, player->node,
            player->previous_node, session->flow.steps_remaining, &session->state.ai_rng_state);
    }
    if (result == ROUTE_MOVE_INVALID) session->flow.steps_remaining = 0;
}

static void rich4_dice_request_roll(GameSession *session, int current,
                                    uint32_t now)
{
    GameCommand command;
    GameEventQueue events;
    GameEvent event;

    command = rich4_game_command_roll_dice(
        session->state.players[current].is_ai
            ? GAME_COMMAND_SOURCE_AI : GAME_COMMAND_SOURCE_HUMAN,
        current);
    rich4_game_event_queue_reset(&events);
    if (!rich4_game_dispatch_command(&session->state, &session->flow,
                                     &command, &events)) return;
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_DICE_ROLL_REQUESTED)
            rich4_dice_begin_roll(session, now);
    }
}

int rich4_dice_update(GameSession *session, const MapData *map,
                      int action_frames, int action_delay_ms,
                      int graph_frames, int graph_delay_ms,
                      uint32_t now)
{
    int current;

    if (session == NULL || session->flow.turn_state != 2 ||
        session->state.player_count <= 0) return 0;
    if (session->presentation.god_acquisition.active) return 1;
    current = rich4_dice_player_index(session);
    if (current < 0) return 0;
    if (action_delay_ms <= 0)
        action_delay_ms = RICH4_DICE_DEFAULT_FRAME_MS;
    if (graph_delay_ms <= 0) graph_delay_ms = 10;
    if (graph_frames <= 0) graph_frames = 1;

    if (!session->presentation.dice.popup_active && action_frames > 0 &&
        session->flow.turn_state_frame < action_frames &&
        now - session->flow.turn_state_tick >= (uint32_t)action_delay_ms) {
        session->flow.turn_state_frame++;
        session->flow.turn_state_tick += (uint32_t)action_delay_ms;
    }
    if (!session->presentation.dice.popup_active &&
        (action_frames <= 0 ||
         session->flow.turn_state_frame >= action_frames)) {
        rich4_dice_request_roll(session, current, now);
    }
    if (!session->presentation.dice.popup_active) return 1;

    if (!session->presentation.dice.result_visible) {
        if (session->presentation.dice.animation_frame < graph_frames &&
            rich4_animation_clock_step(
                &session->presentation.animation_clock[RICH4_ANIMATION_DICE],
                now, graph_delay_ms)) {
            int old_frame = session->presentation.dice.animation_frame;
            session->presentation.dice.animation_frame++;
            session->presentation.dice.animation_tick = now;
            /* 0x45138F compares the one-based decoded frame count. */
            if (old_frame < 29 &&
                session->presentation.dice.animation_frame >= 29)
                session->presentation.dice.sound_pending++;
        }
        if (session->presentation.dice.animation_frame >= graph_frames) {
            session->presentation.dice.sound_pending++;
            session->presentation.dice.result_visible = 1;
            session->presentation.dice.result_tick = now;
        }
    } else if (now - session->presentation.dice.result_tick >=
               RICH4_DICE_RESULT_HOLD_MS) {
        session->presentation.dice.popup_active = 0;
        session->presentation.dice.result_visible = 0;
        session->presentation.dice.cache_reset_pending = 1;
        session->flow.turn_state = 1;
        session->flow.turn_state_frame = 0;
        session->flow.turn_state_tick = now;
        session->flow.steps_remaining = session->flow.dice_value;
        rich4_dice_begin_route_move(session, map);
        rich4_route_event_collection_reset(&session->flow.route_facts);
        rich4_route_event_executor_reset_dispatch(
            &session->flow.route_events);
        session->presentation.movement_tick = 0;
        session->presentation.actors[current].move_active = 0;
    }
    return 1;
}

void rich4_dice_show_action_panel(GameSession *session, uint32_t now,
                                  int action_hint, int input_refresh)
{
    if (session == NULL) return;
    session->presentation.dice.status_visible = 1;
    session->presentation.dice.status_tick = now;
    session->presentation.dice.action_hint_active = action_hint ? 1 : 0;
    session->presentation.dice.input_refresh_active = input_refresh ? 1 : 0;
}

void rich4_dice_clear_action_panel_flags(GameSession *session)
{
    if (session == NULL) return;
    session->presentation.dice.status_visible = 0;
    session->presentation.dice.status_dragging = 0;
    session->presentation.dice.action_hint_active = 0;
    session->presentation.dice.input_refresh_active = 0;
}

void rich4_dice_release_status_drag(GameSession *session)
{
    if (session != NULL) session->presentation.dice.status_dragging = 0;
}

int rich4_dice_consume_cache_reset(GameSession *session)
{
    if (session == NULL || !session->presentation.dice.cache_reset_pending)
        return 0;
    session->presentation.dice.cache_reset_pending = 0;
    return 1;
}

static int rich4_dice_count_at_status(const GamePlayerState *player, int rel_y)
{
    int i;
    if (player == NULL) return 0;
    if (player->traffic_method == 1) {
        for (i = 0; i < 2; i++)
            if (rel_y >= 16 + 19 * i && rel_y <= 32 + 19 * i) return i + 1;
    } else if (player->traffic_method == 2) {
        for (i = 0; i < 3; i++)
            if (rel_y >= 9 + 16 * i && rel_y <= 25 + 16 * i) return i + 1;
    }
    return 0;
}

int rich4_dice_status_mouse_down(
    GameSession *session, const unsigned char *mask_data, size_t mask_size,
    int mouse_x, int mouse_y, uint32_t now)
{
    const GamePlayerState *player;
    GameCommand command;
    int rel_x;
    int rel_y;
    int mask;
    if (session == NULL || !session->state.active ||
        !session->presentation.dice.status_visible ||
        session->flow.turn_flow_active ||
        session->presentation.dice.popup_active ||
        session->state.player_count <= 0)
        return 0;
    rel_x = mouse_x - session->presentation.dice.status_x;
    rel_y = mouse_y - session->presentation.dice.status_y;
    if (rel_x < 0 || rel_x >= GAME_DICE_STATUS_W ||
        rel_y < 0 || rel_y >= GAME_DICE_STATUS_H)
        return 0;
    mask = rich4_dice_status_mask_at(mask_data, mask_size, rel_x, rel_y);
    if (mask < 0) return 4;
    player = &session->state.players[rich4_dice_player_index(session)];
    if (mask + 10 == 0x0B) {
        int dice_count = rich4_dice_count_at_status(player, rel_y);
        if (dice_count == 0) return 1;
        command = rich4_game_command_select_dice_count(
            GAME_COMMAND_SOURCE_HUMAN, session->state.current_player,
            dice_count);
        (void)rich4_dice_apply_command(session, &command, now);
        return 1;
    }
    if (mask + 10 == 0x0C) {
        session->presentation.dice.status_dragging = 1;
        session->presentation.dice.status_drag_offset_x = rel_x;
        session->presentation.dice.status_drag_offset_y = rel_y;
        return 3;
    }
    if (mask + 10 == 0x0D) {
        command = rich4_game_command_roll_dice(
            GAME_COMMAND_SOURCE_HUMAN, session->state.current_player);
        if (rich4_dice_apply_command(session, &command, now)) {
            rich4_dice_clear_action_panel_flags(session);
            return 2;
        }
        return 1;
    }
    return 4;
}

void rich4_dice_update_status_drag(GameSession *session,
                                   int mouse_x, int mouse_y)
{
    if (session == NULL || !session->presentation.dice.status_dragging) return;
    session->presentation.dice.status_x = rich4_dice_clamp(
        mouse_x - session->presentation.dice.status_drag_offset_x,
        0, SCREEN_W - GAME_DICE_STATUS_W);
    session->presentation.dice.status_y = rich4_dice_clamp(
        mouse_y - session->presentation.dice.status_drag_offset_y,
        0, SCREEN_H - GAME_DICE_STATUS_H);
}

int rich4_dice_next_count(const GameSession *session)
{
    const GamePlayerState *player;
    int max_dice;
    if (session == NULL || !session->state.active ||
        session->state.player_count <= 0 || session->flow.turn_flow_active ||
        session->presentation.dice.popup_active ||
        session->flow.steps_remaining > 0)
        return 0;
    player = &session->state.players[rich4_dice_player_index(session)];
    max_dice = player->traffic_method == 1 ? 2 :
               player->traffic_method == 2 ? 3 : 0;
    return max_dice == 0 ? 0 :
           player->ndices >= max_dice ? 1 : player->ndices + 1;
}

int rich4_dice_shortcut_key_down(GameSession *session, Rich4AppKey key,
                                 int game_active, uint32_t now)
{
    GameCommand command;
    int dice_count;
    if (!game_active || key != RICH4_APP_KEY_D) return 0;
    dice_count = rich4_dice_next_count(session);
    command = rich4_game_command_select_dice_count(
        GAME_COMMAND_SOURCE_HUMAN,
        session != NULL ? session->state.current_player : 0, dice_count);
    if (dice_count > 0 && rich4_dice_apply_command(session, &command, now))
        return 2;
    return 1;
}

void rich4_dice_update_status_blink(GameSession *session, uint32_t now)
{
    if (session == NULL || !session->presentation.dice.status_visible ||
        session->flow.turn_flow_active) return;
    if (session->presentation.dice.status_tick == 0)
        session->presentation.dice.status_tick = now;
    else if (now - session->presentation.dice.status_tick >= 500) {
        session->presentation.dice.status_variant ^= 1;
        session->presentation.dice.status_tick = now;
    }
}
