#include "view/game_view_model.h"
#include "core/character_speech_rules.h"

#include <stdint.h>
#include <string.h>

#include "core/bank.h"
#include "core/stock.h"
#include "view/view_constants.h"

#include "data/rich4_map_tables.inc"
#include "data/confinement_presentation.h"

#define VIEW_GRID_X(direction, row, col) rich4_map_grid[(direction)][(row)][(col)][1]
#define VIEW_GRID_Y(direction, row, col) rich4_map_grid[(direction)][(row)][(col)][0]

static int wrap_index_view(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    return value < 0 ? value + count : value;
}

/* 0x4169BC uses the same original calendar records as bank/stock dates. */
static void build_calendar_view(const GameState *state, GameCalendarView *view)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    GameState date;
    int i;
    view->event_index = -1;
    if (!rich4_stock_calendar_date(state, &view->year, &view->month,
                                  &view->day, &view->weekday)) return;
    view->map_index = state->stage * 4 + state->map;
    view->event_index = rich4_stock_calendar_event_index(state);
    view->month_days = days[view->month - 1];
    if (view->month == 2 && view->year % 4 == 0 &&
        (view->year % 100 != 0 || view->year % 400 == 0)) view->month_days++;
    view->first_weekday = (view->weekday - (view->day - 1) % 7 + 7) % 7;
    date = *state;
    date.calendar_start_year = view->year;
    date.calendar_start_month = view->month;
    date.calendar_start_day = 1;
    for (i = 0; i < view->month_days; i++) {
        date.elapsed_days = i;
        if (rich4_stock_calendar_is_closed(&date))
            view->closed_days |= UINT32_C(1) << i;
    }
}

static int confinement_anchor(const GamePlayerState *player,
                             const MapData *map,
                             int *out_x,
                             int *out_y)
{
    int special_index;
    if (player == NULL || map == NULL)
        return 0;
    /* +0x32 stores a hotel stay; 0x40D5A5 keeps its road node. The
     * entering actor still follows its interpolation while +21 & 0x20. */
    if (player->delayed_status_block != 0 &&
        (player->play_state & GAME_PLAYER_PLAY_STATE_DELAYED_BLOCK_VISIBLE) == 0 &&
        map->routes != NULL && map->facilities != NULL && player->node > 0 &&
        (size_t)player->node < map->route_count) {
        int raw = map->routes[player->node].object.raw_object_id;
        int facility = raw - 4000;
        if (facility > 0 && (size_t)facility < map->facility_count) {
            if (out_x != NULL) *out_x = map->facilities[facility].x;
            if (out_y != NULL) *out_y = map->facilities[facility].y;
            return 1;
        }
    }
    if (map->specials == NULL) return 0;
    special_index = player->confinement_actor_type == GAME_CONFINEMENT_HOSPITAL
        ? 1 : player->confinement_actor_type == GAME_CONFINEMENT_PRISON
        ? 2 : 0;
    if (special_index <= 0 || (size_t)special_index >= map->special_count)
        return 0;
    if (out_x != NULL) *out_x = map->specials[special_index].x;
    if (out_y != NULL) *out_y = map->specials[special_index].y;
    return 1;
}

static int original_player_draw_queue_eligible(
    const GamePlayerState *player)
{
    int status_block;
    if (player == NULL) return 0;
    /* sub_40829D admits an ordinary player when +50..+53 are all zero, or
     * when the raw +21 state byte has bit 0x20 set. */
    status_block = player->delayed_status_block != 0 ||
        player->days_disappearing != 0 || player->days_in_prison != 0 ||
        player->days_in_hospital != 0;
    return !status_block ||
        (player->play_state &
         GAME_PLAYER_PLAY_STATE_DELAYED_BLOCK_VISIBLE) != 0;
}

static void subtile_offset(int direction, int world_x, int world_y, int *out_x, int *out_y)
{
    static const signed char coeffs[8][4] = {
        {-34,11,-14,-25}, {-34,-9,16,-25}, {-13,-25,37,-9}, {16,-25,36,11},
        {37,-9,16,26}, {36,11,-14,26}, {16,26,-35,11}, {-14,26,-35,-9}
    };
    int sub_x = world_x & 31;
    int sub_y = world_y & 31;
    const signed char *c = coeffs[direction & 7];
    if (out_x != NULL) *out_x = ((sub_x * c[0]) >> 5) + ((sub_y * c[2]) >> 5);
    if (out_y != NULL) *out_y = ((sub_x * c[1]) >> 5) + ((sub_y * c[3]) >> 5);
}

void rich4_game_viewport_build(const GameState *state,
                               const GameViewModelInput *input,
                               const MapData *map,
                               GameViewportModel *out_viewport)
{
    GameViewportModel value;
    memset(&value, 0, sizeof(value));
    if (input != NULL) {
        value.direction = input->map_direction & 7;
        value.has_override = input->has_view_override != 0;
        if (value.has_override) {
            value.center_world_x = input->view_world_x;
            value.center_world_y = input->view_world_y;
        }
        if (input->special_actor.active && !value.has_override) {
            value.has_override = 1;
            value.center_world_x = input->special_actor.visual_x;
            value.center_world_y = input->special_actor.visual_y;
        }
    }
    if (!value.has_override && state != NULL && state->active && state->player_count > 0) {
        int current = wrap_index_view(state->current_player, state->player_count);
        const GamePlayerState *player = &state->players[current];
        if (!input->actors[current].move_active &&
            confinement_anchor(player, map,
                               &value.center_world_x,
                               &value.center_world_y)) {
            /* 0x43D593/0x43EC3F leave the logical node on the facility's
             * route record, but draw the confined actor at MapData special
             * record 2 (prison) or 1 (hospital). */
        } else if (!input->actors[current].move_active && map != NULL &&
            player->node > 0 && (size_t)player->node < map->route_count) {
            /* Preserve the pre-refactor camera rule: an idle actor follows
             * the authoritative route node, not potentially stale visual
             * interpolation coordinates. */
            value.center_world_x = map->routes[player->node].x;
            value.center_world_y = map->routes[player->node].y;
        } else {
            value.center_world_x = input->actors[current].visual_x;
            value.center_world_y = input->actors[current].visual_y;
        }
    }
    if (out_viewport != NULL) *out_viewport = value;
}

void rich4_game_viewport_project(const GameViewportModel *viewport,
                                 int world_x,
                                 int world_y,
                                 int *out_x,
                                 int *out_y)
{
    int direction = viewport != NULL ? viewport->direction & 7 : 0;
    int center_x = viewport != NULL ? viewport->center_world_x : world_x;
    int center_y = viewport != NULL ? viewport->center_world_y : world_y;
    int tile_x = world_x >> 5;
    int tile_y = world_y >> 5;
    int center_tile_x = center_x >> 5;
    int center_tile_y = center_y >> 5;
    int grid_x = tile_x - center_tile_x + 14;
    int grid_y = tile_y - center_tile_y + 14;
    int center_sub_x, center_sub_y, point_sub_x, point_sub_y;
    subtile_offset(direction, center_x, center_y, &center_sub_x, &center_sub_y);
    subtile_offset(direction, world_x, world_y, &point_sub_x, &point_sub_y);
    if (grid_x < 0 || grid_x >= 29 || grid_y < 0 || grid_y >= 29) {
        int ux = VIEW_GRID_X(direction, 14, 15) - VIEW_GRID_X(direction, 14, 14);
        int vx = VIEW_GRID_X(direction, 15, 14) - VIEW_GRID_X(direction, 14, 14);
        int uy = VIEW_GRID_Y(direction, 14, 15) - VIEW_GRID_Y(direction, 14, 14);
        int vy = VIEW_GRID_Y(direction, 15, 14) - VIEW_GRID_Y(direction, 14, 14);
        if (out_x != NULL) *out_x = GAME_MAIN_MAP_ORIGIN_X +
            (tile_x - center_tile_x) * ux + (tile_y - center_tile_y) * vx;
        if (out_y != NULL) *out_y = GAME_MAIN_MAP_ORIGIN_Y +
            (tile_x - center_tile_x) * uy + (tile_y - center_tile_y) * vy;
        return;
    }
    if (out_x != NULL) {
        *out_x = GAME_MAIN_MAP_ORIGIN_X + center_sub_x +
                 VIEW_GRID_X(direction, grid_y, grid_x) - point_sub_x;
    }
    if (out_y != NULL) {
        *out_y = GAME_MAIN_MAP_ORIGIN_Y + center_sub_y +
                 VIEW_GRID_Y(direction, grid_y, grid_x) - point_sub_y;
    }
}

int rich4_game_actor_direction_from_delta(int dx, int dy)
{
    static const unsigned char direction_table[8] = {2,3,4,5,6,7,0,1};
    int64_t x = dx;
    int64_t y = -(int64_t)dy;
    int64_t ax = x < 0 ? -x : x;
    int64_t ay = y < 0 ? -y : y;
    int diagonal;
    int sector;
    if (ax == 0 && ay == 0) return direction_table[0];
    if (ax >= ay) {
        int ratio_index = (int)((ay * 2048) / ax);
        /* word_48242C in 0x454FE1 has 1023 at index 848 and 1024 at
         * 849.  At that exact table value, 0x454FB4's angle rounding
         * assigns the boundary to a different side in each quadrant. */
        diagonal = ratio_index > 849 ||
            (ratio_index == 849 &&
             ((x >= 0 && y >= 0) || (x < 0 && y < 0)));
    } else {
        int ratio_index = (int)((ax * 2048) / ay);
        diagonal = ratio_index > 849 ||
            (ratio_index == 849 &&
             ((x >= 0 && y < 0) || (x < 0 && y >= 0)));
    }
    if (!diagonal && ax >= ay) sector = x >= 0 ? 0 : 8;
    else if (!diagonal) sector = y >= 0 ? 4 : 12;
    else if (x >= 0 && y >= 0) sector = 2;
    else if (x < 0 && y >= 0) sector = 6;
    else if (x < 0 && y < 0) sector = 10;
    else sector = 14;
    return direction_table[((sector + 1) >> 1) & 7];
}

int rich4_game_actor_part(const GamePlayerState *player,
                          int special_road_node,
                          int moving,
                          int *out_overlay_part)
{
    int part;
    if (out_overlay_part != NULL) *out_overlay_part = -1;
    if (player == NULL) return -1;
    /* sub_40B93B tests the complete +21 byte for zero, then bit 0x40.
     * Bits 0x10/0x20 can be set while the ordinary traffic resource lane is
     * still active; masking to the low nibble incorrectly chose part 18. */
    if ((player->play_state & 0xff) == 0 ||
        (player->play_state & GAME_PLAYER_PLAY_STATE_FROZEN) != 0) return 18;
    if (player->days_sleep_walking != 0) return moving ? 17 : 16;
    if (special_road_node) return moving ? 14 : 13;
    /* sub_40B93B does not use the status counters that gate queue admission
     * in sub_40829D to choose an EA1/EA2 resource slot. */
    part = (player->traffic_method & 3) * 3 + (moving ? 1 : 0);
    /* 0x4087B5 selects one EA2 resource. Part 12 is EA2=3 demolition,
     * not an overlay on the ordinary engineering vehicle (parts 9/10). */
    return part;
}

int rich4_game_actor_action_part(const GamePlayerState *player,
                                 int special_road_node)
{
    int part;
    if (player == NULL) return -1;
    /* Data[128 + character*21 + 15] is the high-bit/water-road upward
     * dice-throw actor.  Land traffic methods use their usual +2/+5/+8/+11
     * action resources. */
    if (special_road_node) return 15;
    part = (player->traffic_method & 3) * 3 + 2;
    return part >= 0 && part < GAME_CHARACTER_MODEL_RESOURCES ? part : 2;
}

int rich4_game_view_model_build(const GameState *state,
                                const GameViewModelInput *input,
                                const MapData *map,
                                GameViewModel *out_model)
{
    GameViewModel model;
    int i;
    if (state == NULL || input == NULL || out_model == NULL) return 0;
    memset(&model, 0, sizeof(model));
    model.hud.ally_character = -1;
    model.hud.special_actor = -1;
    model.active = state->active != 0;
    model.route_count = input->route_count;
    rich4_game_viewport_build(state, input, map, &model.viewport);
    model.actor_count = state->player_count;
    if (model.actor_count < 0) model.actor_count = 0;
    if (model.actor_count > GAME_MAX_PLAYERS) model.actor_count = GAME_MAX_PLAYERS;
    model.current_actor = -1;
    if (state->turn_rotation_index >= 0 &&
        state->turn_rotation_index < GAME_MAX_PLAYERS * 2) {
        model.current_actor = state->turn_rotation_index;
    } else if (state->player_count > 0) {
        model.current_actor = wrap_index_view(state->current_player,
                                              state->player_count);
    }
    for (i = 0; i < model.actor_count; i++) {
        const GamePlayerState *player = &state->players[i];
        const GameViewActorInput *actor_flow = &input->actors[i];
        GameActorView *actor = &model.actors[i];
        int turn_movement_pending;
        int visual_moving;
        int resource_moving;
        int effect_at_origin = 0;
        GamePlayerState effect_player;
        const Rich4ConfinementPresentation *confinement =
            rich4_confinement_presentation_for_resource(input->tool_effect_resource);
        /* 0x4512BF refreshes Data[532] at frame 3 and Data[524] at
         * frame 30. SDL redraws every frame, so preserve the old actor
         * background until those decoded-frame boundaries. */
        if (input->tool_effect_actor == i + 1 &&
            input->tool_effect_resource == 532 && input->tool_effect_frame < 2) {
            effect_player = *player;
            effect_player.play_state &= ~GAME_PLAYER_PLAY_STATE_FROZEN;
            player = &effect_player;
        } else if (input->tool_effect_actor == i + 1 &&
                   (input->tool_effect_resource == 525 ||
                    (confinement && input->tool_effect_frame < confinement->refresh_frame - 1)) &&
                   (input->tool_effect_node > 0 ||
                    input->tool_effect_has_world_position)) {
            /* 0x41BEA0/0x41B72E -> 0x40CD07 selects Data[128+21*c+18]
             * at the explosion site. Keep that actor through the reaction
             * and until 0x4512BF refreshes Data[524] at decoded frame 30;
             * hospital data is committed after the admission reaction. */
            effect_player = *player;
            if (input->tool_effect_node > 0)
                effect_player.node = input->tool_effect_node;
            effect_player.confinement_actor_type = GAME_CONFINEMENT_NONE;
            effect_player.days_in_hospital = 0;
            effect_player.days_in_prison = 0;
            if (input->tool_effect_resource == 525 && input->tool_effect_frame < 2) {
                effect_player.play_state &= ~GAME_PLAYER_PLAY_STATE_FROZEN;
                effect_player.traffic_method = input->tool_effect_explosion_traffic_method;
            }
            else if (confinement) {
                effect_player.play_state = input->tool_effect_origin_play_state;
                effect_player.traffic_method = input->tool_effect_origin_traffic_method;
            } else {
                effect_player.play_state |= GAME_PLAYER_PLAY_STATE_FROZEN;
                effect_player.traffic_method = 0;
            }
            player = &effect_player;
            effect_at_origin = input->tool_effect_has_world_position;
        }
        int actor_direction = actor_flow->direction;
        /* In sub_40D7C4 case 1, EA2 stays on the movement resource while
         * n9_0 (the remaining route steps) is nonzero.  The route helper
         * finishes an edge before the next timer pulse starts the next one,
         * so move_active is briefly zero at every intermediate node.  That
         * is a timing boundary, not an idle-render boundary. */
        turn_movement_pending = input->turn_flow_active &&
            input->turn_state == GAME_TURN_STATE_MOVE &&
            input->steps_remaining > 0 && i == state->current_player;
        visual_moving = actor_flow->move_active || turn_movement_pending;
        /* sub_40C05C writes its selected destination to word_496B74 before
         * sub_40B93B chooses the current sprite resource. The logical node
         * remains the source until the step completes, so use next_node while
         * the walking resource is active and the committed node while idle. */
        int resource_route_node = visual_moving && (player->play_state & 0x30) == 0 &&
            player->delayed_status_block == 0
            ? player->next_node : player->node;
        int special = map != NULL && resource_route_node > 0 &&
            (size_t)resource_route_node < map->route_count &&
            (map->routes[resource_route_node].flags & 0x80000000u) != 0;
        actor->player = i;
        actor->character = player->character;
        /* 0x4090BA checks +63 only; a bomb alone uses the small offset. */
        actor->has_dynamic_attachment = player->god_info != 0;
        actor->attachments_hidden = player->delayed_status_block != 0 ||
            player->days_disappearing != 0 || player->days_in_prison != 0 ||
            player->days_in_hospital != 0;
        actor->first_turn_landing_pending =
            actor_flow->first_turn_landing_pending;
        /* sub_40D375 removes the route-occupancy bit while player +51 is
         * active; sub_40D4E5 restores it when the countdown resolves. */
        actor->visible = actor_flow->intro_visible &&
            original_player_draw_queue_eligible(player);
        if (effect_at_origin) {
            actor->world_x = input->tool_effect_world_x;
            actor->world_y = input->tool_effect_world_y;
        } else if (!actor_flow->move_active &&
            confinement_anchor(player, map,
                               &actor->world_x, &actor->world_y)) {
            /* See viewport rule above: confinement has a distinct visual
             * anchor even though player->node remains the exit route node. */
        } else if (!actor_flow->move_active && map != NULL &&
            player->node > 0 &&
            (size_t)player->node < map->route_count) {
            /*
             * Confinement and several event handlers teleport the logical
             * player by changing its route node.  The original immediately
             * draws the actor at that node; interpolation coordinates are
             * authoritative only while a route move is active.
             */
            actor->world_x = map->routes[player->node].x;
            actor->world_y = map->routes[player->node].y;
        } else {
            actor->world_x = actor_flow->visual_x;
            actor->world_y = actor_flow->visual_y;
        }
        actor->moving = visual_moving;
        actor->minimap_world_x = actor->world_x;
        actor->minimap_world_y = actor->world_y;
        if (input->tool_effect_actor == i + 1 &&
            confinement) {
            /* 0x4512BF redraws only the main map. 0x43EDA0 refreshes
             * the minimap after the ambulance returns, not at frame 30. */
            if (input->tool_effect_has_world_position) {
                actor->minimap_world_x = input->tool_effect_world_x;
                actor->minimap_world_y = input->tool_effect_world_y;
            } else if (map != NULL && input->tool_effect_node > 0 &&
                       (size_t)input->tool_effect_node < map->route_count) {
                actor->minimap_world_x = map->routes[input->tool_effect_node].x;
                actor->minimap_world_y = map->routes[input->tool_effect_node].y;
            }
        }
        actor->walk_frame = actor_flow->walk_frame;
        if ((player->play_state & 0xff) == 0 && !actor_flow->move_active &&
            map != NULL && player->node > 0 && player->next_node > 0 &&
            (size_t)player->node < map->route_count &&
            (size_t)player->next_node < map->route_count)
            actor_direction = rich4_game_actor_direction_from_delta(
                map->routes[player->next_node].x -
                    map->routes[player->node].x,
                map->routes[player->next_node].y -
                    map->routes[player->node].y);
        actor->visual_direction =
            (8 - model.viewport.direction + actor_direction) & 7;
        /* 0x40D7C4 changes EA2 from the action slot (2) to the movement
         * slot (1) as soon as the action animation ends and the dice popup
         * starts. The original then draws the dice window without rebuilding
         * the map, so the actor remains visually frozen on the last action
         * frame until movement begins. SDL repaints the map every frame and
         * must keep that action resource explicitly during the popup. */
        resource_moving = actor->moving ||
            (input->turn_flow_active &&
             input->turn_state == GAME_TURN_STATE_PREROLL &&
             input->dice_popup_active && i == state->current_player);
        actor->primary_part = rich4_game_actor_part(player, special, actor->moving,
                                                    &actor->overlay_part);
        actor->fallback_part = resource_moving ? 1 : 0;
        actor->part_confidence = VIEW_EVIDENCE_PARTIAL;
        if (input->turn_flow_active && input->turn_state == GAME_TURN_STATE_PREROLL &&
            i == state->current_player) {
            actor->use_action_part = 1;
            actor->action_part = rich4_game_actor_action_part(player, special);
            actor->action_frame = input->turn_state_frame;
            actor->action_holds_last_frame = input->dice_popup_active;
        }
        if (resource_moving && !actor->use_action_part)
            actor->primary_part = rich4_game_actor_part(
                player, special, 1, &actor->overlay_part);
        if (input->engineering_attack_active &&
            input->engineering_attack_player == i) {
            actor->use_action_part = 1;
            actor->action_part = 12;
            actor->action_frame = input->engineering_attack_frame;
            actor->action_holds_last_frame = 0;
            actor->overlay_part = -1;
        }
    }
    model.special_actor_count = GAME_SPECIAL_ACTOR_MODEL_COUNT;
    for (i = 0; i < model.special_actor_count; i++) {
        GameSpecialActorView *actor = &model.special_actors[i];
        int node = state->special_player_node[i];
        int previous = state->special_player_previous_node[i];
        int direction = 0;
        int sleepwalking = (state->special_player_status[i][1] & 0xff) != 0;
        actor->slot = i;
        actor->overlay_chunk = -1;
        if (map == NULL || state->special_player_f10[i] != 0 ||
            node <= 0 || (size_t)node >= map->route_count)
            continue;
        actor->visible = 1;
        if (input->special_actor.active &&
            input->special_actor.slot == i) {
            actor->world_x = input->special_actor.visual_x;
            actor->world_y = input->special_actor.visual_y;
            actor->visual_direction =
                (8 - model.viewport.direction +
                 input->special_actor.direction) & 7;
            actor->resource_part = input->special_actor.resource_part;
            actor->animation_frame = input->special_actor.animation_frame;
            if (sleepwalking)
                actor->overlay_chunk = wrap_index_view(
                    input->special_actor.animation_frame, 6);
            continue;
        }
        actor->world_x = map->routes[node].x;
        actor->world_y = map->routes[node].y;
        if (previous > 0 && (size_t)previous < map->route_count)
            direction = rich4_game_actor_direction_from_delta(
                map->routes[node].x - map->routes[previous].x,
                map->routes[node].y - map->routes[previous].y);
        actor->visual_direction =
            (8 - model.viewport.direction + direction) & 7;
        /* 0x408970/0x408988 select the third fixed-actor resource on a
         * signed/high-bit route.  Idle rescued actors use part 0 elsewhere. */
        actor->resource_part =
            (map->routes[node].flags & 0x80000000u) != 0 ? 2 : 0;
        if (sleepwalking) actor->overlay_chunk = 0;
    }
    model.hud.active_tab = input->active_info_tab >= 0 && input->active_info_tab <= 3
        ? input->active_info_tab : 0;
    model.hud.top_button_hover = input->top_button_hover;
    model.hud.minimap_rotate_pressed = input->minimap_rotate_pressed;
    if (state->player_count > 0) {
        int current = wrap_index_view(state->current_player, state->player_count);
        const GamePlayerState *player = &state->players[current];
        int property;
        model.hud.current_player = current;
        model.hud.visible = state->active != 0;
        model.hud.character = player->character;
        if (player->linked_player >= 0 && player->linked_player < state->player_count &&
            player->linked_player != current)
            model.hud.ally_character = state->players[player->linked_player].character;
        model.hud.loan_days_remaining = rich4_bank_loan_days_remaining(state, current);
        model.hud.cash = player->cash;
        model.hud.bank = player->bank;
        /* sub_415F69 calls sub_4239B9 for the third funds value. */
        model.hud.total_assets = rich4_bank_total_assets(state, current);
        model.hud.cash_and_bank = player->cash + player->bank;
        for (property = 0; property < state->land_count &&
                           property < GAME_MAX_MAP_PROPERTIES; property++) {
            if (state->land_owner[property] != current) continue;
            model.hud.owned_property_count++;
            if (state->land_type[property] != 0)
                model.hud.owned_chain_count++;
        }
        for (property = 0; property < state->facility_count &&
                           property < GAME_MAX_MAP_PROPERTIES; property++) {
            if (state->facility_owner[property] != current) continue;
            model.hud.owned_property_count++;
            if (state->facility_level[property] != 0)
                model.hud.built_facility_count++;
        }
        /* 0x41646C scans all 12 holdings, accumulating amount * current
         * price and amount * average cost through the original rounding
         * helper.  It then counts company records whose owner is player + 1.
         * The SDL domain normalizes those owners to zero-based indices. */
        for (property = 0; property < GAME_STOCK_COUNT; property++) {
            int amount = state->player_stock_amount[current][property];
            if (amount == 0) continue;
            model.hud.stock_market_value = (int)(
                (float)model.hud.stock_market_value +
                (float)amount * state->stocks[property].f20 + 0.5f);
            model.hud.stock_cost = (int)(
                (float)model.hud.stock_cost +
                (float)amount * state->player_stock_average_cost[current][property] +
                0.5f);
        }
        for (property = 0; property < state->company_count &&
                           property < GAME_MAX_MAP_PROPERTIES; property++) {
            int stock = state->company_stock_index[property];
            if (stock >= 0 && stock < GAME_STOCK_COUNT &&
                state->stock_company_owner[stock] == current)
                model.hud.stock_company_control_count++;
        }
        model.hud.points = player->points;
        model.hud.loan = player->loan;
        model.hud.insurance_days = player->days_assurance;
        model.dice.popup_visible = input->dice_popup_active != 0;
        model.dice.count = input->dice_count;
        memcpy(model.dice.faces, input->dice_faces, sizeof(model.dice.faces));
        model.dice.animation_frame = input->dice_anim_frame;
        model.dice.result_visible = input->dice_result_visible != 0;
        model.dice.visual_direction = model.actors[current].visual_direction;
        model.dice.status_visible = input->dice_status_visible &&
            !input->turn_flow_active && input->landing_pending_player == 0 &&
            !input->actors[current].first_turn_landing_pending &&
            (input->actors[current].dirty_flags & GAME_PLAYER_DIRTY_TURN) == 0;
        model.dice.status_x = input->dice_status_x;
        model.dice.status_y = input->dice_status_y;
        model.dice.status_variant = input->dice_status_variant;
        model.dice.traffic_method = player->traffic_method & 3;
        model.dice.available_dice = player->ndices;
    }
    model.hud.price_index = state->price_index > 0 ? state->price_index : 1;
    if (model.current_actor >= 4 && model.current_actor < 8) {
        model.hud.special_actor = model.current_actor;
        model.hud.special_portrait_character =
            state->special_player_actor_type[model.current_actor - 4];
    }
    build_calendar_view(state, &model.calendar);
    model.land_count = state->land_count;
    if (model.land_count < 0) model.land_count = 0;
    if (model.land_count > GAME_MAX_MAP_PROPERTIES)
        model.land_count = GAME_MAX_MAP_PROPERTIES;
    for (i = 0; i < model.land_count; i++) {
        model.lands[i].owner = state->land_owner[i];
        model.lands[i].type = state->land_type[i];
        model.lands[i].level = state->land_level[i];
        if (input->demon_previous_building_target.kind == GAME_DOMAIN_LAND &&
            input->demon_previous_building_target.id == i) {
            model.lands[i].type = input->demon_previous_building_type;
            model.lands[i].level = input->demon_previous_building_level;
        }
        /* 0x40B110 commits +26 before the FLC; the old background survives
         * until 0x450F04's frame-44 redraw. Never roll back the game state. */
        if (((input->worker_previous_level_target.kind == GAME_DOMAIN_LAND &&
              input->worker_previous_level_target.id == i) ||
             (input->angel_previous_level_target.kind == GAME_DOMAIN_LAND &&
              input->angel_previous_level_target.id == i)) &&
            model.lands[i].level > 0)
            model.lands[i].level--;
        model.lands[i].group = state->land_group[i];
        model.lands[i].price_status = state->land_price_status[i];
        if (input->property_flash_adjust != 0 &&
            rich4_property_toll_flash_contains(
                state, input->property_presentation_land, i))
            model.lands[i].flash_adjust = input->property_flash_adjust;
    }
    model.facility_count = state->facility_count;
    if (model.facility_count < 0) model.facility_count = 0;
    if (model.facility_count > GAME_MAX_MAP_PROPERTIES)
        model.facility_count = GAME_MAX_MAP_PROPERTIES;
    for (i = 0; i < model.facility_count; i++) {
        model.facilities[i].owner = state->facility_owner[i];
        model.facilities[i].type = state->facility_type[i];
        model.facilities[i].level = state->facility_level[i];
        if (input->demon_previous_building_target.kind == GAME_DOMAIN_FACILITY &&
            input->demon_previous_building_target.id == i) {
            model.facilities[i].type = input->demon_previous_building_type;
            model.facilities[i].level = input->demon_previous_building_level;
        }
        if (((input->worker_previous_level_target.kind == GAME_DOMAIN_FACILITY &&
              input->worker_previous_level_target.id == i) ||
             (input->angel_previous_level_target.kind == GAME_DOMAIN_FACILITY &&
              input->angel_previous_level_target.id == i)) &&
            model.facilities[i].level > 0)
            model.facilities[i].level--;
        model.facilities[i].research_selection =
            state->facility_research_selection[i];
        model.facilities[i].research_turns = state->facility_research_turns[i];
        model.facilities[i].price_status = state->facility_price_status[i];
    }
    model.company_count = state->company_count;
    if (model.company_count < 0) model.company_count = 0;
    if (model.company_count > GAME_MAX_MAP_PROPERTIES)
        model.company_count = GAME_MAX_MAP_PROPERTIES;
    for (i = 0; i < model.company_count; i++) {
        int stock = state->company_stock_index[i];
        model.companies[i].owner =
            stock >= 0 && stock < GAME_STOCK_COUNT
                ? state->stock_company_owner[stock] : -1;
    }
    model.steps_remaining = input->special_actor.active
        ? input->special_actor.steps_remaining : input->steps_remaining;
    /* 0x409971: one n9_0 display shared by every current actor.  Ordinary
     * players suppress it for play-state 0x30 or the four bytes at +50. */
    if (state->tool_robot.active) {
        model.current_actor = 8;
        model.steps_remaining = state->tool_robot.steps_remaining -
            (input->tool_robot_arrived != 0);
        model.show_steps = input->tool_robot_started;
    } else if (input->special_actor.active) {
        model.show_steps = 1;
    } else if (state->current_player >= 0 &&
               state->current_player < state->player_count) {
        const GamePlayerState *player = &state->players[state->current_player];
        model.show_steps = input->turn_flow_active &&
            !(player->play_state & 0x30) &&
            !(player->delayed_status_block & 0xff) &&
            !(player->days_disappearing & 0xff) &&
            !(player->days_in_prison & 0xff) &&
            !(player->days_in_hospital & 0xff);
    }
    if (model.steps_remaining <= 0) model.show_steps = 0;
    model.property_flash_group = input->property_flash_adjust != 0
        ? input->property_presentation_group : -1;
    model.property_flash_owner = input->property_presentation_owner;
    model.property_flash_adjust = input->property_flash_adjust;
    model.property_flash_active = input->property_flash_active;
    *out_model = model;
    return 1;
}
