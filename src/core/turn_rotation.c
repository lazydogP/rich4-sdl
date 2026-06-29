#include "core/turn_rotation.h"

#include "core/game_event_payload.h"

static int push_typed_turn_event(GameEventQueue *events,
                                 Rich4GameEventConstructor constructor,
    int player, int subject, int value,
    int value2, unsigned int flags)
{
    Rich4GameEventFields fields;
    fields.player = player;
    fields.subject = subject;
    fields.value = value;
    fields.value2 = value2;
    fields.flags = flags;
    fields.subject_ref.kind = GAME_DOMAIN_NONE;
    fields.subject_ref.id = -1;
    fields.target_ref.kind = GAME_DOMAIN_NONE;
    fields.target_ref.id = -1;
    return rich4_game_event_emit_typed(events, constructor, fields);
}
#include <limits.h>
#include <string.h>

#include "core/bank.h"
#include "core/ledger.h"
#include "core/rng.h"
#include "core/victory.h"
#include "core/stock.h"

static int resolve_ordinary_loan_due(GameState *state, int player,
                                     GameEventQueue *events)
{
    GamePlayerState *account;
    GameState due_day;
    int remaining;
    int principal;
    GamePaymentResult payment;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count) return 0;
    account = &state->players[player];
    if (account->loan_due_date == 0) return 1;
    /* sub_433B7E computes +90 once, then advances that stored date until
     * sub_4523D5 reports an open bank day.  Normalize before calculating
     * reminders so 3/2/1 all count back from the shifted deadline. */
    due_day = *state;
    due_day.elapsed_days = account->loan_due_date;
    while (rich4_stock_calendar_is_closed(&due_day)) {
        if (account->loan_due_date == INT_MAX) return 0;
        account->loan_due_date++;
        due_day.elapsed_days = account->loan_due_date;
    }
    remaining = rich4_bank_loan_days_remaining(state, player);
    if (remaining > 3) return 1;
    if (remaining > 0) {
        GameEvent event = rich4_game_event_bank_loan_due(
            player, account->loan, remaining, 0, 0);
        return rich4_game_event_queue_push(events, &event);
    }
    principal = account->loan;
    payment = rich4_ledger_pay(
        state, player, rich4_ledger_system(), principal,
        GAME_PAYMENT_PAYER_BANK_FIRST, events);
    if (payment == GAME_PAYMENT_REJECTED ||
        payment == GAME_PAYMENT_LIQUIDATION)
        return 0;
    state->players[player].loan = 0;
    state->players[player].loan_due_date = 0;
    {
        GameEvent event = rich4_game_event_bank_loan_due(
            player, principal, 0, payment, 1u);
        return rich4_game_event_queue_push(events, &event);
    }
}

static int special_route_node_valid(const MapData *map, int node)
{
    return map != NULL && map->routes != NULL && node > 0 &&
           (size_t)node < map->route_count;
}

static int special_route_options(const MapData *map, int node,
                                 int previous_node, int out_options[4])
{
    int count = 0;
    int fallback = 0;
    int i;
    memset(out_options, 0, sizeof(int) * 4);
    if (!special_route_node_valid(map, node)) return 0;
    for (i = 0; i < 4; i++) {
        int next = map->routes[node].next[i];
        uint32_t blocked_mask = UINT32_C(0x40000000) >> i;
        int duplicate = 0;
        int j;
        if (!special_route_node_valid(map, next)) continue;
        if (next == previous_node) fallback = next;
        if ((map->routes[node].flags & blocked_mask) != 0 ||
            next == previous_node) continue;
        for (j = 0; j < count; j++)
            if (out_options[j] == next) duplicate = 1;
        if (!duplicate) out_options[count++] = next;
    }
    if (count == 0 && fallback != 0) out_options[count++] = fallback;
    return count;
}

int rich4_turn_run_special_auto_action(
    GameState *state, const MapData *map, int special_slot,
    TurnSpecialAutoActionResult *out_result)
{
    TurnSpecialAutoActionResult result;
    Rich4Rng rng;
    int node;
    int previous;
    int steps;
    int i;
    memset(&result, 0, sizeof(result));
    result.kind = TURN_SPECIAL_AUTO_INVALID;
    result.special_slot = special_slot;
    result.start_node = -1;
    result.end_node = -1;
    if (state == NULL || out_result == NULL || special_slot < 0 ||
        special_slot >= GAME_MAX_PLAYERS ||
        state->special_player_f10[special_slot] != 0)
        return 0;
    node = state->special_player_node[special_slot];
    previous = state->special_player_previous_node[special_slot];
    result.start_node = result.end_node = node;
    result.path_count = 1;
    result.path_nodes[0] = node;
    if (!special_route_node_valid(map, node)) {
        *out_result = result;
        return 0;
    }
    /* 0x40C912 suppresses 0x40DD1F only for winter sleep and stopping.
     * Sleepwalking still takes the automatic route, but 0x41B42D suppresses
     * that actor's theft/extortion effects.  Tortoise forces one step. */
    if ((state->special_player_status[special_slot][0] & 0xff) != 0 ||
        (state->special_player_status[special_slot][2] & 0xff) != 0) {
        result.kind = TURN_SPECIAL_AUTO_BLOCKED;
        *out_result = result;
        return 1;
    }
    rng.state = state->ai_rng_state;
    if (state->special_player_route_reversed[special_slot]) {
        int options[4];
        int count = special_route_options(map, node, previous, options);
        if (count > 0)
            previous = options[rich4_rng_state_range(&rng, count)];
        state->special_player_route_reversed[special_slot] = 0;
    }
    steps = (state->special_player_status[special_slot][3] & 0xff) != 0
        ? 1 : rich4_rng_state_range(&rng, 9) + 2;
    result.requested_steps = steps;
    for (i = 0; i < steps; i++) {
        int options[4];
        int count = special_route_options(map, node, previous, options);
        int next;
        if (count <= 0) break;
        next = options[rich4_rng_state_range(&rng, count)];
        previous = node;
        node = next;
        result.completed_steps++;
        result.path_nodes[result.path_count++] = node;
    }
    state->special_player_previous_node[special_slot] = previous;
    state->special_player_node[special_slot] = node;
    state->ai_rng_state = rng.state;
    result.end_node = node;
    result.kind = result.completed_steps > 0
        ? TURN_SPECIAL_AUTO_MOVED : TURN_SPECIAL_AUTO_BLOCKED;
    *out_result = result;
    return 1;
}

int rich4_turn_request_ai_cancel(GameState *state)
{
    int i;
    if (state == NULL || !state->active || state->player_count <= 0 ||
        state->player_count > GAME_MAX_PLAYERS) return 0;
    for (i = 0; i < state->player_count; ++i) {
        const GamePlayerState *p = &state->players[i];
        /* The original compares the whole byte to 1 and only +50..+53. */
        if ((p->play_state & 0xff) == 1 &&
            !(p->delayed_status_block & 0xff) &&
            !(p->days_disappearing & 0xff) &&
            !(p->days_in_prison & 0xff) &&
            !(p->days_in_hospital & 0xff)) return 0;
    }
    state->ai_cancel_pending = 1;
    return 1;
}

static void consume_ai_cancel(GameState *state)
{
    if (!state->ai_cancel_pending) return;
    /* 0x419023 is an unindexed write: player zero, not current_player. */
    state->ai_cancel_pending = 0;
    state->players[0].play_state &= ~4;
    state->players[0].is_ai = (state->players[0].play_state & 6) != 0;
}

int rich4_turn_advance_once(GameState *state, TurnAdvanceResult *out_result)
{
    TurnAdvanceResult result;
    int guard;
    memset(&result, 0, sizeof(result));
    result.previous_index = state != NULL ? state->turn_rotation_index : -1;
    result.selected_index = -1;
    if (state == NULL || out_result == NULL || state->player_count <= 0 ||
        state->player_count > GAME_MAX_PLAYERS) return 0;
    if (state->turn_rotation_index >= 0 && state->turn_rotation_index < GAME_MAX_PLAYERS &&
        (state->players[state->turn_rotation_index].play_state & 0x30) != 0) {
        state->players[state->turn_rotation_index].play_state &= 0x0f;
        result.selected_index = state->turn_rotation_index;
        result.kind = TURN_SELECTION_NORMAL;
        result.events = TURN_EVENT_NORMAL_STATE_CLEARED | TURN_EVENT_MARK_DIRTY;
        *out_result = result;
        return 1;
    }
    for (guard = 0; guard < 16; guard++) {
        int next = state->turn_rotation_index + 1;
        if (next == state->player_count) {
            next = GAME_MAX_PLAYERS;
        } else if (next >= GAME_MAX_PLAYERS * 2) {
            next = 0;
            state->full_round_count++;
            state->elapsed_days++;
            state->day_limit_reached = state->day_limit > 0 &&
                                       state->elapsed_days >= state->day_limit;
            result.events |= TURN_EVENT_FULL_ROUND | TURN_EVENT_DAY_ADVANCED;
        }
        state->turn_rotation_index = next;
        if (next >= GAME_MAX_PLAYERS && next < GAME_MAX_PLAYERS * 2) {
            int slot = next - GAME_MAX_PLAYERS;
            if (slot < 4 && state->special_player_f10[slot] == 0) {
                state->last_special_turn_index = next;
                result.selected_index = next;
                result.kind = TURN_SELECTION_SPECIAL;
                consume_ai_cancel(state);
                result.events |= TURN_EVENT_STATUS_TICK | TURN_EVENT_MARK_DIRTY;
                *out_result = result;
                return 1;
            }
            result.skipped_special_mask |= 1u << slot;
            continue;
        }
        if (next >= 0 && next < state->player_count) {
            GamePlayerState *player = &state->players[next];
            if ((player->play_state & 0xff) == 0) {
                result.skipped_normal_mask |= 1u << next;
                continue;
            }
            state->current_player = next;
            result.selected_index = next;
            result.kind = TURN_SELECTION_NORMAL;
            consume_ai_cancel(state);
            result.events |= TURN_EVENT_STATUS_TICK | TURN_EVENT_MARK_DIRTY;
            *out_result = result;
            return 1;
        }
    }
    *out_result = result;
    return 0;
}

/* 0x41D114..0x41D175: high nibble counts days, low nibble selects color.
 * Keep the existing closed-day fields in step with the packed status. */
static void tick_property_statuses(GameState *state)
{
    int family, i;
    for (family = 0; family < 2; family++) {
        int count = family ? state->facility_count : state->land_count;
        int *status = family ? state->facility_price_status : state->land_price_status;
        int *closed = family ? state->facility_closed_status : state->land_closed_status;
        for (i = 0; i < count && i < GAME_MAX_MAP_PROPERTIES; i++) {
            if (status[i] & 0xf0) {
                status[i] -= 0x10;
                if (!(status[i] & 0xf0)) status[i] = 0;
            }
            if (closed[i] > 0) closed[i]--;
        }
    }
}

static int turn_advance_events_impl(GameState *state,
                                   TurnAdvanceResult *out_result,
                                   GameEventQueue *events, const uint32_t *tick)
{
    GameState next;
    GameEventQueue staged;
    TurnAdvanceResult result;
    int previous_months;
    if (state == NULL || events == NULL) return 0;
    next = *state;
    previous_months = rich4_stock_elapsed_months(state);
    rich4_game_event_queue_reset(&staged);
    if (!rich4_turn_advance_once(&next, &result)) return 0;
    if ((result.events & TURN_EVENT_FULL_ROUND) != 0)
        push_typed_turn_event(
            &staged, rich4_game_event_full_round_completed_from_fields,
            result.selected_index, 0, next.full_round_count, 0, 0);
    if ((result.events & TURN_EVENT_DAY_ADVANCED) != 0)
        push_typed_turn_event(
            &staged, rich4_game_event_day_advanced_from_fields,
            result.selected_index, 0, next.elapsed_days,
            next.day_limit_reached, 0);
    if ((result.events & TURN_EVENT_NORMAL_STATE_CLEARED) != 0)
        push_typed_turn_event(
            &staged, rich4_game_event_normal_state_cleared_from_fields,
            result.selected_index, 0, 0, 0, 0);
    if ((result.events & TURN_EVENT_DAY_ADVANCED) != 0) {
        VictoryResult victory;
        /* 0x41CFB9 checks 0x41D89E before counters, srand and stocks. */
        if (!rich4_victory_evaluate(&next, &victory)) return 0;
        if (!next.game_over && victory.event != VICTORY_EVENT_CANDIDATE) {
            Rich4BankSettlement settlement;
            GameStockDayResult stock_result;
            int player;
            rich4_stock_tick_status(&next);
            /* 0x41D066/0x41D06E reseeds after counters, before 0x4291D6. */
            if (tick != NULL) next.ai_rng_state = *tick;
            stock_result = rich4_stock_day_update(&next, &staged);
            if (stock_result == GAME_STOCK_DAY_INVALID ||
                stock_result == GAME_STOCK_DAY_EVENT_CAPACITY) return 0;
            {
                int day;
                if (rich4_stock_calendar_date(&next, NULL, NULL, &day, NULL) &&
                    day == 15 && !next.game_over && !next.day_limit_reached) {
                    GameEvent event = rich4_game_event_periodic_finance_requested_from_fields(
                        (Rich4GameEventFields){.player=-1, .subject=15});
                    if (!rich4_game_event_queue_push(&staged, &event)) return 0;
                }
            }
            if (rich4_stock_elapsed_months(&next) > previous_months) {
                if (!rich4_bank_monthly_settle(&next, &settlement)) return 0;
                for (player = 0; player < GAME_MAX_PLAYERS; player++) {
                    unsigned int bit = 1u << player;
                    if ((settlement.active_mask & bit) == 0) continue;
                    {
                        GameEvent event = rich4_game_event_bank_settlement(
                            player, settlement.interest[player],
                            settlement.bank_after[player],
                            (settlement.loan_mask & bit) != 0 ? 1u : 0u);
                        if (!rich4_game_event_queue_push(&staged, &event))
                            return 0;
                    }
                }
            }
            tick_property_statuses(&next);
        }
    }
    /* 0x41902E calls the blocking day update before player status ticks. */
    if ((result.events & TURN_EVENT_STATUS_TICK) != 0)
        push_typed_turn_event(
            &staged, rich4_game_event_status_tick_requested_from_fields,
            result.selected_index, 0, 0, 0, 0);
    if ((result.events & TURN_EVENT_MARK_DIRTY) != 0)
        push_typed_turn_event(
            &staged, rich4_game_event_actor_dirty_requested_from_fields,
            result.selected_index, 0, 0, 0, 0);
    if (result.kind == TURN_SELECTION_NORMAL &&
        !resolve_ordinary_loan_due(
            &next, result.selected_index, &staged))
        return 0;
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    if (out_result != NULL) *out_result = result;
    return 1;
}

int rich4_turn_advance_once_events(GameState *state,
    TurnAdvanceResult *out_result, GameEventQueue *events)
{ return turn_advance_events_impl(state, out_result, events, NULL); }

int rich4_turn_advance_once_events_at_tick(GameState *state,
    TurnAdvanceResult *out_result, GameEventQueue *events, uint32_t tick)
{ return turn_advance_events_impl(state, out_result, events, &tick); }
