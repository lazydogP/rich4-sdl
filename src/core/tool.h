#ifndef RICH4_CORE_TOOL_H
#define RICH4_CORE_TOOL_H

#include <stddef.h>
#include <stdint.h>

#include "core/game_event.h"
#include "core/game_state.h"

typedef enum GameToolTargetKind {
    GAME_TOOL_TARGET_NONE = 0,
    GAME_TOOL_TARGET_ROUTE_NODE,
    GAME_TOOL_TARGET_PROPERTY,
    GAME_TOOL_TARGET_AREA,
    GAME_TOOL_TARGET_TRANSFER,
    GAME_TOOL_TARGET_TIME,
    GAME_TOOL_TARGET_DEFERRED
} GameToolTargetKind;

typedef enum GameToolImplementation {
    GAME_TOOL_IMPLEMENTED = 1,
    GAME_TOOL_DEFERRED
} GameToolImplementation;

typedef struct GameToolInfo {
    int original_id;
    const char *name;
    int initial_pool;
    int shop_price;
    int metadata_f6;
    int ai_difficulty;
    GameToolTargetKind target_kind;
    GameToolImplementation implementation;
    uintptr_t handler_slot_address;
    const char *handler_symbol;
    const char *precondition;
    const char *side_effect;
} GameToolInfo;

typedef enum GameToolUseResult {
    GAME_TOOL_USE_INVALID = 0,
    GAME_TOOL_USE_APPLIED,
    GAME_TOOL_USE_DEFERRED,
    GAME_TOOL_USE_RESTORE_REQUIRED,
    GAME_TOOL_USE_UNAVAILABLE
} GameToolUseResult;

typedef enum GameToolTriggerResult {
    GAME_TOOL_TRIGGER_INVALID = -1,
    GAME_TOOL_TRIGGER_NONE = 0,
    GAME_TOOL_TRIGGER_PASS,
    GAME_TOOL_TRIGGER_STOP,
    GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT
} GameToolTriggerResult;

typedef struct GameToolPlayerSnapshot {
    uint8_t inventory[GAME_PLAYER_TOOL_SLOTS];
    int traffic_method;
    int ndices;
    int remote_dice_value;
    int engineering_saved_traffic_method;
    int engineering_saved_ndices;
    int attached_timed_bomb_slot;
} GameToolPlayerSnapshot;

typedef struct GameToolSnapshot {
    uint32_t version;
    uint8_t amount[GAME_TOOL_COUNT];
    int route_node_count;
    uint8_t route_node_placeable[GAME_MAX_MAP_ROUTE_NODES];
    int16_t route_x[GAME_MAX_MAP_ROUTE_NODES];
    int16_t route_y[GAME_MAX_MAP_ROUTE_NODES];
    uint16_t route_next[GAME_MAX_MAP_ROUTE_NODES][4];
    int16_t land_x[GAME_MAX_MAP_PROPERTIES];
    int16_t land_y[GAME_MAX_MAP_PROPERTIES];
    int16_t facility_x[GAME_MAX_MAP_PROPERTIES];
    int16_t facility_y[GAME_MAX_MAP_PROPERTIES];
    struct GameToolPlacedObjectState placed[GAME_TOOL_PLACED_OBJECT_COUNT];
    struct GameToolRobotState robot;
    GameToolPlayerSnapshot players[GAME_MAX_PLAYERS];
} GameToolSnapshot;

#define GAME_TOOL_SNAPSHOT_VERSION 2u

extern const GameToolInfo rich4_tool_catalog[GAME_TOOL_COUNT];

const GameToolInfo *rich4_tool_info(int tool);
int rich4_tool_inventory_count(const GameState *state, int player);
int rich4_tool_set_route_targets(GameState *state, int route_node_count,
                                 const uint8_t *placeable, size_t count);
int rich4_tool_set_map_geometry(
    GameState *state, int route_node_count,
    const int16_t *route_x, const int16_t *route_y,
    const uint16_t (*route_next)[4], int land_count,
    const int16_t *land_x, const int16_t *land_y, int facility_count,
    const int16_t *facility_x, const int16_t *facility_y);
GameToolUseResult rich4_tool_inspect(const GameState *state,
                                     const GameCommand *command);
GameToolUseResult rich4_tool_apply_command(GameState *state,
                                           const GameCommand *command,
                                           GameEventQueue *events);
GameToolUseResult rich4_tool_apply_command_deferred_confinement(GameState *state,
    const GameCommand *command, GameEventQueue *events);
GameToolTriggerResult rich4_tool_trigger_at_node(GameState *state, int player,
                                                  int node, int stopped,
                                                  GameEventQueue *events);
GameToolTriggerResult rich4_tool_advance_step(GameState *state, int player,
                                               int node, int settlement,
                                               GameEventQueue *events);
/* SDL's blocking explosion continuation enters hospital after its reactions. */
GameToolTriggerResult rich4_tool_trigger_at_node_deferred_hospital(
    GameState *state, int player, int node, int stopped, GameEventQueue *events);
GameToolTriggerResult rich4_tool_advance_step_deferred_hospital(
    GameState *state, int player, int node, int settlement, GameEventQueue *events);
GameToolTriggerResult rich4_tool_robot_advance(GameState *state,
                                                GameEventQueue *events);
int rich4_tool_snapshot_export(const GameState *state, GameToolSnapshot *out);
int rich4_tool_snapshot_import(GameState *state, const GameToolSnapshot *in);

#endif
