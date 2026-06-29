/* App-owned dynamic-object presentation tick.  It mutates only the bounded
 * app presentation objects, borrows the immutable viewport, and has no SDL
 * or resource ownership. */

#include "app/dynamic_object_flow.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/confinement.h"
#include "core/game_constants.h"
#include "core/game_event_payload.h"
#include "core/god.h"
#include "core/rng.h"

void rich4_app_init_dynamic_object_pool(GamePresentationState *presentation)
{
    int slot;

    if (presentation == NULL) return;
    memset(presentation->dynamic_objects, 0,
           sizeof(presentation->dynamic_objects));
    for (slot = 0; slot < GAME_MAP_DYNAMIC_OBJECT_COUNT; slot++) {
        if (slot < 14) presentation->dynamic_objects[slot].type =
            (uint8_t)(slot + 1);
        else if (slot < 16) presentation->dynamic_objects[slot].type = 15;
        else if (slot < 26) presentation->dynamic_objects[slot].type = 16;
        else if (slot < 36) presentation->dynamic_objects[slot].type = 17;
        else presentation->dynamic_objects[slot].type = 18;
    }
}

static int dynamic_route_node_has_exit(const MapRouteNode *route)
{
    return route != NULL &&
        (route->next[0] != 0 || route->next[2] != 0);
}

/* sub_40AA6C scans one-based route nodes and selects rand() % count.  The
 * original marks a selected node through the route +0x24 high-word field;
 * this port keeps MapData immutable and observes the same occupancy through
 * the active presentation objects. */
static int choose_dynamic_node(const GamePresentationState *presentation,
                               const MapData *map, int source_node,
                               int ignore_object_id, uint32_t *rng_state)
{
    int candidates[GAME_MAX_MAP_ROUTE_NODES];
    int candidate_count = 0;
    int route_count;
    int node;
    int object_id;

    if (map == NULL || map->routes == NULL || map->route_count <= 1)
        return 0;
    route_count = map->route_count > GAME_MAX_MAP_ROUTE_NODES
        ? GAME_MAX_MAP_ROUTE_NODES : (int)map->route_count;
    for (node = 1; node < route_count; node++) {
        if ((map->routes[node].flags & 0x80ffff00u) != 0 ||
            !dynamic_route_node_has_exit(&map->routes[node]))
            continue;
        if (presentation != NULL) {
            int occupied = 0;
            for (object_id = 0;
                 object_id < GAME_MAP_DYNAMIC_OBJECT_COUNT; object_id++) {
                if (object_id == ignore_object_id) continue;
                if (presentation->dynamic_objects[object_id].node == node) {
                    occupied = 1;
                    break;
                }
            }
            if (occupied) continue;
        }
        if (source_node > 0 && (size_t)source_node < map->route_count &&
            map->routes[source_node].x - map->routes[node].x > -300 &&
            map->routes[source_node].x - map->routes[node].x < 300 &&
            map->routes[source_node].y - map->routes[node].y > -300 &&
            map->routes[source_node].y - map->routes[node].y < 300)
            continue;
        candidates[candidate_count++] = node;
    }
    if (candidate_count <= 0) return 0;
    {
        int selected_index = rich4_rng_range_at(rng_state, candidate_count);
        int selected = candidates[selected_index];
        return selected;
    }
}

static int dynamic_type_for_slot(int object_id)
{
    if (object_id < 0 || object_id >= GAME_MAP_DYNAMIC_OBJECT_COUNT)
        return 0;
    if (object_id < 14) return object_id + 1;
    if (object_id < 16) return 15;
    if (object_id < 26) return 16;
    if (object_id < 36) return 17;
    return 18;
}

static void clear_dynamic_object_instance(
    GamePresentationState *presentation, int object_id)
{
    GameMapDynamicObjectState *object;

    if (presentation == NULL || object_id < 0 ||
        object_id >= GAME_MAP_DYNAMIC_OBJECT_COUNT)
        return;
    object = &presentation->dynamic_objects[object_id];
    memset(object, 0, sizeof(*object));
    object->type = (uint8_t)dynamic_type_for_slot(object_id);
}

static int dynamic_slot_begin_for_type(int type)
{
    if (type >= 1 && type <= 14) return type - 1;
    if (type == 15) return 14;
    if (type == 16) return 16;
    if (type == 17) return 26;
    if (type == 18) return 36;
    return -1;
}

static int dynamic_slot_end_for_type(int type)
{
    if (type >= 1 && type <= 14) return type;
    if (type == 15) return 16;
    if (type == 16) return 26;
    if (type == 17) return 36;
    if (type == 18) return 46;
    return -1;
}

static int dynamic_free_slot_for_type(
    const GamePresentationState *presentation, int type)
{
    int slot;
    int begin = dynamic_slot_begin_for_type(type);
    int end = dynamic_slot_end_for_type(type);

    if (presentation == NULL || begin < 0 || end < 0) return -1;
    for (slot = begin; slot < end; slot++)
        if (presentation->dynamic_objects[slot].node == 0) return slot;
    return -1;
}

static void initialize_dynamic_object_at_slot(
    GamePresentationState *presentation, const MapData *map,
    int object_id, int type, int node)
{
    GameMapDynamicObjectState *object;
    int next_node = 0;
    int edge;

    if (presentation == NULL || map == NULL || map->routes == NULL ||
        object_id < 0 || object_id >= GAME_MAP_DYNAMIC_OBJECT_COUNT ||
        dynamic_type_for_slot(object_id) != type || node <= 0 ||
        (size_t)node >= map->route_count)
        return;
    object = &presentation->dynamic_objects[object_id];
    memset(object, 0, sizeof(*object));
    object->type = (uint8_t)type;
    object->node = (uint16_t)node;
    for (edge = 0; edge < 4; edge++) {
        if (map->routes[node].next[edge] != 0) {
            next_node = map->routes[node].next[edge];
            break;
        }
    }
    if (next_node > 0 && (size_t)next_node < map->route_count)
        object->node_direction = (uint8_t)
            rich4_game_actor_direction_from_delta(
                map->routes[node].x - map->routes[next_node].x,
                map->routes[node].y - map->routes[next_node].y);
    object->world_x = (float)map->routes[node].x;
    object->world_y = (float)map->routes[node].y;
}

static void initialize_dynamic_object_at_node(
    GamePresentationState *presentation, const MapData *map,
    int type, int node)
{
    int object_id;

    if (presentation == NULL || map == NULL || map->routes == NULL ||
        dynamic_slot_begin_for_type(type) < 0 || node <= 0 ||
        (size_t)node >= map->route_count)
        return;
    object_id = dynamic_free_slot_for_type(presentation, type);
    if (object_id >= 0)
        initialize_dynamic_object_at_slot(
            presentation, map, object_id, type, node);
}

void rich4_app_initialize_dynamic_map_objects_with_rng(
    GamePresentationState *presentation, const MapData *map, uint32_t *rng_state)
{
    static const uint8_t initial_types[] = {1, 3, 5, 7, 9, 11, 13, 14};
    size_t i;

    if (presentation == NULL || map == NULL || map->routes == NULL)
        return;
    for (i = 0; i < sizeof(initial_types) / sizeof(initial_types[0]); i++) {
        int node = choose_dynamic_node(presentation, map, 0, -1, rng_state);
        if (node <= 0 || node >= GAME_MAX_MAP_ROUTE_NODES) continue;
        initialize_dynamic_object_at_node(
            presentation, map, initial_types[i], node);
    }
}

int rich4_app_remove_dynamic_object_with_rng(
    GamePresentationState *presentation, const MapData *map, int object_id, uint32_t *rng_state)
{
    GameMapDynamicObjectState *object;
    int old_type;
    int old_node;
    int paired_type;
    int new_node;

    if (presentation == NULL || object_id < 0 ||
        object_id >= GAME_MAP_DYNAMIC_OBJECT_COUNT)
        return 0;
    object = &presentation->dynamic_objects[object_id];
    old_type = object->type;
    old_node = object->node;
    if (old_type == 0 || old_node == 0) return 0;

    /* 0x40E14D clears instance fields but the fixed pool type byte remains. */
    clear_dynamic_object_instance(presentation, object_id);

    /* The original generic remover only replaces the first twelve singleton
     * slots.  Type 13/14 are reset by the month-rollover path instead. */
    if (object_id >= 12) return 1;
    paired_type = (object_id & 1) != 0 ? object_id : object_id + 2;
    new_node = choose_dynamic_node(
        presentation, map, old_node, object_id, rng_state);
    if (new_node > 0)
        initialize_dynamic_object_at_node(
            presentation, map, paired_type, new_node);
    return 1;
}

int rich4_app_robot_clear_dynamic_object_with_rng(
    GamePresentationState *presentation, const MapData *map,
    int node, int previous_node, uint32_t *rng_state)
{
    int slot;
    if (presentation == NULL || map == NULL || map->routes == NULL ||
        node <= 0 || previous_node <= 0 ||
        (size_t)node >= map->route_count ||
        (size_t)previous_node >= map->route_count) return -1;
    for (slot = 0; slot < GAME_MAP_DYNAMIC_OBJECT_COUNT; slot++) {
        GameMapDynamicObjectState *object = &presentation->dynamic_objects[slot];
        int direction;
        if (object->node != node || object->attached_player) continue;
        direction = object->node_direction;
        if (!rich4_app_remove_dynamic_object_with_rng(presentation, map, slot, rng_state))
            return -1;
        /* 0x46352C = 0.5; +6=255, +7 retains the object's direction.
         * The first displayed position is already one velocity past node. */
        object->velocity_x = (map->routes[node].x -
                              map->routes[previous_node].x) * 0.5f;
        object->velocity_y = (map->routes[node].y -
                              map->routes[previous_node].y) * 0.5f;
        object->world_x = map->routes[node].x + object->velocity_x;
        object->world_y = map->routes[node].y + object->velocity_y;
        object->flight_direction = (uint8_t)direction;
        object->flight_active = 255;
        return slot;
    }
    return -1;
}

int rich4_app_reset_monthly_dynamic_objects_with_rng(
    GamePresentationState *presentation, const MapData *map, uint32_t *rng_state)
{
    int object_id;

    if (presentation == NULL || map == NULL || map->routes == NULL)
        return 0;
    for (object_id = 12; object_id <= 13; object_id++) {
        int old_node = presentation->dynamic_objects[object_id].node;
        int new_node;

        clear_dynamic_object_instance(presentation, object_id);
        new_node = choose_dynamic_node(
            presentation, map, old_node, object_id, rng_state);
        if (new_node > 0)
            initialize_dynamic_object_at_node(
                presentation, map, object_id + 1, new_node);
    }
    return 1;
}

int rich4_app_sync_dynamic_god_attachment(
    GamePresentationState *presentation, const GameState *state,
    int player, int object_id)
{
    GameMapDynamicObjectState *object;
    int owner;
    int i;

    if (presentation == NULL || state == NULL || player < 0 ||
        player >= state->player_count || object_id < 0 ||
        object_id >= GAME_MAP_DYNAMIC_OBJECT_COUNT)
        return 0;
    object = &presentation->dynamic_objects[object_id];
    if (!rich4_god_is_road_type(object->type) ||
        state->players[player].god_info != object_id + 1 ||
        state->players[player].god_type != object->type)
        return 0;
    owner = player + 1;
    if (object->attached_player != 0 &&
        object->attached_player != owner)
        return 0;

    /* 0x40EAD7's first operation is 0x40E32C on the player's old god.  The
     * state has already selected the new slot, so presentation ownership is
     * the authoritative way to find and clear that old visual object. */
    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        GameMapDynamicObjectState *other =
            &presentation->dynamic_objects[i];
        if (i != object_id &&
            rich4_god_is_road_type(other->type) &&
            other->attached_player == owner)
            clear_dynamic_object_instance(presentation, i);
    }

    object->node = state->players[player].node > 0
        ? (uint16_t)state->players[player].node : 0;
    object->attached_player = (uint8_t)owner;
    object->countdown = object->type == 15 ? 13 : 7;
    object->flight_active = 0;
    object->velocity_x = 0.0f;
    object->velocity_y = 0.0f;
    return 1;
}

int rich4_app_begin_dynamic_god_detach(
    GamePresentationState *presentation, const GameState *state,
    int player, uint32_t now)
{
    GameGodDetachmentPresentationState *detachment;
    int owner;
    int i;

    if (presentation == NULL || state == NULL || player < 0 ||
        player >= state->player_count)
        return 0;
    owner = player + 1;
    detachment = &presentation->god_detachment;
    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        GameMapDynamicObjectState *object =
            &presentation->dynamic_objects[i];
        int node;

        if (!rich4_god_is_road_type(object->type) ||
            object->attached_player != owner)
            continue;

        memset(detachment, 0, sizeof(*detachment));
        /* 0x40E356 tests the four bytes +50..+53, then directly clears. */
        if (state->players[player].delayed_status_block ||
            state->players[player].days_disappearing ||
            state->players[player].days_in_prison || state->players[player].days_in_hospital) {
            clear_dynamic_object_instance(presentation, i);
            return 1;
        }
        detachment->active = 1;
        detachment->sound_pending = 1;
        detachment->player = player;
        detachment->object_slot = i;
        detachment->god_type = object->type;
        detachment->frame_count = 24;
        detachment->frame_delay_ms = 60;
        /* 0x4090A2 turns the attached god four directions from the actor,
         * after map rotation. 0x40E32C advances once before its first draw. */
        detachment->direction =
            (8 - (presentation->map_direction & 7) +
             presentation->actors[player].direction + 4) & 7;
        /* 0x40E384 and the attached queue in 0x408FCC use the owner's
         * current coordinates. object->node can still be the acquisition
         * node; it is not the anchor of an attached god. */
        node = state->players[player].node;
        if (presentation->actors[player].move_active) {
            detachment->world_x = presentation->actors[player].visual_x;
            detachment->world_y = presentation->actors[player].visual_y;
        } else if (node > 0 && node < state->tool_route_node_count) {
            detachment->world_x = state->tool_route_x[node];
            detachment->world_y = state->tool_route_y[node];
        } else {
            detachment->world_x = presentation->actors[player].visual_x;
            detachment->world_y = presentation->actors[player].visual_y;
        }
        detachment->frame_tick = now;
        detachment->started = now != 0;

        /* The old slot is no longer a collectible/attached god while the
         * direct release animation is on screen. */
        clear_dynamic_object_instance(presentation, i);
        return 1;
    }
    return 0;
}

int rich4_app_detach_dynamic_god(
    GamePresentationState *presentation, int player)
{
    int owner;
    int detached = 0;
    int i;

    if (presentation == NULL || player < 0 || player >= GAME_MAX_PLAYERS)
        return 0;
    owner = player + 1;
    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        GameMapDynamicObjectState *object =
            &presentation->dynamic_objects[i];
        if (rich4_god_is_road_type(object->type) &&
            object->attached_player == owner) {
            clear_dynamic_object_instance(presentation, i);
            detached = 1;
        }
    }
    return detached;
}

int rich4_app_collect_dynamic_god(
    GamePresentationState *presentation, GameState *state, int player,
    GameEventQueue *events)
{
    int object_id = -1;
    int i;

    if (presentation == NULL || state == NULL || events == NULL ||
        player < 0 || player >= state->player_count ||
        state->players[player].node <= 0 ||
        (state->players[player].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0)
        return 0;
    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        const GameMapDynamicObjectState *object =
            &presentation->dynamic_objects[i];
        if (object->node == state->players[player].node &&
            object->attached_player == 0 &&
            rich4_god_is_road_type(object->type)) {
            object_id = i;
            break;
        }
    }
    if (object_id < 0 || rich4_god_attach_replace(
            state, player, object_id + 1,
            presentation->dynamic_objects[object_id].type, events) !=
            GAME_GOD_APPLIED)
        return 0;
    /* 0x40EAD7 releases a previously attached god before the newly selected
     * object follows the player.  The app path has no clock, so a zero start
     * tick is adopted by the platform on its next frame. */
    (void)rich4_app_begin_dynamic_god_detach(
        presentation, state, player, 0);
    if (!rich4_app_sync_dynamic_god_attachment(
        presentation, state, player, object_id))
        return 0;
    return object_id + 1;
}

int rich4_app_resolve_dynamic_dog(
    GamePresentationState *presentation, GameState *state,
    const MapData *map, int player, GameEventQueue *events)
{
    const GameMapDynamicObjectState *object;
    GameDomainRef subject;
    GameDomainRef target;
    int object_id = -1;
    int resource;
    int effect;
    int i;

    int node;
    if (presentation == NULL || state == NULL || events == NULL ||
        player < 0 || player >= 8 ||
        (player < 4 && (player >= state->player_count ||
         (state->players[player].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0)) ||
        (player >= 4 && state->special_player_f10[player - 4] != 0))
        return 0;
    node = player < 4 ? state->players[player].node :
                       state->special_player_node[player - 4];
    if (node <= 0) return 0;
    for (i = 0; i < GAME_MAP_DYNAMIC_OBJECT_COUNT; i++) {
        object = &presentation->dynamic_objects[i];
        if (object->type == 11 && object->node == node &&
            object->attached_player == 0) {
            object_id = i;
            break;
        }
    }
    if (object_id < 0) return 0;

    /* The original uses Data[552]/Effect[85] for a human riding a vehicle;
     * all other cases use the on-foot Data[532]/Effect[93] encounter. */
    if (player < 4 && state->players[player].traffic_method != 0) {
        resource = 552;
        effect = 85;
    } else {
        resource = 532;
        effect = 93;
    }
    if (!rich4_app_remove_dynamic_object_with_rng(presentation, map, object_id, &state->ai_rng_state))
        return 0;

    subject = rich4_game_domain_ref(GAME_DOMAIN_GOD, object_id);
    target = rich4_game_domain_ref(
        GAME_DOMAIN_ROUTE_NODE, node);
    /* Dog encounters use the same graph/effect presentation sink as tool
     * animations.  The domain refs keep the event identifiable without
     * treating the dog as a collectible god. */
    if (!rich4_game_event_emit_typed_domain(
            events, rich4_game_event_tool_presentation_requested_from_fields,
            player, subject, target, resource, effect, 0u))
        return 0;
    /* 0x41B89F -> 0x40CD07 selects the hurt actor before Data[532].
     * 0x41B8EF enters hospital only after the FLC and reaction speech.
     * The animation queue owns that continuation; no hospital commit here. */
    if (resource == 532 && player < 4 &&
        state->players[player].delayed_status_block == 0 &&
        state->players[player].days_disappearing == 0 &&
        state->players[player].days_in_prison == 0 &&
        state->players[player].days_in_hospital == 0)
        state->players[player].play_state |= GAME_PLAYER_PLAY_STATE_FROZEN;
    return object_id + 1;
}

void rich4_app_sync_tool_dynamic_objects(
    GamePresentationState *presentation, const GameState *state)
{
    int placement_slot;
    if (presentation == NULL || state == NULL) return;
    for (placement_slot = 0;
         placement_slot < GAME_TOOL_PLACED_OBJECT_COUNT;
         placement_slot++) {
        const struct GameToolPlacedObjectState *placed =
            &state->tool_placed_objects[placement_slot];
        int dynamic_slot = placement_slot + 16;
        GameMapDynamicObjectState *object =
            &presentation->dynamic_objects[dynamic_slot];
        int active_flight =
            presentation->tool_placement_animation.active &&
            presentation->tool_placement_animation.slot == placement_slot;
        int node = placed->node;
        int next_node = 0;
        int edge;
        uint8_t expected_type = (uint8_t)(16 + placement_slot / 10);

        if (placed->kind == 0) {
            /* 0x40E14D clears node/owner, but not 0x40FAFD's flight. */
            if (object->node == 0 && object->flight_active) continue;
            memset(object, 0, sizeof(*object));
            object->type = expected_type;
            continue;
        }
        if (!active_flight) {
            memset(object, 0, sizeof(*object));
            object->type = placed->kind;
            object->node = node > 0 ? (uint16_t)node : 0;
            object->attached_player = placed->attached_player == 0xff
                ? 0 : placed->attached_player;
            object->countdown = placed->countdown;
            if (node > 0 && node < state->tool_route_node_count) {
                for (edge = 0; edge < 4; edge++) {
                    next_node = state->tool_route_next[node][edge];
                    if (next_node > 0 &&
                        next_node < state->tool_route_node_count)
                        break;
                    next_node = 0;
                }
                if (next_node != 0) {
                    object->node_direction = (uint8_t)
                        rich4_game_actor_direction_from_delta(
                            state->tool_route_x[node] -
                                state->tool_route_x[next_node],
                            state->tool_route_y[node] -
                                state->tool_route_y[next_node]);
                }
                object->world_x = (float)state->tool_route_x[node];
                object->world_y = (float)state->tool_route_y[node];
            }
        } else {
            object->type = placed->kind;
            object->node = node > 0 ? (uint16_t)node : 0;
            object->countdown = placed->countdown;
        }
    }
}

int rich4_app_find_nearest_dynamic_god(
    const GamePresentationState *presentation, const GameState *state,
    int player, int *out_type)
{
    int player_node;
    int player_x;
    int player_y;
    int best_object = -1;
    /* 0x444D1A starts at Euclidean distance 10000 and replaces only for a
     * strictly smaller result.  Compare squared distances against the squared
     * bound here to preserve the strict cutoff without sqrt rounding. */
    int64_t best_distance = 100000000LL;
    int object_id;

    if (out_type != NULL) *out_type = 0;
    if (presentation == NULL || state == NULL || player < 0 ||
        player >= state->player_count ||
        state->tool_route_node_count <= 1)
        return -1;

    player_node = state->players[player].node;
    if (player_node <= 0 ||
        player_node >= state->tool_route_node_count)
        return -1;
    player_x = state->tool_route_x[player_node];
    player_y = state->tool_route_y[player_node];

    /* The original scans the eligible dynamic entries from the global object
     * table, rejects attached objects, and replaces only for a strictly
     * smaller distance.  Slot order preserves the source order for equal
     * distance in this typed projection. */
    for (object_id = 0;
         object_id < GAME_MAP_DYNAMIC_OBJECT_COUNT; object_id++) {
        const GameMapDynamicObjectState *object =
            &presentation->dynamic_objects[object_id];
        int64_t dx;
        int64_t dy;
        int64_t distance;

        if (!rich4_god_is_road_type(object->type) ||
            object->attached_player != 0 ||
            object->node == 0 ||
            object->node >= state->tool_route_node_count)
            continue;
        dx = (long)state->tool_route_x[object->node] - player_x;
        dy = (long)state->tool_route_y[object->node] - player_y;
        distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            best_object = object_id;
            if (out_type != NULL) *out_type = object->type;
        }
    }
    return best_object;
}

static int dynamic_god_projection_before(
    const GameAiGlobalObjectProjection *left,
    const GameAiGlobalObjectProjection *right)
{
    if (left->screen_y != right->screen_y)
        return left->screen_y < right->screen_y;
    return left->screen_x < right->screen_x;
}

int rich4_app_find_nearest_dynamic_god_with_projection(
    const GamePresentationState *presentation, const GameState *state,
    int player, const GameAiGlobalProjection *projection, int *out_type)
{
    GameAiGlobalObjectProjection *ordered = NULL;
    int player_node;
    int ordered_count = 0;
    int best_object = -1;
    int best_type = 0;
    int i;
    int player_x;
    int player_y;
    int64_t best_distance = 100000000LL;

    if (out_type != NULL) *out_type = 0;
    if (presentation == NULL || state == NULL || projection == NULL ||
        player < 0 || player >= state->player_count ||
        !projection->object_table_available || projection->object_count < 0 ||
        (projection->object_count > 0 && projection->objects == NULL) ||
        (size_t)projection->object_count >
            SIZE_MAX / sizeof(*ordered) ||
        state->tool_route_node_count <= 1)
        return -1;
    player_node = state->players[player].node;
    if (player_node <= 0 || player_node >= state->tool_route_node_count)
        return -1;
    player_x = state->tool_route_x[player_node];
    player_y = state->tool_route_y[player_node];
    if (projection->object_count > 0) {
        ordered = (GameAiGlobalObjectProjection *)malloc(
            (size_t)projection->object_count * sizeof(*ordered));
        if (ordered == NULL) return -1;
    }
    for (i = 0; i < projection->object_count; i++) {
        GameAiGlobalObjectProjection item = projection->objects[i];
        int pos;
        if (item.raw_id == 0 || item.screen_x < 0 || item.screen_x >= 440 ||
            item.screen_y < 0 || item.screen_y >= 440) continue;
        pos = ordered_count++;
        while (pos > 0 && dynamic_god_projection_before(
                   &item, &ordered[pos - 1])) {
            ordered[pos] = ordered[pos - 1];
            pos--;
        }
        ordered[pos] = item;
    }
    for (i = 0; i < ordered_count;) {
        uint16_t raw = ordered[i].raw_id;
        int x = ordered[i].screen_x;
        int y = ordered[i].screen_y;
        int slot;
        const GameMapDynamicObjectState *object;
        int64_t dx;
        int64_t dy;
        int64_t distance;
        i++;
        while (i < ordered_count && ordered[i].screen_x == x &&
               ordered[i].screen_y == y) {
            raw |= ordered[i].raw_id;
            i++;
        }
        if ((raw & 0x8000u) == 0) continue;
        slot = (raw & 0x7f00u) >> 8;
        if (slot <= 0 || slot > GAME_MAP_DYNAMIC_OBJECT_COUNT) continue;
        object = &presentation->dynamic_objects[slot - 1];
        if (!rich4_god_is_road_type(object->type) ||
            object->attached_player != 0 ||
            object->node >= state->tool_route_node_count)
            continue;
        dx = (int64_t)state->tool_route_x[object->node] - player_x;
        dy = (int64_t)state->tool_route_y[object->node] - player_y;
        distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            best_object = slot - 1;
            best_type = object->type;
        }
    }
    free(ordered);
    if (out_type != NULL) *out_type = best_type;
    return best_object;
}

void rich4_app_update_dynamic_objects(
    GamePresentationState *presentation, const GameViewportModel *viewport,
    int newly_launched_object)
{
    int slot;
    if (presentation == NULL || viewport == NULL) return;
    for (slot = 0; slot < GAME_MAP_DYNAMIC_OBJECT_COUNT; slot++) {
        GameMapDynamicObjectState *object =
            &presentation->dynamic_objects[slot];
        int tile_x;
        int tile_y;
        if (presentation->tool_placement_animation.active &&
            slot == presentation->tool_placement_animation.slot + 16)
            continue;
        if (object->attached_player != 0 || object->flight_active == 0)
            continue;
        tile_x = ((int)object->world_x >> 5) -
                 (viewport->center_world_x >> 5) + 14;
        tile_y = ((int)object->world_y >> 5) -
                 (viewport->center_world_y >> 5) + 14;
        if (tile_x < 0 || tile_x > 28 || tile_y < 0 || tile_y > 28) {
            object->flight_active = 0;
            continue;
        }
        object->flight_active--;
        if (slot == newly_launched_object) continue;
        object->world_x += object->velocity_x;
        object->world_y += object->velocity_y;
    }
}

int rich4_app_update_dynamic_god_detachment(
    GamePresentationState *presentation, uint32_t now)
{
    GameGodDetachmentPresentationState *detachment;
    uint32_t elapsed;
    uint32_t advance;

    if (presentation == NULL) return 0;
    detachment = &presentation->god_detachment;
    if (!detachment->active) return 0;
    if (detachment->frame_count <= 0 || detachment->frame_delay_ms <= 0) {
        detachment->active = 0;
        return 0;
    }
    if (!detachment->started) {
        detachment->started = 1;
        detachment->frame_tick = now;
        return 1;
    }
    elapsed = now - detachment->frame_tick;
    advance = elapsed / (uint32_t)detachment->frame_delay_ms;
    if (advance == 0) return 1;
    if (advance >= (uint32_t)(detachment->frame_count - detachment->frame)) {
        detachment->frame = detachment->frame_count;
        detachment->active = 0;
        detachment->frame_tick += advance *
            (uint32_t)detachment->frame_delay_ms;
        return 0;
    }
    detachment->frame += (int)advance;
    detachment->frame_tick += advance *
        (uint32_t)detachment->frame_delay_ms;
    return 1;
}

void rich4_app_clear_dynamic_object(
    GamePresentationState *presentation, int object_id)
{
    if (presentation == NULL || object_id < 0 ||
        object_id >= GAME_MAP_DYNAMIC_OBJECT_COUNT)
        return;
    memset(&presentation->dynamic_objects[object_id], 0,
           sizeof(presentation->dynamic_objects[object_id]));
}

/* Compatibility helpers for stand-alone presentation/bootstrap callers.
 * Every active-session path supplies its serialized word explicitly. */
void rich4_app_initialize_dynamic_map_objects(GamePresentationState *p, const MapData *m)
{
    Rich4Rng rng; rich4_rng_global_export(&rng);
    rich4_app_initialize_dynamic_map_objects_with_rng(p, m, &rng.state);
    rich4_rng_global_import(&rng);
}
int rich4_app_remove_dynamic_object(GamePresentationState *p, const MapData *m, int id)
{
    Rich4Rng rng; int result; rich4_rng_global_export(&rng);
    result = rich4_app_remove_dynamic_object_with_rng(p, m, id, &rng.state);
    rich4_rng_global_import(&rng); return result;
}
int rich4_app_reset_monthly_dynamic_objects(GamePresentationState *p, const MapData *m)
{
    Rich4Rng rng; int result; rich4_rng_global_export(&rng);
    result = rich4_app_reset_monthly_dynamic_objects_with_rng(p, m, &rng.state);
    rich4_rng_global_import(&rng); return result;
}
int rich4_app_robot_clear_dynamic_object(GamePresentationState *p, const MapData *m, int node, int previous)
{
    Rich4Rng rng; int result; rich4_rng_global_export(&rng);
    result = rich4_app_robot_clear_dynamic_object_with_rng(p, m, node, previous, &rng.state);
    rich4_rng_global_import(&rng); return result;
}
