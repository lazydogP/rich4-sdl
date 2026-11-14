#ifndef RICH4_CORE_MAP_PROJECTION_H
#define RICH4_CORE_MAP_PROJECTION_H

/* 0x40829D / 0x407A2C: projected offset from the 440x440 viewport centre.
 * Returns 1 inside the original 29x29 tile table. Outside it, writes the
 * renderer's linear extrapolation and returns 0 (no original object entry). */
int rich4_map_project_offset(int direction, int center_x, int center_y,
    int world_x, int world_y, int *out_x, int *out_y);

#endif
