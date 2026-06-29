#include "core/god.h"

#include <limits.h>

#include "core/game_event_payload.h"
#include "core/ledger.h"

/* 0x40EC68/0x40ED42/0x40EFA0/0x40F061: settle the unscaled result returned
 * by 0x440706. Keep the entire settlement transactional on queue failure. */
static GameGodResult resolve_money(GameState *state, int player,
                                      int object_slot, int god_type,
                                      int amount, int other, GameEventQueue *events)
{
    GameState next;
    GameEventQueue staged;
    int digits = rich4_god_money_digits(god_type);
    int i;
    if (state == NULL || events == NULL || !state->active ||
        state->game_over || player < 0 || player >= state->player_count ||
        (state->players[player].play_state & 0xff) == 0 ||
        object_slot <= 0 || state->players[player].god_info != object_slot ||
        state->players[player].god_type != god_type || digits == 0 ||
        other < -1 || other >= state->player_count || other == player ||
        (other >= 0 && god_type != 1 && god_type != 5) ||
        amount < 0 || amount > (digits == 3 ? 999 : 9999))
        return GAME_GOD_INVALID;
    if (amount == 0) return GAME_GOD_APPLIED;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (god_type == 2) {
        if (next.players[player].cash < 0 ||
            next.players[player].cash > INT_MAX - amount)
            return GAME_GOD_INVALID;
        next.players[player].cash += amount;
        /* 0x40ED42 -> 0x41D3F4 records this cash windfall. */
        next.players[player].monthly_income = (int32_t)(
            (uint32_t)next.players[player].monthly_income + (uint32_t)amount);
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_player_cash_changed_from_fields,
                player, rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
                rich4_game_domain_ref(GAME_DOMAIN_GOD, object_slot - 1),
                next.players[player].cash, amount, 0))
            return GAME_GOD_EVENT_CAPACITY;
    } else if (god_type == 6) {
        if (rich4_ledger_pay(&next, player, rich4_ledger_system(), amount,
                            0, &staged) == GAME_PAYMENT_REJECTED)
            return GAME_GOD_INVALID;
    } else {
        for (i = 0; i < next.player_count && !next.game_over; i++) {
            int payer;
            int receiver;
            if ((next.players[player].play_state & 0xff) == 0) break;
            if (other >= 0 && i != other) continue;
            if (i == player || (next.players[i].play_state & 0xff) == 0)
                continue;
            payer = god_type == 1 ? i : player;
            receiver = god_type == 1 ? player : i;
            if (rich4_ledger_pay(
                    &next, payer, rich4_ledger_player(receiver), amount,
                    god_type == 1 ? GAME_PAYMENT_RECEIVER_CASH : 0,
                    &staged) == GAME_PAYMENT_REJECTED)
                return GAME_GOD_INVALID;
        }
    }
    if (!rich4_game_event_queue_append(events, &staged))
        return GAME_GOD_EVENT_CAPACITY;
    *state = next;
    return GAME_GOD_APPLIED;
}

GameGodResult rich4_god_resolve_money(GameState *state, int player,
                                      int object_slot, int god_type,
                                      int amount, GameEventQueue *events)
{
    return resolve_money(state, player, object_slot, god_type, amount, -1, events);
}

GameGodResult rich4_god_resolve_money_player(GameState *state, int player,
    int object_slot, int god_type, int amount, int other, GameEventQueue *events)
{
    if (other < 0) return GAME_GOD_INVALID;
    return resolve_money(state, player, object_slot, god_type, amount, other, events);
}
