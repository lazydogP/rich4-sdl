#ifndef RICH4_CORE_PLAYER_STATUS_H
#define RICH4_CORE_PLAYER_STATUS_H

#include "core/game_state.h"
#include "core/game_event.h"

typedef enum PlayerStatusDomainEvent {
    PLAYER_STATUS_EVENT_NONE = 0,
    PLAYER_STATUS_EVENT_ACTOR_REFRESH = 1 << 0,
    PLAYER_STATUS_EVENT_SCENE_REFRESH = 1 << 1,
    PLAYER_STATUS_EVENT_TEXT = 1 << 2,
    PLAYER_STATUS_EVENT_VOICE = 1 << 3
} PlayerStatusDomainEvent;

typedef struct PlayerStatusDecision {
    int result;
    unsigned int events;
    int status_slot;
} PlayerStatusDecision;

int rich4_player_status_evaluate(const GameState *state,
                                 int player_index,
                                 int a1,
                                 PlayerStatusDecision *out_decision);
int rich4_player_status_evaluate_events(const GameState *state,
                                        int player_index,
                                        int a1,
                                        PlayerStatusDecision *out_decision,
                                        GameEventQueue *events);

#endif
