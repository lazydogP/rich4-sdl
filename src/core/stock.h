#ifndef RICH4_CORE_STOCK_H
#define RICH4_CORE_STOCK_H

#include <stdint.h>

#include "core/game_event.h"
#include "core/game_state.h"
#include "core/rng.h"

typedef enum GameStockTradeResult {
    GAME_STOCK_TRADE_APPLIED = 1,
    GAME_STOCK_TRADE_INVALID,
    GAME_STOCK_TRADE_CLOSED,
    GAME_STOCK_TRADE_SUSPENDED,
    GAME_STOCK_TRADE_LIMIT_UP,
    GAME_STOCK_TRADE_LIMIT_DOWN,
    GAME_STOCK_TRADE_INSUFFICIENT_MONEY,
    GAME_STOCK_TRADE_INSUFFICIENT_VOLUME,
    GAME_STOCK_TRADE_INSUFFICIENT_HOLDING,
    GAME_STOCK_TRADE_OVERFLOW,
    GAME_STOCK_TRADE_EVENT_CAPACITY
} GameStockTradeResult;

typedef enum GameStockPriceState {
    GAME_STOCK_PRICE_FLAT = 0,
    GAME_STOCK_PRICE_RISE,
    GAME_STOCK_PRICE_LIMIT_UP,
    GAME_STOCK_PRICE_FALL,
    GAME_STOCK_PRICE_LIMIT_DOWN
} GameStockPriceState;

typedef enum GameStockDayResult {
    GAME_STOCK_DAY_UPDATED = 1,
    GAME_STOCK_DAY_CLOSED,
    GAME_STOCK_DAY_ALREADY_UPDATED,
    GAME_STOCK_DAY_INVALID,
    GAME_STOCK_DAY_EVENT_CAPACITY
} GameStockDayResult;

typedef struct GameStockSnapshot {
    uint32_t version;
    GameStockInfo stocks[GAME_STOCK_COUNT];
    float history[GAME_STOCK_COUNT][GAME_STOCK_HISTORY_COUNT];
    float average_cost[GAME_MAX_PLAYERS][GAME_STOCK_COUNT];
    int holdings[GAME_MAX_PLAYERS][GAME_STOCK_COUNT];
    int company_owner[GAME_STOCK_COUNT];
    int company_ranking[GAME_STOCK_COUNT][GAME_MAX_PLAYERS];
    int company_auxiliary[GAME_STOCK_COUNT];
    unsigned int company_value[GAME_STOCK_COUNT];
    unsigned int company_available[GAME_STOCK_COUNT];
    int history_cursor;
    int market_total_x10;
    int market_closed;
    int last_update_day;
    uint32_t rng_state;
} GameStockSnapshot;

#define GAME_STOCK_SNAPSHOT_VERSION 1u

void rich4_stock_state_init(GameState *state, Rich4Rng *rng);
void rich4_stock_refresh_company_ownership(GameState *state, int stock,
                                           int player);
void rich4_stock_tick_status(GameState *state);
int rich4_stock_market_is_closed(const GameState *state);
/* sub_4523D5 calendar closure only: Sunday plus map holidays.  Unlike
 * rich4_stock_market_is_closed, this ignores temporary stock-only closure. */
int rich4_stock_calendar_is_closed(const GameState *state);
int rich4_stock_elapsed_months(const GameState *state);
int rich4_stock_calendar_date(const GameState *state,
                              int *out_year, int *out_month,
                              int *out_day, int *out_weekday);
/* sub_4521F0: first visible map-calendar record matching the current date,
 * or -1 when the bank calendar uses the seasonal Panel.MKF fallback. */
int rich4_stock_calendar_event_index(const GameState *state);
int rich4_stock_apply_forced_quote(GameState *state, int stock);
GameStockTradeResult rich4_stock_apply_command(GameState *state,
                                                const GameCommand *command,
                                                GameEventQueue *events);
GameStockDayResult rich4_stock_day_update(GameState *state,
                                           GameEventQueue *events);
float rich4_stock_price_after_trend(float previous, float trend);
GameStockPriceState rich4_stock_price_state(float previous, float current);
int rich4_stock_snapshot_export(const GameState *state, GameStockSnapshot *out);
int rich4_stock_snapshot_import(GameState *state, const GameStockSnapshot *in);

#endif
