#ifndef RICH4_APP_GAME_UPDATE_FLOW_H
#define RICH4_APP_GAME_UPDATE_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "app/turn_start_flow.h"

typedef enum Rich4AppGameUpdateStage {
    RICH4_APP_GAME_UPDATE_IDLE = 0,
    RICH4_APP_GAME_UPDATE_ROUTE_EVENTS,
    RICH4_APP_GAME_UPDATE_FIRST_LANDING,
    RICH4_APP_GAME_UPDATE_DICE,
    RICH4_APP_GAME_UPDATE_MOVEMENT
} Rich4AppGameUpdateStage;

typedef struct Rich4AppGameUpdatePlan {
    Rich4AppGameUpdateStage stage;
    Rich4TurnStartRequest turn_start;
} Rich4AppGameUpdatePlan;

typedef int (*Rich4AppGameUpdateCallback)(void *context);
typedef void (*Rich4AppTurnStartCallback)(
    void *context, const Rich4TurnStartRequest *request);

/* The app owns the frame ordering and stage decision.  Platform services
 * remain callbacks because route presentation, resource-backed dice timing,
 * and animation/audio effects still need SDL-owned inputs during migration. */
typedef struct Rich4AppGameUpdateServices {
    void *presentation_context;
    void *turn_start_context;
    void *route_context;
    void *first_landing_context;
    void *dice_context;
    void *movement_context;
    Rich4AppGameUpdateCallback update_presentation;
    Rich4AppTurnStartCallback apply_turn_start;
    Rich4AppGameUpdateCallback update_route_events;
    Rich4AppGameUpdateCallback update_first_landing;
    Rich4AppGameUpdateCallback update_dice;
    Rich4AppGameUpdateCallback update_movement;
} Rich4AppGameUpdateServices;

/* Selects the next mutually exclusive game-flow stage from app-owned state.
 * Platform code consumes the result to supply resource-backed adapters. */
Rich4AppGameUpdateStage rich4_app_game_update_stage(
    const GameSession *session);

/* Apply app-owned turn-start semantics once, then select the mutually
 * exclusive stage that remains for this frame. */
int rich4_app_game_update_prepare(
    GameSession *session, uint32_t now, Rich4AppGameUpdatePlan *out_plan);

/* Execute one production frame using the app-owned priority/stage plan. */
int rich4_app_game_update(
    GameSession *session, uint32_t now,
    const Rich4AppGameUpdateServices *services);

#endif
