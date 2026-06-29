#include "app/special_actor_flow.h"

#include <stddef.h>
#include <string.h>

#include "app/route_geometry_flow.h"
#include "app/dynamic_object_flow.h"
#include "core/game_constants.h"
#include "core/game_event.h"
#include "core/special_actor_action.h"
#include "view/game_view_model.h"

static void prepare_edge(GameSpecialActorPresentationState *flow,
                         const MapData *map)
{
    int from_node;
    int to_node;
    int dx;
    int dy;
    float duration;

    if (flow == NULL || map == NULL || !flow->active ||
        flow->path_index < 0 || flow->path_index + 1 >= flow->path_count)
        return;
    from_node = flow->path_nodes[flow->path_index];
    to_node = flow->path_nodes[flow->path_index + 1];
    if (from_node <= 0 || to_node <= 0 ||
        (size_t)from_node >= map->route_count ||
        (size_t)to_node >= map->route_count)
        return;
    flow->move_from_x = map->routes[from_node].x;
    flow->move_from_y = map->routes[from_node].y;
    flow->move_to_x = map->routes[to_node].x;
    flow->move_to_y = map->routes[to_node].y;
    flow->visual_x = flow->move_from_x;
    flow->visual_y = flow->move_from_y;
    dx = flow->move_to_x - flow->move_from_x;
    dy = flow->move_to_y - flow->move_from_y;
    duration = rich4_original_fixed_move_duration(dx, dy);
    flow->move_distance = (int)duration;
    if (flow->move_distance <= 0) flow->move_distance = 1;
    flow->move_progress = 0;
    flow->direction = rich4_game_actor_direction_from_delta(dx, dy);
    /* Data.MKF[380 + slot*4 + 3] is sleepwalking.  Otherwise a high-bit
     * route uses part 2 and an ordinary land edge uses walking part 1. */
    flow->resource_part = flow->resource_part == 3 ? 3 :
        ((map->routes[to_node].flags & 0x80000000u) != 0 ? 2 : 1);
}

void rich4_special_actor_begin(
    GameSession *session, const MapData *map,
    const TurnSpecialAutoActionResult *action)
{
    GameSpecialActorPresentationState *flow;
    int count;
    int i;

    if (session == NULL || map == NULL || action == NULL ||
        action->kind != TURN_SPECIAL_AUTO_MOVED ||
        action->special_slot < 0 || action->special_slot >= 4 ||
        action->path_count < 2)
        return;
    count = action->path_count;
    if (count > TURN_SPECIAL_AUTO_MAX_STEPS + 1)
        count = TURN_SPECIAL_AUTO_MAX_STEPS + 1;
    for (i = 0; i < count; i++)
        if (action->path_nodes[i] <= 0 ||
            (size_t)action->path_nodes[i] >= map->route_count)
            return;

    flow = &session->presentation.special_actor;
    memset(flow, 0, sizeof(*flow));
    flow->active = 1;
    flow->slot = action->special_slot;
    flow->path_count = count;
    memcpy(flow->path_nodes, action->path_nodes,
           sizeof(flow->path_nodes[0]) * (size_t)count);
    flow->path_index = 0;
    flow->steps_remaining = count - 1;
    flow->resource_part =
        (session->state.special_player_status[action->special_slot][1] &
         0xff) != 0 ? 3 : 1;
    flow->visual_x = map->routes[action->path_nodes[0]].x;
    flow->visual_y = map->routes[action->path_nodes[0]].y;
    memset(&flow->action_batch, 0, sizeof(flow->action_batch));
    flow->action_index = 0;
    flow->action_until = 0;
    prepare_edge(flow, map);
}

static uint32_t action_hold_ms(GameSpecialActorActionKind kind)
{
    if (kind == GAME_SPECIAL_ACTOR_ACTION_ROBBER_BANK) return 2000;
    if (kind == GAME_SPECIAL_ACTOR_ACTION_HOOLIGAN_FEE ||
        kind == GAME_SPECIAL_ACTOR_ACTION_SPY_TOLL ||
        kind == GAME_SPECIAL_ACTOR_ACTION_SPY_DIVIDEND)
        return 1500;
    return 1000;
}

static int begin_endpoint_dog(GameSession *session, const MapData *map)
{
    GameSpecialActorPresentationState *flow = &session->presentation.special_actor;
    if (rich4_app_resolve_dynamic_dog(&session->presentation, &session->state,
            map, flow->slot + 4, &session->deferred_events) <= 0)
        return 0;
    /* 0x41B8A7 -> 0x43EE12: retain this endpoint until the bite ends. */
    session->presentation.view_world_x = flow->visual_x;
    session->presentation.view_world_y = flow->visual_y;
    session->presentation.has_view_override = 1;
    return 1;
}

Rich4SpecialActorUpdateResult rich4_special_actor_update(
    GameSession *session, const MapData *map, int frame_ms, uint32_t now)
{
    GameSpecialActorPresentationState *flow;

    if (session == NULL || map == NULL ||
        !session->presentation.special_actor.active)
        return RICH4_SPECIAL_ACTOR_UPDATE_IDLE;
    flow = &session->presentation.special_actor;
    if (flow->action_batch.count > 0) {
        GameSpecialActorActionKind kind;
        if (flow->action_index < 0 ||
            flow->action_index >= flow->action_batch.count) {
            memset(&flow->action_batch, 0, sizeof(flow->action_batch));
            flow->action_index = 0;
            flow->action_until = 0;
        } else if (flow->action_until == 0) {
            kind = flow->action_batch.action[flow->action_index].kind;
            flow->action_until = now + action_hold_ms(kind);
        }
        if (flow->action_batch.count > 0) {
            if ((int32_t)(now - flow->action_until) < 0)
                return RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE;
            flow->action_index++;
            if (flow->action_index < flow->action_batch.count) {
                kind = flow->action_batch.action[flow->action_index].kind;
                flow->action_until = now + action_hold_ms(kind);
                return RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE;
            }
            memset(&flow->action_batch, 0, sizeof(flow->action_batch));
            flow->action_index = 0;
            flow->action_until = 0;
            flow->last_step_tick = now;
        }
    }
    if (flow->path_index + 1 >= flow->path_count) {
        if (begin_endpoint_dog(session, map))
            return RICH4_SPECIAL_ACTOR_UPDATE_DOG;
        session->presentation.view_world_x = flow->visual_x;
        session->presentation.view_world_y = flow->visual_y;
        session->presentation.has_view_override = 1;
        memset(flow, 0, sizeof(*flow));
        return RICH4_SPECIAL_ACTOR_UPDATE_FINISHED;
    }
    if (rich4_consume_animation_pulse(
            &flow->last_step_tick, now, frame_ms)) {
        flow->move_progress++;
        flow->animation_frame++;
        if (flow->move_progress >= flow->move_distance) {
            GameEventQueue events;
            GameEvent event;
            int reached_node;
            int endpoint;

            flow->visual_x = flow->move_to_x;
            flow->visual_y = flow->move_to_y;
            flow->path_index++;
            flow->steps_remaining = flow->path_count - flow->path_index - 1;
            reached_node = flow->path_nodes[flow->path_index];
            endpoint = flow->path_index + 1 >= flow->path_count;
            rich4_game_event_queue_reset(&events);
            memset(&flow->action_batch, 0, sizeof(flow->action_batch));
            if (!rich4_special_actor_apply_step(
                    &session->state, map, flow->slot, reached_node,
                    endpoint, &flow->action_batch, &events))
                memset(&flow->action_batch, 0, sizeof(flow->action_batch));
            while (rich4_game_event_queue_pop(&events, &event))
                if (event.player >= 0 &&
                    event.player < session->state.player_count)
                    session->presentation.actors[event.player].dirty_flags |=
                        GAME_PLAYER_DIRTY_TURN;
            flow->action_index = 0;
            flow->action_until = 0;
            if (flow->action_batch.count > 0)
                return RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE;
            if (endpoint && begin_endpoint_dog(session, map))
                return RICH4_SPECIAL_ACTOR_UPDATE_DOG;
            if (endpoint) {
                session->presentation.view_world_x = flow->visual_x;
                session->presentation.view_world_y = flow->visual_y;
                session->presentation.has_view_override = 1;
                memset(flow, 0, sizeof(*flow));
                return RICH4_SPECIAL_ACTOR_UPDATE_FINISHED;
            }
            prepare_edge(flow, map);
        } else {
            float duration = rich4_original_fixed_move_duration(
                flow->move_to_x - flow->move_from_x,
                flow->move_to_y - flow->move_from_y);
            flow->visual_x = rich4_original_interpolated_axis(
                flow->move_from_x, flow->move_to_x,
                flow->move_progress, duration);
            flow->visual_y = rich4_original_interpolated_axis(
                flow->move_from_y, flow->move_to_y,
                flow->move_progress, duration);
        }
    }
    return RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE;
}
