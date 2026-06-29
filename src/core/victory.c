#include "core/victory.h"

#include "core/game_event_payload.h"

#include <string.h>

static int valid_player(const GameState *state, int player)
{
    return state != NULL && player >= 0 && player < state->player_count &&
           player < GAME_MAX_PLAYERS;
}

int rich4_player_cash_bank_assets(const GameState *state, int player)
{
    const GamePlayerState *value;
    if (!valid_player(state, player)) return 0;
    value = &state->players[player];
    return value->cash + value->bank - value->loan;
}

int rich4_player_stock_assets(const GameState *state, int player)
{
    int total = 0;
    int i;
    if (!valid_player(state, player)) return 0;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        total = (int)((float)total +
                      state->player_stock_amount[player][i] * state->stocks[i].f20);
    }
    return total;
}

int rich4_player_land_assets(const GameState *state, int player)
{
    int total = 0;
    int i;
    if (!valid_player(state, player)) return 0;
    for (i = 0; i < state->land_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (state->land_owner[i] == player) total += state->land_value[i];
    }
    return total;
}

int rich4_player_facility_assets(const GameState *state, int player)
{
    int total = 0;
    int i;
    if (!valid_player(state, player)) return 0;
    for (i = 0; i < state->facility_count && i < GAME_MAX_MAP_PROPERTIES; i++) {
        if (state->facility_owner[i] == player) total += state->facility_value[i];
    }
    return total;
}

int rich4_player_total_assets(const GameState *state,
                              int player,
                              PlayerAssetBreakdown *out_breakdown)
{
    PlayerAssetBreakdown value;
    memset(&value, 0, sizeof(value));
    if (!valid_player(state, player)) {
        if (out_breakdown != NULL) *out_breakdown = value;
        return 0;
    }
    value.cash_and_bank = state->players[player].cash + state->players[player].bank;
    value.loan_deduction = state->players[player].loan;
    value.stock_value = rich4_player_stock_assets(state, player);
    value.land_value = rich4_player_land_assets(state, player);
    value.facility_value = rich4_player_facility_assets(state, player);
    value.total = value.cash_and_bank - value.loan_deduction + value.stock_value +
                  value.land_value + value.facility_value;
    if (out_breakdown != NULL) *out_breakdown = value;
    return value.total;
}

int rich4_victory_evaluate(GameState *state, VictoryResult *out_result)
{
    VictoryResult result;
    int best_player = -1;
    int best_assets = 0;
    int i;
    memset(&result, 0, sizeof(result));
    result.player = -1;
    result.special_finance_excluded = 1;
    if (state == NULL || out_result == NULL || state->player_count <= 0) return 0;
    for (i = 0; i < state->player_count && i < GAME_MAX_PLAYERS; i++) {
        int assets;
        if ((state->players[i].play_state & 0xff) == 0) continue;
        assets = rich4_player_total_assets(state, i, NULL);
        if (assets > best_assets) {
            best_assets = assets;
            best_player = i;
        }
    }
    state->win_candidate_player = -1;
    state->win_candidate_reason = GAME_WIN_CANDIDATE_NONE;
    state->win_candidate_assets = best_assets;
    if (best_player >= 0 && best_assets != 0 &&
        ((state->day_limit > 0 && state->elapsed_days >= state->day_limit) ||
         (state->win_target > 0 && best_assets >= state->win_target))) {
        result.event = VICTORY_EVENT_CANDIDATE;
        result.player = best_player;
        result.reason = state->day_limit > 0 && state->elapsed_days >= state->day_limit
            ? GAME_WIN_CANDIDATE_DAY_LIMIT : GAME_WIN_CANDIDATE_TARGET_ASSETS;
        rich4_player_total_assets(state, best_player, &result.assets);
        state->win_candidate_player = best_player;
        state->win_candidate_reason = result.reason;
        state->win_candidate_assets = best_assets;
    }
    *out_result = result;
    return 1;
}

int rich4_victory_evaluate_events(GameState *state,
                                  VictoryResult *out_result,
                                  GameEventQueue *events)
{
    VictoryResult result;
    Rich4GameEventFields fields;
    if (!rich4_victory_evaluate(state, &result)) return 0;
    if (result.event == VICTORY_EVENT_CANDIDATE) {
        fields.player = result.player;
        fields.subject = result.reason;
        fields.value = result.assets.total;
        fields.value2 = result.special_finance_excluded;
        fields.flags = 0u;
        fields.subject_ref.kind = GAME_DOMAIN_NONE;
        fields.subject_ref.id = -1;
        fields.target_ref.kind = GAME_DOMAIN_NONE;
        fields.target_ref.id = -1;
        rich4_game_event_emit_typed(
            events, rich4_game_event_victory_candidate_from_fields, fields);
    }
    if (out_result != NULL) *out_result = result;
    return 1;
}

int rich4_victory_finalize_events(GameState *state,
                                  VictoryResult *out_result,
                                  GameEventQueue *events)
{
    GameState next;
    VictoryResult result;
    GameEventQueue staged;

    if (state == NULL || events == NULL) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_victory_evaluate(&next, &result)) return 0;
    if (result.event != VICTORY_EVENT_CANDIDATE || next.game_over) {
        if (out_result != NULL) *out_result = result;
        *state = next;
        return 1;
    }

    /* Preserve the same event payload as the candidate-only API, then add
     * the durable transition that the original end-of-round dispatcher
     * performs after 0x41D89E returns success. */
    {
        Rich4GameEventFields fields;
        fields.player = result.player;
        fields.subject = result.reason;
        fields.value = result.assets.total;
        fields.value2 = result.special_finance_excluded;
        fields.flags = 0u;
        fields.subject_ref.kind = GAME_DOMAIN_NONE;
        fields.subject_ref.id = -1;
        fields.target_ref.kind = GAME_DOMAIN_NONE;
        fields.target_ref.id = -1;
        if (!rich4_game_event_emit_typed(
                &staged, rich4_game_event_victory_candidate_from_fields,
                fields))
            return 0;
    }
    next.game_over = 1;
    next.winner_player = result.player;
    next.winner_reason = result.reason;
    if (!rich4_game_event_game_ended_emit(
            &staged, next.winner_player, next.winner_reason,
            result.assets.total, 0, 0))
        return 0;
    if (events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY) return 0;
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    if (out_result != NULL) *out_result = result;
    return 1;
}
