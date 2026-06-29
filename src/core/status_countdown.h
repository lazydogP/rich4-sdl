#ifndef RICH4_CORE_STATUS_COUNTDOWN_H
#define RICH4_CORE_STATUS_COUNTDOWN_H

#include "core/game_state.h"
#include "core/game_event.h"

typedef enum StatusHandlerFamily {
    STATUS_HANDLER_NONE = 0,
    STATUS_HANDLER_40D6BE,
    STATUS_HANDLER_40D4E5,
    STATUS_HANDLER_43D7BF,
    STATUS_HANDLER_43EE6E,
    STATUS_HANDLER_40B8D8_40B93B,
    STATUS_HANDLER_SLEEPWALK_40B93B,
    STATUS_HANDLER_CLEAR_ONLY,
    STATUS_HANDLER_40CC1A
} StatusHandlerFamily;

typedef enum StatusEffectResult {
    STATUS_EFFECT_APPLIED = 1,
    STATUS_EFFECT_UNIMPLEMENTED_CONFIRMED,
    STATUS_EFFECT_INVALID_STATE
} StatusEffectResult;

typedef struct StatusEvent {
    int original_offset;
    StatusHandlerFamily handler;
    StatusEffectResult effect_result;
} StatusEvent;

typedef struct StatusEventBatch {
    StatusEvent events[GAME_MAX_STATUS_EVENT_RECORDS];
    int count;
} StatusEventBatch;

typedef struct StatusTickResult {
    int offsets[GAME_MAX_STATUS_EVENT_RECORDS];
    int values[GAME_MAX_STATUS_EVENT_RECORDS];
    int count;
    int god_relation_tick_requested;
} StatusTickResult;

void rich4_status_tick_player(GamePlayerState *player, StatusTickResult *out_result);
void rich4_status_tick_player_events(GamePlayerState *player,
                                     int player_index,
                                     StatusTickResult *out_result,
                                     GameEventQueue *events);
int rich4_status_player_has_pending(const GamePlayerState *player);
int rich4_status_consume_pending(GameState *state,
                                 int player_index,
                                 StatusEventBatch *out_batch);
int rich4_status_consume_pending_events(GameState *state,
                                        int player_index,
                                        StatusEventBatch *out_batch,
                                        GameEventQueue *events);

#endif
