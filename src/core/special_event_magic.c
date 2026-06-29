#include "core/special_event_internal.h"
#include "core/auction.h"
#include "core/confinement.h"
#include "core/card.h"
#include "core/insurance.h"
#include "core/ledger.h"
#include "core/stock.h"
#include "core/tool.h"
#include "core/victory.h"
#include "core/game_event_payload.h"

#include <limits.h>
#include <string.h>

unsigned int rich4_magic_house_action4_eligible(const GameState *state)
{
    return rich4_magic_house_target_mask(state, 4);
}

static int player_property_count(const GameState *state, int player,
                                 int built_only)
{
    int count = 0;
    int i;
    for (i = 0; i < state->land_count; i++)
        if (state->land_owner[i] == player &&
            (!built_only || state->land_level[i] != 0))
            count++;
    for (i = 0; i < state->facility_count; i++)
        if (state->facility_owner[i] == player &&
            (!built_only || state->facility_level[i] != 0))
            count++;
    return count;
}

unsigned int rich4_magic_house_target_mask(const GameState *state,
                                            int condition)
{
    int values[GAME_MAX_PLAYERS] = {0};
    unsigned int mask = 0;
    int highest = 0;
    int i;
    if (state == NULL || condition < 0 || condition >= 12)
        return 0;
    for (i = 0; i < state->player_count; i++) {
        const GamePlayerState *p = &state->players[i];
        if (!rich4_special_event_active_player(state, i)) continue;
        switch (condition) {
        case 0: values[i] = rich4_player_total_assets(state, i, NULL); break;
        case 1: values[i] = player_property_count(state, i, 0); break;
        case 2: values[i] = player_property_count(state, i, 1); break;
        case 3: values[i] = p->cash; break;
        case 4: values[i] = p->bank; break;
        case 5: values[i] = p->points; break;
        case 6: if ((p->traffic_method & 3) == 0) mask |= 1u << i; break;
        case 7: if ((p->traffic_method & 3) == 1) mask |= 1u << i; break;
        case 8: if ((p->traffic_method & 3) == 2) mask |= 1u << i; break;
        case 9: if (p->god_info != 0) mask |= 1u << i; break;
        case 10: if (p->sex != 0) mask |= 1u << i; break;
        case 11: if (p->sex == 0) mask |= 1u << i; break;
        }
        if (condition <= 5 && values[i] > highest) highest = values[i];
    }
    if (condition > 5 || highest == 0) return mask;
    for (i = 0; i < state->player_count; i++)
        if (rich4_special_event_active_player(state, i) && values[i] == highest)
            mask |= 1u << i;
    return mask;
}

GameSpecialEventResult rich4_magic_house_choose_condition(
    GameState *state, int *out_condition, unsigned int *out_target_mask)
{
    Rich4Rng rng;
    if (state == NULL || out_condition == NULL || out_target_mask == NULL)
        return GAME_SPECIAL_EVENT_INVALID;
    rng.state = state->ai_rng_state;

    for (;;) {
        int condition = rich4_rng_original_state_range(&rng, 12);
        unsigned int mask =
            rich4_magic_house_target_mask(state, condition);
        if (mask != 0) {
            state->ai_rng_state = rng.state;
            *out_condition = condition;
            *out_target_mask = mask;
            return GAME_SPECIAL_EVENT_APPLIED;
        }
    }
}

static int magic_current_property(const GameState *state, int player,
                                  GameDomainRef *out)
{
    int node;
    int raw;
    if (!rich4_special_event_active_player(state, player) || out == NULL)
        return 0;
    node = state->players[player].node;
    if (node < 0 || node >= state->tool_route_node_count)
        return 0;
    raw = state->route_object_raw[node];
    if (raw >= 0x7d1 && raw < 0xfa0) {
        out->kind = GAME_DOMAIN_LAND;
        out->id = raw - 0x7d1;
        return out->id >= 0 && out->id < state->land_count;
    }
    if (raw >= 0xfa1 && raw < 0x1770) {
        out->kind = GAME_DOMAIN_FACILITY;
        out->id = raw - 0xfa1;
        return out->id >= 0 && out->id < state->facility_count;
    }
    return 0;
}

int rich4_special_event_magic_sell_cards(GameState *state, int player)
{
    int total = 0;
    int slot;
    for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS; slot++) {
        int original = state->players[player].card_inventory[slot];
        if (original > 0 && original <= GAME_CARD_COUNT) {
            int card = original - 1;
            total += rich4_card_catalog[card].shop_price;
            if (state->card_amount[card] != 255)
                state->card_amount[card]++;
            state->players[player].card_inventory[slot] = 0;
        }
    }
    state->players[player].points =
        (uint16_t)(state->players[player].points + total);
    return total;
}

int rich4_special_event_magic_sell_tools(GameState *state, int player)
{
    int total = 0;
    int tool;
    int method = state->players[player].traffic_method & 3;
    if (method == 1 && state->players[player].tool_inventory[4] != 255)
        state->players[player].tool_inventory[4]++;
    else if (method == 2 &&
             state->players[player].tool_inventory[5] != 255)
        state->players[player].tool_inventory[5]++;
    else if (method == 3 &&
             state->players[player].tool_inventory[11] != 255)
        state->players[player].tool_inventory[11]++;
    state->players[player].traffic_method = 0;
    state->players[player].ndices = 1;
    for (tool = 0; tool < GAME_TOOL_COUNT; tool++) {
        int quantity = state->players[player].tool_inventory[tool];
        total += quantity * rich4_tool_catalog[tool].shop_price;
        if (tool < 8) {
            int available = state->tool_amount[tool] + quantity;
            state->tool_amount[tool] =
                (uint8_t)(available > 255 ? 255 : available);
        }
        state->players[player].tool_inventory[tool] = 0;
    }
    state->players[player].points =
        (uint16_t)(state->players[player].points + total);
    return total;
}

static int magic_gain_random_card(GameState *state, int player,
                                  GameEventQueue *events)
{
    Rich4Rng rng;
    int total = 0;
    int pick;
    int card;
    for (card = 0; card < GAME_CARD_COUNT; card++)
        total += state->card_amount[card];
    if (total == 0 ||
        rich4_card_inventory_count(state, player) >=
            GAME_PLAYER_CARD_SLOTS)
        return 1;
    rng.state = state->ai_rng_state;

    pick = rich4_rng_original_state_range(&rng, total);
    state->ai_rng_state = rng.state;
    for (card = 0; card < GAME_CARD_COUNT; card++) {
        if (pick < state->card_amount[card]) break;
        pick -= state->card_amount[card];
    }
    return card < GAME_CARD_COUNT &&
           rich4_card_inventory_add_from_pool(
               state, player, card, events);
}

static int magic_adjust_property(GameState *state, int player, int build,
                                 GameEventQueue *events)
{
    static const unsigned char facility_max_level[5] = {1, 5, 5, 1, 5};
    GameDomainRef target;
    int owner;
    int old_value;
    int new_value;
    int level;
    if (state->players[player].delayed_status_block != 0 ||
        !magic_current_property(state, player, &target))
        return 1;
    if (target.kind == GAME_DOMAIN_LAND) {
        int id = target.id;
        if (build) {
            if (!((state->land_type[id] == 0 &&
                   state->land_level[id] < 5) ||
                  (state->land_type[id] == 1 &&
                   state->land_level[id] == 0)))
                return 1;
            old_value = state->land_value[id];
            state->land_level[id]++;
        } else {
            if (state->land_level[id] == 0) return 1;
            old_value = state->land_value[id];
            state->land_level[id]--;
            if (state->land_type[id] != 0) {
                state->land_level[id] = 0;
                state->land_type[id] = 0;
            }
        }
        owner = state->land_owner[id];
        state->land_value[id] = state->land_price[id] +
            state->land_level[id] * state->land_price_per_level[id];
        new_value = state->land_value[id];
        level = state->land_level[id];
    } else {
        int id = target.id;
        if (build) {
            int type = state->facility_type[id];
            if (type < 0 || type > 4)
                return 1;
            old_value = state->facility_value[id];
            if (state->facility_level[id] == 0 &&
                (state->players[player].play_state & 0xff) != 1) {
                Rich4Rng rng;
                rng.state = state->ai_rng_state;

                type = 1 + rich4_rng_original_state_range(&rng, 4);
                state->ai_rng_state = rng.state;
                state->facility_type[id] = type;
            } else if (state->facility_level[id] >=
                       facility_max_level[type]) {
                return 1;
            }
            state->facility_level[id]++;
        } else {
            if (state->facility_level[id] == 0) return 1;
            old_value = state->facility_value[id];
            state->facility_level[id]--;
            if (state->facility_level[id] == 0)
                state->facility_type[id] = 0;
        }
        owner = state->facility_owner[id];
        state->facility_value[id] = state->facility_price[id] +
            state->facility_level[id] *
                state->facility_price_per_level[id];
        new_value = state->facility_value[id];
        level = state->facility_level[id];
    }
    if (owner >= 0 && owner < state->player_count)
        state->fixed_asset_value[owner] += new_value - old_value;
    return rich4_game_event_emit_typed_domain(
        events, rich4_game_event_property_level_changed_from_fields, player,
        target,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, owner), level,
        new_value, build ? 1u : 0u);
}

static int magic_add_relation(GameState *state, int target, int actor)
{
    int64_t value;
    if (target == actor) return 1;
    value = (int64_t)state->players[target].relation_balance[actor] +
            (int64_t)90 * state->price_index;
    if (value > INT_MAX || value < INT_MIN) return 0;
    state->players[target].relation_balance[actor] = (int)value;
    return 1;
}

static int magic_turn_around(GameState *state, int player)
{
    Rich4Rng rng;
    int candidates[4];
    int count = 0;
    int node;
    int edge;
    if (!rich4_special_event_active_player(state, player)) return 0;
    node = state->players[player].node;
    if (node <= 0 || node >= state->tool_route_node_count) {
        state->players[player].next_node = 0;
        return 1;
    }
    for (edge = 0; edge < 4; edge++) {
        int next = state->tool_route_next[node][edge];
        if (next <= 0 || next >= state->tool_route_node_count ||
            next == state->players[player].next_node ||
            (state->tool_route_edge_blocked[node] & (1u << edge)) != 0)
            continue;
        candidates[count++] = next;
    }
    if (count == 0) {
        state->players[player].next_node = 0;
        return 1;
    }
    rng.state = state->ai_rng_state;

    state->players[player].next_node =
        candidates[rich4_rng_original_state_range(&rng, count)];
    state->ai_rng_state = rng.state;
    return 1;
}

static int magic_consume_immunity(
    GameState *state, int player, GameEventQueue *events)
{
    int slot;
    if (!rich4_card_inventory_has(state, player, 20))
        return 0;
    for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS; slot++)
        if (state->players[player].card_inventory[slot] == 21) {
            /* 0x444691 -> 0x441343 compacts the remaining raw slots. */
            int i;
            for (i=slot;i+1<GAME_PLAYER_CARD_SLOTS;i++)
                state->players[player].card_inventory[i]=
                    state->players[player].card_inventory[i+1];
            state->players[player].card_inventory[GAME_PLAYER_CARD_SLOTS-1]=0;
            if (state->card_amount[20] != 255)
                state->card_amount[20]++;
            return rich4_game_event_emit_typed_domain(
                events, rich4_game_event_card_used_from_fields, player,
                rich4_game_domain_ref(GAME_DOMAIN_CARD, 20),
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
                0, state->card_amount[20], 0) ? 1 : -1;
        }
    return -1;
}

static GameSpecialEventResult rich4_magic_house_apply_action_impl(
    GameState *state, int actor, int condition, int action,
    unsigned int target_mask, GameEventQueue *events, int defer_confinement)
{
    GameState next;
    GameEventQueue staged;
    int player;
    if (state == NULL || events == NULL || !rich4_special_event_active_player(state, actor) ||
        condition < 0 || condition >= 12 || action < 0 || action >= 12 ||
        target_mask == 0 ||
        (target_mask &
         ~rich4_magic_house_target_mask(state, condition)) != 0)
        return GAME_SPECIAL_EVENT_INVALID;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    for (player = 0; player < next.player_count; player++) {
        int value = 0;
        if ((target_mask & (1u << player)) == 0) continue;
        switch (action) {
        case 0:
            value = rich4_special_event_magic_sell_cards(&next, player);
            break;
        case 1:
            /*
             * The route layer sequences the three full fortune
             * presentations.  Keeping this action side-effect free here
             * avoids collapsing their phase-0/phase-1 callbacks.
             */
            continue;
        case 2:
            value = magic_consume_immunity(
                &next, player, &staged);
            if (!magic_add_relation(&next, player, actor) ||
                value < 0)
                return GAME_SPECIAL_EVENT_INVALID;
            if (value == 0 &&
                (defer_confinement ? rich4_confinement_request_enter : rich4_confinement_enter)(
                    &next, player, GAME_CONFINEMENT_PRISON, 3,
                    &staged) != GAME_CONFINEMENT_APPLIED)
                return GAME_SPECIAL_EVENT_INVALID;
            value = value == 0 ? 3 : 0;
            break;
        case 3:
            next.players[player].days_stopping =
                (next.players[player].days_stopping + 1) & 0x7f;
            value = next.players[player].days_stopping;
            break;
        case 4:
            if (next.players[player].cash > 0 &&
                next.players[player].bank >
                    INT_MAX - next.players[player].cash)
                return GAME_SPECIAL_EVENT_INVALID;
            value = next.players[player].cash;
            next.players[player].bank += value;
            next.players[player].cash = 0;
            break;
        case 5:
            if (!magic_adjust_property(
                    &next, player, 1, &staged))
                return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
            break;
        case 6:
            if (!magic_gain_random_card(
                    &next, player, &staged))
                return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
            break;
        case 7:
            if (!magic_turn_around(&next, player))
                return GAME_SPECIAL_EVENT_INVALID;
            value = next.players[player].next_node;
            break;
        case 8:
            value = rich4_special_event_magic_sell_tools(&next, player);
            break;
        case 9:
            if (!magic_adjust_property(
                    &next, player, 0, &staged))
                return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
            break;
        case 10:
            value = magic_consume_immunity(
                &next, player, &staged);
            if (!magic_add_relation(&next, player, actor) ||
                value < 0)
                return GAME_SPECIAL_EVENT_INVALID;
            if (value == 0 &&
                (defer_confinement ? rich4_confinement_request_enter : rich4_confinement_enter)(
                    &next, player, GAME_CONFINEMENT_HOSPITAL, 3,
                    &staged) != GAME_CONFINEMENT_APPLIED)
                return GAME_SPECIAL_EVENT_INVALID;
            value = value == 0 ? 3 : 0;
            break;
        case 11:
            {
                GameDomainRef property;
                GameEvent request;
                if (next.players[player].delayed_status_block==0 &&
                    magic_current_property(&next,player,&property)) {
                    request=rich4_game_event_auction_request(
                        actor,property,player,1,
                        GAME_AUCTION_TRIGGER_MAGIC_HOUSE,0);
                    if (!rich4_game_event_queue_push(&staged,&request))
                        return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
                }
            }
            break;
        }
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_special_event_committed_from_fields,
                actor,
                rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
                rich4_game_domain_ref(GAME_DOMAIN_NONE, action),
                action, value, (unsigned int)condition))
            return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    }
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    *state = next;
    return GAME_SPECIAL_EVENT_APPLIED;
}

GameSpecialEventResult rich4_special_event_begin_magic_action4(
    GameState *state, int actor, unsigned int target_mask, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    unsigned int eligible;
    if (state==NULL || events==NULL || !rich4_special_event_active_player(state,actor) || target_mask==0 ||
        state->special_event_phase==GAME_SPECIAL_PHASE_PREPARED) return GAME_SPECIAL_EVENT_INVALID;
    eligible=rich4_magic_house_action4_eligible(state);
    if ((target_mask & ~eligible)!=0) return GAME_SPECIAL_EVENT_INVALID;
    next=*state;
    rich4_special_event_set_pending(&next,GAME_SPECIAL_EVENT_MAGIC_HOUSE,actor,4,4,-1,18,0,
                target_mask,1,GAME_SPECIAL_PHASE_PREPARED);
    rich4_game_event_queue_reset(&staged);
    if (!rich4_special_event_emit_prepared(&staged,&next) || !rich4_game_event_queue_append(events,&staged))
        return GAME_SPECIAL_EVENT_EVENT_CAPACITY;
    *state=next; return GAME_SPECIAL_EVENT_PREPARED;
}

GameSpecialEventResult rich4_magic_house_apply_action(GameState *state, int actor, int condition, int action, unsigned int target_mask, GameEventQueue *events)
{
    return rich4_magic_house_apply_action_impl(state, actor, condition, action, target_mask, events, 0);
}

GameSpecialEventResult rich4_magic_house_apply_action_deferred_confinement(GameState *state, int actor, int condition, int action, unsigned int target_mask, GameEventQueue *events)
{
    return rich4_magic_house_apply_action_impl(state, actor, condition, action, target_mask, events, 1);
}
