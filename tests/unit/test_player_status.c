#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/player_status.h"

static PlayerStatusDecision evaluate(GameState *state, int player, int a1)
{
    PlayerStatusDecision decision;
    assert(rich4_player_status_evaluate(state, player, a1, &decision));
    return decision;
}

int main(void)
{
    GameState state;
    PlayerStatusDecision decision;
    GameEventQueue events;
    GameEvent event;
    int i;
    memset(&state, 0, sizeof(state));
    state.players[0].play_state = 2;
    assert(evaluate(&state, 0, 0).result == 2);
    assert(evaluate(&state, 0, 1).result == 2);
    state.players[0].play_state = 0;
    assert(evaluate(&state, 0, 0).result == 0);
    assert(evaluate(&state, 0, 1).result == 0);
    state.players[0].play_state = 2;
    for (i = 0; i < 4; i++) {
        int *fields[4] = {&state.players[0].delayed_status_block,
                          &state.players[0].days_disappearing,
                          &state.players[0].days_in_prison,
                          &state.players[0].days_in_hospital};
        *fields[i] = 3;
        decision = evaluate(&state, 0, 0);
        assert(decision.result == 0 && decision.status_slot == 50 + i);
        assert((decision.events & PLAYER_STATUS_EVENT_TEXT) != 0);
        if (i >= 2) assert((decision.events & PLAYER_STATUS_EVENT_VOICE) != 0);
        assert(evaluate(&state, 0, 1).result == 0);
        *fields[i] = 0;
    }
    state.players[0].days_sleeping = 2;
    decision = evaluate(&state, 0, 0);
    assert(decision.result == 0 && decision.status_slot == 54);
    assert((decision.events & PLAYER_STATUS_EVENT_VOICE) != 0);
    assert(evaluate(&state, 0, 1).result == 0);
    state.players[0].days_sleeping = 0;
    state.players[0].days_sleep_walking = 2;
    rich4_game_event_queue_reset(&events);
    assert(rich4_player_status_evaluate_events(&state, 0, 0, &decision, &events));
    assert(decision.result == -1 && decision.status_slot == 55);
    assert(decision.events == PLAYER_STATUS_EVENT_ACTOR_REFRESH);
    assert(rich4_game_event_queue_pop(&events, &event));
    assert(event.type == GAME_EVENT_ACTOR_REFRESH_REQUESTED &&
           event.player == 0 && rich4_game_event_subject(&event) == 55 &&
           events.count == 0);
    assert(evaluate(&state, 0, 1).result == 2);
    state.players[0].days_sleep_walking = 0;
    state.players[0].play_state = 0x32;
    state.players[0].days_sleeping = 1;
    decision = evaluate(&state, 0, 0);
    assert(decision.result == 0 &&
           decision.events == PLAYER_STATUS_EVENT_ACTOR_REFRESH);
    state.players[0].days_sleeping = 0;
    assert(evaluate(&state, 0, 1).result == 0);
    memset(state.special_player_status, 0, sizeof(state.special_player_status));
    for (i = 4; i <= 8; i++) {
        assert(evaluate(&state, i, 0).result == 2);
        assert(evaluate(&state, i, 1).result == 0);
        state.special_player_status[i - 4][1] = 1;
        assert(evaluate(&state, i, 0).result == 0);
        state.special_player_status[i - 4][1] = 0;
    }
    puts("player status 0x40C912: ok");
    return 0;
}
