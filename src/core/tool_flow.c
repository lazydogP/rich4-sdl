#include <limits.h>
#include <string.h>

#include "core/tool_internal.h"
#include "core/insurance.h"
#include "core/confinement.h"
#include "core/game_event_payload.h"

#define NO_PLAYER 0xffu

/* Immediate core callers use the same 0x43EC3F admission/accounting as the
 * runtime continuation, retaining only its insurance events here. The tool
 * caller supplies its own hospital presentation events. */
static int tool_admit_hospital(GameState *state, int player, int days,
                               GameEventQueue *events)
{
    GameEventQueue admission;
    GameEvent event;
    rich4_game_event_queue_reset(&admission);
    rich4_confinement_injure_player(state, player);
    if (rich4_confinement_enter(state, player, GAME_CONFINEMENT_HOSPITAL,
            days, &admission) != GAME_CONFINEMENT_APPLIED) return 0;
    while (rich4_game_event_queue_pop(&admission, &event))
        if ((event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED ||
             event.type == GAME_EVENT_MONEY_TRANSFERRED) &&
            !rich4_game_event_queue_push(events, &event)) return 0;
    return 1;
}

/* Tool command application and route-trigger effects; domain-only state is borrowed. */
int rich4_tool_stage_events(GameEventQueue *destination, const GameEventQueue *staged)
{
    return destination != NULL && staged != NULL &&
           destination->count + staged->count <= GAME_EVENT_QUEUE_CAPACITY &&
           rich4_game_event_queue_append(destination, staged);
}

int rich4_tool_return_vehicle(GamePlayerState *player, int tool)
{
    if (player->tool_inventory[tool] == 255) return 0;
    player->tool_inventory[tool]++;
    return 1;
}

int rich4_tool_emit_hospital_events(GameEventQueue *events, int player, int days,
                                int tool);

int rich4_tool_add_relation(GamePlayerState *player, int target, int amount)
{
    int64_t value;
    if (player == NULL || target < 0 ||
        target >= GAME_PLAYER_RELATION_BALANCE_COUNT || amount < 0) return 0;
    value = (int64_t)player->relation_balance[target] + amount;
    if (value > INT_MAX) return 0;
    player->relation_balance[target] = (int)value;
    return 1;
}

void rich4_tool_adjust_fixed_asset(GameState *state, int owner,
                               int old_value, int new_value)
{
    int64_t value;
    if (state == NULL || owner < 0 || owner >= state->player_count) return;
    value = (int64_t)state->fixed_asset_value[owner] - old_value + new_value;
    if (value < 0) value = 0;
    if (value > INT_MAX) value = INT_MAX;
    state->fixed_asset_value[owner] = (int)value;
}

static int rich4_tool_area_object_node(
    const GameState *state, const struct GameToolPlacedObjectState *object)
{
    int player;
    if (state == NULL || object == NULL) return -1;
    if (rich4_tool_valid_node(state, object->node)) return object->node;
    /* A timed bomb has no route node after it attaches.  The original
     * 0x40AC7B object table still projects it at the attached player's
     * position, so area effects must use that position rather than silently
     * leaving the bomb behind. */
    if (object->kind != 0x12 || object->attached_player == NO_PLAYER ||
        object->attached_player == 0) return -1;
    player = (int)object->attached_player - 1;
    if (player < 0 || player >= state->player_count) return -1;
    return rich4_tool_valid_node(state, state->players[player].node)
        ? state->players[player].node : -1;
}

void rich4_tool_release_area_object(GameState *state, int slot)
{
    struct GameToolPlacedObjectState *object;
    int tool;
    int attached;
    if (state == NULL || slot < 0 || slot >= GAME_TOOL_PLACED_OBJECT_COUNT)
        return;
    object = &state->tool_placed_objects[slot];
    tool = object->kind - 0x0f;
    attached = object->attached_player;
    /* fcn_0040E14D returns placed roadblocks, mines and timed bombs to the
     * public 1..3 tool supplies before clearing their 24-byte record. */
    if (tool >= 1 && tool <= 3 && state->tool_amount[tool] < 255)
        state->tool_amount[tool]++;
    if (object->kind == 0x12 && attached > 0 &&
        attached <= state->player_count)
        state->players[attached - 1].attached_timed_bomb_slot = -1;
    memset(object, 0, sizeof(*object));
}

static int tool_area_effect(GameState *next, int actor, GameDomainRef center,
    int full_destroy, GameEventQueue *events, int defer_confinement);

int rich4_tool_area_effect(GameState *next, int actor, GameDomainRef center,
    int full_destroy, GameEventQueue *events)
{
    return tool_area_effect(next, actor, center, full_destroy, events, 0);
}

static int tool_area_effect(GameState *next, int actor, GameDomainRef center,
    int full_destroy, GameEventQueue *events, int defer_confinement)
{
    int center_x, center_y, half = full_destroy ? 220 : 100;
    int first_land = -1, first_facility = -1, first_object = -1;
    int land_count = 0, facility_count = 0, object_count = 0;
    int player, i;
    if (!rich4_tool_target_xy(next, center, &center_x, &center_y) ||
        next->price_index <= 0 || events == NULL) return 0;
    for (i = 0; i < next->land_count; i++) {
        int owner, old_value, new_value;
        if (!rich4_tool_in_square(next->tool_land_x[i], next->tool_land_y[i],
                       center_x, center_y, half)) continue;
        owner = next->land_owner[i];
        old_value = next->land_value[i];
        if (owner >= 0 && owner < next->player_count &&
            !rich4_tool_add_relation(&next->players[owner], actor,
                          30 * next->price_index *
                          (full_destroy && next->land_level[i] > 0
                               ? next->land_level[i] : 1))) return 0;
        if (full_destroy) {
            next->land_owner[i] = -1;
            next->land_level[i] = 0;
            next->land_type[i] = 0;
            next->land_value[i] = 0;
        } else {
            if (next->land_level[i] > 0) next->land_level[i]--;
            if (next->land_type[i] != 0) {
                next->land_level[i] = 0;
                next->land_type[i] = 0;
            }
            next->land_value[i] = next->land_price[i] +
                next->land_level[i] * next->land_price_per_level[i];
        }
        new_value = next->land_value[i];
        rich4_tool_adjust_fixed_asset(next, owner, old_value, new_value);
        if (first_land < 0) first_land = i;
        land_count++;
    }
    for (i = 0; i < next->facility_count; i++) {
        int owner, old_value, new_value;
        if (!rich4_tool_in_square(next->tool_facility_x[i],
                       next->tool_facility_y[i],
                       center_x, center_y, half)) continue;
        owner = next->facility_owner[i];
        old_value = next->facility_value[i];
        if (owner >= 0 && owner < next->player_count &&
            !rich4_tool_add_relation(&next->players[owner], actor,
                          30 * next->price_index *
                          (full_destroy && next->facility_level[i] > 0
                               ? next->facility_level[i] : 1))) return 0;
        if (full_destroy) {
            next->facility_owner[i] = -1;
            next->facility_level[i] = 0;
            next->facility_type[i] = 0;
            next->facility_variant[i] = 0;
            next->facility_runtime_value[i] = 0;
            next->facility_last_change[i] = 0;
            next->facility_value[i] = 0;
            next->facility_research_selection[i] = 0;
            next->facility_research_turns[i] = 0;
        } else {
            if (next->facility_level[i] > 0) next->facility_level[i]--;
            if (next->facility_level[i] == 0) next->facility_type[i] = 0;
            next->facility_value[i] = next->facility_price[i] +
                next->facility_level[i] *
                next->facility_price_per_level[i];
        }
        new_value = next->facility_value[i];
        rich4_tool_adjust_fixed_asset(next, owner, old_value, new_value);
        if (first_facility < 0) first_facility = i;
        facility_count++;
    }
    for (i = 0; i < GAME_TOOL_PLACED_OBJECT_COUNT; i++) {
        int node = rich4_tool_area_object_node(next,
                                               &next->tool_placed_objects[i]);
        if (next->tool_placed_objects[i].kind == 0 ||
            node < 0 ||
            !rich4_tool_in_square(next->tool_route_x[node], next->tool_route_y[node],
                       center_x, center_y, half)) continue;
        rich4_tool_release_area_object(next, i);
        if (first_object < 0) first_object = i;
        object_count++;
    }
    for (i = 0; i < GAME_SPECIAL_PLAYER_COUNT; i++) {
        int node = next->special_player_node[i];
        /* Record +8 is the rescuer/owner player index; zero is a valid
         * player, so visibility is controlled by record +10 (f10), not by
         * the owner value.  The original area-effect scan includes a
         * rescued special actor even when player 0 owns it. */
        if (next->special_player_f10[i] != 0 ||
            !rich4_tool_valid_node(next, node) ||
            !rich4_tool_in_square(next->tool_route_x[node], next->tool_route_y[node],
                       center_x, center_y, half)) continue;
        next->special_player_play_state[i] = 0;
        next->special_player_node[i] = 0;
    }
    for (player = 0; player < next->player_count; player++) {
        int node = next->players[player].node;
        if (!rich4_tool_active_player(next, player) || !rich4_tool_valid_node(next, node) ||
            !rich4_tool_in_square(next->tool_route_x[node], next->tool_route_y[node],
                       center_x, center_y, half)) continue;
        if (!rich4_tool_valid_node(next, next->hospital_node) ||
            !rich4_tool_add_relation(&next->players[player], actor,
                          90 * next->price_index)) return 0;
        if (defer_confinement) {
            rich4_confinement_injure_player(next, player);
            if (rich4_confinement_request_enter(next, player,
                    GAME_CONFINEMENT_HOSPITAL, 3, events) != GAME_CONFINEMENT_APPLIED)
                return 0;
            continue;
        }
        if (!rich4_game_event_emit_typed_domain(
                events, rich4_game_event_tool_presentation_requested_from_fields,
                player,
                rich4_game_domain_ref(
                    GAME_DOMAIN_TOOL, full_destroy ? 12 : 6),
                rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node),
                524, 92, 0)) return 0;
        if (!tool_admit_hospital(next, player, 3, events) ||
            !rich4_tool_emit_hospital_events(events, player, 3,
                                  full_destroy ? 12 : 6)) return 0;
    }
    if (land_count != 0 &&
        !rich4_game_event_emit_typed_domain(
            events, rich4_game_event_tool_area_resolved_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_LAND, first_land), center,
            land_count, full_destroy, 0)) return 0;
    if (facility_count != 0 &&
        !rich4_game_event_emit_typed_domain(
            events, rich4_game_event_tool_area_resolved_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_FACILITY, first_facility),
            center, facility_count, full_destroy, 0)) return 0;
    if (object_count != 0 &&
        !rich4_game_event_emit_typed_domain(
            events, rich4_game_event_tool_area_resolved_from_fields, actor,
            rich4_game_domain_ref(GAME_DOMAIN_MAP_OBJECT, first_object),
            center, object_count, full_destroy, 0)) return 0;
    return 1;
}

int rich4_tool_transfer_effect(GameState *next, int actor,
                           GameDomainRef source, GameDomainRef destination,
                           GameEventQueue *events)
{
    int i;
    if (!rich4_tool_transfer_targets_valid(next, source, destination) || events == NULL)
        return 0;
    if (source.kind == GAME_DOMAIN_LAND) {
        int from = source.id, to = destination.id;
        next->land_owner[to] = next->land_owner[from];
        next->land_type[to] = next->land_type[from];
        next->land_level[to] = next->land_level[from];
        next->land_value[to] = next->land_value[from];
        next->land_owner[from] = -1;
        next->land_type[from] = 0;
        next->land_level[from] = 0;
        next->land_value[from] = 0;
    } else if (source.kind == GAME_DOMAIN_FACILITY) {
        int from = source.id, to = destination.id;
        next->facility_owner[to] = next->facility_owner[from];
        next->facility_type[to] = next->facility_type[from];
        next->facility_level[to] = next->facility_level[from];
        next->facility_variant[to] = next->facility_variant[from];
        next->facility_runtime_value[to] =
            next->facility_runtime_value[from];
        next->facility_last_change[to] =
            next->facility_last_change[from];
        next->facility_value[to] = next->facility_value[from];
        next->facility_research_selection[to] =
            next->facility_research_selection[from];
        next->facility_research_turns[to] =
            next->facility_research_turns[from];
        next->facility_owner[from] = -1;
        next->facility_type[from] = 0;
        next->facility_level[from] = 0;
        next->facility_variant[from] = 0;
        next->facility_runtime_value[from] = 0;
        next->facility_last_change[from] = 0;
        next->facility_value[from] = 0;
        next->facility_research_selection[from] = 0;
        next->facility_research_turns[from] = 0;
    } else if (source.kind == GAME_DOMAIN_PLAYER) {
        GamePlayerState *player = &next->players[source.id];
        player->node = destination.id;
        player->previous_node = 0;
        player->next_node = 0;
        for (i = 0; i < 4; i++)
            if (next->tool_route_next[destination.id][i] != 0) {
                player->next_node =
                    next->tool_route_next[destination.id][i];
                break;
            }
    } else if (source.kind == GAME_DOMAIN_SPECIAL_PLAYER) {
        next->special_player_node[source.id] = destination.id;
    } else if (source.kind == GAME_DOMAIN_MAP_OBJECT) {
        next->tool_placed_objects[source.id].node = destination.id;
    } else {
        return 0;
    }
    return rich4_game_event_emit_typed_domain(
        events, rich4_game_event_map_object_changed_from_fields, actor,
        source, destination,
        source.id, destination.id, 0);
}

static GameToolUseResult tool_apply_command(GameState *state,
    const GameCommand *command, GameEventQueue *events, int defer_confinement);

GameToolUseResult rich4_tool_apply_command(GameState *state,
    const GameCommand *command, GameEventQueue *events)
{
    return tool_apply_command(state, command, events, 0);
}

GameToolUseResult rich4_tool_apply_command_deferred_confinement(GameState *state,
    const GameCommand *command, GameEventQueue *events)
{
    return tool_apply_command(state, command, events, 1);
}

static GameToolUseResult tool_apply_command(GameState *state,
                                           const GameCommand *command,
                                           GameEventQueue *events, int defer_confinement)
{
    GameToolUseResult result = rich4_tool_inspect(state, command);
    GameState next;
    GameEventQueue staged;
    GamePlayerState *player;
    int tool, slot = -1, old_traffic;
    if (result != GAME_TOOL_USE_APPLIED || events == NULL) return result;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    tool = command->subject.id;
    player = &next.players[command->player];
    old_traffic = player->traffic_method;
    if (tool == 0) {
        int next_node = rich4_tool_robot_choose_next(
            &next, player->node, player->previous_node);
        if (!rich4_tool_valid_node(&next, next_node)) return GAME_TOOL_USE_INVALID;
        next.tool_robot.active = 1;
        next.tool_robot.owner = command->player;
        next.tool_robot.node = player->node;
        next.tool_robot.previous_node = player->previous_node;
        next.tool_robot.next_node = next_node;
        next.tool_robot.steps_remaining = 9;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_tool_presentation_requested_from_fields,
                command->player, command->subject,
                rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, player->node),
                0, 38, 1)) return GAME_TOOL_USE_INVALID;
    } else if (tool >= 1 && tool <= 3) {
        slot = rich4_tool_placement_slot(&next, tool);
        if (slot < 0) return GAME_TOOL_USE_INVALID;
        next.tool_placed_objects[slot].kind = (uint8_t)(0x0f + tool);
        next.tool_placed_objects[slot].attached_player = NO_PLAYER;
        next.tool_placed_objects[slot].node = command->target.id;
        next.tool_placed_objects[slot].owner = command->player;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_map_object_changed_from_fields,
                command->player, command->subject, command->target,
                slot, next.tool_placed_objects[slot].kind, 0)) return GAME_TOOL_USE_INVALID;
    } else if (tool == 4) {
        if (old_traffic == 2 && !rich4_tool_return_vehicle(player, 5)) return GAME_TOOL_USE_INVALID;
        player->traffic_method = 1;
        player->ndices = 2;
    } else if (tool == 5) {
        if (old_traffic == 1 && !rich4_tool_return_vehicle(player, 4)) return GAME_TOOL_USE_INVALID;
        player->traffic_method = 2;
        player->ndices = 3;
    } else if (tool == 6 || tool == 12) {
        int full_destroy = tool == 12;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_tool_presentation_requested_from_fields,
                command->player, command->subject, command->target,
                full_destroy ? 530 : 528,
                full_destroy ? 83 : 81, 0) ||
            !tool_area_effect(&next, command->player, command->target,
                         full_destroy, &staged, defer_confinement))
            return GAME_TOOL_USE_INVALID;
    } else if (tool == 7) {
        player->remote_dice_value = command->value;
    } else if (tool == 8) {
        int id = command->target.id;
        int level;
        if (command->target.kind == GAME_DOMAIN_LAND) {
            next.land_level[id]++;
            level = next.land_level[id];
        } else {
            if (next.facility_level[id] == 0)
                next.facility_type[id] = command->value;
            next.facility_level[id]++;
            level = next.facility_level[id];
        }
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_property_level_changed_from_fields,
                command->player,
                command->target, command->subject, level,
                command->target.kind == GAME_DOMAIN_FACILITY
                    ? next.facility_type[id] : 0,
                level == 5 ? 1u : 0u) ||
            !rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_tool_presentation_requested_from_fields,
                command->player, command->subject, command->target,
                553, 91, 0) ||
            (level == 5 && !rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_tool_presentation_requested_from_fields,
                command->player, command->subject, command->target,
                523, 90, 1)))
            return GAME_TOOL_USE_INVALID;
    } else if (tool == 9) {
        return GAME_TOOL_USE_RESTORE_REQUIRED;
    } else if (tool == 10) {
        if (!rich4_tool_transfer_effect(&next, command->player, command->target,
                             command->target2, &staged))
            return GAME_TOOL_USE_INVALID;
    } else if (tool == 11) {
        if (old_traffic == 1 && !rich4_tool_return_vehicle(player, 4)) return GAME_TOOL_USE_INVALID;
        if (old_traffic == 2 && !rich4_tool_return_vehicle(player, 5)) return GAME_TOOL_USE_INVALID;
        player->engineering_saved_traffic_method = old_traffic;
        player->engineering_saved_ndices = player->ndices;
        player->traffic_method = 0x1f;
        player->ndices = 1;
    } else {
        return GAME_TOOL_USE_DEFERRED;
    }
    if (tool == 4 || tool == 5 || tool == 11)
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_traffic_changed_from_fields,
                command->player,
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, command->player),
                command->subject, player->traffic_method, old_traffic, 0))
            return GAME_TOOL_USE_INVALID;
    if (player->tool_inventory[tool] == 0) return GAME_TOOL_USE_INVALID;
    player->tool_inventory[tool]--;
    /* sub_445AA2 replenishes the public shop pool only for original IDs
     * 1..8; IDs 9..13 remain scarce. */
    if (tool < 8 && next.tool_amount[tool] < 255)
        next.tool_amount[tool]++;
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields,
            command->player, command->subject,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, command->player), -1,
            rich4_tool_inventory_count(&next, command->player), 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_tool_used_from_fields,
            command->player, command->subject, command->target, slot,
            player->tool_inventory[tool], 0) || !rich4_tool_stage_events(events, &staged))
        return GAME_TOOL_USE_INVALID;
    *state = next;
    return GAME_TOOL_USE_APPLIED;
}

static GameToolTriggerResult tool_trigger_at_node(GameState *state, int player,
                                                  int node, int stopped,
                                                  GameEventQueue *events,
                                                  int defer_hospital)
{
    GameState next;
    GameEventQueue staged;
    struct GameToolPlacedObjectState *object;
    GameToolTriggerResult result;
    int slot, tool;
    if (state == NULL || events == NULL || player < 0 || player >= state->player_count ||
        node <= 0 || node >= state->tool_route_node_count) return GAME_TOOL_TRIGGER_INVALID;
    slot = rich4_tool_object_on_node(state, node);
    if (slot < 0) return GAME_TOOL_TRIGGER_NONE;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    object = &next.tool_placed_objects[slot];
    /* 0x41BE5F and 0x41BFD2 gate mines and timed bombs on the original
     * remaining-step counter being zero.  Roadblocks deliberately have no
     * such gate and stop a player as soon as they are crossed. */
    if ((object->kind == 0x11 || object->kind == 0x12) && !stopped)
        return GAME_TOOL_TRIGGER_NONE;
    tool = object->kind - 0x0f;
    if (object->kind == 0x10) {
        rich4_tool_release_area_object(&next, slot);
        result = GAME_TOOL_TRIGGER_STOP;
    } else if (object->kind == 0x11) {
        rich4_tool_release_area_object(&next, slot);
        if (next.hospital_node < 0 ||
            next.hospital_node >= GAME_MAX_MAP_ROUTE_NODES)
            return GAME_TOOL_TRIGGER_INVALID;
        if (!defer_hospital) {
            if (!tool_admit_hospital(&next, player, 3, &staged))
                return GAME_TOOL_TRIGGER_INVALID;
        }
        result = GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT;
    } else if (object->kind == 0x12) {
        if (next.players[player].attached_timed_bomb_slot >= 0)
            return GAME_TOOL_TRIGGER_NONE;
        object->node = 0;
        object->attached_player = (uint8_t)(player + 1);
        object->countdown = 0x26;
        next.players[player].attached_timed_bomb_slot = slot;
        result = GAME_TOOL_TRIGGER_PASS;
    } else {
        return GAME_TOOL_TRIGGER_INVALID;
    }
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_map_object_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node), slot, result, 0) ||
        ((tool == 1 || tool == 3) && !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_voice_requested_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player), 1, 0, 0)) ||
        (tool == 2 && !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_tool_presentation_requested_from_fields,
            player,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node),
            525, 82, 0)) ||
        (tool == 2 && !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_voice_requested_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player), 1, 0, 0)) ||
        (tool == 2 && !rich4_game_event_text_requested_emit(
            &staged, player, 53, GAME_CONFINEMENT_HOSPITAL, 3, 0)) ||
        (tool == 2 && !defer_hospital && !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_tool_presentation_requested_from_fields,
            player,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_STATUS,
                                  GAME_STATUS_HOSPITAL),
            524, 92, 0)) ||
        (tool == 2 && !defer_hospital &&
         !rich4_tool_emit_hospital_events(&staged, player, 3, tool)) ||
        !rich4_tool_stage_events(events, &staged)) return GAME_TOOL_TRIGGER_INVALID;
    *state = next;
    return result;
}

int rich4_tool_emit_hospital_events(GameEventQueue *events, int player, int days,
                                int tool)
{
    return rich4_game_event_emit_typed_domain(
               events, rich4_game_event_status_changed_from_fields, player,
               rich4_game_domain_ref(GAME_DOMAIN_STATUS,
                                     GAME_STATUS_HOSPITAL),
               rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool), days, 1, 0) &&
           rich4_game_event_actor_dirty_requested_emit(
               events, player, 53, GAME_CONFINEMENT_HOSPITAL, 1, 0) &&
           rich4_game_event_scene_refresh_requested_emit(
               events, player, 53, GAME_CONFINEMENT_HOSPITAL, 1, 0) &&
           rich4_game_event_emit_typed_domain(
               events, rich4_game_event_voice_requested_from_fields, player,
               rich4_game_domain_ref(GAME_DOMAIN_STATUS,
                                     GAME_STATUS_HOSPITAL),
               rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
               days, 2, 0);
}

static GameToolTriggerResult tool_advance_step(GameState *state, int player,
                                               int node, int settlement,
                                               GameEventQueue *events,
                                               int defer_hospital)
{
    GameState next;
    GameEventQueue staged;
    GamePlayerState *actor;
    int slot, other;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count || node <= 0 ||
        node >= state->tool_route_node_count) return GAME_TOOL_TRIGGER_INVALID;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    actor = &next.players[player];

    if (settlement && actor->traffic_method > 3 &&
        (actor->traffic_method & 3) == 3) {
        actor->traffic_method -= 4;
        if (actor->traffic_method == 3) {
            int restored = actor->engineering_saved_traffic_method;
            int restored_dice = actor->engineering_saved_ndices;
            if (restored == 1 && actor->tool_inventory[4] > 0)
                actor->tool_inventory[4]--;
            if (restored == 2 && actor->tool_inventory[5] > 0)
                actor->tool_inventory[5]--;
            actor->traffic_method = restored;
            actor->ndices = restored_dice;
            if (!rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_traffic_changed_from_fields, player,
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
                    rich4_game_domain_ref(GAME_DOMAIN_TOOL, 11), restored,
                    0x1f, 0) ||
                !rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_inventory_changed_from_fields, player,
                    rich4_game_domain_ref(GAME_DOMAIN_TOOL, 11),
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player), 0,
                    rich4_tool_inventory_count(&next, player), 0))
                return GAME_TOOL_TRIGGER_INVALID;
        }
    }

    slot = actor->attached_timed_bomb_slot;
    if (slot >= 0 && slot < GAME_TOOL_PLACED_OBJECT_COUNT &&
        next.tool_placed_objects[slot].kind == 0x12) {
        struct GameToolPlacedObjectState *bomb = &next.tool_placed_objects[slot];
        if (bomb->countdown > 0) bomb->countdown--;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_map_object_changed_from_fields, player,
                rich4_game_domain_ref(GAME_DOMAIN_TOOL, 3),
                rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node), slot,
                bomb->countdown, 0)) return GAME_TOOL_TRIGGER_INVALID;
        if (bomb->countdown == 0) {
            rich4_tool_release_area_object(&next, slot);
            if (next.hospital_node < 0 ||
                next.hospital_node >= GAME_MAX_MAP_ROUTE_NODES)
                return GAME_TOOL_TRIGGER_INVALID;
            if (!defer_hospital) {
                if (!tool_admit_hospital(&next, player, 5, &staged))
                    return GAME_TOOL_TRIGGER_INVALID;
            }
            if (!rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_tool_presentation_requested_from_fields,
                    player,
                    rich4_game_domain_ref(GAME_DOMAIN_TOOL, 3),
                    rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, node),
                    525, 82, 0) ||
                !rich4_game_event_text_requested_emit(
                    &staged, player, 53, GAME_CONFINEMENT_HOSPITAL, 5, 0) ||
                (!defer_hospital && !rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_tool_presentation_requested_from_fields,
                    player,
                    rich4_game_domain_ref(GAME_DOMAIN_TOOL, 3),
                    rich4_game_domain_ref(GAME_DOMAIN_STATUS,
                                          GAME_STATUS_HOSPITAL),
                    524, 92, 0)) ||
                (!defer_hospital &&
                 !rich4_tool_emit_hospital_events(&staged, player, 5, 3)))
                return GAME_TOOL_TRIGGER_INVALID;
            if (!rich4_tool_stage_events(events, &staged))
                return GAME_TOOL_TRIGGER_INVALID;
            *state = next;
            return GAME_TOOL_TRIGGER_STOP_DEFERRED_EFFECT;
        }
        /* 0x41B6C4 decrements first; zero explodes on the carrier and
         * returns. 0x41B790 considers only the first occupied player slot,
         * even when that player cannot receive the surviving bomb. */
        for (other = 0; other < next.player_count; other++) {
            if (other == player || next.players[other].node != node) continue;
            if ((next.players[other].play_state & 0xff) != 0 &&
                next.players[other].attached_timed_bomb_slot < 0) {
                actor->attached_timed_bomb_slot = -1;
                next.players[other].attached_timed_bomb_slot = slot;
                bomb->attached_player = (uint8_t)(other + 1);
            }
            break;
        }
    }
    if (!rich4_tool_stage_events(events, &staged)) return GAME_TOOL_TRIGGER_INVALID;
    *state = next;
    return GAME_TOOL_TRIGGER_PASS;
}

GameToolTriggerResult rich4_tool_trigger_at_node(GameState *state, int player,
    int node, int stopped, GameEventQueue *events)
{
    return tool_trigger_at_node(state, player, node, stopped, events, 0);
}

GameToolTriggerResult rich4_tool_trigger_at_node_deferred_hospital(
    GameState *state, int player, int node, int stopped, GameEventQueue *events)
{
    return tool_trigger_at_node(state, player, node, stopped, events, 1);
}

GameToolTriggerResult rich4_tool_advance_step(GameState *state, int player,
    int node, int settlement, GameEventQueue *events)
{
    return tool_advance_step(state, player, node, settlement, events, 0);
}

GameToolTriggerResult rich4_tool_advance_step_deferred_hospital(
    GameState *state, int player, int node, int settlement, GameEventQueue *events)
{
    return tool_advance_step(state, player, node, settlement, events, 1);
}
