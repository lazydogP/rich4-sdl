#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/dice_flow.h"
#include "app/game_command_flow.h"
#include "app/turn_movement_flow.h"
#include "app/turn_rotation_flow.h"
#include "app/turn_start_flow.h"
#include "core/card.h"
#include "core/rng.h"
#include "core/special_event.h"
#include "view/view_constants.h"

static void news_stopping_turn(int news_id, int is_ai, int tortoise)
{
    static GameSession session;
    MapData map;
    MapRouteNode routes[4];
    GameEventQueue events;
    Rich4AppTurnMovementResult movement;
    unsigned char mask[GAME_DICE_STATUS_W * GAME_DICE_STATUS_H];
    uint32_t rng_before;

    memset(&session, 0, sizeof(session));
    memset(&map, 0, sizeof(map));
    memset(routes, 0, sizeof(routes));
    memset(mask, 0, sizeof(mask));
    map.routes = routes;
    map.route_count = 4;
    routes[1].next[0] = 2;
    routes[1].object.raw_object_id = 1;
    routes[2].next[0] = 3;
    routes[3].next[0] = 1;
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.players[0].play_state = is_ai ? 2 : 1;
    session.state.players[0].is_ai = is_ai;
    session.state.players[0].traffic_method = news_id == 16 ? 0 : 2;
    session.state.players[0].ndices = news_id == 16 ? 1 : 3;
    session.state.players[0].node = 1;
    session.state.players[0].previous_node = 3;
    session.state.players[0].remote_dice_value = 4;
    session.state.players[0].days_tortoise_walking = tortoise;
    session.state.players[1].play_state = 1;
    session.state.players[1].traffic_method = news_id == 16 ? 2 : 0;
    session.state.news_event_order[0] = (uint8_t)news_id;
    rich4_game_event_queue_reset(&events);
    assert(rich4_special_event_begin_news(&session.state, 0, &events) ==
           GAME_SPECIAL_EVENT_PREPARED);
    assert(session.state.players[0].days_stopping == 1);
    assert(session.state.players[1].days_stopping == 0);
    assert(rich4_special_event_commit(&session.state,
        GAME_SPECIAL_DEFENSE_NONE, &events) == GAME_SPECIAL_EVENT_APPLIED);

    /* Turn selection decrements 1 to 0x80; it still blocks this turn. */
    session.state.turn_rotation_index = -1;
    assert(rich4_app_advance_turn_rotation(&session, &map, 1000, NULL));
    assert(session.state.current_player == 0);
    assert(session.state.players[0].days_stopping == GAME_STATUS_PENDING_BIT);
    rng_before = session.state.ai_rng_state;
    if (is_ai) {
        assert(rich4_turn_start_begin(&session, 1000));
    } else {
        rich4_dice_show_action_panel(&session, 1000, 0, 1);
        mask[0] = 3;
        assert(rich4_dice_status_mouse_down(
            &session, mask, sizeof(mask), session.presentation.dice.status_x,
            session.presentation.dice.status_y, 1000) == 2);
    }
    /* Even with action frames available, no dice animation or RNG runs. */
    assert(rich4_dice_update(&session, &map, 12, 20, 30, 10, 1000));
    assert(session.flow.turn_state == GAME_TURN_STATE_MOVE);
    assert(session.flow.turn_wait == 2);
    assert(session.flow.steps_remaining == 0 && session.flow.dice_value == 0);
    assert(!session.presentation.dice.popup_active);
    if (is_ai) {
        const GameState *snapshot_state;
        const GameFlowState *snapshot_flow;
        const GameActorPresentationState *snapshot_actors;
        const GameMapDynamicObjectState *snapshot_dynamic_objects;
        assert(!rich4_app_time_machine_get(
            &session, 0, &snapshot_state, &snapshot_flow,
            &snapshot_actors, &snapshot_dynamic_objects));
    } else {
        assert(session.time_machine_valid);
        assert(session.time_machine_state.players[0].days_stopping ==
               GAME_STATUS_PENDING_BIT);
    }
    assert(session.state.ai_rng_state == rng_before);
    assert(session.state.players[0].remote_dice_value == 4);
    assert(rich4_app_update_turn_movement(&session, &map, 20, 1019, &movement));
    assert(session.flow.turn_wait == 2);
    assert(rich4_app_update_turn_movement(&session, &map, 20, 1020, &movement));
    assert(session.flow.turn_wait == 1);
    assert(rich4_app_update_turn_movement(&session, &map, 20, 1040, &movement));
    assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
    assert(session.flow.route_facts.count == 1);
    assert(session.flow.route_facts.events[0].phase == ROUTE_EVENT_PHASE_STOP);
    assert(session.flow.route_facts.events[0].node == 1);
    assert(session.state.players[0].node == 1);
    assert(!session.presentation.actors[0].move_active);

    /* On the following selection the pending bit clears, restoring dice. */
    session.state.turn_rotation_index = -1;
    assert(rich4_app_complete_route_event_turn(&session, &map, 2000, NULL));
    assert(session.state.players[0].days_stopping == 0);
    assert(rich4_turn_start_begin(&session, 2000));
    assert(rich4_dice_update(&session, &map, 0, 20, 30, 10, 2000));
    assert(session.presentation.dice.popup_active);
    assert(session.flow.dice_value == 4);
    assert(session.state.players[0].remote_dice_value == 0);
}

static void tortoise_card_turns(int self, int is_ai, int traffic, int remote)
{
    static GameSession session;
    MapData map;
    MapRouteNode routes[4];
    GameEventQueue events;
    GameCommand command;
    GamePlayerState *player;
    uint32_t now = 1000;
    int target = self ? 0 : 1;
    int i;

    memset(&session, 0, sizeof(session));
    memset(&map, 0, sizeof(map));
    memset(routes, 0, sizeof(routes));
    map.routes = routes;
    map.route_count = 4;
    for (i = 1; i <= 3; i++) {
        routes[i].x = i * 8;
        routes[i].next[0] = i == 3 ? 1 : i + 1;
        routes[i].object.raw_object_id = (uint16_t)i;
    }
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.players[0].play_state = 1;
    session.state.players[1].play_state = 1;
    session.state.players[0].card_inventory[0] = 30;
    player = &session.state.players[target];
    player->play_state = is_ai ? 2 : 1;
    player->is_ai = is_ai;
    player->traffic_method = traffic;
    player->ndices = traffic + 1;
    player->remote_dice_value = remote;
    player->attached_timed_bomb_slot = -1;
    player->node = 1;
    player->next_node = 2;
    command = rich4_game_domain_command(GAME_COMMAND_USE_CARD,
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 29),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, target), 0);
    rich4_game_event_queue_reset(&events);
    assert(rich4_card_apply_command(&session.state, &command, &events) ==
           GAME_CARD_USE_APPLIED);
    assert(player->days_tortoise_walking == (self ? 2 : 3));
    assert(!rich4_card_inventory_has(&session.state, 0, 30));
    if (!self) {
        session.state.turn_rotation_index = target - 1;
        assert(rich4_app_advance_turn_rotation(&session, &map, now, NULL));
    }

    /* 0x4458DF writes 2 for self and 3 for others. Turn selection ticks
     * the latter to 2; both include the final pending-0x80 turn. */
    for (i = 0; i < 3; i++) {
        Rich4AppTurnMovementResult movement;
        uint32_t rng_before = session.state.ai_rng_state;
        uint32_t rng_after = rng_before;
        int node = player->node;
        int next = node == 3 ? 1 : node + 1;
        int guard;
        assert(player->days_tortoise_walking ==
               (i == 2 ? GAME_STATUS_PENDING_BIT : 2 - i));
        assert(rich4_turn_start_begin(&session, now));
        /* Only uncached route selection draws RNG; dice do not. */
        player->previous_node = is_ai ? (node == 1 ? 3 : node - 1) : 0;
        player->next_node = next;
        if (is_ai) (void)rich4_rng_range_at(&rng_after, 1);
        assert(rich4_dice_update(&session, &map, 12, 20, 30, 10, now));
        assert(session.flow.turn_state == GAME_TURN_STATE_MOVE);
        assert(session.flow.steps_remaining == 1);
        assert(session.flow.route_move.steps_remaining == 1);
        assert(session.flow.route_move.next_node == next);
        assert(!session.presentation.dice.popup_active);
        assert(!session.presentation.dice.result_visible);
        assert(!session.presentation.dice.sound_pending);
        assert(session.state.ai_rng_state == rng_after);
        assert(player->remote_dice_value == remote);
        assert(player->ndices == traffic + 1);
        {
            const GameState *snapshot_state;
            const GameFlowState *snapshot_flow;
            const GameActorPresentationState *snapshot_actors;
            const GameMapDynamicObjectState *snapshot_dynamic_objects;
            int captured = rich4_app_time_machine_get(
                &session, target, &snapshot_state, &snapshot_flow,
                &snapshot_actors, &snapshot_dynamic_objects);
            assert(captured == !is_ai);
            if (captured) {
                assert(snapshot_state->ai_rng_state == rng_before);
                assert(snapshot_state->players[target].node == node);
                assert(snapshot_state->players[target].remote_dice_value == remote);
            }
        }
        for (guard = 0; guard < 20 &&
             session.flow.turn_state == GAME_TURN_STATE_MOVE; guard++) {
            now += 20;
            assert(rich4_app_update_turn_movement(
                &session, &map, 20, now, &movement));
        }
        assert(session.flow.turn_state == GAME_TURN_STATE_ROUTE_EVENTS);
        assert(player->node == next);
        assert(session.flow.steps_remaining == 0);
        assert(session.flow.route_facts.count == 1);
        assert(session.flow.route_facts.events[0].phase == ROUTE_EVENT_PHASE_STOP);
        assert(session.flow.route_facts.events[0].node == next);
        session.state.turn_rotation_index = target - 1;
        assert(rich4_app_complete_route_event_turn(
            &session, &map, now + 100, NULL));
        now += 100;
    }
    assert(player->days_tortoise_walking == 0);
    assert(rich4_turn_start_begin(&session, now));
    assert(rich4_dice_update(&session, &map, 0, 20, 30, 10, now));
    assert(session.presentation.dice.popup_active);
    assert(session.presentation.dice.count == (remote ? 1 : traffic + 1));
    assert(player->remote_dice_value == 0);
    if (remote) assert(session.flow.dice_value == remote);
}

static void per_player_time_machine_restore(void)
{
    static GameSession session;
    GameEventQueue events;
    GameCommand command;
    const GameState *snapshot_state;
    const GameFlowState *snapshot_flow;
    const GameActorPresentationState *snapshot_actors;
    const GameMapDynamicObjectState *snapshot_dynamic_objects;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 2;
    session.state.current_player = 1;
    session.state.players[0].play_state = 1;
    session.state.players[1].play_state = 1;
    session.state.players[1].cash = 1200;
    session.state.players[1].tool_inventory[9] = 1;
    session.state.ai_rng_state = 17;
    session.presentation.map_direction = 3;
    session.presentation.actors[1].direction = 2;
    session.presentation.dynamic_objects[0].type = 4;
    session.presentation.dynamic_objects[0].countdown = 3;
    assert(rich4_app_time_machine_capture(&session, 1));
    assert(!session.time_machine_valid);
    assert(rich4_app_time_machine_get(
        &session, 1, &snapshot_state, &snapshot_flow,
        &snapshot_actors, &snapshot_dynamic_objects));
    assert(snapshot_state->players[1].cash == 1200);

    session.state.players[1].cash = 75;
    session.state.ai_rng_state = 99;
    session.presentation.map_direction = 7;
    session.presentation.actors[1].direction = 6;
    session.presentation.dynamic_objects[0].countdown = 9;
    command = rich4_game_command_use_tool(
        GAME_COMMAND_SOURCE_HUMAN, 1,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 9), 0,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1));
    rich4_game_event_queue_reset(&events);
    assert(rich4_app_apply_game_command(&session, &command, &events));
    assert(session.state.current_player == 1);
    assert(session.state.players[1].cash == 1200);
    assert(session.state.players[1].tool_inventory[9] == 0);
    assert(session.state.ai_rng_state == 99);
    assert(session.presentation.map_direction == 7);
    assert(session.presentation.actors[1].direction == 2);
    assert(session.presentation.dynamic_objects[0].type == 4);
    assert(session.presentation.dynamic_objects[0].countdown == 3);
}

int main(void)
{
    GameSession session;
    unsigned char mask[GAME_DICE_STATUS_W * GAME_DICE_STATUS_H];

    per_player_time_machine_restore();

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].ndices = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].is_ai = 0;
    session.state.players[0].remote_dice_value = 4;
    session.flow.turn_flow_active = 1;
    session.flow.turn_state = 2;

    session.presentation.god_acquisition.active = 1;
    session.presentation.god_acquisition.money_pending = 1;
    rich4_dice_begin_roll(&session, 900);
    assert(rich4_dice_update(&session, NULL, 0, 30, 1, 10, 900));
    assert(!session.presentation.dice.popup_active);
    assert(!session.time_machine_valid);
    assert(session.state.players[0].remote_dice_value == 4);
    session.presentation.god_acquisition.active = 0;
    session.presentation.god_acquisition.money_pending = 0;
    session.state.players[0].cash = 731;
    assert(rich4_dice_update(&session, NULL, 0, 30, 1, 10, 1000));
    assert(session.time_machine_valid);
    assert(session.time_machine_state.players[0].cash == 731);
    assert(!session.time_machine_presentation.god_acquisition.money_pending);
    assert(session.presentation.dice.popup_active);
    assert(session.presentation.dice.faces[0] == 4);
    assert(session.flow.dice_value == 4);
    assert(session.state.players[0].remote_dice_value == 0);

    assert(rich4_dice_update(&session, NULL, 0, 30, 1, 10, 1010));
    assert(session.presentation.dice.result_visible);
    assert(rich4_dice_update(&session, NULL, 0, 30, 1, 10, 3011));
    assert(!session.presentation.dice.popup_active);
    /* A null map is an invalid route handoff, so the app flow consumes the
     * movement request without leaving stale steps behind. */
    assert(session.flow.steps_remaining == 0);

    /* A human click starts the turn-flow event first.  The SDL/app bridge
     * must consume it so the following updates reach the original 0x419572
     * action + Panel[4..6] dice animation sequence. */
    memset(&session, 0, sizeof(session));
    memset(mask, 0, sizeof(mask));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].play_state = 1;
    session.state.players[0].ndices = 1;
    session.presentation.dice.status_visible = 1;
    session.presentation.dice.status_x = 10;
    session.presentation.dice.status_y = 20;
    mask[0] = 3;
    assert(rich4_dice_status_mouse_down(
        &session, mask, sizeof(mask), 10, 20, 4000) == 2);
    assert(session.flow.turn_flow_active);
    assert(session.flow.turn_state == GAME_TURN_STATE_PREROLL);
    assert(!session.presentation.dice.status_visible);
    assert(rich4_dice_update(&session, NULL, 0, 30, 3, 10, 4000));
    assert(session.presentation.dice.popup_active);
    assert(session.presentation.dice.animation_frame == 0);
    assert(rich4_dice_update(&session, NULL, 0, 30, 3, 10, 4010));
    assert(session.presentation.dice.animation_frame == 1);
    assert(!session.presentation.dice.result_visible);
    assert(rich4_dice_update(&session, NULL, 0, 30, 3, 10, 4020));
    assert(session.presentation.dice.animation_frame == 2);
    assert(!session.presentation.dice.result_visible);
    assert(rich4_dice_update(&session, NULL, 0, 30, 3, 10, 4030));
    assert(session.presentation.dice.animation_frame == 3);
    assert(session.presentation.dice.result_visible);

    session.flow.turn_state = GAME_TURN_STATE_IDLE;
    session.flow.turn_flow_active = 0;
    session.presentation.dice.popup_active = 0;
    session.state.players[0].traffic_method = 1;
    session.presentation.dice.status_x = 10;
    session.presentation.dice.status_y = 20;
    rich4_dice_show_action_panel(&session, 100, 1, 1);
    assert(session.presentation.dice.status_visible);
    assert(rich4_dice_next_count(&session) == 2);
    session.presentation.dice.status_dragging = 1;
    session.presentation.dice.status_drag_offset_x = 2;
    session.presentation.dice.status_drag_offset_y = 3;
    rich4_dice_update_status_drag(&session, SCREEN_W, SCREEN_H);
    assert(session.presentation.dice.status_x ==
           SCREEN_W - GAME_DICE_STATUS_W);
    assert(session.presentation.dice.status_y ==
           SCREEN_H - GAME_DICE_STATUS_H);
    assert(session.presentation.dice.status_dragging);
    rich4_dice_release_status_drag(&session);
    assert(!session.presentation.dice.status_dragging);
    session.presentation.dice.cache_reset_pending = 1;
    assert(rich4_dice_consume_cache_reset(&session));
    assert(!rich4_dice_consume_cache_reset(&session));
    session.presentation.dice.status_tick = 100;
    session.presentation.dice.status_variant = 0;
    rich4_dice_update_status_blink(&session, 600);
    assert(session.presentation.dice.status_variant == 1);
    rich4_dice_clear_action_panel_flags(&session);
    assert(!session.presentation.dice.status_visible);

    for (int tortoise = 0; tortoise <= 1; tortoise++) {
        news_stopping_turn(16, 0, tortoise);
        news_stopping_turn(16, 1, tortoise);
        news_stopping_turn(17, 0, tortoise);
        news_stopping_turn(17, 1, tortoise);
    }
    for (int self = 0; self <= 1; self++)
        for (int ai = 0; ai <= 1; ai++)
            for (int traffic = 0; traffic <= 2; traffic++)
                for (int remote = 0; remote <= 4; remote += 4)
                    tortoise_card_turns(self, ai, traffic, remote);
    puts("app dice flow: ok");
    return 0;
}
