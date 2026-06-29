#include "app/tool_followup_flow.h"

#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/tool.h"
#include "app/route_geometry_flow.h"
#include "app/dynamic_object_flow.h"
#include "view/game_view_model.h"

#include <string.h>

static int rich4_tool_followup_wrap_index(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    return value < 0 ? value + count : value;
}

int rich4_tool_followup_begin_placement(
    GameSession *session, const GameCommand *command,
    const GameEvent *event, uint32_t now)
{
    GameToolPlacementAnimation *animation;
    GameMapDynamicObjectState *object;
    int dynamic_slot;
    int source_node;
    int target_node;
    int next_node = 0;
    int edge;
    if (session == NULL || command == NULL || event == NULL ||
        event->type != GAME_EVENT_MAP_OBJECT_CHANGED ||
        command->type != GAME_COMMAND_USE_TOOL ||
        command->subject.kind != GAME_DOMAIN_TOOL ||
        command->subject.id < 1 || command->subject.id > 3 ||
        command->target.kind != GAME_DOMAIN_ROUTE_NODE ||
        command->target.id <= 0 ||
        command->target.id >= session->state.tool_route_node_count ||
        rich4_game_event_tool_followup_slot(event) < 0 ||
        rich4_game_event_tool_followup_slot(event) >=
            GAME_TOOL_PLACED_OBJECT_COUNT ||
        command->player < 0 || command->player >= session->state.player_count)
        return 0;
    source_node = session->state.players[command->player].node;
    target_node = command->target.id;
    animation = &session->presentation.tool_placement_animation;
    memset(animation, 0, sizeof(*animation));
    animation->active = 1;
    animation->slot = rich4_game_event_tool_followup_slot(event);
    animation->tool = command->subject.id;
    if (source_node > 0 && source_node < session->state.tool_route_node_count) {
        animation->from_x = session->state.tool_route_x[source_node];
        animation->from_y = session->state.tool_route_y[source_node];
    }
    animation->to_x = session->state.tool_route_x[target_node];
    animation->to_y = session->state.tool_route_y[target_node];
    animation->started = now;
    /* Core placement slots 0..29 map to original dynamic slots 16..45. */
    dynamic_slot = animation->slot + 16;
    object = &session->presentation.dynamic_objects[dynamic_slot];
    memset(object, 0, sizeof(*object));
    object->type = (uint8_t)(0x0f + animation->tool);
    object->node = (uint16_t)target_node;
    for (edge = 0; edge < 4; edge++) {
        next_node = session->state.tool_route_next[target_node][edge];
        if (next_node > 0 &&
            next_node < session->state.tool_route_node_count)
            break;
        next_node = 0;
    }
    if (next_node != 0) {
        /* 0x40E033 calls 0x407A8C(neighbor, node). */
        object->node_direction = (uint8_t)rich4_game_actor_direction_from_delta(
            animation->to_x - session->state.tool_route_x[next_node],
            animation->to_y - session->state.tool_route_y[next_node]);
    }
    object->flight_direction = object->node_direction;
    object->flight_active = 1;
    object->world_x = (float)animation->from_x;
    object->world_y = (float)animation->from_y;
    return 1;
}

int rich4_tool_followup_mark_robot_trigger(
    GameSession *session, uint32_t now)
{
    if (session == NULL) return 0;
    session->presentation.tool_robot_tick = now;
    memset(&session->presentation.tool_robot_motion, 0,
           sizeof(session->presentation.tool_robot_motion));
    if (session->state.tool_robot.owner >= 0 &&
        session->state.tool_robot.owner < session->state.player_count)
        session->presentation.tool_robot_motion.direction =
            session->presentation.actors[session->state.tool_robot.owner].direction;
    return 1;
}

int rich4_tool_followup_mark_turn_pending(GameSession *session)
{
    if (session == NULL) return 0;
    session->presentation.tool_trigger_turn_pending = 1;
    return 1;
}

int rich4_tool_followup_update_placement(
    GameSession *session, const GameViewportModel *viewport, uint32_t now,
    Rich4AudioEventQueue *audio_events)
{
    GameToolPlacementAnimation *animation;
    GameMapDynamicObjectState *object;
    int dynamic_slot;
    int steps;
    uint32_t flight_until;
    if (session == NULL || !session->presentation.tool_placement_animation.active)
        return 0;
    animation = &session->presentation.tool_placement_animation;
    dynamic_slot = animation->slot + 16;
    if (dynamic_slot < 16 || dynamic_slot >= GAME_MAP_DYNAMIC_OBJECT_COUNT) {
        memset(animation, 0, sizeof(*animation));
        return 0;
    }
    object = &session->presentation.dynamic_objects[dynamic_slot];
    if (animation->until == 0) {
        int from_screen_x;
        int from_screen_y;
        int to_screen_x;
        int to_screen_y;
        int dx;
        int dy;
        int distance;
        if (viewport == NULL) return 1;
        rich4_game_viewport_project(
            viewport, animation->from_x, animation->from_y,
            &from_screen_x, &from_screen_y);
        rich4_game_viewport_project(
            viewport, animation->to_x, animation->to_y,
            &to_screen_x, &to_screen_y);
        dx = to_screen_x - from_screen_x;
        dy = to_screen_y - from_screen_y;
        distance = rich4_int_sqrt_floor((long)dx * dx + (long)dy * dy);
        /* 0x40E740..0x40E754: round(distance * 0.125 + 1). */
        steps = distance > 0 ? (distance + 4) / 8 + 1 : 0;
        flight_until = animation->started + (uint32_t)(steps * 24);
        /* a6=100 is a post-flight hold and is skipped when both projected
         * endpoints are identical. */
        animation->until = flight_until + (steps > 0 ? 100u : 0u);
    }
    flight_until = animation->until > animation->started
        ? animation->until - 100u : animation->started;
    steps = (int)((flight_until - animation->started) / 24u);
    if ((int32_t)(now - flight_until) < 0) {
        uint32_t elapsed = now - animation->started;
        int step = (int)(elapsed / 24) + 1;
        if (step > steps) step = steps;
        object->flight_active = 1;
        object->world_x = (float)(animation->from_x +
            (animation->to_x - animation->from_x) * step /
                steps);
        object->world_y = (float)(animation->from_y +
            (animation->to_y - animation->from_y) * step /
                steps);
        return 1;
    }
    object->world_x = (float)animation->to_x;
    object->world_y = (float)animation->to_y;
    if ((int32_t)(now - animation->until) < 0) {
        object->flight_active = 1;
        return 1;
    }
    object->flight_active = 0;
    if (audio_events != NULL)
        rich4_audio_event_emit(
            audio_events, RICH4_AUDIO_EVENT_TOOL_PLACEMENT, 1,
            animation->tool);
    memset(animation, 0, sizeof(*animation));
    return 0;
}

int rich4_tool_followup_update_robot(
    GameSession *session, const MapData *map,
    int frame_ms, uint32_t now, GameEventQueue *events,
    Rich4AudioEventQueue *audio_events, Rich4ToolRobotUpdate *update)
{
    int node, destination, from_x, from_y, to_x, to_y, frames;
    float duration;
    GamePresentationState *p;
    if (update != NULL) *update = (Rich4ToolRobotUpdate){0, 0, -1, 0, -1};
    if (session == NULL || events == NULL || frame_ms <= 0 ||
        !session->state.tool_robot.active) return 0;
    p = &session->presentation;
    node = session->state.tool_robot.node;
    destination = session->state.tool_robot.next_node;
    if (node <= 0 || node >= session->state.tool_route_node_count ||
        (!p->tool_robot_motion.end_wait && (destination <= 0 ||
         destination >= session->state.tool_route_node_count)))
        return 0;
    if (update != NULL) *update = (Rich4ToolRobotUpdate){1, 0, node, 0, -1};
    if (!p->tool_robot_motion.initialized) {
        if (audio_events != NULL)
            rich4_audio_event_emit(audio_events,
                RICH4_AUDIO_EVENT_TOOL_USE, 1, 0);
        p->tool_robot_motion.world_x = session->state.tool_route_x[node];
        p->tool_robot_motion.world_y = session->state.tool_route_y[node];
        p->tool_robot_motion.initialized = 1;
    }
    if (!rich4_consume_animation_pulse(&p->tool_robot_tick, now, frame_ms))
        return 1;
    if (update != NULL) update->pulse = 1;
    if (p->tool_robot_motion.end_wait) {
        int i, flying = 0;
        /* 0x40D825 checks the complete 46-slot flight pool before drawing.
         * Once the five-pulse hold clears, 0x418E7F sets 0x83 (-125). */
        if (p->tool_robot_motion.end_wait > 0) {
            for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++)
                flying |= p->dynamic_objects[i].flight_active != 0;
            if (--p->tool_robot_motion.end_wait == 0)
                p->tool_robot_motion.end_wait = flying ? 1 : -3;
        } else if (++p->tool_robot_motion.end_wait == 0) {
            int owner = session->state.tool_robot.owner;
            session->state.tool_robot.active = 0;
            p->has_view_override = 0;
            if (owner >= 0 && owner < session->state.player_count)
                p->actors[owner].dirty_flags |= GAME_PLAYER_DIRTY_TURN;
            if (update != NULL) update->stopped = 1;
        }
        return 1;
    }
    /* 0x40D942 processes arrival on the following pulse, before starting
     * the next edge. Preserve the endpoint for one rendered frame. */
    if (p->tool_robot_motion.arrived) {
        GameToolTriggerResult result =
            rich4_tool_robot_advance(&session->state, events);
        if (result == GAME_TOOL_TRIGGER_INVALID) return 0;
        if (update != NULL)
            update->launched_object = rich4_app_robot_clear_dynamic_object_with_rng(
                p, map, session->state.tool_robot.node,
                session->state.tool_robot.previous_node, &session->state.ai_rng_state);
        else
            (void)rich4_app_robot_clear_dynamic_object_with_rng(
                p, map, session->state.tool_robot.node,
                session->state.tool_robot.previous_node, &session->state.ai_rng_state);
        p->tool_robot_motion.arrived = 0;
        p->tool_robot_motion.progress = 0;
        node = session->state.tool_robot.node;
        if (update != NULL) update->node = node;
        if (result == GAME_TOOL_TRIGGER_STOP) {
            /* Core traversal has finished; keep actor 8 modal/visible for
             * the original stationary hold and return countdown. */
            session->state.tool_robot.active = 1;
            p->tool_robot_motion.end_wait = 5;
            p->tool_robot_motion.animation_frame = 0;
            if (audio_events != NULL)
                rich4_audio_event_emit(audio_events,
                    RICH4_AUDIO_EVENT_TOOL_ROBOT_STOP, 1, 0);
            return 1;
        }
    }
    destination = session->state.tool_robot.next_node;
    from_x = session->state.tool_route_x[node];
    from_y = session->state.tool_route_y[node];
    to_x = session->state.tool_route_x[destination];
    to_y = session->state.tool_route_y[destination];
    p->tool_robot_motion.direction =
        rich4_game_actor_direction_from_delta(to_x - from_x, to_y - from_y);
    duration = rich4_original_fixed_move_duration(to_x - from_x, to_y - from_y);
    frames = (int)duration;
    if (frames < 1) frames = 1;
    p->tool_robot_motion.animation_frame++;
    p->tool_robot_motion.progress++;
    if (p->tool_robot_motion.progress >= frames) {
        p->tool_robot_motion.world_x = to_x;
        p->tool_robot_motion.world_y = to_y;
        p->tool_robot_motion.arrived = 1;
    } else {
        p->tool_robot_motion.world_x = rich4_original_interpolated_axis(
            from_x, to_x, p->tool_robot_motion.progress, duration);
        p->tool_robot_motion.world_y = rich4_original_interpolated_axis(
            from_y, to_y, p->tool_robot_motion.progress, duration);
    }
    return 1;
}

int rich4_tool_followup_finish_trigger_turn(
    GameSession *session, uint32_t now)
{
    if (session == NULL || !session->presentation.tool_trigger_turn_pending ||
        session->presentation.speech.active ||
        session->presentation.tool_animation_count != 0)
        return 0;
    session->presentation.tool_trigger_turn_pending = 0;
    session->flow.steps_remaining = 0;
    session->flow.turn_flow_active = 0;
    session->flow.turn_state = GAME_TURN_STATE_IDLE;
    session->flow.turn_state_frame = 0;
    session->flow.turn_state_tick = now;
    session->presentation.dice.popup_active = 0;
    session->presentation.dice.status_visible = 0;
    session->presentation.dice.status_dragging = 0;
    session->presentation.dice.action_hint_active = 0;
    session->presentation.dice.input_refresh_active = 0;
    session->presentation.movement_tick = 0;
    return 1;
}

int rich4_tool_followup_finish_trigger_turn_and_advance(
    GameSession *session, const MapData *map, uint32_t now,
    Rich4AppTurnRotationResult *out_result)
{
    int next;
    int route_events_pending;
    if (session == NULL) return 0;
    /* 0x41B42D returns to movement's stop/countdown state after its modal
     * effects. It does not rotate or dispatch the landing directly. */
    if (session->flow.turn_state == GAME_TURN_STATE_MOVE) {
        if (!session->presentation.tool_trigger_turn_pending ||
            session->presentation.speech.active ||
            session->presentation.tool_animation_count != 0)
            return 0;
        session->presentation.tool_trigger_turn_pending = 0;
        return 1;
    }
    route_events_pending =
        session->flow.route_events.state.dispatch.total_count > 0 &&
        session->flow.route_events.state.dispatch.phase !=
            GAME_ROUTE_EVENT_DISPATCH_DONE;
    if (!rich4_tool_followup_finish_trigger_turn(session, now)) return 0;
    if (route_events_pending) {
        session->flow.turn_flow_active = 1;
        session->flow.turn_state = GAME_TURN_STATE_ROUTE_EVENTS;
        session->flow.turn_state_frame = 0;
        session->flow.turn_state_tick = now;
        return 1;
    }
    if (!rich4_app_advance_turn_rotation(session, map, now, out_result))
        return 0;
    next = rich4_tool_followup_next_player(session);
    if (next >= 0 && next < session->state.player_count)
        session->presentation.actors[next].dirty_flags |=
            GAME_PLAYER_DIRTY_TURN;
    return 1;
}

int rich4_tool_followup_next_player(const GameSession *session)
{
    if (session == NULL) return -1;
    return rich4_tool_followup_wrap_index(
        session->state.current_player, session->state.player_count);
}
