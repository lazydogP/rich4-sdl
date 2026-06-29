#ifndef RICH4_APP_DYNAMIC_OBJECT_FLOW_H
#define RICH4_APP_DYNAMIC_OBJECT_FLOW_H

#include "app/game_presentation.h"
#include "core/ai.h"
#include "core/game_event.h"
#include "data/map_data.h"
#include "view/game_view_model.h"

void rich4_app_init_dynamic_object_pool(GamePresentationState *presentation);
/* 0x407AD2 -> 0x40AA6C/0x40E033: place the eight dynamic objects created
 * when a new MAPDAT is loaded.  MapData is borrowed and is not mutated. */
void rich4_app_initialize_dynamic_map_objects(
    GamePresentationState *presentation, const MapData *map);
void rich4_app_initialize_dynamic_map_objects_with_rng(
    GamePresentationState *presentation, const MapData *map, uint32_t *rng_state);
int rich4_app_remove_dynamic_object_with_rng(
    GamePresentationState *presentation, const MapData *map, int object_id,
    uint32_t *rng_state);
int rich4_app_reset_monthly_dynamic_objects_with_rng(
    GamePresentationState *presentation, const MapData *map, uint32_t *rng_state);
int rich4_app_robot_clear_dynamic_object_with_rng(
    GamePresentationState *presentation, const MapData *map,
    int node, int previous_node, uint32_t *rng_state);
/* 0x40E14D: remove a one-based-original/zero-based-view slot.  For types
 * 1..12 this also registers the paired type in its fixed singleton slot. */
int rich4_app_remove_dynamic_object(
    GamePresentationState *presentation, const MapData *map, int object_id);
/* 0x41CF67: on month rollover, re-place the singleton gift/chest objects
 * (types 13 and 14) while excluding their former 300x300 neighborhood. */
int rich4_app_reset_monthly_dynamic_objects(
    GamePresentationState *presentation, const MapData *map);
/* Presentation half of the common 0x40EAD7 attach path.  object_id is a
 * zero-based presentation slot; GameState god_info remains one-based. */
int rich4_app_sync_dynamic_god_attachment(
    GamePresentationState *presentation, const GameState *state,
    int player, int object_id);
/* 0x40E32C: retain the old god's resource long enough to play its
 * presentation-only release animation, then clear the map-object slot. */
int rich4_app_begin_dynamic_god_detach(
    GamePresentationState *presentation, const GameState *state,
    int player, uint32_t now);
int rich4_app_detach_dynamic_god(
    GamePresentationState *presentation, int player);
/* Collect the first unowned road god at the player's current node.  Returns
 * its one-based original slot, or zero when no god was collected. */
int rich4_app_collect_dynamic_god(
    GamePresentationState *presentation, GameState *state, int player,
    GameEventQueue *events);
/* 0x41B837: stopping on the fixed type-11 dog consumes it and requests the
 * resource-backed encounter animation.  The dog is deliberately separate
 * from road gods: it is not collectible and does not attach to a player. */
int rich4_app_resolve_dynamic_dog(
    GamePresentationState *presentation, GameState *state,
    const MapData *map, int player, GameEventQueue *events);
void rich4_app_sync_tool_dynamic_objects(
    GamePresentationState *presentation, const GameState *state);
/* 0x444D1A: choose the nearest active, unowned road god for 请神符.  The
 * returned id is zero-based for GameDomainRef; out_type is the original
 * dynamic-object type used by the core attach path. */
int rich4_app_find_nearest_dynamic_god(
    const GamePresentationState *presentation, const GameState *state,
    int player, int *out_type);
/* Same target selection with the active 440x440 raw table: preserves its
 * pixel-OR and row-major tie order. Use the typed helper when no table exists. */
int rich4_app_find_nearest_dynamic_god_with_projection(
    const GamePresentationState *presentation, const GameState *state,
    int player, const GameAiGlobalProjection *projection, int *out_type);
void rich4_app_update_dynamic_objects(
    GamePresentationState *presentation, const GameViewportModel *viewport,
    int newly_launched_object);
/* 0x41B519 -> 0x40FAFD/0x40E14D: retain the removed object's flight,
 * including paired singleton replacement, independently of route occupancy. */
int rich4_app_robot_clear_dynamic_object(
    GamePresentationState *presentation, const MapData *map,
    int node, int previous_node);
int rich4_app_update_dynamic_god_detachment(
    GamePresentationState *presentation, uint32_t now);
void rich4_app_clear_dynamic_object(
    GamePresentationState *presentation, int object_id);

#endif
