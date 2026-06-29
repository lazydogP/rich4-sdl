#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/player_status.h"
#include "core/status_countdown.h"

int main(void)
{
    GameState state;
    GamePlayerState *player;
    StatusTickResult tick;
    StatusEventBatch batch;
    GameEventQueue events;
    GameEvent event;
    PlayerStatusDecision decision;
    int expected[] = {50, 51, 52, 53, 54, 55, 57, 56, 59, 60, 61, 62};
    int i;
    memset(&state, 0, sizeof(state));
    state.player_count = 2;
    player = &state.players[0];
    player->delayed_status_block = 1;
    player->days_disappearing = 0x41;
    player->days_in_prison = 2;
    player->days_in_hospital = 2;
    player->days_sleeping = 1;
    player->days_sleep_walking = 1;
    player->days_tortoise_walking = 1;
    player->days_stopping = 1;
    player->days_rejected_by_bank = 1;
    player->days_bank_no_loans = 1;
    player->god_status_days = 1;
    player->days_assurance = 1;
    rich4_game_event_queue_reset(&events);
    rich4_status_tick_player_events(player, 0, &tick, &events);
    assert(tick.count == 9);
    assert(tick.offsets[0] == 50 && tick.offsets[1] == 51 &&
           tick.offsets[2] == 52 && tick.offsets[3] == 53);
    assert(player->delayed_status_block == GAME_STATUS_PENDING_BIT);
    assert(player->days_disappearing == (GAME_STATUS_PENDING_BIT | 0x40));
    assert(player->days_sleeping == 1 && player->days_sleep_walking == 1 &&
           player->days_tortoise_walking == 1);
    assert(tick.god_relation_tick_requested == 1);
    assert(events.count == 10);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_STATUS_EXPIRED && event.player == 0 &&
           rich4_game_event_subject(&event) == 50);
    player->delayed_status_block = 2;
    player->days_disappearing = 0;
    player->days_in_prison = 0;
    player->days_in_hospital = 0;
    player->days_sleeping = GAME_STATUS_PENDING_BIT;
    assert(rich4_status_consume_pending(&state, 0, &batch) == 5);
    assert(player->days_sleeping == GAME_STATUS_PENDING_BIT);
    assert(batch.events[0].original_offset == 56);
    player->delayed_status_block = GAME_STATUS_PENDING_BIT;
    player->days_disappearing = GAME_STATUS_PENDING_BIT;
    player->days_in_prison = GAME_STATUS_PENDING_BIT;
    player->days_in_hospital = GAME_STATUS_PENDING_BIT;
    player->days_sleeping = GAME_STATUS_PENDING_BIT;
    player->days_sleep_walking = GAME_STATUS_PENDING_BIT;
    player->days_tortoise_walking = GAME_STATUS_PENDING_BIT;
    player->days_stopping = GAME_STATUS_PENDING_BIT;
    player->days_rejected_by_bank = GAME_STATUS_PENDING_BIT;
    player->days_bank_no_loans = GAME_STATUS_PENDING_BIT;
    player->god_status_days = GAME_STATUS_PENDING_BIT;
    player->days_assurance = GAME_STATUS_PENDING_BIT;
    player->play_state = GAME_PLAYER_PLAY_STATE_FROZEN | 2;
    player->saved_traffic_method = 2;
    player->saved_ndices = 3;
    player->god_info = 9;
    player->god_type = 2;
    player->linked_player = 1;
    state.players[1].god_info = 10;
    state.players[1].god_type = 1;
    state.players[1].linked_player = 0;
    state.players[1].god_status_days = 3;
    rich4_game_event_queue_reset(&events);
    assert(rich4_status_consume_pending_events(&state, 0, &batch, &events) == 12);
    assert(events.count == 20);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_STATUS_EFFECT_RESOLVED &&
           rich4_game_event_subject(&event) == 50 &&
           rich4_game_event_value(&event) == STATUS_HANDLER_40D6BE &&
           rich4_game_event_value2(&event) == STATUS_EFFECT_APPLIED);
    for (i = 0; i < 12; i++) assert(batch.events[i].original_offset == expected[i]);
    assert(batch.events[0].handler == STATUS_HANDLER_40D6BE);
    assert(batch.events[0].effect_result == STATUS_EFFECT_APPLIED);
    assert(batch.events[6].handler == STATUS_HANDLER_CLEAR_ONLY);
    assert((player->play_state & GAME_PLAYER_PLAY_STATE_FROZEN) == 0);
    assert((player->play_state & GAME_PLAYER_PLAY_STATE_STATUS_RETURN) != 0);
    assert(player->traffic_method == 2 && player->ndices == 3);
    assert(player->god_info == 9 && state.players[1].god_info == 10);
    assert(player->linked_player == -1 && state.players[1].linked_player == -1);
    assert(!rich4_status_player_has_pending(player));

    /* A one-day confinement is still a blocked, visible "1 day" turn.
     * Only the next status pass consumes 0x80 and releases the actor. */
    memset(player, 0, sizeof(*player));
    player->play_state = 1;
    player->days_in_prison = 1;
    player->confinement_actor_type = GAME_CONFINEMENT_PRISON;
    rich4_status_tick_player(player, &tick);
    assert(player->days_in_prison == GAME_STATUS_PENDING_BIT);
    assert(rich4_player_status_evaluate(&state, 0, 0, &decision));
    assert(decision.result == 0 && decision.status_slot == 52 &&
           ((player->days_in_prison & 0x7f) + 1) == 1);
    assert(rich4_status_consume_pending(&state, 0, &batch) == 1);
    assert(player->days_in_prison == 0 &&
           player->confinement_actor_type == GAME_CONFINEMENT_NONE);
    puts("status countdown: ok");
    return 0;
}
