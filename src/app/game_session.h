#ifndef RICH4_APP_GAME_SESSION_H
#define RICH4_APP_GAME_SESSION_H

/* App-owned session aggregate. It owns deterministic domain state, flow
 * facts, and transient presentation state; SDL resources and render caches
 * remain borrowed explicitly by platform seams. Evidence/tests: A12.1. */
#include <stdint.h>

#include "app/audio_event.h"
#include "app/game_presentation.h"
#include "core/game_constants.h"
#include "core/ai.h"
#include "core/game_event.h"
#include "core/game_flow_state.h"
#include "core/special_actor_action.h"
#include "data/character_speech.h"
#include "debug/game_debug_trace.h"

/* WinMain consumes the original 0x46CAF8 latch only after the bankruptcy
 * animation/auction boundary.  Keep the SDL equivalent typed and transient;
 * it is not part of GameState or the save codec. */
typedef enum Rich4AppLifecycleCode {
    RICH4_APP_LIFECYCLE_NONE = 0,
    RICH4_APP_LIFECYCLE_OUTER_MENU = 1,
    RICH4_APP_LIFECYCLE_RESTART_GAME = 2,
    RICH4_APP_LIFECYCLE_OUTER_MENU_AFTER_JUMP = 3,
    RICH4_APP_LIFECYCLE_LOAD_GAME = 4
} Rich4AppLifecycleCode;

typedef struct Rich4AppLifecycleRequest {
    Rich4AppLifecycleCode code;
    int player;
} Rich4AppLifecycleRequest;

typedef struct GameSession {
    GameState state;
    GameAiSelectionMemory ai_selection;
    GameDebugTrace debug;
    GameFlowState flow;
    GamePresentationState presentation;
    GameState time_machine_state;
    GameFlowState time_machine_flow;
    GamePresentationState time_machine_presentation;
    int time_machine_valid;
    /* One transient app/platform audio handoff queue per live session. */
    Rich4AudioEventQueue audio_events;
    /* Follow-up domain events produced while a property presentation commits.
     * This queue is transient and is drained by route_event_flow. */
    GameEventQueue deferred_events;
    /* 0x41CF67 calendar modals return before 0x41CCxx player status ticks. */
    GameEventQueue pending_turn_events;
    /* Bankruptcy cleanup is committed synchronously by the ledger, but the
     * original blocks on its animation before starting auctions and showing
     * the terminal ending.  These queues are transient presentation handoff
     * state, never part of a save file. */
    GameEventQueue bankruptcy_auction_events;
    GameEventQueue bankruptcy_terminal_events;
    Rich4AppLifecycleRequest lifecycle;
    /* GetTickCount minus the renderer clock; transient across save/load. */
    uint32_t rng_clock_offset;
} GameSession;

/* Lifecycle reset for the app-owned aggregate. SDL resources are owned by
 * their platform bootstrap and are intentionally outside this operation. */
void rich4_app_reset_session(GameSession *session);
void rich4_app_sync_rng_clock(GameSession *session,
    uint32_t original_tick, uint32_t frame_tick);

/* The original ending confirmation enters a load selector. Select a local
 * human as the command actor while it is open, then restore on cancel. */
int rich4_app_begin_ending_load(GameSession *session,
                                int *out_previous_player);
void rich4_app_cancel_ending_load(GameSession *session,
                                  int previous_player);
void rich4_app_set_ending_load_previous_player(GameSession *session,
                                                int previous_player);

/* Queue/take the one outer-loop transition produced by a terminal bankruptcy
 * branch.  First-writer-wins matches the original single-byte latch. */
void rich4_app_request_lifecycle(GameSession *session,
                                 Rich4AppLifecycleCode code, int player);
int rich4_app_take_lifecycle(GameSession *session,
                             Rich4AppLifecycleRequest *out_request);

#endif
