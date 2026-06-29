#include "core/tool.h"
#include "core/tool_internal.h"

#include <limits.h>
#include <string.h>

#include "core/insurance.h"
#include "core/rng.h"
#include "core/game_event_payload.h"

#define TOOL_SLOT(id) (0x475dd5u + (uintptr_t)(id) * 4u)
#define NO_PLAYER 0xffu

const GameToolInfo rich4_tool_catalog[GAME_TOOL_COUNT] = {
    {1,"机器娃娃",10,15,0,0,GAME_TOOL_TARGET_NONE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(1),"jiqiwawa","route geometry available","autonomous traversal"},
    {2,"路障",10,30,0,1,GAME_TOOL_TARGET_ROUTE_NODE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(2),"luzhang","registered empty route node","place 0x10 roadblock"},
    {3,"地雷",10,25,0,1,GAME_TOOL_TARGET_ROUTE_NODE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(3),"dilei","registered empty route node","place 0x11 mine"},
    {4,"定时炸弹",10,25,0,1,GAME_TOOL_TARGET_ROUTE_NODE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(4),"dingshizhadan","registered empty route node","place 0x12 timed bomb"},
    {5,"机车",10,80,0,0,GAME_TOOL_TARGET_NONE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(5),"jiche","not already motorcycle","persistent traffic method 1"},
    {6,"汽车",10,150,1,0,GAME_TOOL_TARGET_NONE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(6),"qiche","not already car","persistent traffic method 2"},
    {7,"飞弹",10,100,1,2,GAME_TOOL_TARGET_AREA,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(7),"feidan","typed map target with coordinates","radius-100 area effect"},
    {8,"遥控骰子",10,30,1,0,GAME_TOOL_TARGET_NONE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(8),"yaokongtouzi","dice value 1..6","one-shot forced roll"},
    {9,"机器工人",0,30,2,1,GAME_TOOL_TARGET_PROPERTY,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(9),"jiqigongren","upgradeable property","build or upgrade once"},
    {10,"时光机",0,40,2,2,GAME_TOOL_TARGET_TIME,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(10),"shiguangji","session snapshot available","restore then consume"},
    {11,"传送机",0,95,2,1,GAME_TOOL_TARGET_TRANSFER,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(11),"chuansongji","compatible source and destination","atomic transfer"},
    {12,"工程车",0,150,2,2,GAME_TOOL_TARGET_NONE,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(12),"gongchengche","traffic low bits not 3","persistent traffic method 0x1f"},
    {13,"核子飞弹",0,250,2,2,GAME_TOOL_TARGET_AREA,GAME_TOOL_IMPLEMENTED,TOOL_SLOT(13),"hezifeidan","typed map target with coordinates","viewport area destruction"}
};

const GameToolInfo *rich4_tool_info(int tool)
{
    return tool >= 0 && tool < GAME_TOOL_COUNT ? &rich4_tool_catalog[tool] : NULL;
}

int rich4_tool_inventory_count(const GameState *state, int player)
{
    int sum = 0, i;
    if (state == NULL || player < 0 || player >= GAME_MAX_PLAYERS) return 0;
    for (i = 0; i < GAME_TOOL_COUNT; i++) sum += state->players[player].tool_inventory[i];
    return sum;
}

int rich4_tool_set_route_targets(GameState *state, int route_node_count,
                                 const uint8_t *placeable, size_t count)
{
    if (state == NULL || route_node_count <= 1 ||
        route_node_count > GAME_MAX_MAP_ROUTE_NODES || placeable == NULL ||
        count < (size_t)route_node_count) return 0;
    state->tool_route_node_count = route_node_count;
    memset(state->tool_route_node_placeable, 0,
           sizeof(state->tool_route_node_placeable));
    memcpy(state->tool_route_node_placeable, placeable,
           (size_t)route_node_count);
    state->tool_route_node_placeable[0] = 0;
    return 1;
}

int rich4_tool_set_map_geometry(
    GameState *state, int route_node_count,
    const int16_t *route_x, const int16_t *route_y,
    const uint16_t (*route_next)[4], int land_count,
    const int16_t *land_x, const int16_t *land_y, int facility_count,
    const int16_t *facility_x, const int16_t *facility_y)
{
    if (state == NULL || route_node_count <= 1 ||
        route_node_count > GAME_MAX_MAP_ROUTE_NODES || route_x == NULL ||
        route_y == NULL || route_next == NULL || land_count < 0 ||
        land_count > GAME_MAX_MAP_PROPERTIES || facility_count < 0 ||
        facility_count > GAME_MAX_MAP_PROPERTIES ||
        (land_count != 0 && (land_x == NULL || land_y == NULL)) ||
        (facility_count != 0 &&
         (facility_x == NULL || facility_y == NULL))) return 0;
    state->tool_route_node_count = route_node_count;
    memcpy(state->tool_route_x, route_x,
           (size_t)route_node_count * sizeof(*route_x));
    memcpy(state->tool_route_y, route_y,
           (size_t)route_node_count * sizeof(*route_y));
    memcpy(state->tool_route_next, route_next,
           (size_t)route_node_count * sizeof(*route_next));
    if (land_count != 0) {
        memcpy(state->tool_land_x, land_x,
               (size_t)land_count * sizeof(*land_x));
        memcpy(state->tool_land_y, land_y,
               (size_t)land_count * sizeof(*land_y));
    }
    if (facility_count != 0) {
        memcpy(state->tool_facility_x, facility_x,
               (size_t)facility_count * sizeof(*facility_x));
        memcpy(state->tool_facility_y, facility_y,
               (size_t)facility_count * sizeof(*facility_y));
    }
    return 1;
}

int rich4_tool_active_player(const GameState *state, int player)
{
    return state != NULL && player >= 0 && player < state->player_count &&
           (state->players[player].play_state & 0xff) != 0;
}

int rich4_tool_valid_node(const GameState *state, int node)
{
    return state != NULL && node > 0 &&
           node < state->tool_route_node_count;
}

int rich4_tool_target_xy(const GameState *state, GameDomainRef ref,
                         int *out_x, int *out_y)
{
    int node = -1;
    if (state == NULL || out_x == NULL || out_y == NULL) return 0;
    switch (ref.kind) {
    case GAME_DOMAIN_ROUTE_NODE:
        node = ref.id;
        break;
    case GAME_DOMAIN_PLAYER:
        if (!rich4_tool_active_player(state, ref.id)) return 0;
        node = state->players[ref.id].node;
        break;
    case GAME_DOMAIN_SPECIAL_PLAYER:
        if (ref.id < 0 || ref.id >= GAME_SPECIAL_PLAYER_COUNT ||
            state->special_player_f10[ref.id] != 0) return 0;
        node = state->special_player_node[ref.id];
        break;
    case GAME_DOMAIN_LAND:
        if (ref.id < 0 || ref.id >= state->land_count) return 0;
        *out_x = state->tool_land_x[ref.id];
        *out_y = state->tool_land_y[ref.id];
        return 1;
    case GAME_DOMAIN_FACILITY:
        if (ref.id < 0 || ref.id >= state->facility_count) return 0;
        *out_x = state->tool_facility_x[ref.id];
        *out_y = state->tool_facility_y[ref.id];
        return 1;
    case GAME_DOMAIN_MAP_OBJECT:
        if (ref.id < 0 || ref.id >= GAME_TOOL_PLACED_OBJECT_COUNT ||
            state->tool_placed_objects[ref.id].kind == 0) return 0;
        node = state->tool_placed_objects[ref.id].node;
        break;
    default:
        return 0;
    }
    if (!rich4_tool_valid_node(state, node)) return 0;
    *out_x = state->tool_route_x[node];
    *out_y = state->tool_route_y[node];
    return 1;
}

int rich4_tool_in_square(int x, int y, int center_x, int center_y, int half)
{
    long dx = (long)x - center_x;
    long dy = (long)y - center_y;
    return dx >= -half && dx < half && dy >= -half && dy < half;
}

int rich4_tool_object_on_node(const GameState *state, int node)
{
    int i;
    for (i = 0; i < GAME_TOOL_PLACED_OBJECT_COUNT; i++)
        if (state->tool_placed_objects[i].kind != 0 &&
            state->tool_placed_objects[i].node == node) return i;
    return -1;
}

int rich4_tool_placement_slot(const GameState *state, int tool)
{
    int first = (tool - 1) * GAME_TOOL_PLACED_OBJECTS_PER_KIND;
    int i;
    for (i = first; i < first + GAME_TOOL_PLACED_OBJECTS_PER_KIND; i++)
        if (state->tool_placed_objects[i].kind == 0) return i;
    return -1;
}

int rich4_tool_transfer_targets_valid(const GameState *state,
                                      GameDomainRef source,
                                      GameDomainRef destination)
{
    if (state == NULL || rich4_game_domain_ref_is_none(source) ||
        rich4_game_domain_ref_is_none(destination)) return 0;
    if (source.kind == GAME_DOMAIN_LAND)
        return source.id >= 0 && source.id < state->land_count &&
               destination.kind == GAME_DOMAIN_LAND &&
               destination.id >= 0 && destination.id < state->land_count &&
               source.id != destination.id &&
               state->land_owner[destination.id] < 0 &&
               state->land_level[destination.id] == 0;
    if (source.kind == GAME_DOMAIN_FACILITY)
        return source.id >= 0 && source.id < state->facility_count &&
               destination.kind == GAME_DOMAIN_FACILITY &&
               destination.id >= 0 &&
               destination.id < state->facility_count &&
               source.id != destination.id &&
               state->facility_owner[destination.id] < 0 &&
               state->facility_level[destination.id] == 0;
    if (source.kind == GAME_DOMAIN_PLAYER)
        return rich4_tool_active_player(state, source.id) &&
               destination.kind == GAME_DOMAIN_ROUTE_NODE &&
               rich4_tool_valid_node(state, destination.id);
    if (source.kind == GAME_DOMAIN_SPECIAL_PLAYER)
        return source.id >= 0 && source.id < GAME_SPECIAL_PLAYER_COUNT &&
               state->special_player_f10[source.id] == 0 &&
               destination.kind == GAME_DOMAIN_ROUTE_NODE &&
               rich4_tool_valid_node(state, destination.id);
    if (source.kind == GAME_DOMAIN_MAP_OBJECT)
        return source.id >= 0 &&
               source.id < GAME_TOOL_PLACED_OBJECT_COUNT &&
               state->tool_placed_objects[source.id].kind != 0 &&
               state->tool_placed_objects[source.id].node > 0 &&
               destination.kind == GAME_DOMAIN_ROUTE_NODE &&
               rich4_tool_valid_node(state, destination.id) &&
               rich4_tool_object_on_node(state, destination.id) < 0;
    return 0;
}

GameToolUseResult rich4_tool_inspect(const GameState *state,
                                     const GameCommand *command)
{
    const GameToolInfo *info;
    int tool;
    if (state == NULL || command == NULL ||
        !rich4_game_command_is_valid(command) ||
        command->type != GAME_COMMAND_USE_TOOL ||
        command->player < 0 || command->player >= state->player_count ||
        (state->players[command->player].play_state & 0xff) == 0) return GAME_TOOL_USE_INVALID;
    tool = command->subject.id;
    info = rich4_tool_info(tool);
    if (info == NULL || state->players[command->player].tool_inventory[tool] == 0)
        return GAME_TOOL_USE_UNAVAILABLE;
    /* 0x420E9A's personality gate is AI command-selection policy. */
    if (info->implementation == GAME_TOOL_DEFERRED) return GAME_TOOL_USE_DEFERRED;
    if (tool == 7) {
        if (command->value < 1 || command->value > 6 ||
            !rich4_game_domain_ref_is_none(command->target))
            return GAME_TOOL_USE_INVALID;
    } else if (info->target_kind == GAME_TOOL_TARGET_AREA) {
        int x, y;
        if (command->value != 0 ||
            !rich4_game_domain_ref_is_none(command->target2) ||
            !rich4_tool_target_xy(state, command->target, &x, &y))
            return GAME_TOOL_USE_INVALID;
    } else if (info->target_kind == GAME_TOOL_TARGET_TRANSFER) {
        if (command->value != 0 ||
            !rich4_tool_transfer_targets_valid(state, command->target,
                                    command->target2))
            return GAME_TOOL_USE_INVALID;
    } else if (info->target_kind == GAME_TOOL_TARGET_TIME) {
        if (command->value != 0 ||
            !rich4_game_domain_ref_is_none(command->target) ||
            !rich4_game_domain_ref_is_none(command->target2))
            return GAME_TOOL_USE_INVALID;
    } else if (info->target_kind == GAME_TOOL_TARGET_ROUTE_NODE) {
        int node;
        if (command->target.kind != GAME_DOMAIN_ROUTE_NODE) return GAME_TOOL_USE_INVALID;
        node = command->target.id;
        if (node <= 0 || node >= state->tool_route_node_count ||
            !state->tool_route_node_placeable[node] ||
            rich4_tool_object_on_node(state, node) >= 0 ||
            rich4_tool_placement_slot(state, tool) < 0)
            return GAME_TOOL_USE_INVALID;
    } else if (info->target_kind == GAME_TOOL_TARGET_PROPERTY) {
        int id = command->target.id;
        if (command->target.kind == GAME_DOMAIN_LAND) {
            if (id < 0 || id >= state->land_count || command->value != 0 ||
                state->land_level[id] < 0 || state->land_level[id] >= 5)
                return GAME_TOOL_USE_INVALID;
        } else if (command->target.kind == GAME_DOMAIN_FACILITY) {
            static const unsigned char max_level[5] = {1,5,5,1,5};
            int type;
            if (id < 0 || id >= state->facility_count ||
                state->facility_level[id] < 0 ||
                state->facility_level[id] > 5) return GAME_TOOL_USE_INVALID;
            type = state->facility_type[id];
            if (state->facility_level[id] == 0) {
                if (command->value < 0 || command->value > 4)
                    return GAME_TOOL_USE_INVALID;
            } else if (command->value != 0 || type < 0 || type > 4 ||
                       state->facility_level[id] >= max_level[type]) {
                return GAME_TOOL_USE_INVALID;
            }
        } else return GAME_TOOL_USE_INVALID;
    } else if (!rich4_game_domain_ref_is_none(command->target) ||
               !rich4_game_domain_ref_is_none(command->target2)) {
        return GAME_TOOL_USE_INVALID;
    }
    if (tool == 4 && state->players[command->player].traffic_method == 1)
        return GAME_TOOL_USE_UNAVAILABLE;
    if (tool == 5 && state->players[command->player].traffic_method == 2)
        return GAME_TOOL_USE_UNAVAILABLE;
    if (tool == 11 && (state->players[command->player].traffic_method & 3) == 3)
        return GAME_TOOL_USE_UNAVAILABLE;
    if (tool == 0 && state->tool_robot.active)
        return GAME_TOOL_USE_UNAVAILABLE;
    return GAME_TOOL_USE_APPLIED;
}

/* 0x40C4C9..0x40C557: directional links, reverse only at a dead end.
 * Even a single forward candidate consumes the original RNG draw. */
int rich4_tool_robot_choose_next(GameState *state, int node, int previous)
{
    int candidates[4], count = 0, i, chosen;
    Rich4Rng rng;
    if (!rich4_tool_valid_node(state, node)) return 0;
    for (i = 0; i < 4; i++) {
        int next = state->tool_route_next[node][i];
        if (rich4_tool_valid_node(state, next) && next != previous &&
            !(state->tool_route_flags[node] & (UINT32_C(0x40000000) >> i)))
            candidates[count++] = next;
    }
    if (!count)
        return rich4_tool_valid_node(state, previous) ? previous : 0;
    rng.state = state->ai_rng_state;
    chosen = candidates[rich4_rng_state_range(&rng, count)];
    state->ai_rng_state = rng.state;
    return chosen;
}

GameToolTriggerResult rich4_tool_robot_advance(GameState *state,
                                                GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int destination, old_node, cleared_slot, candidate = 0;
    if (state == NULL || events == NULL || !state->tool_robot.active)
        return GAME_TOOL_TRIGGER_NONE;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    destination = next.tool_robot.next_node;
    old_node = next.tool_robot.node;
    if (!rich4_tool_valid_node(&next, destination))
        return GAME_TOOL_TRIGGER_INVALID;
    next.tool_robot.previous_node = old_node;
    next.tool_robot.node = destination;
    cleared_slot = rich4_tool_object_on_node(&next, destination);
    if (cleared_slot >= 0) {
        rich4_tool_release_area_object(&next, cleared_slot);
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_map_object_changed_from_fields,
                next.tool_robot.owner,
                rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, cleared_slot),
                rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, destination),
                cleared_slot, 0, 0))
            return GAME_TOOL_TRIGGER_INVALID;
    }
    if (next.tool_robot.steps_remaining > 0)
        next.tool_robot.steps_remaining--;
    if (next.tool_robot.steps_remaining > 0) {
        candidate = rich4_tool_robot_choose_next(
            &next, destination, next.tool_robot.previous_node);
        if (!candidate) return GAME_TOOL_TRIGGER_INVALID;
    }
    if (next.tool_robot.steps_remaining == 0) {
        next.tool_robot.active = 0;
        next.tool_robot.next_node = 0;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_tool_presentation_requested_from_fields,
                next.tool_robot.owner,
                rich4_game_domain_ref(GAME_DOMAIN_TOOL, 0),
                rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, destination),
                0, 38, 2))
            return GAME_TOOL_TRIGGER_INVALID;
    } else {
        next.tool_robot.next_node = candidate;
    }
    if (!rich4_tool_stage_events(events, &staged))
        return GAME_TOOL_TRIGGER_INVALID;
    *state = next;
    return next.tool_robot.active ? GAME_TOOL_TRIGGER_PASS :
        GAME_TOOL_TRIGGER_STOP;
}

int rich4_tool_snapshot_export(const GameState *state, GameToolSnapshot *out)
{
    int i;
    if (state == NULL || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    out->version = GAME_TOOL_SNAPSHOT_VERSION;
    memcpy(out->amount, state->tool_amount, sizeof(out->amount));
    out->route_node_count = state->tool_route_node_count;
    memcpy(out->route_node_placeable, state->tool_route_node_placeable,
           sizeof(out->route_node_placeable));
    memcpy(out->route_x, state->tool_route_x, sizeof(out->route_x));
    memcpy(out->route_y, state->tool_route_y, sizeof(out->route_y));
    memcpy(out->route_next, state->tool_route_next,
           sizeof(out->route_next));
    memcpy(out->land_x, state->tool_land_x, sizeof(out->land_x));
    memcpy(out->land_y, state->tool_land_y, sizeof(out->land_y));
    memcpy(out->facility_x, state->tool_facility_x,
           sizeof(out->facility_x));
    memcpy(out->facility_y, state->tool_facility_y,
           sizeof(out->facility_y));
    memcpy(out->placed, state->tool_placed_objects, sizeof(out->placed));
    out->robot = state->tool_robot;
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        memcpy(out->players[i].inventory, state->players[i].tool_inventory,
               sizeof(out->players[i].inventory));
        out->players[i].traffic_method = state->players[i].traffic_method;
        out->players[i].ndices = state->players[i].ndices;
        out->players[i].remote_dice_value = state->players[i].remote_dice_value;
        out->players[i].engineering_saved_traffic_method = state->players[i].engineering_saved_traffic_method;
        out->players[i].engineering_saved_ndices = state->players[i].engineering_saved_ndices;
        out->players[i].attached_timed_bomb_slot = state->players[i].attached_timed_bomb_slot;
    }
    return 1;
}

static int snapshot_valid(const GameToolSnapshot *in)
{
    int occupied[GAME_MAX_MAP_ROUTE_NODES] = {0};
    int i;
    if (in == NULL || in->version != GAME_TOOL_SNAPSHOT_VERSION ||
        in->route_node_count < 0 || in->route_node_count > GAME_MAX_MAP_ROUTE_NODES)
        return 0;
    if (in->robot.active &&
        (in->robot.owner < 0 || in->robot.owner >= GAME_MAX_PLAYERS ||
         in->robot.steps_remaining <= 0 || in->robot.steps_remaining > 9 ||
         in->robot.node <= 0 || in->robot.node >= in->route_node_count ||
         in->robot.next_node <= 0 ||
         in->robot.next_node >= in->route_node_count))
        return 0;
    for (i = 0; i < GAME_TOOL_PLACED_OBJECT_COUNT; i++) {
        const struct GameToolPlacedObjectState *o = &in->placed[i];
        int expected = i < 10 ? 0x10 : (i < 20 ? 0x11 : 0x12);
        if (o->kind == 0) continue;
        if (o->kind != expected || o->owner < 0 || o->owner >= GAME_MAX_PLAYERS) return 0;
        if (o->node > 0) {
            if (o->node >= in->route_node_count || !in->route_node_placeable[o->node] ||
                occupied[o->node]++) return 0;
        } else if (o->kind != 0x12 || o->attached_player == NO_PLAYER ||
                   o->attached_player == 0 || o->attached_player > GAME_MAX_PLAYERS)
            return 0;
        else if (in->players[o->attached_player - 1].attached_timed_bomb_slot != i)
            return 0;
    }
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        int slot = in->players[i].attached_timed_bomb_slot;
        if (in->players[i].remote_dice_value < 0 ||
            in->players[i].remote_dice_value > 6) return 0;
        if (slot < -1 || slot >= GAME_TOOL_PLACED_OBJECT_COUNT) return 0;
        if (slot >= 0 && (in->placed[slot].kind != 0x12 || in->placed[slot].node != 0 ||
            in->placed[slot].attached_player != i + 1)) return 0;
    }
    return 1;
}

int rich4_tool_snapshot_import(GameState *state, const GameToolSnapshot *in)
{
    int i;
    if (state == NULL || !snapshot_valid(in)) return 0;
    memcpy(state->tool_amount, in->amount, sizeof(state->tool_amount));
    state->tool_route_node_count = in->route_node_count;
    memcpy(state->tool_route_node_placeable, in->route_node_placeable,
           sizeof(state->tool_route_node_placeable));
    memcpy(state->tool_route_x, in->route_x, sizeof(state->tool_route_x));
    memcpy(state->tool_route_y, in->route_y, sizeof(state->tool_route_y));
    memcpy(state->tool_route_next, in->route_next,
           sizeof(state->tool_route_next));
    memcpy(state->tool_land_x, in->land_x, sizeof(state->tool_land_x));
    memcpy(state->tool_land_y, in->land_y, sizeof(state->tool_land_y));
    memcpy(state->tool_facility_x, in->facility_x,
           sizeof(state->tool_facility_x));
    memcpy(state->tool_facility_y, in->facility_y,
           sizeof(state->tool_facility_y));
    memcpy(state->tool_placed_objects, in->placed, sizeof(state->tool_placed_objects));
    state->tool_robot = in->robot;
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        memcpy(state->players[i].tool_inventory, in->players[i].inventory,
               sizeof(state->players[i].tool_inventory));
        state->players[i].traffic_method = in->players[i].traffic_method;
        state->players[i].ndices = in->players[i].ndices;
        state->players[i].remote_dice_value = in->players[i].remote_dice_value;
        state->players[i].engineering_saved_traffic_method = in->players[i].engineering_saved_traffic_method;
        state->players[i].engineering_saved_ndices = in->players[i].engineering_saved_ndices;
        state->players[i].attached_timed_bomb_slot = in->players[i].attached_timed_bomb_slot;
    }
    return 1;
}
