#include "core/turn_around.h"

#include "core/rng.h"
#include <stddef.h>

static int route_node_valid(const GameState *state, int node)
{
    return state != NULL && node > 0 &&
           node < state->tool_route_node_count;
}

static int collect_turn_options(const GameState *state, int node,
                                int excluded, int options[4])
{
    int count = 0;
    int edge;
    if (!route_node_valid(state, node)) return 0;
    for (edge = 0; edge < 4; edge++) {
        int next = state->tool_route_next[node][edge];
        if (!route_node_valid(state, next) || next == excluded ||
            (state->tool_route_edge_blocked[node] & (1u << edge)) != 0)
            continue;
        options[count++] = next;
    }
    return count;
}

int rich4_turn_around_route(GameState *state, GameDomainRef target)
{
    Rich4Rng rng;
    int options[4];
    int count;
    int node;
    int *route_edge;

    if (state == NULL) return 0;
    if (target.kind == GAME_DOMAIN_PLAYER) {
        if (target.id < 0 || target.id >= state->player_count ||
            (state->players[target.id].play_state & 0xff) == 0)
            return 0;
        node = state->players[target.id].node;
        route_edge = &state->players[target.id].next_node;
        state->players[target.id].route_reversed = 0;
    } else if (target.kind == GAME_DOMAIN_SPECIAL_PLAYER) {
        if (target.id < 0 || target.id >= GAME_SPECIAL_PLAYER_COUNT ||
            state->special_player_f10[target.id] != 0)
            return 0;
        node = state->special_player_node[target.id];
        route_edge = &state->special_player_previous_node[target.id];
        state->special_player_route_reversed[target.id] = 0;
    } else {
        return 0;
    }

    count = collect_turn_options(state, node, *route_edge, options);
    if (count == 0) {
        *route_edge = 0;
        return 1;
    }
    rng.state = state->ai_rng_state;
    *route_edge = options[rich4_rng_original_state_range(&rng, count)];
    state->ai_rng_state = rng.state;
    return 1;
}
