#include "core/periodic_finance.h"
#include "core/bank.h"
#include "core/game_event_payload.h"
#include "core/ledger.h"
#include "core/rng.h"

#include <limits.h>
#include <string.h>

static int active(const GameState *s, int p)
{
    return p >= 0 && p < s->player_count && p < GAME_MAX_PLAYERS &&
        (s->players[p].play_state & 0xff) != 0;
}

int rich4_company_dividend_prepare(GameState *state, Rich4CompanyDividend *out)
{
    Rich4CompanyDividend result;
    int stock, p;
    if (state == NULL || out == NULL || state->player_count < 0 ||
        state->player_count > GAME_MAX_PLAYERS || state->company_count < 0 ||
        state->company_count > GAME_MAX_MAP_PROPERTIES) return 0;
    memset(&result, 0, sizeof(result));
    for (p = 0; p < state->player_count; ++p)
        if (active(state, p)) result.player_mask |= 1u << p;
    for (stock = 0; stock < GAME_STOCK_COUNT; ++stock) {
        int company = state->stock_company_auxiliary[stock];
        int64_t shares = 0;
        int profit;
        result.company[stock] = -1;
        if (state->stocks[stock].f4 == 0) continue;
        if (company < 0 || company >= state->company_count) return 0;
        result.stock_mask |= 1u << stock;
        result.company[stock] = company;
        profit = (int32_t)state->company_period_business[company];
        result.profit[stock] = profit;
        for (p = 0; p < state->player_count; ++p) {
            if (!active(state, p)) continue;
            if (state->player_stock_amount[p][stock] < 0) return 0;
            shares += state->player_stock_amount[p][stock];
        }
        if (shares > INT_MAX) return 0;
        for (p = 0; p < state->player_count; ++p) {
            float ratio;
            double value;
            int64_t sum;
            if (!active(state, p) || shares == 0) continue;
            /* 0x42BC5B fstp dword, then 0x42BC93 fmul that float. */
            ratio = (float)((double)state->player_stock_amount[p][stock] /
                            (double)shares);
            value = (double)profit * (double)ratio;
            if (value < INT_MIN || value > INT_MAX) return 0;
            result.amount[stock][p] = (int)value; /* CHP: towards zero. */
            sum = (int64_t)result.total[p] + result.amount[stock][p];
            if (sum < INT_MIN || sum > INT_MAX) return 0;
            result.total[p] = (int)sum;
        }
    }
    /* Only publish the original pre-window clearing after validation. */
    for (stock = 0; stock < GAME_STOCK_COUNT; ++stock) {
        int company = result.company[stock];
        int shares = 0;
        if (company < 0) continue;
        for (p = 0; p < state->player_count; ++p)
            if (active(state, p)) shares += state->player_stock_amount[p][stock];
        if (shares != 0) state->company_period_business[company] = 0;
    }
    *out = result;
    return 1;
}

int rich4_company_dividend_commit(GameState *state,
    const Rich4CompanyDividend *dividend, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int p;
    if (state == NULL || dividend == NULL || events == NULL) return 0;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    for (p = 0; p < next.player_count; ++p) {
        int64_t bank, cash;
        if (!active(&next, p) || (dividend->player_mask & (1u << p)) == 0)
            continue;
        bank = (int64_t)next.players[p].bank + dividend->total[p];
        cash = next.players[p].cash;
        if (bank < 0) { cash += bank; bank = 0; }
        if (bank > INT_MAX || cash > INT_MAX) return 0;
        next.players[p].bank = (int)bank;
        next.players[p].cash = cash < 0 ? 0 : (int)cash;
        if (cash < 0 && !rich4_ledger_surrender_player(&next, p, &staged))
            return 0;
    }
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    return 1;
}

int rich4_lottery_has_tickets(const GameState *state)
{
    int i;
    if (state == NULL) return 0;
    for (i = 0; i < 36; ++i) if (state->lottery_owner[i] != 0) return 1;
    return 0;
}

int rich4_lottery_draw(GameState *state, int *out_number, int *out_player)
{
    int count[GAME_MAX_PLAYERS] = {0};
    int sold[36], sold_count = 0, forced = 0, i, number;
    Rich4Rng rng;
    if (state == NULL || out_number == NULL || out_player == NULL) return 0;
    for (i = 0; i < 36; ++i) {
        int owner = state->lottery_owner[i];
        if (owner < 0 || owner > state->player_count ||
            owner > GAME_MAX_PLAYERS) return 0;
        if (owner != 0) { ++count[owner - 1]; sold[sold_count++] = i + 1; }
    }
    if (sold_count == 0) return 0;
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) if (count[i] > 10) forced = 1;
    rng.state = state->ai_rng_state;
    number = forced ? sold[rich4_rng_original_state_next(&rng) % sold_count] :
                     rich4_rng_original_state_next(&rng) % 36 + 1;
    state->ai_rng_state = rng.state;
    *out_number = number;
    *out_player = (int)state->lottery_owner[number - 1] - 1;
    return 1;
}

int rich4_lottery_claim(GameState *state, int number, GameEventQueue *events)
{
    GamePlayerState *p;
    GameEvent event;
    int winner;
    int64_t cash;
    if (state == NULL || events == NULL || number < 1 || number > 36) return 0;
    winner = (int)state->lottery_owner[number - 1] - 1;
    if (winner < 0) return 1;
    if (!active(state, winner)) return 0;
    p = &state->players[winner];
    cash = (int64_t)p->cash + state->lottery_pool;
    if (cash < INT_MIN || cash > INT_MAX) return 0;
    event = rich4_game_event_player_cash_changed_from_fields(
        (Rich4GameEventFields){.player=winner, .value=(int)cash,
                              .value2=state->lottery_pool});
    if (!rich4_game_event_queue_push(events, &event)) return 0;
    p->cash = (int)cash;
    p->monthly_income = (int32_t)((uint32_t)p->monthly_income +
                                  (uint32_t)state->lottery_pool);
    state->lottery_pool = 0;
    memset(state->lottery_owner, 0, sizeof(state->lottery_owner));
    return 1;
}

int rich4_monthly_unlucky_player(const GameState *s, unsigned int mask)
{
    int score[GAME_MAX_PLAYERS] = {0};
    int highest = 0, runner_up = 0, selected = -1, i;
    if (s == NULL) return -1;
    for (i = 0; i < s->player_count; ++i) {
        const GamePlayerState *p = &s->players[i];
        uint32_t value;
        if ((mask & (1u << i)) == 0) continue;
        value = (uint32_t)p->monthly_loss - (uint32_t)p->monthly_income +
            2500u * (uint32_t)s->price_index * (uint8_t)p->total_confinement_days +
            (uint32_t)(10 * (int16_t)p->god_income_modifier);
        score[i] = (int32_t)value;
        if (score[i] > highest) { highest = score[i]; selected = i; }
    }
    for (i = 0; i < s->player_count; ++i)
        if (score[i] != highest && score[i] > runner_up) runner_up = score[i];
    return highest != 0 && runner_up != 0 &&
        (double)(highest - runner_up) / highest > 0.4 ? selected : -1;
}

int rich4_monthly_richest_player(const GameState *s, unsigned int mask)
{
    int highest = 0, selected = -1, i;
    if (s == NULL) return -1;
    for (i = 0; i < s->player_count; ++i) {
        int assets;
        if ((mask & (1u << i)) == 0) continue;
        if (selected < 0) selected = i;
        assets = rich4_bank_total_assets(s, i);
        if (assets > highest) { highest = assets; selected = i; }
    }
    return selected;
}

void rich4_monthly_clear_statistics(GameState *s, unsigned int mask)
{
    int i;
    if (s == NULL) return;
    for (i = 0; i < s->player_count; ++i) if ((mask & (1u << i)) != 0) {
        s->players[i].monthly_loss = s->players[i].monthly_income = 0;
        s->players[i].total_confinement_days = 0;
    }
}
