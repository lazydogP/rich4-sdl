#ifndef RICH4_CORE_SPECIAL_EVENT_H
#define RICH4_CORE_SPECIAL_EVENT_H

#include <stdint.h>

#include "core/game_event.h"
#include "core/game_state.h"
#include "core/rng.h"

typedef enum GameSpecialEventFamily {
    GAME_SPECIAL_EVENT_NONE = 0,
    GAME_SPECIAL_EVENT_NEWS = 1,
    GAME_SPECIAL_EVENT_FORTUNE = 2,
    GAME_SPECIAL_EVENT_MAGIC_HOUSE = 3
} GameSpecialEventFamily;

typedef enum GameSpecialEventPhase {
    GAME_SPECIAL_PHASE_IDLE = 0,
    GAME_SPECIAL_PHASE_PREPARED = 1,
    GAME_SPECIAL_PHASE_DEFERRED = 2
} GameSpecialEventPhase;

typedef enum GameSpecialDefenseOutcome {
    GAME_SPECIAL_DEFENSE_NONE = 0,
    GAME_SPECIAL_DEFENSE_BLOCKED = 1,
    GAME_SPECIAL_DEFENSE_DOUBLED = 2
} GameSpecialDefenseOutcome;

typedef enum GameSpecialEventResult {
    GAME_SPECIAL_EVENT_APPLIED = 1,
    GAME_SPECIAL_EVENT_PREPARED,
    GAME_SPECIAL_EVENT_CANCELED,
    GAME_SPECIAL_EVENT_NO_CANDIDATE,
    GAME_SPECIAL_EVENT_DEFERRED,
    GAME_SPECIAL_EVENT_NOT_CANCELABLE,
    GAME_SPECIAL_EVENT_LIQUIDATION,
    GAME_SPECIAL_EVENT_BANKRUPTCY,
    GAME_SPECIAL_EVENT_INVALID,
    GAME_SPECIAL_EVENT_EVENT_CAPACITY
} GameSpecialEventResult;

typedef struct GameSpecialEventSnapshot {
    uint32_t version;
    uint8_t news_order[GAME_NEWS_EVENT_COUNT];
    uint8_t fortune_order[GAME_FORTUNE_EVENT_COUNT];
    uint8_t stock_f7[GAME_STOCK_COUNT];
    int news_cursor, fortune_cursor;
    uint32_t rng_state;
    int family, actor, logical_id, resolved_id, resource_id, panel_id;
    int prepared_value, cancelable, phase;
    unsigned int target_mask;
    int player_amount[GAME_MAX_PLAYERS];
} GameSpecialEventSnapshot;

#define GAME_SPECIAL_EVENT_SNAPSHOT_VERSION 1u

void rich4_special_event_sequences_init(GameState *state, Rich4Rng *rng);
int rich4_special_event_select(const uint8_t *order, int count, int *cursor,
                               const uint8_t *eligible, int *out_id);
GameSpecialEventResult rich4_special_event_begin_news(
    GameState *state, int actor, GameEventQueue *events);
/* 0x44913D chooses news 4's center only after the 2400ms page. */
int rich4_special_event_prepare_news_phase1(GameState *state);
typedef struct GameNewsAreaAnchor {
    uint16_t raw_id;
    int screen_x, screen_y; /* 440x440 map viewport, HUD already removed. */
} GameNewsAreaAnchor;
/* 0x409DE7/0x40A45C(100): supply resource-backed render-queue anchors
 * after centering the callback view. Same-pixel IDs are ORed. News 4/20
 * require this context before commit; no world-coordinate approximation. */
int rich4_special_event_apply_news_area(GameState *state,
    const GameNewsAreaAnchor *anchors, int count);
/* Iterate the original row-major pixel scan; start cursor at -1.
 * Co-located raw IDs are ORed, as in 0x40A45C's 440x440 table. */
int rich4_special_event_news_area_next_raw(const GameNewsAreaAnchor *anchors,
    int count, int *cursor, uint16_t *out_raw);
GameSpecialEventResult rich4_special_event_begin_fortune(
    GameState *state, int actor, GameEventQueue *events);
unsigned int rich4_magic_house_action4_eligible(const GameState *state);
unsigned int rich4_magic_house_target_mask(const GameState *state,
                                            int condition);
GameSpecialEventResult rich4_magic_house_choose_condition(
    GameState *state, int *out_condition, unsigned int *out_target_mask);
GameSpecialEventResult rich4_magic_house_apply_action(
    GameState *state, int actor, int condition, int action,
    unsigned int target_mask, GameEventQueue *events);
GameSpecialEventResult rich4_special_event_begin_magic_action4(
    GameState *state, int actor, unsigned int target_mask,
    GameEventQueue *events);
GameSpecialEventResult rich4_special_event_commit(
    GameState *state, GameSpecialDefenseOutcome defense,
    GameEventQueue *events);
/* 0x441210: only the days-based fortunes enter the passive target chain.
 * A human redirect of -1 declines; AI chooses via 0x40D2D3/0x40D31C. */
int rich4_special_event_fortune_has_target(int physical_id);
int rich4_special_event_fortune_choose_redirect(GameState *state, int actor);
/* Returns actual victim, -1 for absolution, -2 on failure (no mutation). */
int rich4_special_event_resolve_passive_target(GameState *state, int actor,
    int redirect, GameEventQueue *events);
GameSpecialEventResult rich4_special_event_commit_resolved_target(
    GameState *state, GameSpecialDefenseOutcome defense, int target,
    GameEventQueue *events, int defer_confinement);
GameSpecialEventResult rich4_special_event_commit_with_target(
    GameState *state, GameSpecialDefenseOutcome defense, int redirect_target,
    GameEventQueue *events, int defer_confinement);
/* 0x44192A(...,0): a human chooses one victim's card slot, or -1 cancels.
 * No random word is consumed. Called while fortune 5 remains prepared. */
int rich4_special_event_fortune_take_card(GameState *state, int victim, int slot);
#define RICH4_SPECIAL_COMMAND_DEFENSE_RESOLVED 0x40000000u
#define RICH4_SPECIAL_COMMAND_TARGET_RESOLVED 0x20000000u
/* Resolves the automatic 0x44B896 modifier branch for the fortune callbacks
 * that consult the player's attached god.  The function advances the
 * special-event RNG only when the original (50,100] range needs its coin flip.
 */
GameSpecialDefenseOutcome rich4_special_event_default_defense(GameState *state);
GameSpecialEventResult rich4_special_event_cancel(GameState *state,
                                                   GameEventQueue *events);
int rich4_special_event_decode_property_target(
    const GameState *state, unsigned int encoded, GameDomainRef *out);
int rich4_special_event_snapshot_export(const GameState *state,
                                        GameSpecialEventSnapshot *out);
int rich4_special_event_snapshot_import(GameState *state,
                                        const GameSpecialEventSnapshot *in);

GameSpecialEventResult rich4_special_event_commit_deferred_confinement(GameState *state, GameSpecialDefenseOutcome defense, GameEventQueue *events);

GameSpecialEventResult rich4_magic_house_apply_action_deferred_confinement(GameState *state, int actor, int condition, int action, unsigned int target_mask, GameEventQueue *events);

#endif
