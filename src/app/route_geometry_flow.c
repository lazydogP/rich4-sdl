#include "app/route_geometry_flow.h"

#include <stdint.h>

#include "core/game_constants.h"
#include "core/route_movement.h"
#include "view/game_view_model.h"

static int wrap_route_node(int value)
{
    value %= GAME_ROUTE_NODE_COUNT;
    return value < 0 ? value + GAME_ROUTE_NODE_COUNT : value;
}

static void default_route_node_xy(int node, int *out_x, int *out_y)
{
    static const int points[GAME_ROUTE_NODE_COUNT][2] = {
        { 96, 380 }, { 150, 380 }, { 204, 380 }, { 258, 380 },
        { 312, 380 }, { 366, 380 }, { 420, 380 }, { 474, 380 },
        { 528, 380 }, { 528, 330 }, { 528, 280 }, { 528, 230 },
        { 528, 180 }, { 474, 180 }, { 420, 180 }, { 366, 180 },
        { 312, 180 }, { 258, 180 }, { 204, 180 }, { 150, 180 },
        { 96, 180 }, { 96, 230 }, { 96, 280 }, { 96, 330 }
    };
    node = wrap_route_node(node);
    if (out_x != NULL) *out_x = points[node][0];
    if (out_y != NULL) *out_y = points[node][1];
}

int rich4_route_count_for_map(const MapData *map)
{
    return map != NULL && map->routes != NULL && map->route_count > 0
        ? (int)map->route_count : GAME_ROUTE_NODE_COUNT;
}

int rich4_route_first_node_for_map(const MapData *map)
{
    return map != NULL && map->route_count > 1 ? 1 : 0;
}

int rich4_route_node_is_valid(const MapData *map, int node)
{
    if (map == NULL) return node >= 0 && node < GAME_ROUTE_NODE_COUNT;
    return map->routes != NULL && node >= 1 &&
        (size_t)node < map->route_count &&
        (map->routes[node].next[0] != 0 ||
         map->routes[node].next[1] != 0 ||
         map->routes[node].next[2] != 0 ||
         map->routes[node].next[3] != 0);
}

void rich4_route_node_xy_for_map(const MapData *map, int node,
                                 int *out_x, int *out_y)
{
    if (map == NULL || map->route_count == 0 || map->routes == NULL) {
        default_route_node_xy(node, out_x, out_y);
        return;
    }
    if (node <= 0 || (size_t)node >= map->route_count)
        node = rich4_route_first_node_for_map(map);
    if (out_x != NULL) *out_x = map->routes[node].x;
    if (out_y != NULL) *out_y = map->routes[node].y;
}

int rich4_route_direction8_for_nodes(const MapData *map,
                                     int from_node, int to_node)
{
    int x0;
    int y0;
    int x1;
    int y1;
    rich4_route_node_xy_for_map(map, from_node, &x0, &y0);
    rich4_route_node_xy_for_map(map, to_node, &x1, &y1);
    return rich4_game_actor_direction_from_delta(x1 - x0, y1 - y0);
}

int rich4_route_next_for_node(const MapData *map, int node, int previous_node)
{
    int options[4];
    int count;
    if (map == NULL) return wrap_route_node(node + 1);
    if (!rich4_route_node_is_valid(map, node))
        return rich4_route_first_node_for_map(map);
    count = rich4_route_collect_non_return(map, node, previous_node, options);
    return count > 0 ? options[0] : rich4_route_first_node_for_map(map);
}

int rich4_int_sqrt_floor(long value)
{
    long bit = 1L << 30;
    long result = 0;

    if (value <= 0) return 0;
    while (bit > value) bit >>= 2;
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (int)result;
}

float rich4_original_fixed_move_duration(int dx, int dy)
{
    long double squared = (long double)dx * dx +
                          (long double)dy * dy;
    long double distance;
    int i;

    if (squared <= 0.0L) return 0.0f;
    distance = (long double)rich4_int_sqrt_floor(
        (long)((long)dx * dx + (long)dy * dy));
    if (distance <= 0.0L) distance = 1.0L;
    /* Match the x87 FSQRT result used by 0x40C05C closely, then store the
     * duration as a single-precision value before converting to frame count. */
    for (i = 0; i < 5; i++)
        distance = (distance + squared / distance) * 0.5L;
    return (float)(distance * 0.125L);
}

int rich4_original_interpolated_axis(int from, int to, int progress,
                                     float duration)
{
    volatile float position = (float)from;
    volatile float step;
    int i;

    if (duration <= 0.0f) return to;
    /* 0x40C05C stores dx/duration in a float accumulator and truncates the
     * accumulated value to the world coordinate on every movement pulse. */
    step = (float)((long double)(to - from) / (long double)duration);
    for (i = 0; i < progress; i++)
        position = position + step;
    return (int)position;
}

int rich4_consume_animation_pulse(uint32_t *last_step_tick, uint32_t now,
                                  int frame_ms)
{
    uint32_t elapsed;
    uint32_t due_pulses;

    if (last_step_tick == NULL) return 0;
    if (frame_ms <= 0) frame_ms = 1;
    if (*last_step_tick == 0) {
        *last_step_tick = now;
        return 0;
    }
    elapsed = now - *last_step_tick;
    if (elapsed < (uint32_t)frame_ms) return 0;
    due_pulses = elapsed / (uint32_t)frame_ms;
    *last_step_tick += due_pulses * (uint32_t)frame_ms;
    return 1;
}
