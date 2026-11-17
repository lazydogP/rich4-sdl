#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/game_event_payload.h"
#include "core/tool.h"
#include "core/rng.h"
#include "core/map_projection.h"

static GameDomainRef none(void)
{
    return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
}

static GameCommand use_tool(int tool, GameDomainRef target)
{
    return rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool), target, 0);
}

static void setup(GameState *state)
{
    uint8_t legal[8] = {0,1,1,0,1,1,1,1};
    int16_t route_x[8] = {0,0,50,100,150,200,250,300};
    int16_t route_y[8] = {0,0,0,0,0,0,0,0};
    uint16_t route_next[8][4] = {{0}};
    int i;
    for (i = 1; i < 8; i++) {
        if (i > 1) route_next[i][0] = (uint16_t)(i - 1);
        if (i < 7) route_next[i][1] = (uint16_t)(i + 1);
    }
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->player_count = 2;
    state->hospital_node = 7;
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        state->players[i].attached_timed_bomb_slot = -1;
    }
    for (i = 0; i < state->player_count; i++) {
        state->players[i].play_state = 1;
        state->players[i].node = i + 1;
        state->players[i].next_node = i + 2;
    }
    assert(rich4_tool_set_route_targets(state, 8, legal, sizeof(legal)));
    assert(rich4_tool_set_map_geometry(
        state, 8, route_x, route_y, route_next, 0, NULL, NULL,
        0, NULL, NULL));
}

static void catalog_and_layout(void)
{
    static const int pool[GAME_TOOL_COUNT] = {10,10,10,10,10,10,10,10,0,0,0,0,0};
    static const int price[GAME_TOOL_COUNT] = {15,30,25,25,80,150,100,30,30,40,95,150,250};
    static const int f6[GAME_TOOL_COUNT] = {0,0,0,0,0,1,1,1,2,2,2,2,2};
    static const int f7[GAME_TOOL_COUNT] = {0,1,1,1,0,0,2,0,1,2,1,2,2};
    int i;
    assert(sizeof(((GamePlayerState *)0)->tool_inventory) == 15);
    assert(sizeof(((GamePlayerState *)0)->tool_inventory) * GAME_MAX_PLAYERS == 60);
    for (i = 0; i < GAME_TOOL_COUNT; i++) {
        const GameToolInfo *info = rich4_tool_info(i);
        assert(info != NULL && info->original_id == i + 1);
        assert(info->initial_pool == pool[i] && info->shop_price == price[i]);
        assert(info->metadata_f6 == f6[i] && info->ai_difficulty == f7[i]);
        assert(info->handler_slot_address == 0x475dd5u + (uintptr_t)(i + 1) * 4u);
    }
    assert(rich4_tool_info(-1) == NULL && rich4_tool_info(GAME_TOOL_COUNT) == NULL);
}

static void placement_and_rejection(void)
{
    GameState state, before;
    GameEventQueue events;
    GameCommand command;
    setup(&state);
    rich4_game_event_queue_reset(&events);
    state.players[0].tool_inventory[1] = 2;
    command = use_tool(1, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 4));
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    assert(state.players[0].tool_inventory[1] == 1);
    assert(state.tool_amount[1] == 1);
    assert(state.tool_placed_objects[0].kind == 0x10 &&
           state.tool_placed_objects[0].node == 4 &&
           state.tool_placed_objects[0].owner == 0);
    assert(events.count == 3 && events.events[0].type == GAME_EVENT_MAP_OBJECT_CHANGED &&
           events.events[1].type == GAME_EVENT_INVENTORY_CHANGED &&
           events.events[2].type == GAME_EVENT_TOOL_USED);

    before = state;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);
    command.target = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 3);
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_INVALID);
    command.target = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 7);
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    state.players[0].tool_inventory[1] = 1;
    command.target = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 8);
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_INVALID);
    command.target = none();
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_INVALID);

    state.players[0].tool_inventory[1] = 0;
    command.target = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 4);
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_UNAVAILABLE);
    command = rich4_game_command(GAME_COMMAND_CANCEL, GAME_COMMAND_SOURCE_HUMAN, 0, 0);
    before = state;
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
}

static void occupied_route_placement_rejection(void)
{
    GameState state, before;
    GameEventQueue events;
    GameCommand command;

    setup(&state);
    state.players[0].tool_inventory[3] = 1;
    command = use_tool(3, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 2));
    before = state;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    /* 0x40CE0E retains the bankrupt player's 0x100 << player occupancy
     * bit.  The shared 0x409BC0 target table therefore rejects a bomb on
     * the beggar's node even though the player's active state is now zero. */
    state.players[1].play_state = 0;
    before = state;
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    setup(&state);
    state.players[0].tool_inventory[3] = 1;
    state.players[1].days_in_hospital = 1;
    command = use_tool(3, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 2));
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);

    setup(&state);
    state.players[0].tool_inventory[3] = 1;
    state.special_player_node[0] = 4;
    state.special_player_f10[0] = 0;
    command = use_tool(3, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 4));
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_INVALID);

    state.special_player_node[0] = 0;
    state.tool_route_flags[4] = UINT32_C(0x00000100);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_INVALID);
}

static void vehicles_and_deferred(void)
{
    GameState state, before;
    GameEventQueue events;
    GameCommand command;
    setup(&state);
    rich4_game_event_queue_reset(&events);
    state.players[0].traffic_method = 2;
    state.players[0].ndices = 3;
    state.players[0].tool_inventory[4] = 1;
    command = use_tool(4, none());
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    assert(state.players[0].traffic_method == 1 && state.players[0].ndices == 2);
    assert(state.players[0].tool_inventory[4] == 0 && state.players[0].tool_inventory[5] == 1);

    state.players[0].tool_inventory[5] = 2;
    command = use_tool(5, none());
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    assert(state.players[0].traffic_method == 2 && state.players[0].ndices == 3);
    assert(state.players[0].tool_inventory[4] == 1 && state.players[0].tool_inventory[5] == 1);

    state.players[0].tool_inventory[11] = 1;
    command = use_tool(11, none());
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    assert(state.players[0].traffic_method == 0x1f && state.players[0].ndices == 1);
    assert(state.players[0].engineering_saved_traffic_method == 2 &&
           state.players[0].engineering_saved_ndices == 3);

    state.players[0].tool_inventory[7] = 1;
    command = use_tool(7, none());
    command.value = 6;
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.players[0].remote_dice_value == 6 &&
           state.players[0].tool_inventory[7] == 0);

    setup(&state);
    state.players[0].traffic_method = 2;
    state.players[0].tool_inventory[4] = 1;
    state.players[0].tool_inventory[5] = 255;
    before = state;
    command = use_tool(4, none());
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    setup(&state);
    state.players[0].tool_inventory[0] = 1;
    command = use_tool(0, none());
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.tool_robot.active &&
           state.tool_robot.steps_remaining == 9 &&
           state.players[0].tool_inventory[0] == 0);
    while (state.tool_robot.active) {
        rich4_game_event_queue_reset(&events);
        assert(rich4_tool_robot_advance(&state, &events) !=
               GAME_TOOL_TRIGGER_INVALID);
    }
    assert(state.tool_robot.node == 4);
    state.players[0].is_ai = 1;
    command.source = GAME_COMMAND_SOURCE_AI;
    state.players[0].ai_personality = 0;
    state.players[0].tool_inventory[12] = 1;
    command.subject = rich4_game_domain_ref(GAME_DOMAIN_TOOL, 12);
    command.target = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 4);
    assert(rich4_tool_inspect(&state, &command) == GAME_TOOL_USE_APPLIED);
}

/* Golden offsets read from the original IDB: grid 0x46CCF0, signed
 * coefficients 0x474910, focus (647,659). Includes sub-tile rounding and
 * both sides of each edge of 0x40A45C(100), in all eight directions. */
static void missile_projected_area(void)
{
    static const struct {
        int direction, x, y, screen_dx, screen_dy, hit;
    } cases[] = {
        {0,572,630,-101,0,0},
        {0,573,630,-100,0,1},
        {0,573,631,-99,0,1},
        {0,723,692,99,0,1},
        {0,724,692,100,0,0},
        {0,725,692,101,-1,0},
        {0,693,552,0,-101,0},
        {0,693,553,0,-100,1},
        {0,692,554,0,-99,1},
        {0,606,767,0,99,1},
        {0,605,767,-1,100,0},
        {0,605,769,0,101,0},
        {1,573,689,-101,0,0},
        {1,574,689,-100,0,1},
        {1,575,689,-99,0,1},
        {1,725,628,99,0,1},
        {1,725,626,100,-1,0},
        {1,726,626,101,0,0},
        {1,605,550,0,-101,0},
        {1,605,551,0,-100,1},
        {1,606,552,0,-99,1},
        {1,694,766,0,99,1},
        {1,694,767,0,100,0},
        {1,694,768,0,101,0},
        {2,617,734,-101,0,0},
        {2,617,733,-100,0,1},
        {2,618,733,-99,0,1},
        {2,680,583,99,0,1},
        {2,679,582,100,-1,0},
        {2,680,582,101,0,0},
        {2,541,613,0,-101,0},
        {2,542,614,0,-100,1},
        {2,543,614,0,-99,1},
        {2,757,701,0,99,1},
        {2,758,701,0,100,0},
        {2,760,702,0,101,0},
        {3,680,733,-101,1,0},
        {3,679,733,-100,0,1},
        {3,679,732,-99,0,1},
        {3,616,583,99,0,1},
        {3,616,582,100,0,0},
        {3,616,581,101,1,0},
        {3,540,700,0,-101,0},
        {3,541,700,0,-100,1},
        {3,542,699,0,-99,1},
        {3,754,614,0,99,1},
        {3,754,613,1,100,0},
        {3,756,613,0,101,0},
        {4,722,690,-101,0,0},
        {4,722,688,-100,1,1},
        {4,721,688,-99,0,1},
        {4,570,627,99,0,1},
        {4,569,628,100,-1,0},
        {4,569,627,101,0,0},
        {4,601,766,0,-101,0},
        {4,601,765,1,-100,1},
        {4,601,764,1,-99,1},
        {4,690,551,0,99,1},
        {4,691,549,0,100,0},
        {4,691,548,0,101,0},
        {5,721,627,-101,0,0},
        {5,720,627,-100,0,1},
        {5,719,626,-99,1,1},
        {5,568,688,99,0,1},
        {5,568,689,100,0,0},
        {5,567,688,101,1,0},
        {5,690,767,-1,-101,0},
        {5,689,767,0,-100,1},
        {5,689,766,0,-99,1},
        {5,601,552,0,99,1},
        {5,601,551,0,100,0},
        {5,600,549,0,101,0},
        {6,677,583,-101,0,0},
        {6,677,584,-100,0,1},
        {6,676,585,-99,0,1},
        {6,613,734,99,0,1},
        {6,613,735,100,0,0},
        {6,613,736,101,1,0},
        {6,752,704,0,-101,0},
        {6,752,703,-1,-100,1},
        {6,751,703,0,-99,1},
        {6,538,616,-1,99,1},
        {6,537,616,0,100,0},
        {6,536,616,0,101,0},
        {7,615,584,-101,0,0},
        {7,615,585,-100,0,1},
        {7,615,586,-99,0,1},
        {7,677,736,99,-1,1},
        {7,676,737,100,1,0},
        {7,677,737,101,0,0},
        {7,755,616,0,-101,0},
        {7,754,617,0,-100,1},
        {7,753,617,0,-99,1},
        {7,540,704,-1,99,1},
        {7,540,705,1,100,0},
        {7,538,705,0,101,0},
    };
    GameState state;
    GameCommand command;
    GameEventQueue events;
    size_t n;
    int kind;
    for (n = 0; n < sizeof(cases) / sizeof(cases[0]); n++) {
        int dx, dy;
        assert(rich4_map_project_offset(cases[n].direction, 647, 659,
            cases[n].x, cases[n].y, &dx, &dy));
        assert(dx == cases[n].screen_dx && dy == cases[n].screen_dy);
        /* Isolate categories: the original raw object table ORs IDs that
         * share a pixel, so do not overlay synthetic properties/actors. */
        for (kind = 0; kind < 5; kind++) {
            setup(&state);
            state.price_index = 1;
            state.tool_route_x[3] = 647;
            state.tool_route_y[3] = 659;
            state.tool_route_x[4] = (int16_t)cases[n].x;
            state.tool_route_y[4] = (int16_t)cases[n].y;
            state.tool_route_x[7] = state.tool_route_y[7] = 3000;
            state.players[0].node = state.players[1].node = 7;
            state.players[0].tool_inventory[6] = 1;
            if (kind == 0) {
                state.land_count = 1;
                state.tool_land_x[0] = (int16_t)cases[n].x;
                state.tool_land_y[0] = (int16_t)cases[n].y;
                state.land_owner[0] = 1;
                state.land_level[0] = 3;
            } else if (kind == 1) {
                state.facility_count = 1;
                state.tool_facility_x[0] = (int16_t)cases[n].x;
                state.tool_facility_y[0] = (int16_t)cases[n].y;
                state.facility_owner[0] = 1;
                state.facility_level[0] = 3;
            } else if (kind == 2) {
                state.tool_placed_objects[0].kind = 0x10;
                state.tool_placed_objects[0].node = 4;
            } else if (kind == 3) {
                state.special_player_node[0] = 4;
                state.special_player_play_state[0] = 1;
            } else {
                state.players[1].node = 4;
            }
            command = rich4_game_command_with_missile_direction(
                use_tool(6, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 3)),
                cases[n].direction);
            rich4_game_event_queue_reset(&events);
            assert(rich4_tool_apply_command(&state, &command, &events) ==
                GAME_TOOL_USE_APPLIED);
            assert(state.players[0].tool_inventory[6] == 0);
            if (kind == 0)
                assert(state.land_level[0] == 3 - cases[n].hit);
            else if (kind == 1)
                assert(state.facility_level[0] == 3 - cases[n].hit);
            else if (kind == 2)
                assert(state.tool_placed_objects[0].kind ==
                    (cases[n].hit ? 0 : 0x10));
            else if (kind == 3)
                assert(state.special_player_node[0] ==
                    (cases[n].hit ? 0 : 4));
            else {
                assert(state.players[1].days_in_hospital == 3 * cases[n].hit);
                assert(state.players[1].node == (cases[n].hit ? 7 : 4));
            }
        }
    }
}

static void area_time_and_transfer(void)
{
    GameState state, before;
    GameEventQueue events;
    GameCommand command;
    int16_t land_x[3] = {100,180,250};
    int16_t land_y[3] = {0,0,0};
    int16_t facility_x[2] = {100,250};
    int16_t facility_y[2] = {0,0};

    setup(&state);
    state.price_index = 2;
    state.land_count = 3;
    state.facility_count = 2;
    memcpy(state.tool_land_x, land_x, sizeof(land_x));
    memcpy(state.tool_land_y, land_y, sizeof(land_y));
    memcpy(state.tool_facility_x, facility_x, sizeof(facility_x));
    memcpy(state.tool_facility_y, facility_y, sizeof(facility_y));
    state.land_owner[0] = state.land_owner[1] = state.land_owner[2] = 1;
    state.land_level[0] = state.land_level[1] = state.land_level[2] = 3;
    state.land_price[0] = state.land_price[1] = state.land_price[2] = 100;
    state.land_price_per_level[0] = state.land_price_per_level[1] =
        state.land_price_per_level[2] = 20;
    state.land_value[0] = state.land_value[1] = state.land_value[2] = 160;
    state.facility_owner[0] = state.facility_owner[1] = 1;
    state.facility_level[0] = state.facility_level[1] = 2;
    state.facility_type[0] = state.facility_type[1] = 1;
    state.facility_price[0] = state.facility_price[1] = 200;
    state.facility_price_per_level[0] =
        state.facility_price_per_level[1] = 30;
    state.facility_value[0] = state.facility_value[1] = 260;
    state.players[0].node = 7;
    state.players[1].node = 3;
    state.tool_placed_objects[0].kind = 0x10;
    state.tool_placed_objects[0].node = 2;
    state.tool_placed_objects[10].kind = 0x11;
    state.tool_placed_objects[10].node = 4;
    state.tool_placed_objects[20].kind = 0x12;
    state.tool_placed_objects[20].attached_player = 2;
    state.tool_placed_objects[20].countdown = 7;
    state.players[1].attached_timed_bomb_slot = 20;
    state.players[0].tool_inventory[6] = 1;
    command = use_tool(6, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 3));
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.land_level[0] == 2 && state.land_level[1] == 2 &&
           state.land_level[2] == 3);
    assert(state.facility_level[0] == 1 &&
           state.facility_level[1] == 2);
    assert(state.players[1].node == 7 &&
           state.players[1].days_in_hospital == 3 &&
           state.players[1].relation_balance[0] == 360);
    state.players[0].tool_inventory[12] = 1;
    command = use_tool(
        12, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 5));
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.land_owner[0] == -1 && state.land_level[0] == 0 &&
           state.land_owner[1] == -1 && state.land_level[1] == 0 &&
           state.land_owner[2] == -1 && state.land_level[2] == 0);
    assert(state.facility_owner[0] == -1 &&
           state.facility_level[0] == 0 &&
           state.facility_owner[1] == -1 &&
           state.facility_level[1] == 0);
    assert(state.tool_placed_objects[0].kind == 0 &&
           state.tool_placed_objects[10].kind == 0 &&
           state.tool_placed_objects[20].kind == 0 &&
           state.tool_amount[1] == 1 && state.tool_amount[2] == 1 &&
           state.tool_amount[3] == 1 &&
           state.players[1].attached_timed_bomb_slot == -1);

    setup(&state);
    state.players[0].tool_inventory[9] = 1;
    command = use_tool(9, none());
    before = state;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_RESTORE_REQUIRED);
    assert(memcmp(&state, &before, sizeof(state)) == 0 && events.count == 0);

    setup(&state);
    state.land_count = 2;
    state.land_owner[0] = 1;
    state.land_level[0] = 4;
    state.land_type[0] = 2;
    state.land_value[0] = 900;
    state.land_owner[1] = -1;
    state.players[0].tool_inventory[10] = 2;
    command = use_tool(10, rich4_game_domain_ref(GAME_DOMAIN_LAND, 0));
    command.target2 = rich4_game_domain_ref(GAME_DOMAIN_LAND, 1);
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.land_owner[0] == -1 && state.land_level[0] == 0 &&
           state.land_owner[1] == 1 && state.land_level[1] == 4 &&
           state.land_type[1] == 2 && state.land_value[1] == 900);

    command.target = rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1);
    command.target2 = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 6);
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.players[1].node == 6 &&
           state.players[1].previous_node == 0 &&
           state.players[1].next_node == 5);

    /* A released special actor rescued by player 0 has a valid +8 owner
     * value of zero.  Tool target validation and transfer must use +10/f10
     * for visibility rather than treating that owner value as "not active". */
    setup(&state);
    state.special_player_f10[0] = 0;
    state.special_player_play_state[0] = 0;
    state.special_player_node[0] = 3;
    state.players[0].tool_inventory[10] = 1;
    command = use_tool(10,
        rich4_game_domain_ref(GAME_DOMAIN_SPECIAL_PLAYER, 0));
    command.target2 = rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 6);
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_inspect(&state, &command) == GAME_TOOL_USE_APPLIED);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.special_player_node[0] == 6 &&
           state.special_player_play_state[0] == 0);

    setup(&state);
    state.price_index = 1;
    state.players[0].node = 7;
    state.players[1].node = 7;
    state.special_player_f10[0] = 0;
    state.special_player_play_state[0] = 0;
    state.special_player_node[0] = 3;
    state.players[0].tool_inventory[6] = 1;
    command = use_tool(6,
        rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 3));
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.special_player_node[0] == 0 &&
           state.special_player_play_state[0] == 0);
}

static void robot_worker(void)
{
    GameState state;
    GameEventQueue events;
    GameCommand command;
    setup(&state);
    state.land_count = 1;
    state.land_level[0] = 4;
    state.facility_count = 1;
    state.facility_level[0] = 0;
    state.facility_type[0] = 0;
    state.players[0].tool_inventory[8] = 2;
    rich4_game_event_queue_reset(&events);
    command = use_tool(8, rich4_game_domain_ref(GAME_DOMAIN_LAND, 0));
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.land_level[0] == 5 &&
           state.players[0].tool_inventory[8] == 1);
    rich4_game_event_queue_reset(&events);
    command.target = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0);
    command.value = 4;
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.facility_type[0] == 4 && state.facility_level[0] == 1 &&
           state.players[0].tool_inventory[8] == 0);
}

static void triggers(void)
{
    GameState state;
    GameEventQueue events;
    GameEvent event;
    GameCommand command;
    int slot;
    setup(&state);
    rich4_game_event_queue_reset(&events);
    state.players[0].tool_inventory[1] = 1;
    state.players[0].node = 3;
    command = use_tool(1, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 1));
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_trigger_at_node(&state, 1, 1, 0, &events) == GAME_TOOL_TRIGGER_STOP);
    assert(state.tool_placed_objects[0].kind == 0 && events.count == 2);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_MAP_OBJECT_CHANGED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_VOICE_REQUESTED &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_TOOL &&
           rich4_game_event_subject_ref(&event).id == 1 &&
           event.player == 1);

    state.players[0].tool_inventory[2] = 1;
    state.players[1].node = 4;
    command = use_tool(2, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 2));
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    state.players[1].node = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_trigger_at_node(&state, 1, 2, 1, &events) ==
           GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT);
    assert(state.tool_placed_objects[10].kind == 0);
    assert(state.players[1].node == 7 &&
           state.players[1].days_in_hospital == 3 && events.count == 9);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_MAP_OBJECT_CHANGED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED &&
           rich4_game_event_value(&event) == 525 &&
           rich4_game_event_value2(&event) == 82 &&
           rich4_game_event_target_ref(&event).kind == GAME_DOMAIN_ROUTE_NODE);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_VOICE_REQUESTED &&
           rich4_game_event_subject_ref(&event).id == 2 &&
           rich4_game_event_target_ref(&event).kind == GAME_DOMAIN_PLAYER &&
           rich4_game_event_target_ref(&event).id == 1);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_TEXT_REQUESTED &&
           rich4_game_event_text_requested_fields(&event).subject == 53 &&
           rich4_game_event_text_requested_fields(&event).value ==
               GAME_CONFINEMENT_HOSPITAL &&
           rich4_game_event_text_requested_fields(&event).value2 == 3);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED &&
           rich4_game_event_value(&event) == 524 &&
           rich4_game_event_value2(&event) == 92 &&
           rich4_game_event_target_ref(&event).kind == GAME_DOMAIN_STATUS &&
           rich4_game_event_target_ref(&event).id == GAME_STATUS_HOSPITAL);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_STATUS_CHANGED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_ACTOR_DIRTY_REQUESTED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_SCENE_REFRESH_REQUESTED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_VOICE_REQUESTED &&
           rich4_game_event_subject_ref(&event).kind == GAME_DOMAIN_STATUS &&
           rich4_game_event_subject_ref(&event).id == GAME_STATUS_HOSPITAL &&
           rich4_game_event_target_ref(&event).kind == GAME_DOMAIN_TOOL &&
           events.count == 0);

    /* The following movement scenario starts after the prior mine admission. */
    state.players[1].days_in_hospital = 0;
    state.players[1].confinement_actor_type = GAME_CONFINEMENT_NONE;
    state.players[0].tool_inventory[3] = 1;
    command = use_tool(3, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 4));
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_trigger_at_node(&state, 1, 4, 1, &events) == GAME_TOOL_TRIGGER_PASS);
    assert(state.tool_placed_objects[20].kind == 0x12 && state.tool_placed_objects[20].node == 0);
    assert(state.tool_placed_objects[20].attached_player == 2 &&
           state.tool_placed_objects[20].countdown == 0x26 &&
           state.players[1].attached_timed_bomb_slot == 20);
    assert(rich4_tool_trigger_at_node(&state, 1, 6, 1, &events) == GAME_TOOL_TRIGGER_NONE);

    state.players[1].node = 4;
    state.tool_placed_objects[20].countdown = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_advance_step(&state, 1, 4, 0, &events) ==
           GAME_TOOL_TRIGGER_PASS);
    assert(state.tool_placed_objects[20].countdown == 1);
    state.players[0].node = 5;
    state.players[1].node = 5;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_advance_step(&state, 1, 5, 0, &events) ==
           GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT);
    assert(state.tool_placed_objects[20].kind == 0 &&
           state.players[0].attached_timed_bomb_slot == -1 &&
           state.players[1].attached_timed_bomb_slot == -1 &&
           state.players[1].node == 7 && state.players[1].days_in_hospital == 5 &&
           state.players[0].node == 5 && state.players[0].days_in_hospital == 0);
    /* 0x41B6C4: zero explodes on the carrier before transfer. */
    assert(events.count == 8);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_MAP_OBJECT_CHANGED);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED &&
           rich4_game_event_tool_resource(&event) == 525 &&
           rich4_game_event_tool_effect(&event) == 82);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_TEXT_REQUESTED &&
           rich4_game_event_text_requested_fields(&event).value ==
               GAME_CONFINEMENT_HOSPITAL &&
           rich4_game_event_text_requested_fields(&event).value2 == 5);
    assert(rich4_game_event_queue_pop(&events, &event) &&
           event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED &&
           rich4_game_event_tool_resource(&event) == 524 &&
           rich4_game_event_tool_effect(&event) == 92);

    state.players[1].traffic_method = 0x1f;
    state.players[1].ndices = 1;
    state.players[1].engineering_saved_traffic_method = 2;
    state.players[1].engineering_saved_ndices = 3;
    state.players[1].tool_inventory[5] = 1;
    for (slot = 0; slot < 7; slot++) {
        rich4_game_event_queue_reset(&events);
        assert(rich4_tool_advance_step(&state, 1, 5, 1, &events) ==
               GAME_TOOL_TRIGGER_PASS);
    }
    assert(state.players[1].traffic_method == 2 &&
           state.players[1].ndices == 3 &&
           state.players[1].tool_inventory[5] == 0);
}

static void rollback_and_snapshot(void)
{
    GameState state, before, restored;
    GameEventQueue events;
    GameEvent filler;
    GameCommand command;
    GameToolSnapshot snapshot, corrupt;
    int i;
    setup(&state);
    state.players[0].tool_inventory[1] = 1;
    command = use_tool(1, rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 4));
    rich4_game_event_queue_reset(&events);
    filler = rich4_game_event_text_requested_from_fields(
        (Rich4GameEventFields){
            0, 0, 0, 0, 0,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1)
        });
    for (i = 0; i < GAME_EVENT_QUEUE_CAPACITY; i++)
        assert(rich4_game_event_queue_push(&events, &filler));
    before = state;
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) == GAME_TOOL_USE_APPLIED);
    state.players[0].tool_inventory[14] = 77;
    assert(rich4_tool_snapshot_export(&state, &snapshot));
    memset(&restored, 0x5a, sizeof(restored));
    assert(rich4_tool_snapshot_import(&restored, &snapshot));
    assert(memcmp(restored.tool_amount, state.tool_amount, sizeof(state.tool_amount)) == 0);
    assert(memcmp(restored.players[0].tool_inventory,
                  state.players[0].tool_inventory, GAME_PLAYER_TOOL_SLOTS) == 0);
    assert(memcmp(restored.tool_placed_objects, state.tool_placed_objects,
                  sizeof(state.tool_placed_objects)) == 0);
    corrupt = snapshot;
    corrupt.placed[0].node = GAME_MAX_MAP_ROUTE_NODES;
    before = restored;
    assert(!rich4_tool_snapshot_import(&restored, &corrupt));
    assert(memcmp(&restored, &before, sizeof(restored)) == 0);
}

static void robot_route_policy(void)
{
    GameState state, before;
    GameEventQueue events;
    GameCommand command = use_tool(0, none());
    Rich4Rng expected;
    int seed;
    int seen = 0;
    for (seed = 1; seed <= 8; seed++) {
        setup(&state);
        state.players[0].node = 2;
        state.players[0].previous_node = 1;
        state.players[0].next_node = 1; /* stale planned player route */
        state.players[0].tool_inventory[0] = 1;
        state.tool_route_next[2][2] = 4;
        state.tool_route_next[2][3] = 5;
        state.tool_route_flags[2] = UINT32_C(0x10000000); /* block slot 2 */
        state.ai_rng_state = (uint32_t)seed;
        expected.state = state.ai_rng_state;
        int chosen = rich4_rng_state_range(&expected, 2) == 0 ? 3 : 5;
        rich4_game_event_queue_reset(&events);
        assert(rich4_tool_apply_command(&state, &command, &events) ==
               GAME_TOOL_USE_APPLIED);
        assert(state.tool_robot.next_node == chosen);
        assert(state.ai_rng_state == expected.state);
        seen |= chosen == 3 ? 1 : 2;
    }
    assert(seen == 3);
    setup(&state);
    state.players[0].node = 7;
    state.players[0].previous_node = 6;
    state.players[0].tool_inventory[0] = 1;
    state.ai_rng_state = 123;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_apply_command(&state, &command, &events) ==
           GAME_TOOL_USE_APPLIED);
    assert(state.tool_robot.next_node == 6 && state.ai_rng_state == 123);
    events.count = GAME_EVENT_QUEUE_CAPACITY;
    state.tool_placed_objects[0].kind = 0x10;
    state.tool_placed_objects[0].node = 6;
    before = state;
    assert(rich4_tool_robot_advance(&state, &events) == GAME_TOOL_TRIGGER_INVALID);
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_robot_advance(&state, &events) == GAME_TOOL_TRIGGER_PASS);
    assert(state.tool_placed_objects[0].kind == 0 && state.tool_amount[1] == 1);
}

static void attached_bomb_first_occupant(void)
{
    GameState state;
    GameEventQueue events;
    setup(&state);
    state.player_count = 3;
    state.players[0].node = state.players[1].node = state.players[2].node = 4;
    state.players[0].play_state = 0;
    state.players[2].play_state = 1;
    state.players[2].attached_timed_bomb_slot = 20;
    state.tool_placed_objects[20].kind = 18;
    state.tool_placed_objects[20].attached_player = 3;
    state.tool_placed_objects[20].countdown = 3;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_advance_step(&state, 2, 4, 0, &events) == GAME_TOOL_TRIGGER_PASS);
    assert(state.tool_placed_objects[20].countdown == 2);
    assert(state.players[2].attached_timed_bomb_slot == 20);
    assert(state.players[1].attached_timed_bomb_slot == -1);
    state.players[0].play_state = 1;
    rich4_game_event_queue_reset(&events);
    assert(rich4_tool_advance_step(&state, 2, 4, 0, &events) == GAME_TOOL_TRIGGER_PASS);
    assert(state.tool_placed_objects[20].countdown == 1);
    assert(state.players[0].attached_timed_bomb_slot == 20);
    assert(state.players[2].attached_timed_bomb_slot == -1);
}

int main(void)
{
    robot_route_policy();
    catalog_and_layout();
    placement_and_rejection();
    occupied_route_placement_rejection();
    vehicles_and_deferred();
    robot_worker();
    missile_projected_area();
    area_time_and_transfer();
    triggers();
    attached_bomb_first_occupant();
    rollback_and_snapshot();
    puts("13-tool catalog, prerequisites, effects and snapshot: ok");
    return 0;
}
