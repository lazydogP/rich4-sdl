#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/dice_flow.h"
#include "app/turn_movement_flow.h"
#include "app/turn_rotation_flow.h"
#include "app/turn_start_flow.h"
#include "core/special_event.h"
#include "view/view_constants.h"

static void news_stopping_turn(int news_id, int is_ai)
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
    assert(session.time_machine_valid);
    assert(session.time_machine_state.players[0].days_stopping ==
           GAME_STATUS_PENDING_BIT);
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

int main(void)
{
    GameSession session;
    unsigned char mask[GAME_DICE_STATUS_W * GAME_DICE_STATUS_H];

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.current_player = 0;
    session.state.players[0].ndices = 1;
    session.state.players[0].play_state = 2;
    session.state.players[0].is_ai = 1;
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

    news_stopping_turn(16, 0);
    news_stopping_turn(16, 1);
    news_stopping_turn(17, 0);
    news_stopping_turn(17, 1);
    puts("app dice flow: ok");
    return 0;
}
