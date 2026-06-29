#ifndef RICH4_CORE_TOOL_INTERNAL_H
#define RICH4_CORE_TOOL_INTERNAL_H

#include "core/tool.h"

void rich4_tool_release_area_object(GameState *state, int slot);
int rich4_tool_robot_choose_next(GameState *state, int node, int previous);
int rich4_tool_active_player(const GameState *state, int player);
int rich4_tool_valid_node(const GameState *state, int node);
int rich4_tool_target_xy(const GameState *state, GameDomainRef ref,
                         int *out_x, int *out_y);
int rich4_tool_in_square(int x, int y, int center_x, int center_y, int half);
int rich4_tool_object_on_node(const GameState *state, int node);
int rich4_tool_placement_slot(const GameState *state, int tool);
int rich4_tool_transfer_targets_valid(const GameState *state,
                                      GameDomainRef source,
                                      GameDomainRef destination);
int rich4_tool_stage_events(GameEventQueue *destination,
                            const GameEventQueue *staged);
int rich4_tool_return_vehicle(GamePlayerState *player, int tool);
int rich4_tool_add_relation(GamePlayerState *player, int target, int amount);
void rich4_tool_adjust_fixed_asset(GameState *state, int owner,
                                   int old_value, int new_value);
int rich4_tool_area_effect(GameState *next, int actor,
                           GameDomainRef center, int full_destroy,
                           GameEventQueue *events);
int rich4_tool_transfer_effect(GameState *next, int actor,
                               GameDomainRef source,
                               GameDomainRef destination,
                               GameEventQueue *events);
int rich4_tool_emit_hospital_events(GameEventQueue *events, int player,
                                    int days, int tool);

#endif
