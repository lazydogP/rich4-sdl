#include "core/god.h"

#include <limits.h>
#include <stdint.h>

#include "core/card.h"
#include "core/card_internal.h"
#include "core/game_event_payload.h"
#include "core/ledger.h"
#include "core/rng.h"

/* These are the three signed words written by 0x40EAD7 and undone by
 * 0x40E14D.  The original tables are indexed by the one-based road-god
 * type, not by the dynamic-object slot. */
static const int god_income_modifier[GAME_GOD_TYPE_COUNT + 1] = {
    0, -100, -200, -100, -200, 100, 200, 100, 200, -100,
    100, 0, -500, 0, 0, 1000, 0, 0, 0
};
static const int god_investment_modifier[GAME_GOD_TYPE_COUNT + 1] = {
    0, 100, 150, 0, 0, -60, -100, 0, 0, 60,
    -60, 0, 0, 0, 0, -200, 0, 0, 0
};
static const int god_land_modifier[GAME_GOD_TYPE_COUNT + 1] = {
    0, 0, 0, 100, 150, 0, 0, -60, -100, 60,
    -60, 0, 0, 0, 0, -200, 0, 0, 0
};
static const unsigned char god_facility_max_level[5] = {1, 5, 5, 1, 5};

typedef struct GodEffectResult {
    int card_1;
    int card_2;
    int card_count;
    int lost_count;
} GodEffectResult;

static void reset_god_effect_result(GodEffectResult *result)
{
    if (result == NULL) return;
    result->card_1 = -1;
    result->card_2 = -1;
    result->card_count = 0;
    result->lost_count = 0;
}

static int push_god_event(GameEventQueue *events,
                          Rich4GameEventConstructor constructor,
                          int player, int subject, int value, int value2)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject;
    fields.value = value;
    fields.value2 = value2;
    fields.flags = 0u;
    fields.subject_ref.kind = GAME_DOMAIN_NONE;
    fields.subject_ref.id = -1;
    fields.target_ref.kind = GAME_DOMAIN_NONE;
    fields.target_ref.id = -1;
    return rich4_game_event_emit_typed(events, constructor, fields);
}

int rich4_god_stat_deltas(int type, int *out_income,
                          int *out_investment, int *out_land)
{
    if (out_income != NULL) *out_income = 0;
    if (out_investment != NULL) *out_investment = 0;
    if (out_land != NULL) *out_land = 0;
    if (type <= 0 || type > GAME_GOD_TYPE_COUNT) return 0;
    if (out_income != NULL) *out_income = god_income_modifier[type];
    if (out_investment != NULL)
        *out_investment = god_investment_modifier[type];
    if (out_land != NULL) *out_land = god_land_modifier[type];
    return 1;
}

static int add_int_delta(int *value, int delta)
{
    int64_t next;
    if (value == NULL) return 0;
    next = (int64_t)*value + delta;
    if (next < INT_MIN || next > INT_MAX) return 0;
    *value = (int)next;
    return 1;
}

static int apply_god_stat_delta(GamePlayerState *player, int type, int sign)
{
    int income;
    int investment;
    int land;
    if (player == NULL || (sign != 1 && sign != -1) ||
        !rich4_god_stat_deltas(type, &income, &investment, &land))
        return 0;
    return add_int_delta(&player->god_income_modifier, sign * income) &&
           add_int_delta(&player->god_investment_modifier, sign * investment) &&
           add_int_delta(&player->god_land_modifier, sign * land);
}

static int emit_god_acquired(GameEventQueue *events, int player,
                             int god_type, int object_slot,
                             int previous_type, const GodEffectResult *result)
{
    Rich4GameEventFields fields;
    unsigned int flags = previous_type == 0 ? RICH4_GOD_ACQUIRED_FIRST : 0u;
    if (rich4_god_money_digits(god_type) != 0)
        flags |= RICH4_GOD_ACQUIRED_MONEY_PENDING;
    if (god_type == 15) flags |= RICH4_GOD_ACQUIRED_DEATH_PENDING;
    fields.player = player;
    fields.subject = god_type;
    fields.value = object_slot;
    fields.value2 = previous_type;
    fields.subject_ref = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    fields.target_ref = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    if (result != NULL && result->card_count > 0 &&
        result->card_1 >= 0 && result->card_1 < GAME_CARD_COUNT) {
        fields.subject_ref = rich4_game_domain_ref(
            GAME_DOMAIN_CARD, result->card_1);
        flags |= RICH4_GOD_ACQUIRED_RESULT_CARD1;
    }
    if (result != NULL && result->card_count > 1 &&
        result->card_2 >= 0 && result->card_2 < GAME_CARD_COUNT) {
        fields.target_ref = rich4_game_domain_ref(
            GAME_DOMAIN_CARD, result->card_2);
        flags |= RICH4_GOD_ACQUIRED_RESULT_CARD2;
    }
    if (result != NULL && result->lost_count > 0) {
        unsigned int lost = (unsigned int)result->lost_count;
        if (lost > 0xffu) lost = 0xffu;
        flags |= lost << RICH4_GOD_ACQUIRED_LOST_COUNT_SHIFT;
    }
    fields.flags = flags;
    return rich4_game_event_emit_typed(
        events, rich4_game_event_god_acquired_from_fields, fields);
}

static GameDomainRef god_ref(int object_slot)
{
    return rich4_game_domain_ref(GAME_DOMAIN_GOD, object_slot - 1);
}

static int emit_inventory_delta(GameEventQueue *events, int player,
                                GameDomainRef subject, int delta, int count)
{
    return rich4_game_event_emit_typed_domain(
        events, rich4_game_event_inventory_changed_from_fields, player,
        subject, rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player), delta,
        count, 0);
}

static int discard_card_slot(GameState *state, int player, int slot,
                             GameEventQueue *events, int emit_event)
{
    int original;
    int card;
    int i;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count || slot < 0 ||
        slot >= GAME_PLAYER_CARD_SLOTS)
        return 0;
    original = state->players[player].card_inventory[slot];
    if (original <= 0 || original > GAME_CARD_COUNT) return 0;
    card = original - 1;
    /* 0x441343 receives an ID, even after a random/indexed selection. */
    for (i = 0; i < slot; i++) {
        if (state->players[player].card_inventory[i] == original) {
            slot = i;
            break;
        }
    }
    for (i = slot; i + 1 < GAME_PLAYER_CARD_SLOTS; i++)
        state->players[player].card_inventory[i] =
            state->players[player].card_inventory[i + 1];
    state->players[player].card_inventory[GAME_PLAYER_CARD_SLOTS - 1] = 0;
    if (state->card_amount[card] < 255) state->card_amount[card]++;
    return !emit_event || emit_inventory_delta(
        events, player, rich4_game_domain_ref(GAME_DOMAIN_CARD, card), -1,
        rich4_card_inventory_count(state, player));
}

static int lowest_card_slot(const GameState *state, int player)
{
    int slot;
    int result = -1;
    int lowest = INT_MAX;
    if (state == NULL || player < 0 || player >= state->player_count)
        return -1;
    for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS; slot++) {
        int original = state->players[player].card_inventory[slot];
        const GameCardInfo *info;
        if (original <= 0 || original > GAME_CARD_COUNT) continue;
        info = rich4_card_info(original - 1);
        if (info != NULL && info->metadata_f6 < lowest) {
            lowest = info->metadata_f6;
            result = slot;
        }
    }
    return result;
}

static int draw_god_card(GameState *state, int player, GameEventQueue *events,
                         int *out_card)
{
    Rich4Rng rng;
    int total = 0;
    int pick;
    int card;
    int hand;
    if (out_card != NULL) *out_card = -1;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count)
        return 0;
    for (card = 0; card < GAME_CARD_COUNT; card++)
        total += state->card_amount[card];
    if (total <= 0) return 1;
    hand = rich4_card_inventory_count(state, player);
    if (hand < 0) return 0;
    rng.state = state->ai_rng_state;

    pick = rich4_rng_state_range(&rng, total);
    state->ai_rng_state = rng.state;
    for (card = 0; card < GAME_CARD_COUNT; card++) {
        if (pick < state->card_amount[card]) break;
        pick -= state->card_amount[card];
    }
    if (card >= GAME_CARD_COUNT) return 0;
    if (hand >= GAME_PLAYER_CARD_SLOTS) {
        int slot = lowest_card_slot(state, player);
        if (slot < 0 || !discard_card_slot(state, player, slot, events, 1))
            return 0;
    }
    if (!rich4_card_inventory_add_from_pool(state, player, card, events))
        return 0;
    if (out_card != NULL) *out_card = card;
    return 1;
}

static int random_held_card_slot(GameState *state, int player)
{
    Rich4Rng rng;
    int count;
    int wanted;
    int slot;
    if (state == NULL || player < 0 || player >= state->player_count)
        return -1;
    count = rich4_card_inventory_count(state, player);
    if (count <= 0) return -1;
    rng.state = state->ai_rng_state;

    wanted = rich4_rng_state_range(&rng, count);
    state->ai_rng_state = rng.state;
    for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS; slot++) {
        if (state->players[player].card_inventory[slot] == 0) continue;
        if (wanted-- == 0) return slot;
    }
    return -1;
}

static int return_all_cards(GameState *state, int player,
                            GameEventQueue *events, int object_slot)
{
    int count = 0;
    if (state == NULL || events == NULL) return 0;
    while (rich4_card_inventory_count(state, player) > 0) {
        if (!discard_card_slot(state, player, 0, events, 0)) return 0;
        count++;
    }
    return count == 0 || emit_inventory_delta(
        events, player, god_ref(object_slot), -count, 0);
}

static int return_all_tools(GameState *state, int player,
                            GameEventQueue *events, int object_slot)
{
    GamePlayerState *p;
    int tool;
    int count = 0;
    int old_traffic;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count) return 0;
    p = &state->players[player];
    old_traffic = p->traffic_method;
    switch (old_traffic & 3) {
    case 1:
        if (p->tool_inventory[4] < 255) p->tool_inventory[4]++;
        break;
    case 2:
        if (p->tool_inventory[5] < 255) p->tool_inventory[5]++;
        break;
    case 3:
        if (p->tool_inventory[11] < 255) p->tool_inventory[11]++;
        break;
    default:
        break;
    }
    p->traffic_method = 0;
    p->ndices = 1;
    for (tool = 0; tool < GAME_TOOL_COUNT; tool++) {
        int quantity = p->tool_inventory[tool];
        int available;
        count += quantity;
        if (tool < 8) {
            available = state->tool_amount[tool] + quantity;
            state->tool_amount[tool] = (uint8_t)(available > 255 ? 255 : available);
        }
        p->tool_inventory[tool] = 0;
    }
    if (old_traffic != 0 && !rich4_game_event_emit_typed_domain(
            events, rich4_game_event_traffic_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            god_ref(object_slot), 0, old_traffic, 0))
        return 0;
    return count == 0 || emit_inventory_delta(
        events, player, god_ref(object_slot), -count, 0);
}

static int apply_god_effect(GameState *state, int player, int object_slot,
                            int god_type, GameEventQueue *events,
                            GodEffectResult *result)
{
    int i;
    (void)object_slot;
    reset_god_effect_result(result);
    if (state == NULL || events == NULL) return 0;
    switch (god_type) {
    case 1:
    case 2:
    case 5:
    case 6:
        /* The acquisition event opens the original number wheel. Settlement
         * is resumed through rich4_god_resolve_money with its actual result. */
        return 1;
    case 3:
        if (!draw_god_card(state, player, events, &i)) return 0;
        if (result != NULL && i >= 0) {
            result->card_1 = i;
            result->card_count = 1;
        }
        return 1;
    case 4:
        if (!draw_god_card(state, player, events, &i)) return 0;
        if (result != NULL && i >= 0) {
            result->card_1 = i;
            result->card_count = 1;
        }
        if (!draw_god_card(state, player, events, &i)) return 0;
        if (result != NULL && i >= 0) {
            result->card_2 = i;
            result->card_count = 2;
        }
        return 1;
    case 7:
        i = random_held_card_slot(state, player);
        if (i < 0) return 1;
        if (result != NULL && state->players[player].card_inventory[i] > 0)
            result->card_1 = state->players[player].card_inventory[i] - 1;
        if (result != NULL && result->card_1 >= 0)
            result->card_count = 1;
        return discard_card_slot(state, player, i, events, 1);
    case 8:
        i = rich4_card_inventory_count(state, player);
        if (i < 0) return 0;
        i /= 2;
        if (result != NULL) result->lost_count = i;
        {
            int slot;
            /* 0x441ECE increments its index after each compacting removal. */
            for (slot = 0; slot < i; slot++) {
                if (!discard_card_slot(state, player, slot, events, 1))
                    return 0;
            }
        }
        return 1;
    case 15:
        /* 0x40F36E runs only after Data[551] and 0x40E2A2 have finished. */
        return 1;
    default:
        /* Types 9/10/12 are persistent route/property effects.  Their
         * attachment-time state is represented by the exact stat deltas;
         * the landing handlers consume those modifiers later. */
        return 1;
    }
}

GameGodResult rich4_god_resolve_death(GameState *state, int player,
                                     int object_slot, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    if (state == NULL || events == NULL || player < 0 || player >= state->player_count ||
        object_slot <= 0 || state->players[player].god_info != object_slot ||
        rich4_god_player_type(&state->players[player]) != 15)
        return GAME_GOD_INVALID;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!return_all_tools(&next, player, &staged, object_slot) ||
        !return_all_cards(&next, player, &staged, object_slot) ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_GOD_EVENT_CAPACITY;
    *state = next;
    return GAME_GOD_APPLIED;
}

static int god_landing_property(const GameState *state, int node,
                                int *out_facility, int *out_property)
{
    unsigned int raw;
    int property;

    if (out_facility != NULL) *out_facility = 0;
    if (out_property != NULL) *out_property = -1;
    if (state == NULL || node <= 0 || node >= GAME_MAX_MAP_ROUTE_NODES)
        return 0;
    raw = state->route_object_raw[node];
    if (raw >= 0x7d1u && raw < 0xfa0u) {
        property = (int)raw - 0x7d1;
        if (property < 0 || property >= state->land_count ||
            property >= GAME_MAX_MAP_PROPERTIES)
            return 0;
        if (out_property != NULL) *out_property = property;
        return 1;
    }
    if (raw >= 0xfa1u && raw < 0x1770u) {
        property = (int)raw - 0xfa1;
        if (property < 0 || property >= state->facility_count ||
            property >= GAME_MAX_MAP_PROPERTIES)
            return 0;
        if (out_facility != NULL) *out_facility = 1;
        if (out_property != NULL) *out_property = property;
        return 1;
    }
    return 0;
}

static int god_property_value(int facility, const GameState *state, int property,
                              int *out_value)
{
    int64_t value;
    if (out_value == NULL || state == NULL || property < 0 ||
        property >= GAME_MAX_MAP_PROPERTIES)
        return 0;
    if (facility)
        value = (int64_t)state->facility_price[property] +
            (int64_t)state->facility_level[property] *
                state->facility_price_per_level[property];
    else
        value = (int64_t)state->land_price[property] +
            (int64_t)state->land_level[property] *
                state->land_price_per_level[property];
    if (value < 0 || value > INT_MAX) return 0;
    *out_value = (int)value;
    return 1;
}

static int god_add_relation(GameState *state, int source, int target,
                            int64_t amount)
{
    int64_t value;
    if (state == NULL || source < 0 || source >= state->player_count ||
        target < 0 || target >= state->player_count || source == target ||
        amount <= 0)
        return 0;
    value = (int64_t)state->players[source].relation_balance[target] + amount;
    if (value > INT_MAX) return 0;
    state->players[source].relation_balance[target] = (int)value;
    return 1;
}

static int god_adjust_fixed_assets(GameState *state, int old_owner,
                                   int new_owner, int value)
{
    int64_t old_assets;
    int64_t new_assets;
    if (state == NULL || value < 0 || new_owner < 0 ||
        new_owner >= state->player_count)
        return 0;
    if (old_owner >= 0 && old_owner < state->player_count) {
        old_assets = (int64_t)state->fixed_asset_value[old_owner] - value;
        if (old_assets < 0) old_assets = 0;
        if (old_assets > INT_MAX) return 0;
        state->fixed_asset_value[old_owner] = (int)old_assets;
    }
    new_assets = (int64_t)state->fixed_asset_value[new_owner] + value;
    if (new_assets > INT_MAX) return 0;
    state->fixed_asset_value[new_owner] = (int)new_assets;
    return 1;
}

static int god_emit_property_refresh(GameEventQueue *events, int player,
                                     int facility, int property, int old_owner,
                                     int new_owner, int value, int level,
                                     int type, int owner_changed,
                                     int level_changed, int object_slot)
{
    GameDomainKind kind = facility ? GAME_DOMAIN_FACILITY : GAME_DOMAIN_LAND;
    GameDomainRef ref = rich4_game_domain_ref(kind, property);
    GameDomainRef source = god_ref(object_slot);
    if (owner_changed && !rich4_game_event_emit_typed_domain(
            events, rich4_game_event_property_owner_changed_from_fields,
            player, ref, source, new_owner, value, (unsigned int)old_owner))
        return 0;
    if (level_changed && !rich4_game_event_emit_typed_domain(
            events, rich4_game_event_property_level_changed_from_fields,
            player, ref, source, level, value, (unsigned int)type))
        return 0;
    return rich4_game_event_emit_typed_domain(
        events, rich4_game_event_scene_refresh_requested_from_fields, player,
        ref, source, 0, 0, 0);
}

static int apply_god_landing_type9(GameState *state, int player,
                                   int facility, int property,
                                   GameEventQueue *events, int object_slot, int facility_choice)
{
    int old_level;
    int old_type;
    int new_value;
    int old_value;
    int max_level;
    int type;
    int owner;
    GameState next;
    GameEventQueue staged;

    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count || property < 0 || property >=
            (facility ? state->facility_count : state->land_count))
        return 0;
    next = *state;
    if (facility) {
        old_level = state->facility_level[property];
        old_type = state->facility_type[property];
        old_value = state->facility_value[property];
        if (old_level < 0 || old_level > 5 || old_type < 0 || old_type > 4 ||
            (old_value <= 0 && !god_property_value(1, state, property,
                                                     &old_value)))
            return 0;
        type = old_type;
        if (old_level == 0) {
            /* 0x40B1B4: AI chooses randomly only on its own facility;
             * humans must finish the original 0x440AAC choice first. */
            if (state->players[player].is_ai ||
                (state->players[player].ai_action_flags & 6) != 0) {
                type = 0;
                if (state->facility_owner[property] == player) {
                    Rich4Rng rng;
                    rng.state = next.ai_rng_state;

                    type = 1 + rich4_rng_state_range(&rng, 4);
                    next.ai_rng_state = rng.state;
                }
            } else {
                if (facility_choice < 0 || facility_choice > 4) return 0;
                type = facility_choice;
            }
            next.facility_type[property] = type;
            next.facility_level[property] = 1;
        } else {
            max_level = god_facility_max_level[type];
            if (old_level >= max_level) return 1;
            next.facility_level[property] = old_level + 1;
        }
    } else {
        /* 0x40B110 has a separate ordinary-land branch.  Type 0 lands are
         * upgraded through level 5; the original also increments a type-1
         * empty land (level 0) once, while other type/level combinations are
         * left untouched. */
        old_level = state->land_level[property];
        old_type = state->land_type[property];
        old_value = state->land_value[property];
        if (old_level < 0 || old_level > 5 || old_type < 0 ||
            (old_value <= 0 && !god_property_value(0, state, property,
                                                     &old_value)))
            return 0;
        if ((old_type == 0 && old_level >= 5) ||
            (old_type == 1 && old_level != 0) || old_type > 1)
            return 1;
        next.land_level[property] = old_level + 1;
    }

    if (!god_property_value(facility, &next, property, &new_value)) return 0;
    owner = facility ? next.facility_owner[property] : next.land_owner[property];
    if (facility) next.facility_value[property] = new_value;
    else next.land_value[property] = new_value;
    if (owner >= 0 && owner < next.player_count) {
        int64_t assets = (int64_t)next.fixed_asset_value[
            owner] - old_value + new_value;
        if (assets < 0 || assets > INT_MAX) return 0;
        next.fixed_asset_value[owner] = (int)assets;
    }
    rich4_game_event_queue_reset(&staged);
    if (!god_emit_property_refresh(
            &staged, player, facility, property, owner, owner, new_value,
            facility ? next.facility_level[property] : next.land_level[property],
            facility ? next.facility_type[property] : next.land_type[property],
            0, 1, object_slot) || events->count + staged.count >
            GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

static int apply_god_landing_type10_or_12(
    GameState *state, int player, int facility, int property,
    int god_type, GameEventQueue *events, int object_slot)
{
    int owner;
    int level;
    int value = 0;
    int scale;
    int64_t relation_delta = 0;
    int owner_changed = 0;
    int old_value;
    GameState next;
    GameEventQueue staged;

    if (state == NULL || events == NULL || property < 0 ||
        property >= GAME_MAX_MAP_PROPERTIES)
        return 0;
    owner = facility ? state->facility_owner[property] :
        state->land_owner[property];
    level = facility ? state->facility_level[property] :
        state->land_level[property];
    if (level < 0) return 0;
    scale = state->price_index > 0 ? state->price_index : 1;
    if (god_type == 10) {
        /* 0x40F53C/0x40F59C: any built property loses a level, including
         * unowned and self-owned property. Owner relation credit is separate. */
        if (level == 0) return 1;
        if (owner >= 0 && owner < state->player_count && owner != player)
            relation_delta = (int64_t)30 * scale;
    } else {
        /* 0x40F68B..0x40F82F: empty land is also claimed. */
        if (owner != player && owner >= 0 && owner < state->player_count) {
            value = facility ? state->facility_price[property] :
                               state->land_price[property];
            relation_delta = (int64_t)value * scale * (level + 2) / 10;
        }
        if (owner == player) return 1;
        if (owner < -1 || owner >= state->player_count) return 0;
        owner_changed = 1;
    }
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (relation_delta > 0 && !god_add_relation(
            &next, owner, player, relation_delta))
        return 0;
    if (god_type == 10) {
        int new_value;
        old_value = facility ? next.facility_value[property] : next.land_value[property];
        if (old_value <= 0 && !god_property_value(facility, &next, property, &old_value))
            return 0;
        if (facility) {
            next.facility_level[property]--;
            if (next.facility_level[property] == 0) next.facility_type[property] = 0;
        } else {
            next.land_level[property]--;
            if (next.land_type[property] != 0) {
                next.land_level[property] = 0;
                next.land_type[property] = 0;
            }
        }
        if (!god_property_value(facility, &next, property, &new_value)) return 0;
        if (facility) next.facility_value[property] = new_value;
        else next.land_value[property] = new_value;
        if (owner >= 0 && owner < next.player_count) {
            int64_t assets = (int64_t)next.fixed_asset_value[owner] - old_value + new_value;
            if (assets < 0 || assets > INT_MAX) return 0;
            next.fixed_asset_value[owner] = (int)assets;
        }
        value = new_value;
        level = facility ? next.facility_level[property] : next.land_level[property];
    }
    if (owner_changed) {
        old_value = facility ? next.facility_value[property] :
            next.land_value[property];
        if (old_value <= 0 && !god_property_value(facility, &next, property,
                                                   &old_value))
            return 0;
        if (facility)
            next.facility_owner[property] = player;
        else
            next.land_owner[property] = player;
        if (!god_adjust_fixed_assets(&next, owner, player, old_value))
            return 0;
        value = old_value;
    }
    if ((god_type == 10 || owner_changed) &&
        !god_emit_property_refresh(
            &staged, player, facility, property, owner, player, value, level,
            facility ? next.facility_type[property] : next.land_type[property],
            owner_changed, god_type == 10, object_slot))
        return 0;
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return 0;
    *state = next;
    return 1;
}

int rich4_god_apply_landing_effect_with_choice(GameState *state, int player, int node,
                                   int facility_choice, GameEventQueue *events)
{
    int facility;
    int property;
    int type;
    int object_slot;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count ||
        (state->players[player].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0)
        return 0;
    if (state->players[player].delayed_status_block != 0) return 1;
    type = rich4_god_player_type(&state->players[player]);
    if (type != 9 && type != 10 && type != 12) return 1;
    if (!god_landing_property(state, node, &facility, &property)) return 1;
    object_slot = state->players[player].god_info;
    if (type == 9)
        return apply_god_landing_type9(
            state, player, facility, property, events, object_slot, facility_choice);
    return apply_god_landing_type10_or_12(
        state, player, facility, property, type, events, object_slot);
}

int rich4_god_apply_landing_effect(GameState *state, int player, int node,
                                   GameEventQueue *events)
{
    return rich4_god_apply_landing_effect_with_choice(state, player, node, -1, events);
}

static int emit_change(GameEventQueue *events, int player, int old_type, int new_type)
{
    return push_god_event(events, rich4_game_event_status_changed_from_fields,
                          player, 63, old_type, new_type) &&
           push_god_event(events,
                          rich4_game_event_actor_dirty_requested_from_fields,
                          player, 63, new_type, 0) &&
           push_god_event(events,
                          rich4_game_event_scene_refresh_requested_from_fields,
                          player, 63, new_type, 0);
}

static GameGodResult attach_impl(GameState *state, int player, int object_slot,
                                 int god_type, GameEventQueue *events,
                                 int replace_existing)
{
    GameState next;
    GameEventQueue staged;
    int i;
    GodEffectResult result;
    if (state == NULL || events == NULL || player < 0 || player >= state->player_count ||
        !state->active || (state->players[player].play_state & GAME_PLAYER_PLAY_STATE_MASK) == 0 ||
        object_slot <= 0 || object_slot > GAME_MAP_DYNAMIC_OBJECT_COUNT ||
        god_type <= 0 || god_type > GAME_GOD_TYPE_COUNT ||
        !rich4_god_is_road_type(god_type)) return GAME_GOD_INVALID;
    if (!replace_existing && state->players[player].god_info != 0)
        return GAME_GOD_OCCUPIED;
    for (i = 0; i < state->player_count; i++)
        if (i != player && state->players[i].god_info == object_slot)
            return GAME_GOD_OCCUPIED;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    {
        int old_type = rich4_god_player_type(&state->players[player]);

        if (old_type > 0 && !apply_god_stat_delta(
                &next.players[player], old_type, -1))
            return GAME_GOD_INVALID;
        if (!apply_god_stat_delta(
                &next.players[player], god_type, 1) ||
            !apply_god_effect(&next, player, object_slot, god_type, &staged,
                              &result))
            return GAME_GOD_INVALID;
        next.players[player].god_info = object_slot;
        next.players[player].god_type = god_type;
        if (!emit_god_acquired(&staged, player, god_type, object_slot,
                               old_type, &result) ||
            !emit_change(&staged, player, old_type, god_type) ||
            !rich4_game_event_queue_append(events, &staged))
            return GAME_GOD_EVENT_CAPACITY;
    }
    *state = next;
    return GAME_GOD_APPLIED;
}

GameGodResult rich4_god_attach(GameState *state, int player, int object_slot,
                               int god_type, GameEventQueue *events)
{
    return attach_impl(state, player, object_slot, god_type, events, 0);
}

GameGodResult rich4_god_attach_replace(GameState *state, int player,
                                        int object_slot, int god_type,
                                        GameEventQueue *events)
{
    return attach_impl(state, player, object_slot, god_type, events, 1);
}

GameGodResult rich4_god_expire(GameState *state, int player,
                                  GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int old_type;
    if (state == NULL || events == NULL || player < 0 || player >= state->player_count)
        return GAME_GOD_INVALID;
    old_type = rich4_god_player_type(&state->players[player]);
    if (!rich4_god_is_road_type(old_type)) return GAME_GOD_NOT_SENDABLE;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (old_type > 0 && !apply_god_stat_delta(
            &next.players[player], old_type, -1))
        return GAME_GOD_INVALID;
    next.players[player].god_info = 0;
    next.players[player].god_type = 0;
    if (!emit_change(&staged, player, old_type, 0) ||
        !rich4_game_event_queue_append(events, &staged)) return GAME_GOD_EVENT_CAPACITY;
    *state = next;
    return GAME_GOD_APPLIED;
}

GameGodResult rich4_god_send_away(GameState *state, int player, GameEventQueue *events)
{
    if (state == NULL || player < 0 || player >= state->player_count) return GAME_GOD_INVALID;
    if (!rich4_god_is_sendable_type(rich4_god_player_type(&state->players[player])))
        return GAME_GOD_NOT_SENDABLE;
    return rich4_god_expire(state, player, events);
}
