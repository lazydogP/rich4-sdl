#ifndef RICH4_APP_ROUTE_GEOMETRY_FLOW_H
#define RICH4_APP_ROUTE_GEOMETRY_FLOW_H

#include <stddef.h>
#include <stdint.h>

#include "data/map_data.h"

int rich4_route_count_for_map(const MapData *map);
int rich4_route_first_node_for_map(const MapData *map);
int rich4_route_node_is_valid(const MapData *map, int node);
void rich4_route_node_xy_for_map(const MapData *map, int node,
                                 int *out_x, int *out_y);
int rich4_route_direction8_for_nodes(const MapData *map,
                                     int from_node, int to_node);
int rich4_route_next_for_node(const MapData *map, int node,
                              int previous_node);
int rich4_int_sqrt_floor(long value);

/* 0x40C05C uses the fixed 1/8 movement lane for special actors and for
 * confinement release. Keep its single-precision duration and accumulator
 * rules shared by every presentation path that uses that lane. */
float rich4_original_fixed_move_duration(int dx, int dy);
int rich4_original_interpolated_axis(int from, int to, int progress,
                                     float duration);

/* 0x401F98 raises one pending pulse and 0x40D7C4 consumes it once. A late
 * presentation update therefore advances at most one animation step and
 * discards any additional elapsed intervals. */
int rich4_consume_animation_pulse(uint32_t *last_step_tick, uint32_t now,
                                  int frame_ms);

#endif
