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

typedef struct GameTimeMachineSnapshot {
    GameState state;
    GameFlowState flow;
    GameActorPresentationState actors[GAME_MAX_PLAYERS];
    GameMapDynamicObjectState dynamic_objects[GAME_MAP_DYNAMIC_OBJECT_COUNT];
    int valid;
} GameTimeMachineSnapshot;

typedef struct GameMissileBuilding {
    int8_t owner;
    uint8_t type, level;
} GameMissileBuilding;

typedef struct GameMissileScene {
    int world_x, world_y;
    GameMissileBuilding lands[GAME_MAX_MAP_PROPERTIES], facilities[GAME_MAX_MAP_PROPERTIES];
    int special_node[GAME_SPECIAL_PLAYER_COUNT], special_play_state[GAME_SPECIAL_PLAYER_COUNT];
    GamePlayerState players[GAME_MAX_PLAYERS];
    GameMapDynamicObjectState objects[GAME_MAP_DYNAMIC_OBJECT_COUNT];
} GameMissileScene;

typedef struct GameSession {
    GameState state;
    GameAiSelectionMemory ai_selection;
    GameDebugTrace debug;
    GameFlowState flow;
    GamePresentationState presentation;
    /* 0x90001 keeps the target map beneath the first eight FLC frames. */
    int missile_scene_active;
    GameMissileScene missile_scene;
    GameState time_machine_state;
    GameFlowState time_machine_flow;
    GamePresentationState time_machine_presentation;
    int time_machine_valid;
    /* The original 0x44808A/0x448544 pair indexes one snapshot per player.
     * Keep player zero in the legacy fields above so formats 4..11 can still
     * be decoded, and store players 1..3 in the additional slots. */
    GameTimeMachineSnapshot time_machine_other[GAME_MAX_PLAYERS - 1];
    /* Original 0x4990F0..0x4990F3.  0x4075C1 marks the selected map after
     * a code-2/code-3 ending; WinMain preserves these four bytes across the
     * code-2 next-game setup and clears them on return to the outer menu.
     * They belong to the main save only, not to a time-machine snapshot. */
    uint8_t map_access_flags[GAME_MAPS_PER_STAGE];
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
    /* 0x401981 calls 0x454D91(0) when a decoded game enters the live loop.
     * Keep that one-shot music handoff outside the save codec. */
    int loaded_music_restart_pending;
} GameSession;

/* Lifecycle reset for the app-owned aggregate. SDL resources are owned by
 * their platform bootstrap and are intentionally outside this operation. */
void rich4_app_reset_session(GameSession *session);
/* Code 2 returns directly to the next map/character setup.  Reset the match
 * while retaining the original process-lifetime four-map progress. */
void rich4_app_reset_session_for_next_map(GameSession *session);
int rich4_app_mark_map_access(GameSession *session, int map);
unsigned int rich4_app_map_access_mask(const GameSession *session);
int rich4_app_all_maps_accessed(const GameSession *session);
/* 0x4065E4/0x4065FC: the single-human ending screen accepts only an
 * unfinished map row inside x=201..439, y=281..439. */
int rich4_app_ending_next_map_at(const GameSession *session, int x, int y);
/* 0x406935 commits the accepted row before the ten-tick transition. */
int rich4_app_commit_ending_next_map(GameSession *session, int x, int y);
void rich4_app_sync_rng_clock(GameSession *session,
    uint32_t original_tick, uint32_t frame_tick);

/* 0x44808A captures only a live human player's slot.  The getter returns
 * false for an absent snapshot and exposes no mutable ownership. */
int rich4_app_time_machine_capture(GameSession *session, int player);
int rich4_app_time_machine_get(const GameSession *session, int player,
                               const GameState **out_state,
                               const GameFlowState **out_flow,
                               const GameActorPresentationState **out_actors,
                               const GameMapDynamicObjectState **out_dynamic_objects);
int rich4_app_time_machine_restore(GameSession *session, int player);
int rich4_app_time_machine_rebind_all(GameSession *session);

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
