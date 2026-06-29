#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/route_movement.h"
#include "core/rng.h"

static void link_node(MapRouteNode *nodes, int node, int a, int b, int c)
{
    nodes[node].next[0] = (unsigned short)a;
    nodes[node].next[1] = (unsigned short)b;
    nodes[node].next[2] = (unsigned short)c;
}

static int run_trace(const MapData *map, unsigned int seed, int trace[4])
{
    RouteMoveState move;
    RouteMoveResult result;
    int count = 0;
    rich4_rng_seed(seed);
    result = rich4_route_move_begin(&move, map, 1, 0, 3);
    trace[count++] = move.node;
    while (result != ROUTE_MOVE_DONE) {
        assert(result == ROUTE_MOVE_READY);
        assert(move.selected_index >= 0 && move.selected_index < move.option_count);
        result = rich4_route_move_commit(&move, map);
        assert(result != ROUTE_MOVE_INVALID);
        trace[count++] = move.node;
    }
    return count;
}

int main(void)
{
    MapRouteNode nodes[7];
    MapData map;
    RouteMoveState move;
    int options[4];
    int trace_a[4];
    int trace_b[4];
    int slot;
    GameEventQueue events;
    GameEvent event;
    GameState game;
    memset(nodes, 0, sizeof(nodes));
    memset(&map, 0, sizeof(map));
    map.routes = nodes;
    map.route_count = 7;
    link_node(nodes, 1, 2, 3, 0);
    link_node(nodes, 2, 1, 4, 0);
    link_node(nodes, 3, 1, 5, 0);
    link_node(nodes, 4, 2, 6, 0);
    link_node(nodes, 5, 3, 6, 0);
    link_node(nodes, 6, 4, 5, 0);
    assert(rich4_route_collect_non_return(&map, 2, 1, options) == 1);
    assert(options[0] == 4);
    link_node(nodes, 4, 2, 0, 0);
    assert(rich4_route_collect_non_return(&map, 4, 2, options) == 1);
    assert(options[0] == 2);
    link_node(nodes, 4, 2, 6, 0);
    nodes[1].next[0] = 2; nodes[1].next[1] = 3;
    nodes[1].next[2] = 4; nodes[1].next[3] = 5;
    for (slot = 0; slot < 4; slot++) {
        int option;
        nodes[1].flags = 0x40000000u >> slot;
        assert(rich4_route_collect_non_return(&map, 1, 0, options) == 3);
        for (option = 0; option < 3; option++)
            assert(options[option] != (int)nodes[1].next[slot]);
    }
    nodes[1].flags = 0x40000000u;
    nodes[1].next[0] = 6; nodes[1].next[1] = 2;
    nodes[1].next[2] = 0; nodes[1].next[3] = 0;
    nodes[6].next[0] = 1; nodes[6].next[1] = 0;
    assert(rich4_route_collect_non_return(&map, 1, 2, options) == 1 &&
           options[0] == 2);
    assert(rich4_route_collect_non_return(&map, 6, 0, options) == 1 &&
           options[0] == 1);
    nodes[1].flags = 0;
    link_node(nodes, 1, 2, 3, 0);
    link_node(nodes, 6, 4, 5, 0);
    rich4_rng_seed(7);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_move_begin_events(&move, &map, 2, 1, 0, 3, &events) ==
           ROUTE_MOVE_READY);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_ROUTE_MOVE_RESOLVED && event.player == 2 &&
           rich4_game_event_subject(&event) == ROUTE_MOVE_READY &&
           rich4_game_event_value(&event) == 1);
    assert(move.option_count == 2 && move.options[0] == 2 && move.options[1] == 3);
    assert(move.selected_index >= 0 && move.selected_index < 2);
    assert(rich4_route_move_begin_with_next(&move, &map, 1, 0, 3, 2) ==
           ROUTE_MOVE_READY);
    assert(move.next_node == 3 && move.selected_index == 1);
    assert(run_trace(&map, 1234, trace_a) == 4);
    assert(run_trace(&map, 1234, trace_b) == 4);
    assert(memcmp(trace_a, trace_b, sizeof(trace_a)) == 0);
    assert(trace_a[0] == 1 && trace_a[3] == 6);
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_move_commit_events(&move, &map, 2, &events) != ROUTE_MOVE_INVALID);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_ROUTE_MOVE_RESOLVED && event.player == 2);
    memset(&game, 0, sizeof(game));
    game.player_count = 1;
    game.players[0].play_state = 1;
    game.players[0].attached_timed_bomb_slot = -1;
    game.tool_route_node_count = 7;
    game.tool_route_node_placeable[2] = 1;
    game.tool_placed_objects[0].kind = 0x10;
    game.tool_placed_objects[0].node = 2;
    game.tool_placed_objects[0].owner = 0;
    memset(&move, 0, sizeof(move));
    move.node = 1; move.next_node = 2; move.steps_remaining = 3;
    move.selected_index = 0; move.option_count = 1; move.options[0] = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_move_commit_tool_events(&move, &map, &game, 0, &events) ==
           ROUTE_MOVE_DONE);
    assert(move.node == 2 && move.steps_remaining == 0 &&
           game.tool_placed_objects[0].kind == 0 && events.count == 3);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_ROUTE_MOVE_RESOLVED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_MAP_OBJECT_CHANGED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_VOICE_REQUESTED &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_TOOL &&
           rich4_game_event_subject_ref(&event).id == 1 &&
           event.player == 0);
    game.tool_placed_objects[0].kind = 0x12;
    game.tool_placed_objects[0].node = 2;
    game.tool_placed_objects[0].attached_player = 0xff;
    game.players[0].attached_timed_bomb_slot = -1;
    memset(&move, 0, sizeof(move));
    move.node = 1; move.next_node = 2; move.steps_remaining = 3;
    move.selected_index = 0; move.option_count = 1; move.options[0] = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_move_commit_tool_events(
        &move, &map, &game, 0, &events) == ROUTE_MOVE_READY);
    assert(game.tool_placed_objects[0].node == 2 &&
           game.players[0].attached_timed_bomb_slot == -1);
    memset(&move, 0, sizeof(move));
    move.node = 1; move.next_node = 2; move.steps_remaining = 1;
    move.selected_index = 0; move.option_count = 1; move.options[0] = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_route_move_commit_tool_events(
        &move, &map, &game, 0, &events) == ROUTE_MOVE_DONE);
    assert(game.tool_placed_objects[0].node == 0 &&
           game.tool_placed_objects[0].attached_player == 1 &&
           game.tool_placed_objects[0].countdown == 0x26 &&
           game.players[0].attached_timed_bomb_slot == 0);
    puts("route movement: ok");
    return 0;
}
