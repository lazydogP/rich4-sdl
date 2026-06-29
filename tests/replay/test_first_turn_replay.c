#include "app/scene.h"
#include "core/game_dispatch.h"
#include "core/new_game_init.h"
#include "core/rng.h"
#include "core/route_movement.h"
#include "core/turn_rotation.h"
#include "core/turn_flow.h"
#include "debug/trace.h"

#include <stdio.h>
#include <string.h>

typedef struct Replay { char text[1024]; unsigned int used; } Replay;

static void append_event(void *context, const Rich4TraceEvent *event)
{
    Replay *replay = (Replay *)context;
    char line[64];
    int written;
    if (!rich4_trace_format(line, sizeof(line), event)) return;
    written = snprintf(replay->text + replay->used,
                       sizeof(replay->text) - replay->used, "%s\n", line);
    if (written > 0 && (unsigned int)written < sizeof(replay->text) - replay->used)
        replay->used += (unsigned int)written;
}

static int send_scene(Rich4SceneMachine *machine, Rich4SceneCommandType type,
                      Rich4SceneId expected)
{
    Rich4SceneCommand command = { type, RICH4_SCENE_NONE };
    Rich4SceneResult result = rich4_scene_machine_handle(machine, &command);
    return result.type == RICH4_SCENE_RESULT_SWITCH && machine->current == expected;
}

static void link_node(MapRouteNode *nodes, int node, int a, int b)
{
    nodes[node].next[0] = (unsigned short)a;
    nodes[node].next[1] = (unsigned short)b;
}

static int move_player(GameState *state, GameFlowState *flow, const MapData *map,
                       int player, GameCommandSource source, Replay *replay)
{
    GameEventQueue events;
    GameEvent event;
    GameCommand command = rich4_game_command(GAME_COMMAND_ROLL_DICE, source,
                                             player, 0);
    RouteMoveState move;
    int dice;
    rich4_game_event_queue_reset(&events);
    if (!rich4_game_dispatch_command(state, flow, &command, &events) ||
        !rich4_game_event_queue_pop(&events, &event) ||
        event.type != GAME_EVENT_TURN_FLOW_REQUESTED) return 0;
    flow->turn_flow_active = 1;
    flow->turn_state = 2;
    if (!rich4_game_dispatch_command(state, flow, &command, &events) ||
        !rich4_game_event_queue_pop(&events, &event) ||
        event.type != GAME_EVENT_DICE_ROLL_REQUESTED) return 0;
    dice = 1 + rich4_rng_range(6);
    rich4_trace_emit(source == GAME_COMMAND_SOURCE_AI ?
                     RICH4_TRACE_AI_AUTO_MOVE : RICH4_TRACE_DICE_ROLL,
                     player, dice, 1);
    if (rich4_route_move_begin_events(&move, map, player, 1, 0, dice, &events) !=
        ROUTE_MOVE_READY) return 0;
    rich4_trace_emit(RICH4_TRACE_MOVE_BEGIN, player, dice, move.node);
    while (move.steps_remaining > 0) {
        RouteMoveResult result = rich4_route_move_commit_events(&move, map,
                                                                 player, &events);
        if (result == ROUTE_MOVE_INVALID) return 0;
    }
    rich4_trace_emit(RICH4_TRACE_MOVE_END, player, dice, move.node);
    (void)replay;
    return 1;
}

static int generate(Replay *replay, unsigned int seed)
{
    Rich4SceneMachine scenes;
    GameState state;
    GameFlowState flow;
    NewGameInitConfig config;
    NewGameInitReport report;
    GameCharacterTemplate templates[2];
    GameStockInfo stocks[GAME_STOCK_COUNT];
    MapRouteNode nodes[8];
    MapData map;
    TurnAdvanceResult turn;
    Rich4Rng special_event_rng;
    int i;
    memset(replay, 0, sizeof(*replay));
    memset(&state, 0, sizeof(state));
    memset(&flow, 0, sizeof(flow));
    memset(&config, 0, sizeof(config));
    memset(templates, 0, sizeof(templates));
    memset(stocks, 0, sizeof(stocks));
    memset(nodes, 0, sizeof(nodes));
    memset(&map, 0, sizeof(map));
    templates[0].init_cash_ratio = 50;
    templates[1].init_cash_ratio = 40;
    config.initial_fund = 10000;
    config.day_limit = 30;
    config.selected_characters[0] = 0;
    config.selected_characters[1] = 1;
    config.selected_ai[1] = 1;
    config.character_templates = templates;
    config.character_template_count = 2;
    config.map_stocks = stocks;
    config.map_stock_count = GAME_STOCK_COUNT;
    rich4_rng_state_seed(&special_event_rng, seed);
    config.special_event_rng = &special_event_rng;
    map.routes = nodes;
    map.route_count = 8;
    for (i = 1; i < 7; ++i) link_node(nodes, i, i == 1 ? 7 : i - 1, i + 1);
    link_node(nodes, 7, 6, 1);

    rich4_rng_seed(seed);
    rich4_trace_set_sink(append_event, replay);
    rich4_scene_machine_init(&scenes, RICH4_SCENE_MENU);
    if (!send_scene(&scenes, RICH4_SCENE_COMMAND_NEW_GAME, RICH4_SCENE_NEW_GAME)) return 0;
    rich4_trace_emit(RICH4_TRACE_MENU_NEW_GAME, 0, 0, 0);
    if (!rich4_new_game_init(&state, &config, &report) || report.num_players != 2)
        return 0;
    rich4_trace_emit(RICH4_TRACE_NEW_GAME_SUBMIT, 0, report.num_players, 0);
    if (!send_scene(&scenes, RICH4_SCENE_COMMAND_START_GAME, RICH4_SCENE_INTRO)) return 0;
    rich4_trace_emit(RICH4_TRACE_INTRO_BEGIN, state.stage, state.map, 0);
    if (!send_scene(&scenes, RICH4_SCENE_COMMAND_INTRO_FINISHED, RICH4_SCENE_GAME)) return 0;
    if (!rich4_turn_flow_begin(&state, &flow, 1, 1000) ||
        flow.turn_state != GAME_TURN_STATE_LANDING ||
        flow.landing_pending_player != 1) return 0;
    rich4_trace_emit(RICH4_TRACE_FIRST_LANDING, state.current_player, 0, 0);
    memset(&flow, 0, sizeof(flow));
    rich4_trace_emit(RICH4_TRACE_FIRST_ACTION_DISPATCH, state.current_player, 0, 0);
    if (!move_player(&state, &flow, &map, 0, GAME_COMMAND_SOURCE_HUMAN, replay)) return 0;
    state.turn_rotation_index = 0;
    if (!rich4_turn_advance_once(&state, &turn) || turn.selected_index != 1) return 0;
    memset(&flow, 0, sizeof(flow));
    if (!move_player(&state, &flow, &map, 1, GAME_COMMAND_SOURCE_AI, replay)) return 0;
    return 1;
}

int main(void)
{
    Replay a, b;
    if (!generate(&a, 311U) || !generate(&b, 311U)) {
        fprintf(stderr, "integrated first-turn replay failed\n");
        return 1;
    }
    if (strcmp(a.text, b.text) != 0 ||
        strstr(a.text, "1,0,0,0\n2,0,2,0\n3,0,0,0\n") == NULL ||
        strstr(a.text, "6,0,") == NULL || strstr(a.text, "9,1,") == NULL) {
        fprintf(stderr, "integrated replay is unstable or incomplete\n%s", a.text);
        return 1;
    }
    puts(a.text);
    puts("menu/new-game/intro/first-turn/human/AI replay: ok");
    return 0;
}
