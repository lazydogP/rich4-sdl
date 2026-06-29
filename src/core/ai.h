#ifndef RICH4_CORE_AI_H
#define RICH4_CORE_AI_H

#include "core/card.h"
#include "core/game_state.h"
#include "core/rng.h"
#include "core/tool.h"

typedef enum GameAiPersonality {
    GAME_AI_GOOD_CHILD = 0,
    GAME_AI_ORDINARY = 1,
    GAME_AI_SCHEMER = 2,
    GAME_AI_PERSONALITY_COUNT = 3
} GameAiPersonality;

typedef enum GameAiPhase {
    GAME_AI_PHASE_STOCK_BUY = 0,
    GAME_AI_PHASE_STOCK_SELL,
    GAME_AI_PHASE_FUNDS,
    GAME_AI_PHASE_CARD_OR_TOOL,
    GAME_AI_PHASE_PROPERTY_UPGRADE,
    GAME_AI_PHASE_MOVE,
    GAME_AI_PHASE_DONE
} GameAiPhase;

typedef enum GameAiCandidateResult {
    GAME_AI_CANDIDATE_SELECTED = 1,
    GAME_AI_CANDIDATE_ELIGIBLE,
    GAME_AI_CANDIDATE_DISABLED,
    GAME_AI_CANDIDATE_NO_INVENTORY,
    GAME_AI_CANDIDATE_DIFFICULTY,
    GAME_AI_CANDIDATE_RANDOM_GATE,
    GAME_AI_CANDIDATE_RULE_REJECTED,
    GAME_AI_CANDIDATE_DEFERRED_TARGET,
    GAME_AI_CANDIDATE_MARKET_CLOSED,
    GAME_AI_CANDIDATE_NO_BUDGET,
    GAME_AI_CANDIDATE_NO_SCORE,
    GAME_AI_CANDIDATE_NOT_OWNED,
    GAME_AI_CANDIDATE_NOT_UPGRADABLE
} GameAiCandidateResult;

typedef struct GameAiCandidateTrace {
    GameDomainRef subject;
    GameDomainRef target;
    GameAiCandidateResult result;
    int score;
    int value;
} GameAiCandidateTrace;

#define GAME_AI_TRACE_CAPACITY 64

/* Original 0x48BE58 is session scratch, not a saved player field. The sell
 * latch carries 0x42C79F's local distress mode across dispatched sales. */
typedef struct GameAiSelectionMemory {
    uint32_t card_target_raw;
    int stock_distress;
} GameAiSelectionMemory;

typedef struct GameAiDecision {
    GameAiPhase phase;
    GameAiPhase next_phase;
    int player;
    GameAiPersonality personality;
    uint32_t rng_before;
    uint32_t rng_after;
    int has_command;
    GameAiSelectionMemory selection_after;
    GameCommand command;
    int candidate_count;
    int candidate_overflow;
    GameAiCandidateTrace candidates[GAME_AI_TRACE_CAPACITY];
} GameAiDecision;

/* Map-derived targets stay outside GameState.  The app layer computes these
 * from the live dynamic-object presentation and passes the validated result
 * into the pure selector. */
typedef struct GameAiMapTargetContext {
    GameDomainRef nearest_dynamic_god;
    int nearest_dynamic_god_type;
} GameAiMapTargetContext;

typedef struct GameAiGlobalObjectProjection {
    uint16_t raw_id;
    int screen_x;
    int screen_y;
} GameAiGlobalObjectProjection;

/* Optional map projection used by original global object-table scans. The
 * app derives it from the active map viewport and visible render-object queue;
 * omitting the object table retains the typed-coordinate fallback for
 * synthetic/core-only states. */
typedef struct GameAiGlobalProjection {
    int center_world_x;
    int center_world_y;
    int direction;
    unsigned int positioned_player_mask;
    unsigned int visible_player_mask;
    int player_world_x[GAME_MAX_PLAYERS];
    int player_world_y[GAME_MAX_PLAYERS];
    int object_table_available;
    const GameAiGlobalObjectProjection *objects;
    int object_count;
    /* Map-derived land-name groups used by handlers that compare raw map
     * record names. -1 marks a name that cannot be safely decoded. */
    const int *land_name_groups;
    int land_name_group_count;
    /* Live unattached non-tool objects, including nodes outside the view.
     * Original route +38 holds their dynamic-object slot. */
    const uint16_t *occupied_dynamic_nodes;
    int occupied_dynamic_node_count;
} GameAiGlobalProjection;

/* Shared purchase reserve used by land and facility inspection. */
int rich4_ai_purchase_allowed(const GameState *state, int actor, int price);
/* 0x4221C0: five-node preview before the ordinary AI roll. */
int rich4_ai_select_dice_count(const GameState *state, int player,
                              uint32_t rng_before, int *out_count,
                              uint32_t *out_rng_after);

/* Pure selector: it never writes GameState.  The caller persists rng_after and
 * submits command through the same dispatcher used by human commands. */
int rich4_ai_select_command(const GameState *state, int player,
                            GameAiPhase phase, uint32_t rng_state,
                            GameAiDecision *out);
int rich4_ai_select_command_with_map_context(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out);
int rich4_ai_select_command_with_map_context_and_projection(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context,
    const GameAiGlobalProjection *projection, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out);
int rich4_ai_select_command_with_selection_memory(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context,
    const GameAiGlobalProjection *projection,
    const GameAiSelectionMemory *selection_before, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out);
int rich4_ai_select_command_with_map(
    const GameState *state, GameDomainRef current_property,
    const uint8_t *player_on_map, int player,
    GameAiPhase phase, uint32_t rng_state, GameAiDecision *out);

const char *rich4_ai_personality_name(GameAiPersonality personality);

#endif
