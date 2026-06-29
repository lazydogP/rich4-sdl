#ifndef RICH4_APP_GAME_COMMAND_FLOW_H
#define RICH4_APP_GAME_COMMAND_FLOW_H

/* App command flow owns the platform-neutral handoff from an AppCommand to
 * deterministic core dispatch. It borrows the app session and event queue;
 * SDL presentation side effects remain in platform adapters. Evidence/tests:
 * A5/A6, test_game_dispatch. */
#include "app/app_command.h"
#include "app/game_session.h"
#include "core/game_event.h"

int rich4_app_dispatch_game_command(GameSession *session,
                                    const Rich4AppCommand *command,
                                    GameEventQueue *events);

/* Full app boundary used by platform adapters: validation, app-owned gates,
 * time-machine handling and core dispatch all receive the typed AppCommand. */
int rich4_app_apply_command(GameSession *session,
                            const Rich4AppCommand *command,
                            GameEventQueue *events);

/* Applies one validated game command to the app-owned session and returns
 * core events for the platform adapter to present.  The app owns command
 * gating, time-machine restore/capture, and deterministic dispatch. */
int rich4_app_apply_game_command(GameSession *session,
                                 const GameCommand *command,
                                 GameEventQueue *events);

/* Starts the app-owned presentation state for an AI card command after the
 * platform adapter has verified that the corresponding Data.MKF animation
 * exists.  The resource/frame values are data, not SDL handles. */
void rich4_app_sync_ai_card_animation(GameSession *session,
                                      const GameCommand *command,
                                      int accepted,
                                      int resource,
                                      int frame_count,
                                      uint32_t now);

#endif
