#include "core/insurance.h"
#include "core/game_event_payload.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static int active_player(const GameState *state, int player)
{
    return state != NULL && player >= 0 && player < state->player_count &&
           (state->players[player].play_state & 0xff) != 0;
}

static GameDomainRef insurance_ref(GameDomainKind kind, int id)
{
    GameDomainRef ref;
    ref.kind = kind;
    ref.id = id;
    return ref;
}

int rich4_insurance_find_company(const GameState *state)
{
    int company;
    if (state == NULL || state->company_count < 0 ||
        state->company_count > GAME_MAX_MAP_PROPERTIES)
        return -1;
    for (company = 0; company < state->company_count; company++)
        if (state->company_type[company] == 4)
            return company;
    return -1;
}

int rich4_insurance_record_business(GameState *state, int company, int amount)
{
    if (state == NULL || company < 0 || company >= state->company_count ||
        company >= GAME_MAX_MAP_PROPERTIES ||
        state->company_type[company] != 4 || amount < 0)
        return 0;
    state->company_period_business[company] += (uint32_t)amount;
    state->company_total_business[company] += (uint32_t)amount;
    return 1;
}

static int insurance_claim(GameState *state, int player, int amount,
                          GameInsuranceClaimCause cause,
                          GameInsuranceClaimResult *out,
                          GameEventQueue *events, int mode, int days)
{
    GameInsuranceClaimResult result;
    GameEventQueue staged;
    GameState next;
    int company;
    int stock;
    GameEvent insurance_event;
    memset(&result, 0, sizeof(result));
    result.player = player;
    result.company = -1;
    result.stock = -1;
    result.amount = amount;
    result.cause = cause;
    if (out != NULL) *out = result;
    if (state == NULL || events == NULL || amount < 0 ||
        cause <= GAME_INSURANCE_CLAIM_NONE ||
        cause > GAME_INSURANCE_CLAIM_FORTUNE ||
        (mode == 1 && (days < 0 || days > 0xff)) ||
        player < 0 || player >= state->player_count)
        return 0;
    if (!active_player(state, player) ||
        (state->players[player].days_assurance & 0xff) == 0)
        return 1;
    company = rich4_insurance_find_company(state);
    if (company < 0) return 1;
    stock = state->company_stock_index[company];
    /* 0x44BA88 selects by company type alone, independently of shares. */
    next = *state;
    rich4_game_event_queue_reset(&staged);
    insurance_event = rich4_game_event_insurance_claim(
        player, company, player, amount, 2000, (unsigned int)cause |
        (mode == 1 ? GAME_INSURANCE_CLAIM_DEFERRED |
            ((unsigned int)(days & 0xff) << GAME_INSURANCE_CLAIM_DAYS_SHIFT) : 0u));
    if ((mode != 2 && !rich4_game_event_queue_push(&staged, &insurance_event)) ||
        (mode != 1 && !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_money_transferred_from_fields, player,
            insurance_ref(GAME_DOMAIN_COMPANY, company),
            insurance_ref(GAME_DOMAIN_PLAYER, player), amount,
            stock, 0)) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY)
        return 0;

    if (mode == 1) return rich4_game_event_queue_append(events, &staged);

    /* 0x41D2C6 does not test the insurer's balance.  The unsigned company
     * business counters therefore deliberately wrap when claims exceed
     * accumulated premiums. */
    next.players[player].cash =
        (int)((uint32_t)next.players[player].cash + (uint32_t)amount);
    next.players[player].monthly_income =
        (int)((uint32_t)next.players[player].monthly_income + (uint32_t)amount);
    next.company_period_business[company] -= (uint32_t)amount;
    next.company_total_business[company] -= (uint32_t)amount;
    if (!rich4_game_event_queue_append(events, &staged)) return 0;
    *state = next;
    result.applied = 1;
    result.company = company;
    result.stock = stock;
    if (out != NULL) *out = result;
    return 1;
}

int rich4_insurance_claim(GameState *state, int player, int amount,
    GameInsuranceClaimCause cause, GameInsuranceClaimResult *out,
    GameEventQueue *events)
{
    return insurance_claim(state, player, amount, cause, out, events, 0, 0);
}

int rich4_insurance_request_claim(GameState *state, int player, int amount,
    GameInsuranceClaimCause cause, int days, GameEventQueue *events)
{
    return insurance_claim(state, player, amount, cause, NULL, events, 1, days);
}

int rich4_insurance_settle_claim(GameState *state, int player, int amount,
    GameInsuranceClaimCause cause, GameEventQueue *events)
{
    return insurance_claim(state, player, amount, cause, NULL, events, 2, 0);
}

int rich4_insurance_claim_for_days(GameState *state, int player, int days,
                                   GameInsuranceClaimCause cause,
                                   GameInsuranceClaimResult *out,
                                   GameEventQueue *events)
{
    int64_t amount;
    if (state == NULL || days <= 0 || player < 0 ||
        player >= state->player_count)
        return 0;
    if (!active_player(state, player) ||
        (state->players[player].days_assurance & 0xff) == 0)
        return rich4_insurance_claim(state, player, 0, cause, out, events);
    if (state->price_index <= 0) return 0;
    amount = (int64_t)2000 * days * state->price_index;
    if (amount > INT_MAX) return 0;
    return rich4_insurance_claim(state, player, (int)amount, cause, out,
                                 events);
}

int rich4_insurance_request_claim_for_days(GameState *state, int player, int days,
    GameInsuranceClaimCause cause, GameEventQueue *events)
{
    int64_t amount;
    if (state == NULL || days <= 0 || events == NULL || player < 0 ||
        player >= state->player_count) return 0;
    if (!active_player(state, player) ||
        (state->players[player].days_assurance & 0xff) == 0) return 1;
    if (state->price_index <= 0) return 0;
    amount = (int64_t)2000 * days * state->price_index;
    if (amount > INT_MAX) return 0;
    return rich4_insurance_request_claim(state, player, (int)amount, cause,
                                        days, events);
}
