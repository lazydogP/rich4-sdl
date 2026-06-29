#include "core/stock.h"
#include "core/game_event_payload.h"

#include <float.h>
#include <limits.h>
#include <string.h>

static int valid_player(const GameState *state, int player)
{
    return state != NULL && player >= 0 && player < state->player_count &&
           player < GAME_MAX_PLAYERS;
}

static int valid_price(float value)
{
    return value >= 1.0f && value <= 9999.0f && value == value;
}

static int float_to_int(double value, int *out)
{
    if (out == NULL || value != value || value > (double)INT_MAX ||
        value < (double)INT_MIN) return 0;
    *out = (int)value; /* Original __CHP selects x87 truncate-toward-zero. */
    return 1;
}

static int calendar_date_valid(int year, int month, int day)
{
    static const int month_days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int days;
    int leap;
    if (year < 1 || month < 1 || month > 12 || day < 1) return 0;
    leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    days = month_days[month - 1] + (month == 2 && leap ? 1 : 0);
    return day <= days;
}

static void stock_calendar_date(const GameState *state,
                                int *out_year, int *out_month, int *out_day)
{
    static const int month_days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int year = state->calendar_start_year;
    int month = state->calendar_start_month;
    int day = state->calendar_start_day;
    int remaining = state->elapsed_days;
    while (remaining-- > 0) {
        int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        int days = month_days[month - 1] + (month == 2 && leap ? 1 : 0);
        day++;
        if (day > days) {
            day = 1;
            month++;
            if (month > 12) {
                month = 1;
                year++;
            }
        }
    }
    if (out_year != NULL) *out_year = year;
    if (out_month != NULL) *out_month = month;
    if (out_day != NULL) *out_day = day;
}

typedef struct StockCalendarEvent {
    int8_t closes_market;
    uint8_t kind;
    uint8_t month;
    uint8_t value;
    uint8_t weekday;
} StockCalendarEvent;

enum {
    STOCK_CALENDAR_FIXED = 0,
    STOCK_CALENDAR_LUNAR = 1,
    STOCK_CALENDAR_NTH_WEEKDAY = 2,
    STOCK_CALENDAR_EVENT_CAPACITY = 24
};

/* byte_47FF4A: all 24 twelve-byte calendar records for each original map.
 * closes_market is also the signed visibility byte: -128 hides the record,
 * zero displays art without closing the market, and one does both. */
static const StockCalendarEvent stock_calendar_events[8]
                                                        [STOCK_CALENDAR_EVENT_CAPACITY] = {
    {
        {1,0,1,1,0}, {0,0,2,14,0}, {1,0,3,29,0}, {0,0,4,1,0},
        {1,0,4,4,0}, {1,0,4,5,0}, {1,0,5,1,0}, {0,2,5,2,0},
        {0,0,8,8,0}, {1,0,9,28,0}, {1,0,10,10,0}, {1,0,10,25,0},
        {-128,0,10,31,0}, {1,0,10,31,0}, {1,0,11,12,0}, {1,0,12,25,0},
        {1,1,12,31,0}, {1,1,1,1,0}, {1,1,1,2,0}, {1,1,1,3,0},
        {0,1,1,15,0}, {1,1,5,5,0}, {0,1,7,15,0}, {1,1,8,15,0}
    },
    {
        {1,0,1,1,0}, {0,0,2,14,0}, {1,0,3,8,0}, {1,0,4,5,0},
        {1,0,5,4,0}, {1,0,6,1,0}, {1,0,7,1,0}, {1,0,8,1,0},
        {1,0,5,1,0}, {1,0,10,1,0}, {0,0,12,25,0}, {1,1,12,31,0},
        {1,1,1,1,0}, {1,1,1,2,0}, {1,1,1,3,0}, {0,1,1,15,0},
        {1,1,5,5,0}, {0,1,7,15,0}, {1,1,8,15,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}
    },
    {
        {1,0,1,1,0}, {1,0,1,2,0}, {1,0,1,3,0}, {1,0,1,15,0},
        {0,0,2,3,0}, {0,0,2,14,0}, {1,0,2,11,0}, {0,0,3,3,0},
        {1,0,3,22,0}, {1,0,4,29,0}, {1,0,5,3,0}, {1,0,5,5,0},
        {1,0,9,15,0}, {1,0,9,23,0}, {1,0,10,10,0}, {1,0,11,3,0},
        {1,0,11,23,0}, {1,0,12,23,0}, {0,0,12,25,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}
    },
    {
        {1,0,12,31,0}, {1,0,1,1,0}, {0,2,1,3,1}, {0,0,2,2,0},
        {1,0,2,12,0}, {0,0,2,14,0}, {1,2,2,3,1}, {0,0,3,30,0},
        {0,2,5,2,0}, {0,2,5,5,1}, {0,0,6,14,0}, {0,0,6,15,0},
        {1,0,7,4,0}, {0,2,9,1,1}, {0,0,10,12,0}, {0,0,10,31,0},
        {0,0,11,11,0}, {1,2,11,4,4}, {1,0,12,24,0}, {1,0,12,25,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}
    },
    {
        {1,0,1,1,0}, {1,0,2,5,0}, {1,0,2,27,0}, {1,0,6,30,0},
        {1,0,7,20,0}, {1,0,8,23,0}, {1,0,11,11,0}, {1,0,12,25,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}
    },
    {
        {1,1,1,1,0}, {1,1,1,2,0}, {1,1,1,3,0}, {0,1,1,7,0},
        {0,1,1,15,0}, {0,1,1,20,0}, {1,1,5,5,0}, {0,1,7,15,0},
        {1,1,8,15,0}, {1,1,12,31,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}
    },
    {
        {1,0,1,1,0}, {1,0,2,24,0}, {1,0,4,5,0}, {1,0,6,10,0},
        {1,0,6,11,0}, {1,0,6,12,0}, {1,0,6,13,0}, {1,0,6,14,0},
        {1,0,6,15,0}, {1,0,6,16,0}, {1,0,8,19,0}, {1,0,11,30,0},
        {1,0,12,25,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}
    },
    {
        {1,0,12,31,0}, {1,0,1,1,0}, {0,0,2,14,0}, {0,0,3,30,0},
        {0,2,5,2,0}, {1,0,8,15,0}, {0,0,10,31,0}, {1,2,11,4,4},
        {1,0,12,24,0}, {1,0,12,25,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0},
        {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}, {0,0,0,0,0}
    }
};

/* Sparse projections of dword_47639C.  The game table covers 1998..2020;
 * two entries preserve leap-month dates whose low lunar month/day repeats. */
static const uint16_t stock_lunar_new_year[23] = {
    128,216,205,124,212,201,122,209,129,218,207,126,
    214,203,123,210,131,219,208,128,216,205,125
};
static const uint16_t stock_lunar_0505[23][2] = {
    {530,628},{618,0},{606,0},{625,0},{615,0},{604,0},
    {622,0},{611,0},{531,0},{619,0},{608,0},{528,627},
    {616,0},{606,0},{623,0},{612,0},{602,0},{620,0},
    {609,0},{530,0},{618,0},{607,0},{625,0}
};
static const uint16_t stock_lunar_0715[23][2] = {
    {905,0},{825,0},{814,0},{902,0},{823,0},{812,0},
    {830,0},{819,0},{808,907},{827,0},{815,0},{903,0},
    {824,0},{814,0},{831,0},{821,0},{810,0},{828,0},
    {817,0},{905,0},{825,0},{815,0},{902,0}
};
static const uint16_t stock_lunar_0815[23] = {
    1005,924,912,1001,921,911,928,918,1006,925,914,1003,
    922,912,930,919,908,927,915,1004,924,913,1001
};

static int stock_calendar_weekday(int year, int month, int day)
{
    static const int offset[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int weekday_year = year - (month < 3 ? 1 : 0);
    return (weekday_year + weekday_year / 4 - weekday_year / 100 +
            weekday_year / 400 + offset[month - 1] + day) % 7;
}

static int stock_calendar_day_of_year(int year, int month, int day)
{
    static const int month_days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int result = day;
    int i;
    for (i = 1; i < month; i++) {
        result += month_days[i - 1];
        if (i == 2 && ((year % 4 == 0 && year % 100 != 0) ||
                       year % 400 == 0))
            result++;
    }
    return result;
}

int rich4_stock_calendar_date(const GameState *state,
                              int *out_year, int *out_month,
                              int *out_day, int *out_weekday)
{
    int year;
    int month;
    int day;
    if (state == NULL ||
        !calendar_date_valid(state->calendar_start_year,
                             state->calendar_start_month,
                             state->calendar_start_day)) return 0;
    stock_calendar_date(state, &year, &month, &day);
    if (out_year != NULL) *out_year = year;
    if (out_month != NULL) *out_month = month;
    if (out_day != NULL) *out_day = day;
    if (out_weekday != NULL)
        *out_weekday = stock_calendar_weekday(year, month, day);
    return 1;
}

static int stock_calendar_lunar_match(int year, int month, int day,
                                      int lunar_month, int lunar_day)
{
    const uint16_t (*repeated)[2] = NULL;
    int gregorian = month * 100 + day;
    int year_index;
    if (year < 1998 || year > 2020) return 0;
    year_index = year - 1998;
    if (lunar_month == 1 && lunar_day >= 1 && lunar_day <= 20) {
        int base = stock_lunar_new_year[year_index];
        int base_ordinal = stock_calendar_day_of_year(
            year, base / 100, base % 100);
        return stock_calendar_day_of_year(year, month, day) ==
               base_ordinal + lunar_day - 1;
    }
    if (lunar_month == 5 && lunar_day == 5)
        repeated = stock_lunar_0505;
    else if (lunar_month == 7 && lunar_day == 15)
        repeated = stock_lunar_0715;
    if (repeated != NULL)
        return gregorian == repeated[year_index][0] ||
               (repeated[year_index][1] != 0 &&
                gregorian == repeated[year_index][1]);
    if (lunar_month == 8 && lunar_day == 15)
        return gregorian == stock_lunar_0815[year_index];
    /* Lunar 12/31 is present in several maps but is never produced by the
     * original lookup (lunar months contain at most 30 days). */
    return 0;
}

static int stock_calendar_event_matches(const StockCalendarEvent *event,
                                        int year, int month, int day)
{
    if (event == NULL) return 0;
    if (event->kind == STOCK_CALENDAR_FIXED)
        return month == event->month && day == event->value;
    if (event->kind == STOCK_CALENDAR_LUNAR)
        return stock_calendar_lunar_match(
            year, month, day, event->month, event->value);
    if (event->kind == STOCK_CALENDAR_NTH_WEEKDAY && month == event->month) {
        int first = stock_calendar_weekday(year, month, 1);
        int wanted = event->weekday;
        int event_day;
        if (wanted < first) wanted += 7;
        event_day = 7 * (event->value - 1) + wanted - first + 1;
        return day == event_day;
    }
    return 0;
}

static int stock_calendar_special_closed(const GameState *state)
{
    int event_index;
    int map_index;
    if (state == NULL) return 0;
    /* sub_4521F0 returns the first visible matching record.  The following
     * sub_4523D5 tests only that record's close byte, so a later matching
     * record must not override a visible non-closing record. */
    event_index = rich4_stock_calendar_event_index(state);
    if (event_index < 0) return 0;
    map_index = state->stage * 4 + state->map;
    return stock_calendar_events[map_index][event_index].closes_market > 0;
}

int rich4_stock_calendar_event_index(const GameState *state)
{
    int year, month, day;
    int map_index;
    int i;
    if (state == NULL ||
        !calendar_date_valid(state->calendar_start_year,
                             state->calendar_start_month,
                             state->calendar_start_day) ||
        state->stage < 0 || state->stage > 1 ||
        state->map < 0 || state->map > 3) return -1;
    stock_calendar_date(state, &year, &month, &day);
    map_index = state->stage * 4 + state->map;
    for (i = 0; i < STOCK_CALENDAR_EVENT_CAPACITY; i++) {
        const StockCalendarEvent *event = &stock_calendar_events[map_index][i];
        if (event->closes_market >= 0 &&
            stock_calendar_event_matches(event, year, month, day))
            return i;
    }
    return -1;
}

static int stock_calendar_sunday(const GameState *state)
{
    int year, month, day;
    if (state == NULL ||
        !calendar_date_valid(state->calendar_start_year,
                             state->calendar_start_month,
                             state->calendar_start_day)) return 0;
    stock_calendar_date(state, &year, &month, &day);
    return stock_calendar_weekday(year, month, day) == 0;
}

int rich4_stock_calendar_is_closed(const GameState *state)
{
    if (state == NULL) return 1;
    /* sub_4523D5: Sunday plus map-specific fixed, lunar and
     * nth-weekday records. */
    return stock_calendar_sunday(state) ||
        stock_calendar_special_closed(state);
}

int rich4_stock_market_is_closed(const GameState *state)
{
    if (state == NULL) return 1;
    /* sub_428D01 checks the stock-only closure byte before sub_4523D5. */
    return state->stock_market_closed != 0 ||
        rich4_stock_calendar_is_closed(state);
}

int rich4_stock_elapsed_months(const GameState *state)
{
    int year, month;
    if (state == NULL ||
        !calendar_date_valid(state->calendar_start_year,
                             state->calendar_start_month,
                             state->calendar_start_day)) return 0;
    stock_calendar_date(state, &year, &month, NULL);
    return (year - state->calendar_start_year) * 12 +
           month - state->calendar_start_month;
}

static void update_company_ranking(GameState *state, int stock, int player)
{
    int next[GAME_MAX_PLAYERS];
    int count = 0;
    int inserted = 0;
    int i;
    if (state->stocks[stock].f4 == 0 ||
        state->stock_company_auxiliary[stock] < 0) return;
    for (i = 0; i < GAME_MAX_PLAYERS; i++) {
        int ranked = state->stock_company_ranking[stock][i];
        if (ranked >= 0 && ranked < state->player_count && ranked != player &&
            state->player_stock_amount[ranked][stock] > 0)
            next[count++] = ranked;
    }
    if (state->player_stock_amount[player][stock] > 0) {
        int amount = state->player_stock_amount[player][stock];
        int at = 0;
        while (at < count &&
               state->player_stock_amount[next[at]][stock] >= amount) at++;
        for (i = count; i > at; i--) next[i] = next[i - 1];
        next[at] = player;
        count++;
        inserted = 1;
    }
    (void)inserted;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
        state->stock_company_ranking[stock][i] = i < count ? next[i] : -1;
    state->stock_company_owner[stock] = count != 0 ? next[0] : -1;
}

void rich4_stock_refresh_company_ownership(GameState *state, int stock,
                                           int player)
{
    if (state == NULL || stock < 0 || stock >= GAME_STOCK_COUNT ||
        player < 0 || player >= GAME_MAX_PLAYERS) return;
    update_company_ranking(state, stock, player);
}

void rich4_stock_tick_status(GameState *state)
{
    int i;
    int closed;
    if (state == NULL) return;
    /* 0x41CF67 ticks these counters before sub_4291D6.  The high bit on
     * the market-wide closure is a one-day expiry latch, like the original
     * status counters: expiry day is still closed, next day clears it. */
    closed = state->stock_market_closed & 0xff;
    if ((closed & 0x80) != 0) {
        closed = 0;
    } else if (closed != 0) {
        closed--;
        if (closed == 0) closed = 0x80;
    }
    state->stock_market_closed = closed;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        unsigned int forced = state->stocks[i].f7;
        if (state->stocks[i].f6 != 0) state->stocks[i].f6--;
        if ((forced & 0xf0u) != 0) forced -= 0x10u;
        if ((forced & 0x0fu) != 0) forced--;
        state->stocks[i].f7 = (uint8_t)forced;
    }
}

static int emit_trade_events(GameEventQueue *events, const GameCommand *command,
                             int money_delta, int holding, int old_owner,
                             int new_owner)
{
    GameEventQueue staged;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_money_transferred_from_fields,
            command->player,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, command->player),
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, command->subject.id),
            money_delta, 0, 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields,
            command->player,
            command->subject, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            command->value, holding, 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_market_changed_from_fields,
            command->player,
            command->subject, rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            old_owner, new_owner, 0)) return 0;
    return rich4_game_event_queue_append(events, &staged);
}

void rich4_stock_state_init(GameState *state, Rich4Rng *rng)
{
    Rich4Rng local;
    int i, j;
    if (state == NULL) return;
    if (rng == NULL) {
        local.state = state->ai_rng_state;
        rng = &local;
    }
    /* The original BSS history is empty at new-game entry.  The startup
      * volume entry 0x42915A does not call the daily entry 0x4291D6.
      * The SDL initial quote snapshot below occupies only slot zero;
      * it does not backfill the other 143 slots. */
    memset(state->stock_history, 0, sizeof(state->stock_history));
    state->stock_history_cursor = 0;
    state->stock_last_update_day = -1;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        if (state->stocks[i].f8 <= 1000) {
            state->stocks[i].f10 = state->stocks[i].f8;
        } else {
            int ratio = rich4_rng_original_state_next(rng) % 2000 + 1000;
            state->stocks[i].f10 = (uint16_t)
                ((float)state->stocks[i].f8 * ((float)ratio / 10000.0f));
        }
        state->stock_company_owner[i] = -1;
        for (j = 0; j < GAME_MAX_PLAYERS; j++)
            state->stock_company_ranking[i][j] = -1;
    }
    if (!rich4_stock_market_is_closed(state)) {
        for (i = 0; i < GAME_STOCK_COUNT; i++)
            state->stock_history[i][0] = state->stocks[i].f20;
        state->stock_history_cursor = 1;
    }
    state->ai_rng_state = rng->state;
}

GameStockTradeResult rich4_stock_apply_command(GameState *state,
                                                const GameCommand *command,
                                                GameEventQueue *events)
{
    GameState next;
    GameStockInfo *stock;
    int player, id, quantity, amount, cost, prior_value, old_owner;
    if (state == NULL || command == NULL || events == NULL || !state->active ||
        !rich4_game_command_is_valid(command) ||
        (command->type != GAME_COMMAND_BUY_STOCK &&
         command->type != GAME_COMMAND_SELL_STOCK) ||
        !valid_player(state, command->player)) return GAME_STOCK_TRADE_INVALID;
    player = command->player;
    id = command->subject.id;
    quantity = command->value;
    if (id < 0 || id >= GAME_STOCK_COUNT || quantity <= 0)
        return GAME_STOCK_TRADE_INVALID;
    if (rich4_stock_market_is_closed(state)) return GAME_STOCK_TRADE_CLOSED;
    stock = &state->stocks[id];
    if (stock->f6 != 0) return GAME_STOCK_TRADE_SUSPENDED;
    if (!valid_price(stock->f16) || !valid_price(stock->f20))
        return GAME_STOCK_TRADE_INVALID;
    /* sub_42AAFF uses sub_4295EA as a one-sided circuit breaker: a
     * limit-up quote rejects only buys, and a limit-down quote only sells. */
    if (command->type == GAME_COMMAND_BUY_STOCK &&
        rich4_stock_price_state(stock->f16, stock->f20) ==
            GAME_STOCK_PRICE_LIMIT_UP)
        return GAME_STOCK_TRADE_LIMIT_UP;
    if (command->type == GAME_COMMAND_SELL_STOCK &&
        rich4_stock_price_state(stock->f16, stock->f20) ==
            GAME_STOCK_PRICE_LIMIT_DOWN)
        return GAME_STOCK_TRADE_LIMIT_DOWN;
    if (
        !float_to_int((double)quantity * stock->f20, &cost) || cost < 0)
        return GAME_STOCK_TRADE_INVALID;
    next = *state;
    stock = &next.stocks[id];
    amount = next.player_stock_amount[player][id];
    old_owner = next.stock_company_owner[id];
    if (command->type == GAME_COMMAND_BUY_STOCK) {
        if (quantity > stock->f10 || quantity > stock->f8)
            return GAME_STOCK_TRADE_INSUFFICIENT_VOLUME;
        if (cost > next.players[player].bank)
            return GAME_STOCK_TRADE_INSUFFICIENT_MONEY;
        if (amount > INT_MAX - quantity)
            return GAME_STOCK_TRADE_OVERFLOW;
        if (!float_to_int((double)amount *
                          next.player_stock_average_cost[player][id],
                          &prior_value) || prior_value > INT_MAX - cost)
            return GAME_STOCK_TRADE_OVERFLOW;
        next.players[player].bank -= cost;
        stock->f8 = (uint16_t)(stock->f8 - quantity);
        stock->f10 = (uint16_t)(stock->f10 - quantity);
        amount += quantity;
        next.player_stock_amount[player][id] = amount;
        next.player_stock_average_cost[player][id] =
            (float)(prior_value + cost) / (float)amount;
    } else {
        if (quantity > amount) return GAME_STOCK_TRADE_INSUFFICIENT_HOLDING;
        if ((unsigned int)stock->f8 + (unsigned int)quantity > UINT16_MAX ||
            (unsigned int)stock->f10 + (unsigned int)quantity > UINT16_MAX ||
            next.players[player].bank > INT_MAX - cost)
            return GAME_STOCK_TRADE_OVERFLOW;
        next.players[player].bank += cost;
        stock->f8 = (uint16_t)(stock->f8 + quantity);
        stock->f10 = (uint16_t)(stock->f10 + quantity);
        amount -= quantity;
        next.player_stock_amount[player][id] = amount;
        if (amount == 0) next.player_stock_average_cost[player][id] = 0.0f;
    }
    update_company_ranking(&next, id, player);
    if (!emit_trade_events(events, command,
                           command->type == GAME_COMMAND_BUY_STOCK ? -cost : cost,
                           amount, old_owner, next.stock_company_owner[id]))
        return GAME_STOCK_TRADE_EVENT_CAPACITY;
    *state = next;
    return GAME_STOCK_TRADE_APPLIED;
}

static double positive_remainder(double value, double tick)
{
    long quotient = (long)(value / tick);
    return value - (double)quotient * tick;
}

float rich4_stock_price_after_trend(float previous, float trend)
{
    /* 0x428EDC stores the raw quote in float32; 0x428EF7/0x428F7F
     * store the magnitude of the change in float32 before DFMOD.  The
     * remainder is removed from a rise and added back to a fall. */
    float raw = (float)(((double)trend + 100.0) / 100.0 * (double)previous);
    float delta = trend > 0.0f ? (float)((double)raw - previous) :
                                 (float)((double)previous - raw);
    double tick;
    double value;
    if (raw < 5.0) tick = 0.01;
    else if (raw < 15.0) tick = 0.05;
    else if (raw < 50.0) tick = 0.1;
    else if (raw < 150.0) tick = 0.5;
    else tick = 1.0;
    if (trend > 0.0f)
        value = (double)raw - positive_remainder(delta, tick);
    else
        value = (double)raw + positive_remainder(delta, tick);
    if (value < 1.0) value = 1.0;
    if (value > 9999.0) value = 9999.0;
    return (float)value;
}

GameStockPriceState rich4_stock_price_state(float previous, float current)
{
    /* sub_4295EA compares the current quote with the tick-aligned +/-10%
     * quotes produced by sub_428EC5; do not classify from a rounded percent. */
    if (current > previous) {
        if (current >= rich4_stock_price_after_trend(previous, 10.0f))
            return GAME_STOCK_PRICE_LIMIT_UP;
        return GAME_STOCK_PRICE_RISE;
    }
    if (current < previous) {
        if (current <= rich4_stock_price_after_trend(previous, -10.0f))
            return GAME_STOCK_PRICE_LIMIT_DOWN;
        return GAME_STOCK_PRICE_FALL;
    }
    return GAME_STOCK_PRICE_FLAT;
}

int rich4_stock_apply_forced_quote(GameState *state, int stock)
{
    GameStockInfo *info;
    int history;
    if (state == NULL || stock < 0 || stock >= GAME_STOCK_COUNT)
        return 0;
    info = &state->stocks[stock];
    if (info->f7 == 0 || !valid_price(info->f16)) return 0;
    /* sub_429040 applies red/black cards immediately from the previous
     * close and overwrites the most recent history slot without advancing
     * the shared history cursor. */
    info->f28 = (info->f7 & 0xf0u) != 0 ? 10.0f : -10.0f;
    info->f20 = rich4_stock_price_after_trend(info->f16, info->f28);
    history = state->stock_history_cursor - 1;
    if (history < 0) history = GAME_STOCK_HISTORY_COUNT - 1;
    state->stock_history[stock][history] = info->f20;
    return 1;
}

GameStockDayResult rich4_stock_day_update(GameState *state,
                                           GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    Rich4Rng rng;
    float market_shock, sum = 0.0f;
    int i;
    if (state == NULL || events == NULL || !state->active)
        return GAME_STOCK_DAY_INVALID;
    if (state->stock_last_update_day == state->elapsed_days)
        return GAME_STOCK_DAY_ALREADY_UPDATED;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    next.stock_last_update_day = next.elapsed_days;
    if (rich4_stock_market_is_closed(&next)) {
        *state = next;
        return GAME_STOCK_DAY_CLOSED;
    }
    rng.state = next.ai_rng_state;
    market_shock = ((float)rich4_rng_original_state_next(&rng) - 16384.0f) / 4097.0f;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        GameStockInfo *stock = &next.stocks[i];
        float fundamental;
        stock->f16 = stock->f20;
        if (stock->f6 != 0) {
            stock->f28 = 0.0f;
        } else if (stock->f7 != 0) {
            stock->f28 = (stock->f7 & 0xf0u) != 0 ? 10.0f : -10.0f;
        } else {
            union { float f; uint32_t u; } shock;
            shock.f = ((float)rich4_rng_original_state_next(&rng) - 16384.0f) / 1171.0f;
            stock->f32 = shock.u;
            stock->f28 += shock.f * stock->f24;
            stock->f28 += market_shock;
            if (stock->f4 != 0 && next.stock_company_auxiliary[i] >= 0)
                fundamental = (float)next.stock_company_value[i] / 10000.0f;
            else
                fundamental = stock->f12;
            if (stock->f4 != 0 && next.stock_company_auxiliary[i] >= 0) {
                if (stock->f16 > fundamental * 3.0f)
                    stock->f28 *= stock->f28 > 0.0f ? 0.5f : 2.0f;
                if (stock->f16 < fundamental * 0.85f)
                    stock->f28 *= stock->f28 <= 0.0f ? 0.5f : 2.0f;
            } else {
                if (stock->f16 > fundamental * 8.0f)
                    stock->f28 *= stock->f28 > 0.0f ? 0.5f : 2.0f;
                if (stock->f16 < fundamental * 0.5f)
                    stock->f28 *= stock->f28 <= 0.0f ? 0.5f : 2.0f;
            }
            if (stock->f28 > 10.0f) stock->f28 = 10.0f;
            if (stock->f28 < -10.0f) stock->f28 = -10.0f;
        }
        stock->f20 = rich4_stock_price_after_trend(stock->f16, stock->f28);
        next.stock_history[i][next.stock_history_cursor] = stock->f20;
        sum += stock->f20;
    }
    next.stock_history_cursor =
        (next.stock_history_cursor + 1) % GAME_STOCK_HISTORY_COUNT;
    next.stock_market_total_x10 = (int)(sum * 10.0f);
    next.ai_rng_state = rng.state;
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_market_changed_from_fields, -1,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            next.stock_market_total_x10, next.stock_history_cursor, 0) ||
        !rich4_game_event_queue_append(events, &staged))
        return GAME_STOCK_DAY_EVENT_CAPACITY;
    *state = next;
    return GAME_STOCK_DAY_UPDATED;
}
