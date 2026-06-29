#ifndef RICH4_APP_GAME_EVENT_PRESENTATION_SNAPSHOT_H
#define RICH4_APP_GAME_EVENT_PRESENTATION_SNAPSHOT_H

#include "app/game_presentation.h"

/* App-owned read-only input for one-shot event overlays.  The builder does
 * not accept GameSession, and the snapshot borrows same-frame state instead
 * of copying the two large aggregates.  The platform renderers cannot reach
 * unrelated lifetime, resource or input state through this boundary. */
typedef struct Rich4GameEventPresentationSnapshot {
    const GameState *state;
    const GamePresentationState *presentation;
} Rich4GameEventPresentationSnapshot;

Rich4GameEventPresentationSnapshot
rich4_app_build_game_event_presentation_snapshot(
    const GameState *state, const GamePresentationState *presentation);

#endif
