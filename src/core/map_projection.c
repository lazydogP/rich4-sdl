#include "core/map_projection.h"

#include "data/rich4_map_tables.inc"

#define GRID_X(direction, row, col) rich4_map_grid[(direction)][(row)][(col)][1]
#define GRID_Y(direction, row, col) rich4_map_grid[(direction)][(row)][(col)][0]

static void subtile_offset(int direction, int world_x, int world_y,
    int *out_x, int *out_y)
{
    static const signed char coeffs[8][4] = {
        {-34,11,-14,-25}, {-34,-9,16,-25}, {-13,-25,37,-9}, {16,-25,36,11},
        {37,-9,16,26}, {36,11,-14,26}, {16,26,-35,11}, {-14,26,-35,-9}
    };
    int sub_x = world_x & 31;
    int sub_y = world_y & 31;
    const signed char *c = coeffs[direction & 7];
    *out_x = ((sub_x * c[0]) >> 5) + ((sub_y * c[2]) >> 5);
    *out_y = ((sub_x * c[1]) >> 5) + ((sub_y * c[3]) >> 5);
}

int rich4_map_project_offset(int direction, int center_x, int center_y,
    int world_x, int world_y, int *out_x, int *out_y)
{
    int tile_x = world_x >> 5;
    int tile_y = world_y >> 5;
    int center_tile_x = center_x >> 5;
    int center_tile_y = center_y >> 5;
    int grid_x = tile_x - center_tile_x + 14;
    int grid_y = tile_y - center_tile_y + 14;
    int center_sub_x, center_sub_y, point_sub_x, point_sub_y;
    direction &= 7;
    subtile_offset(direction, center_x, center_y, &center_sub_x, &center_sub_y);
    subtile_offset(direction, world_x, world_y, &point_sub_x, &point_sub_y);
    if (grid_x < 0 || grid_x >= 29 || grid_y < 0 || grid_y >= 29) {
        int ux = GRID_X(direction, 14, 15) - GRID_X(direction, 14, 14);
        int vx = GRID_X(direction, 15, 14) - GRID_X(direction, 14, 14);
        int uy = GRID_Y(direction, 14, 15) - GRID_Y(direction, 14, 14);
        int vy = GRID_Y(direction, 15, 14) - GRID_Y(direction, 14, 14);
        if (out_x != 0) *out_x =
            (tile_x - center_tile_x) * ux + (tile_y - center_tile_y) * vx;
        if (out_y != 0) *out_y =
            (tile_x - center_tile_x) * uy + (tile_y - center_tile_y) * vy;
        return 0;
    }
    if (out_x != 0) *out_x = center_sub_x +
        GRID_X(direction, grid_y, grid_x) - point_sub_x;
    if (out_y != 0) *out_y = center_sub_y +
        GRID_Y(direction, grid_y, grid_x) - point_sub_y;
    return 1;
}
