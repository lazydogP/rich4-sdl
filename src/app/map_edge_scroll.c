/* Shared map-selector edge scrolling.  The app owns the camera mutation;
 * SDL owns only the normalized cursor and the configured inset. */

#include "app/map_input_flow.h"

#include "view/view_constants.h"

static int map_edge_clamp(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static const int map_edge_direction_x[8] = {
    0, -46341, -65536, -46341, 0, 46341, 65536, 46341
};

static const int map_edge_direction_y[8] = {
    -65536, -46341, 0, 46341, 65536, 46341, 0, -46341
};

static int map_edge_for_cursor(int cursor_x, int cursor_y, int inset)
{
    /* The toolbar is outside the map viewport.  Its hover must not start
     * angle-dependent map scrolling during card/tool target selection. */
    if (cursor_y < GAME_MAIN_MAP_TOP) return 0;
    /* Keep the original x-before-y priority.  With inset=0 these are the
     * original selector comparisons: x <= 0, x >= 440, y <= 40, y >= 479.
     * SDL's positive inset is measured in the normalized 640x480 game
     * coordinate system, not in host-window pixels. */
    if (inset <= 0) {
        if (cursor_x <= GAME_MAIN_MAP_LEFT) return 2;
        if (cursor_x >= GAME_MAIN_MAP_RIGHT) return 4;
        if (cursor_y <= GAME_MAIN_MAP_TOP) return 1;
        if (cursor_y >= GAME_MAIN_MAP_BOTTOM - 1) return 3;
        return 0;
    }
    if (cursor_x < GAME_MAIN_MAP_LEFT + inset) return 2;
    if (cursor_x >= GAME_MAIN_MAP_RIGHT - inset) return 4;
    if (cursor_y < GAME_MAIN_MAP_TOP + inset) return 1;
    if (cursor_y >= GAME_MAIN_MAP_BOTTOM - inset) return 3;
    return 0;
}

static int map_edge_scroll_next_view(
    const GameSession *session, int edge, int speed,
    int *out_x, int *out_y)
{
    int direction;
    if (session == NULL || out_x == NULL || out_y == NULL ||
        edge < 1 || edge > 4 || speed <= 0)
        return 0;
    direction = (session->presentation.map_direction + 2 * edge - 2) & 7;
    *out_x = map_edge_clamp(
        session->presentation.view_world_x +
            ((map_edge_direction_x[direction] * speed) >> 16),
        220, 2084);
    *out_y = map_edge_clamp(
        session->presentation.view_world_y +
            ((map_edge_direction_y[direction] * speed) >> 16),
        220, 2084);
    return *out_x != session->presentation.view_world_x ||
           *out_y != session->presentation.view_world_y;
}

void rich4_app_map_edge_scroll_reset(Rich4MapEdgeScrollState *state)
{
    if (state == NULL) return;
    state->direction = 0;
    state->speed = 8;
    state->tick = 0;
}

int rich4_app_map_edge_scroll_update(
    GameSession *session, Rich4MapEdgeScrollState *state,
    int cursor_x, int cursor_y, int inset, uint32_t now)
{
    int edge;
    int next_x;
    int next_y;
    int direction;

    if (state == NULL) return 0;
    if (session == NULL || !session->state.active) {
        rich4_app_map_edge_scroll_reset(state);
        return 0;
    }
    edge = map_edge_for_cursor(cursor_x, cursor_y, inset);
    if (edge == 0) {
        rich4_app_map_edge_scroll_reset(state);
        return 0;
    }
    if (state->direction == 0) {
        state->speed = 8;
        state->tick = now;
    } else if (state->speed <= 0) {
        state->speed = 8;
    }
    state->direction = edge;
    direction = (session->presentation.map_direction + 2 * edge - 2) & 7;
    if (!map_edge_scroll_next_view(
            session, edge, state->speed, &next_x, &next_y)) {
        /* The edge band is not a permanent dead zone at the world boundary:
         * once the camera clamp makes this direction ineffective, clear the
         * active timer so the target-confirm path can still receive a click. */
        rich4_app_map_edge_scroll_reset(state);
        return 0;
    }
    if (now - state->tick < 50) return 1;
    if (state->speed <= 64) state->speed += 4;
    next_x = map_edge_clamp(
        session->presentation.view_world_x +
            ((map_edge_direction_x[direction] * state->speed) >> 16),
        220, 2084);
    next_y = map_edge_clamp(
        session->presentation.view_world_y +
            ((map_edge_direction_y[direction] * state->speed) >> 16),
        220, 2084);
    if (next_x == session->presentation.view_world_x &&
        next_y == session->presentation.view_world_y) {
        rich4_app_map_edge_scroll_reset(state);
        return 0;
    }
    session->presentation.view_world_x = next_x;
    session->presentation.view_world_y = next_y;
    session->presentation.has_view_override = 1;
    state->tick = now;
    return 1;
}
