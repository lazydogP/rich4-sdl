#include "core/ai_internal.h"

#include <limits.h>
#include <stdint.h>

#include "core/stock.h"

typedef struct ScoredStock {
    int stock;
    int score;
} ScoredStock;

/* __CHP (0x457DBC) writes control-word high byte 0x1F: RC=11, truncate
 * toward zero. These stock quantities are nonnegative; retain the existing
 * saturation guard outside the original signed-int range. */
static int stock_truncate(double value)
{
    if (value <= 0.0) return 0;
    if (value >= (double)INT_MAX) return INT_MAX;
    return (int)value;
}

static GameDomainRef stock_none_ref(void)
{
    return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
}

static void stock_trace(GameAiDecision *d, GameDomainRef subject,
                        GameDomainRef target, GameAiCandidateResult result,
                        int score, int value)
{
    GameAiCandidateTrace *c;
    if (d->candidate_count >= GAME_AI_TRACE_CAPACITY) {
        d->candidate_overflow = 1;
        return;
    }
    c = &d->candidates[d->candidate_count++];
    c->subject = subject;
    c->target = target;
    c->result = result;
    c->score = score;
    c->value = value;
}

static float history_average(const GameState *s, int stock, int count)
{
    float sum = 0.0f;
    int used = 0, i;
    for (i = count; i > 0; i--) {
        int at = s->stock_history_cursor - i;
        float value;
        while (at < 0) at += GAME_STOCK_HISTORY_COUNT;
        value = s->stock_history[stock][at];
        if (value == 0.0f) continue;
        sum += value;
        used++;
    }
    return used ? sum / (float)used : 0.0f;
}

static float history_minimum(const GameState *s, int stock)
{
    float minimum = 10000.0f;
    int i;
    for (i = 0; i < GAME_STOCK_HISTORY_COUNT; i++) {
        float value = s->stock_history[stock][i];
        if (value == 0.0f) continue;
        if (value < minimum) minimum = value;
    }
    return minimum;
}

static int portfolio_value(const GameState *s, int player)
{
    int total = 0;
    int i;
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        double value = (double)s->player_stock_amount[player][i] *
                       (double)s->stocks[i].f20;
        if (s->player_stock_amount[player][i] > 0) {
            /* 0x42BFE3 stores the running integer as float before adding
             * this holding; 0x42BFF1 truncates on every occupied stock. */
            total = stock_truncate((double)(float)total + value);
            if (total == INT_MAX) return INT_MAX;
        }
    }
    return total;
}

static int independent_buy_score(const GameState *s, int stock)
{
    const GameStockInfo *info = &s->stocks[stock];
    float average24 = history_average(s, stock, 24);
    float average6 = history_average(s, stock, 6);
    int score = 0;
    if ((double)info->f20 < (double)info->f12 * 2.5 && info->f28 > 2.0f &&
        average6 > average24) score += 2;
    if ((double)info->f20 < (double)info->f12 * 0.6 && average6 > average24) score += 4;
    if ((double)average6 < (double)average24 * 0.5) score += 2;
    return score;
}

static int company_link_valid(const GameState *s, int stock)
{
    int link = s->stock_company_auxiliary[stock];
    return link >= 0 && link < s->company_count &&
           link < GAME_MAX_MAP_PROPERTIES;
}

static int company_average_business(const GameState *s, int link)
{
    int32_t total = (int32_t)s->company_total_business[link];
    int months = rich4_stock_elapsed_months(s);
    if (months > 0) return (int)((int64_t)total / months);
    return (int)total;
}

static int company_value_scaled(const GameState *s, int stock)
{
    return (int32_t)s->stock_company_value[stock] / 10000;
}

static int company_calendar_day(const GameState *s)
{
    int day = 0;
    (void)rich4_stock_calendar_date(s, NULL, NULL, &day, NULL);
    return day;
}

static int company_buy_score(const GameState *s, int p, int stock)
{
    int link = s->stock_company_auxiliary[stock];
    int business;
    int32_t period_business;
    int company_value;
    int score = 0;
    double value;

    if (!company_link_valid(s, stock)) return 0;
    period_business = (int32_t)s->company_period_business[link];
    /* 0x42BF03 uses the raw +44 total before the first month and divides it
     * by the global completed-month counter afterwards. */
    business = company_average_business(s, link);
    if (business > 0) {
        int64_t first = (int64_t)s->price_index * 5000;
        int64_t second = (int64_t)s->price_index * 10000;
        if ((int64_t)business < first) score += 1;
        else if ((int64_t)business < second) score += 2;
        else score += 3;
    }

    company_value = company_value_scaled(s, stock);
    value = (double)company_value;
    /* The original checks +40 first and only then considers the actor's
     * holding and the current quote against company value (+36). */
    if (period_business > 0 &&
        s->player_stock_amount[p][stock] < 5000 &&
        (double)s->stocks[stock].f20 <= value * 1.2) {
        score++;
        if (s->company_owner[link] >= 0 &&
            s->company_owner[link] < s->player_count &&
            s->company_owner[link] != p) {
            int owner = s->company_owner[link];
            unsigned int held_with_f8 =
                (unsigned int)s->player_stock_amount[p][stock] +
                s->stocks[stock].f8 + s->stock_company_available[stock];
            unsigned int held_with_f10 =
                (unsigned int)s->player_stock_amount[p][stock] +
                s->stocks[stock].f10;
            if (held_with_f8 > (unsigned int)
                    s->player_stock_amount[owner][stock]) score++;
            if (held_with_f10 > (unsigned int)
                    s->player_stock_amount[owner][stock]) score += 2;
        }
    }

    /* The last two company comparisons are against +36/10000, not the
     * business-volume tier above.  They are strict in the original x87
     * branches. */
    if ((double)s->stocks[stock].f20 < value * 0.85) score += 3;
    if ((double)s->stocks[stock].f20 < value * 0.70) score += 5;
    return score;
}

static int company_sell_score(const GameState *s, int p, int stock,
                              int distress)
{
    int link = s->stock_company_auxiliary[stock];
    int business;
    int company_value;
    int day = company_calendar_day(s);
    int owner;
    int total_holdings = 0;
    int i;
    int holdings = s->player_stock_amount[p][stock];
    int32_t period_business;
    double share;
    double current = s->stocks[stock].f20;
    double average_cost = s->player_stock_average_cost[p][stock];
    int score = 0;

    if (!company_link_valid(s, stock)) return 0;
    business = company_average_business(s, link);
    company_value = company_value_scaled(s, stock);
    period_business = (int32_t)s->company_period_business[link];
    for (i = 0; i < s->player_count; i++)
        if (s->player_stock_amount[i][stock] > 0)
            total_holdings += s->player_stock_amount[i][stock];
    /* 0x42C79F divides the total holding by this player's holding. */
    share = holdings > 0 ? (double)total_holdings / holdings : 0.0;
    owner = s->company_owner[link];

    /* 0x42C79F's first two company-only blocks handle a sufficiently
     * negative period result.  The original uses the date day byte, not the
     * calendar month. */
    if (period_business <= -(int64_t)s->price_index * 10000 &&
        share > 0.6 && day > 10 && day < 15)
        score += 3;
    if (period_business <= -(int64_t)s->price_index * 6000 &&
        current > average_cost * 1.2 &&
        owner != p &&
        day > 8 && day < 15)
        score += 2;

    if ((int64_t)business <= (int64_t)s->price_index * 10000 &&
        current >= (double)company_value * 2.0 &&
        current > average_cost * 1.3 && share < 0.4)
        score++;

    if (owner >= 0 && owner < s->player_count && owner != p) {
        unsigned int potential = (unsigned int)holdings +
            s->stocks[stock].f8 + s->stock_company_available[stock];
        if (potential < (unsigned int)s->player_stock_amount[owner][stock] &&
            period_business <= 0 && current >= average_cost * 1.5)
            score++;
    }

    if (owner != p &&
        current > (double)company_value * 2.0 && current > average_cost * 2.0 &&
        current < s->stocks[stock].f16)
        score += 2;
    if (owner == p &&
        current > (double)company_value * 3.0 && current > average_cost * 3.0 &&
        current < s->stocks[stock].f16)
        score += 2;
    if (distress) score++;
    return score;
}

static void sort_scores(ScoredStock *items, int count)
{
    int i, j;
    for (i = 1; i < count; i++) {
        ScoredStock value = items[i];
        j = i;
        while (j > 0 && (items[j - 1].score < value.score ||
               (items[j - 1].score == value.score &&
                items[j - 1].stock > value.stock))) {
            items[j] = items[j - 1];
            j--;
        }
        items[j] = value;
    }
}

void rich4_ai_select_stock_buy(const GameState *s, int p, Rich4Rng *rng,
                             GameAiDecision *d)
{
    const GamePlayerState *player = &s->players[p];
    ScoredStock scored[GAME_STOCK_COUNT];
    int budget, count = 0, holdings, target, i;
    if (rich4_rng_original_state_range(rng, 3) != 0) {
        stock_trace(d, stock_none_ref(), stock_none_ref(), GAME_AI_CANDIDATE_RANDOM_GATE, 0, 0);
        return;
    }
    if (player->ai_stock_ratio <= 0) {
        stock_trace(d, stock_none_ref(), stock_none_ref(), GAME_AI_CANDIDATE_DISABLED, 0, 0);
        return;
    }
    if (rich4_stock_market_is_closed(s)) {
        stock_trace(d, stock_none_ref(), stock_none_ref(), GAME_AI_CANDIDATE_MARKET_CLOSED, 0, 0);
        return;
    }
    if (player->loan_due_date != 0 &&
        (int64_t)player->loan_due_date - s->elapsed_days < 15) {
        stock_trace(d, stock_none_ref(), stock_none_ref(), GAME_AI_CANDIDATE_NO_BUDGET, 0, 0);
        return;
    }
    holdings = portfolio_value(s, p);
    target = (int)(((long long)player->ai_stock_ratio *
             ((long long)player->cash + player->bank + holdings)) / 100);
    budget = target - holdings;
    if (budget > player->bank) budget = player->bank;
    if (budget <= 0) {
        stock_trace(d, stock_none_ref(), stock_none_ref(), GAME_AI_CANDIDATE_NO_BUDGET, 0, budget);
        return;
    }
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        GameDomainRef ref = rich4_game_domain_ref(GAME_DOMAIN_STOCK, i);
        int score;
        if (s->stocks[i].f6 != 0 || s->stocks[i].f10 == 0 ||
            rich4_stock_price_state(s->stocks[i].f16, s->stocks[i].f20) ==
                GAME_STOCK_PRICE_LIMIT_UP) {
            stock_trace(d, ref, stock_none_ref(), GAME_AI_CANDIDATE_RULE_REJECTED, 0, 0);
            continue;
        }
        if (s->stocks[i].f4 != 0) {
            if (s->stock_company_auxiliary[i] < 0 ||
                s->stock_company_auxiliary[i] >= s->company_count ||
                s->stock_company_auxiliary[i] >= GAME_MAX_MAP_PROPERTIES) {
                stock_trace(d, ref, stock_none_ref(),
                            GAME_AI_CANDIDATE_RULE_REJECTED, 0, 0);
                continue;
            }
            if ((int64_t)player->bank <=
                (int64_t)s->price_index * 20000) {
                stock_trace(d, ref, stock_none_ref(),
                            GAME_AI_CANDIDATE_NO_BUDGET, 0, budget);
                continue;
            }
            score = company_buy_score(s, p, i);
        } else {
            if ((int64_t)player->bank <=
                (int64_t)s->price_index * 30000) {
                stock_trace(d, ref, stock_none_ref(),
                            GAME_AI_CANDIDATE_NO_BUDGET, 0, budget);
                continue;
            }
            score = independent_buy_score(s, i);
        }
        stock_trace(d, ref, stock_none_ref(), score ? GAME_AI_CANDIDATE_ELIGIBLE :
              GAME_AI_CANDIDATE_NO_SCORE, score, 0);
        if (score) { scored[count].stock = i; scored[count].score = score; count++; }
    }
    sort_scores(scored, count);
    for (i = 0; i < count; i++) {
        int stock = scored[i].stock;
        int quantity;
        if (rich4_rng_original_state_range(rng, 24) > 12 - i) continue;
        quantity = stock_truncate(
            (double)budget / s->stocks[stock].f20);
        if (quantity > s->stocks[stock].f10) quantity = s->stocks[stock].f10;
        /* 0x42C6CB commits to the first random-accepted stock even if the
         * calculated quantity truncates to zero; it does not try another. */
        if (quantity <= 0) return;
        d->command = rich4_game_domain_command(GAME_COMMAND_BUY_STOCK,
            GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, stock), stock_none_ref(), quantity);
        d->has_command = 1;
        stock_trace(d, d->command.subject, stock_none_ref(), GAME_AI_CANDIDATE_SELECTED,
              scored[i].score, quantity);
        return;
    }
}

static int independent_sell_score(const GameState *s, int p, int stock,
                                  int distress)
{
    const GameStockInfo *info = &s->stocks[stock];
    float average_cost = s->player_stock_average_cost[p][stock];
    float ratio, minimum, average24, average6;
    int liquid = s->players[p].cash + s->players[p].bank;
    int score = 0;
    if (average_cost <= 0.0f) return 0;
    ratio = info->f20 / average_cost;
    minimum = history_minimum(s, stock);
    average24 = history_average(s, stock, 24);
    average6 = history_average(s, stock, 6);
    if ((double)ratio > 1.6 && info->f28 < 1.0f) score += 2;
    if (info->f20 > minimum * 8.0f &&
        (double)(minimum * 8.0f) > (double)average_cost * 1.25)
        score += 2;
    if (average24 > average6 && info->f20 < info->f16) score += 2;
    if (ratio >= 2.0f)
        score += stock_truncate(1.0 + ((double)ratio - 2.0) / 0.5);
    if (liquid < s->price_index * 30000 && ratio > 0.0f)
        score += stock_truncate(1.0 + (double)ratio / 0.5);
    if (liquid < s->price_index * 16000 && ratio > 0.0f)
        score += stock_truncate(1.0 + (double)ratio / 0.5);
    if (distress) score *= 2;
    return score;
}

void rich4_ai_select_stock_sell(const GameState *s, int p, Rich4Rng *rng,
                              GameAiDecision *d)
{
    int due_soon = s->players[p].loan_due_date != 0 &&
        (int64_t)s->players[p].loan_due_date - s->elapsed_days <= 6;
    int distress = d->selection_after.stock_distress || (due_soon &&
        (int64_t)s->players[p].cash + s->players[p].bank <
        s->players[p].loan);
    int best = -1, best_score = 0, i;
    if (d->selection_after.stock_distress &&
        (double)s->players[p].cash + s->players[p].bank >=
            (double)s->players[p].loan * 1.1) {
        d->selection_after.stock_distress = 0;
        return;
    }
    d->selection_after.stock_distress = 0;
    if (!distress && rich4_rng_original_state_range(rng, 3) != 0) {
        stock_trace(d, stock_none_ref(), stock_none_ref(), GAME_AI_CANDIDATE_RANDOM_GATE, 0, 0);
        return;
    }
    if (rich4_stock_market_is_closed(s)) {
        stock_trace(d, stock_none_ref(), stock_none_ref(), GAME_AI_CANDIDATE_MARKET_CLOSED, 0, 0);
        return;
    }
    for (i = 0; i < GAME_STOCK_COUNT; i++) {
        GameDomainRef ref = rich4_game_domain_ref(GAME_DOMAIN_STOCK, i);
        int score;
        if (s->player_stock_amount[p][i] <= 0 || s->stocks[i].f6 != 0 ||
            rich4_stock_price_state(s->stocks[i].f16, s->stocks[i].f20) ==
                GAME_STOCK_PRICE_LIMIT_DOWN) {
            stock_trace(d, ref, stock_none_ref(), GAME_AI_CANDIDATE_NOT_OWNED, 0, 0);
            continue;
        }
        if (s->stocks[i].f4 != 0) {
            if (!company_link_valid(s, i)) {
                stock_trace(d, ref, stock_none_ref(),
                            GAME_AI_CANDIDATE_RULE_REJECTED, 0, 0);
                continue;
            }
            score = company_sell_score(s, p, i, distress);
        } else {
            score = independent_sell_score(s, p, i, distress);
        }
        stock_trace(d, ref, stock_none_ref(), score ? GAME_AI_CANDIDATE_ELIGIBLE :
              GAME_AI_CANDIDATE_NO_SCORE, score, s->player_stock_amount[p][i]);
        if (score > best_score) { best = i; best_score = score; }
    }
    if (best >= 0) {
        d->command = rich4_game_domain_command(GAME_COMMAND_SELL_STOCK,
            GAME_COMMAND_SOURCE_AI, p,
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, best), stock_none_ref(),
            s->player_stock_amount[p][best]);
        d->has_command = 1;
        stock_trace(d, d->command.subject, stock_none_ref(), GAME_AI_CANDIDATE_SELECTED,
              best_score, d->command.value);
        if (distress) {
            d->next_phase = GAME_AI_PHASE_STOCK_SELL;
            d->selection_after.stock_distress = 1;
        }
    }
}
