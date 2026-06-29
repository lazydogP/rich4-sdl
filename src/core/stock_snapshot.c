#include "core/stock.h"

#include <string.h>

int rich4_stock_snapshot_export(const GameState *state, GameStockSnapshot *out)
{
    if (state == NULL || out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    out->version = GAME_STOCK_SNAPSHOT_VERSION;
    memcpy(out->stocks, state->stocks, sizeof(out->stocks));
    memcpy(out->history, state->stock_history, sizeof(out->history));
    memcpy(out->average_cost, state->player_stock_average_cost,
           sizeof(out->average_cost));
    memcpy(out->holdings, state->player_stock_amount, sizeof(out->holdings));
    memcpy(out->company_owner, state->stock_company_owner,
           sizeof(out->company_owner));
    memcpy(out->company_ranking, state->stock_company_ranking,
           sizeof(out->company_ranking));
    memcpy(out->company_auxiliary, state->stock_company_auxiliary,
           sizeof(out->company_auxiliary));
    memcpy(out->company_value, state->stock_company_value,
           sizeof(out->company_value));
    memcpy(out->company_available, state->stock_company_available,
           sizeof(out->company_available));
    out->history_cursor = state->stock_history_cursor;
    out->market_total_x10 = state->stock_market_total_x10;
    out->market_closed = state->stock_market_closed;
    out->last_update_day = state->stock_last_update_day;
    out->rng_state = state->ai_rng_state;
    return 1;
}

int rich4_stock_snapshot_import(GameState *state, const GameStockSnapshot *in)
{
    GameState next;
    if (state == NULL || in == NULL ||
        in->version != GAME_STOCK_SNAPSHOT_VERSION ||
        in->history_cursor < 0 ||
        in->history_cursor >= GAME_STOCK_HISTORY_COUNT) return 0;
    next = *state;
    memcpy(next.stocks, in->stocks, sizeof(in->stocks));
    memcpy(next.stock_history, in->history, sizeof(in->history));
    memcpy(next.player_stock_average_cost, in->average_cost,
           sizeof(in->average_cost));
    memcpy(next.player_stock_amount, in->holdings, sizeof(in->holdings));
    memcpy(next.stock_company_owner, in->company_owner,
           sizeof(in->company_owner));
    memcpy(next.stock_company_ranking, in->company_ranking,
           sizeof(in->company_ranking));
    memcpy(next.stock_company_auxiliary, in->company_auxiliary,
           sizeof(in->company_auxiliary));
    memcpy(next.stock_company_value, in->company_value,
           sizeof(in->company_value));
    memcpy(next.stock_company_available, in->company_available,
           sizeof(in->company_available));
    next.stock_history_cursor = in->history_cursor;
    next.stock_market_total_x10 = in->market_total_x10;
    next.stock_market_closed = in->market_closed;
    next.stock_last_update_day = in->last_update_day;
    next.ai_rng_state = in->rng_state;
    *state = next;
    return 1;
}

