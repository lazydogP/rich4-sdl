#include <limits.h>
#include <string.h>

#include "core/card_internal.h"
#include "core/game_event_payload.h"
#include "core/god.h"
#include "core/stock.h"

/* Active card command application; consumes typed domain commands and emits events. */
static GameCardUseResult card_apply_command(GameState *state,
    const GameCommand *command, GameEventQueue *events, int defer_confinement);

GameCardUseResult rich4_card_apply_command(GameState *state,
    const GameCommand *command, GameEventQueue *events)
{
    return card_apply_command(state, command, events, 0);
}

GameCardUseResult rich4_card_apply_command_deferred_confinement(GameState *state,
    const GameCommand *command, GameEventQueue *events)
{
    return card_apply_command(state, command, events, 1);
}

static GameCardUseResult card_apply_command(GameState *state,
                                           const GameCommand *command,
                                           GameEventQueue *events, int defer_confinement)
{
    GameCardUseResult inspected;
    GameState next;
    GameEventQueue staged;
    int card, target = -1;
    int card_consumed = 0;
    if (events == NULL) return GAME_CARD_USE_INVALID;
    inspected = rich4_card_inspect(state, command);
    if (inspected != GAME_CARD_USE_APPLIED) return inspected;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    card = command->subject.id;
    if (card == 0) {
        if (!rich4_card_apply_average_cash(&next, command->player, &staged))
            return GAME_CARD_USE_INVALID;
    } else if (card == 1) {
        int64_t sum;
        target = command->target.id;
        if (target == command->player) return GAME_CARD_USE_INVALID;
        sum = (int64_t)next.players[command->player].cash + next.players[target].cash;
        if (sum < 0 || sum > (int64_t)INT_MAX * 2) return GAME_CARD_USE_INVALID;
        next.players[command->player].cash = next.players[target].cash = (int)(sum / 2);
        if (!rich4_card_emit_effect(&staged, command->player, command->subject, command->target,
                         (int)(sum / 2), 0)) return GAME_CARD_USE_INVALID;
    } else if ((card >= 2 && card <= 11 && card != 5) ||
               card == 26 || card == 27) {
        if (!rich4_card_apply_property_card(&next, command, card, &staged))
            return GAME_CARD_USE_INVALID;
    } else if (card == 5) {
        target = command->target.id;
        if (command->target.kind == GAME_DOMAIN_SPECIAL_PLAYER)
            next.special_player_route_reversed[target] ^= 1;
        else next.players[target].route_reversed ^= 1;
        if (!rich4_card_emit_effect(&staged, command->player, command->subject,
                         command->target, 1, 0)) return GAME_CARD_USE_INVALID;
    } else if (card == 12) {
        int item = command->value;
        int rob_tool = (command->flags & GAME_CARD_FLAG_ROB_TOOL) != 0;
        target = command->target.id;
        if (target == command->player ||
            command->target.kind != GAME_DOMAIN_PLAYER ||
            !(rob_tool
                ? rich4_card_transfer_tool(&next, target, command->player, item)
                : rich4_card_inventory_transfer(
                      &next, target, command->player, item)) ||
            !rich4_card_emit_effect(&staged, command->player, command->subject,
                         command->target, item, rob_tool))
            return GAME_CARD_USE_INVALID;
        /* 0x443E3D indexes the card price table with the selector's returned
         * id even when the selected row is a tool. */
        if (item >= 0 && item < GAME_CARD_COUNT)
            rich4_card_add_relation_balance(&next, target, command->player,
                                 rich4_card_catalog[item].shop_price);
    } else if (card == 13) {
        target = command->target.id;
        if (command->target.kind == GAME_DOMAIN_SPECIAL_PLAYER)
            next.special_player_status[target][2] = 1;
        else
            next.players[target].days_stopping =
                target == command->player ? GAME_STATUS_PENDING_BIT : 1;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_status_changed_from_fields,
                command->player, command->target, command->subject,
                command->target.kind == GAME_DOMAIN_SPECIAL_PLAYER ? 1 :
                    next.players[target].days_stopping, 0, 0))
            return GAME_CARD_USE_INVALID;
    } else if (card == 14) {
        for (target = 0; target < next.player_count; target++) {
            if (target == command->player ||
                (next.players[target].play_state & 0xff) == 0) continue;
            next.players[target].days_sleeping = 5;
            next.players[target].total_confinement_days =
                (uint8_t)(next.players[target].total_confinement_days + 5);
            next.players[target].days_sleep_walking = 0;
        }
        for (target = 0; target < GAME_SPECIAL_PLAYER_COUNT; target++)
            if (next.special_player_f10[target] == 0)
                next.special_player_status[target][0] = 5;
        target = -1;
        if (!rich4_card_emit_effect(&staged, command->player, command->subject,
                         rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 5, 0))
            return GAME_CARD_USE_INVALID;
    } else if (card == 15 || card == 16) {
        target = command->target.id;
        if (command->target.kind == GAME_DOMAIN_SPECIAL_PLAYER) {
            next.special_player_status[target][card == 15 ? 1 : 2] = 5;
            if (!rich4_card_emit_effect(&staged, command->player, command->subject,
                             command->target, 5, 0)) return GAME_CARD_USE_INVALID;
        } else if (!(defer_confinement ? rich4_card_resolve_attack_deferred_confinement :
                    rich4_card_resolve_attack)(&next, command->player, target,
                    card == 15 ? GAME_CARD_ATTACK_SLEEP_WALK : GAME_CARD_ATTACK_FRAME,
                    (command->flags & GAME_CARD_FLAG_USE_MISDIRECT) ? command->value : -1,
                    &staged)) return GAME_CARD_USE_INVALID;
    } else if (card == 21) {
        GamePlayerState *player = &next.players[command->player];
        int bomb = player->attached_timed_bomb_slot;
        int god_type = rich4_god_player_type(player);
        if (bomb >= 0 && bomb < GAME_TOOL_PLACED_OBJECT_COUNT &&
            next.tool_placed_objects[bomb].kind == 0x12 &&
            next.tool_placed_objects[bomb].attached_player == command->player + 1) {
            int node = next.tool_placed_objects[bomb].node;
            memset(&next.tool_placed_objects[bomb], 0,
                   sizeof(next.tool_placed_objects[bomb]));
            player->attached_timed_bomb_slot = -1;
            if (!rich4_game_event_emit_typed_domain(
                    &staged, rich4_game_event_map_object_changed_from_fields,
                    command->player, command->subject,
                    rich4_game_domain_ref(GAME_DOMAIN_PLAYER, command->player),
                    node, 0, 0)) return GAME_CARD_USE_INVALID;
        }
        if (rich4_god_is_sendable_type(god_type) &&
            rich4_god_send_away(&next, command->player, &staged) !=
                GAME_GOD_APPLIED)
                return GAME_CARD_USE_INVALID;
    } else if (card == 22) {
        target = command->target.id;
        /* 0x41D546 consumes 请神符 before 0x40EAD7 applies type 3/4 card
         * rewards; otherwise a full hand would incorrectly treat the card
         * being used as the discard candidate. */
        if (!rich4_card_consume(&next, command->player, card))
            return GAME_CARD_USE_INVALID;
        card_consumed = 1;
        if (rich4_god_attach_replace(&next, command->player, target + 1,
                                     command->value, &staged) !=
            GAME_GOD_APPLIED)
            return GAME_CARD_USE_INVALID;
        if (!rich4_card_emit_effect(&staged, command->player, command->subject,
                         command->target, command->value, 0))
            return GAME_CARD_USE_INVALID;
    } else if (card == 23 || card == 24) {
        target = command->target.id;
        next.stocks[target].f7 = card == 23 ? 32 : 2;
        if (!rich4_stock_apply_forced_quote(&next, target))
            return GAME_CARD_USE_INVALID;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_market_changed_from_fields,
                command->player, command->target, command->subject,
                next.stocks[target].f7, 0, 0)) return GAME_CARD_USE_INVALID;
    } else if (card == 25) {
        int payer, amount;
        target = command->target.id;
        if (target == command->player ||
            command->target.kind != GAME_DOMAIN_PLAYER)
            return GAME_CARD_USE_INVALID;
        /* 0x4452CE multiplies the selected payer's cash field by 0.2. */
        amount = next.players[target].cash > 0 ? next.players[target].cash / 5 : 0;
        if (!rich4_card_resolve_charge(&next, target, command->player, amount,
                (command->flags & GAME_CARD_FLAG_ACCEPT_FREE) != 0,
                (command->flags & GAME_CARD_FLAG_USE_MISDIRECT) ? command->value : -1,
                &payer, &amount, &staged))
            return GAME_CARD_USE_INVALID;
        if (amount != 0 && payer != target)
            amount = next.players[payer].cash > 0 ? next.players[payer].cash / 5 : 0;
        if (next.players[payer].cash < amount) return GAME_CARD_USE_INVALID;
        next.players[payer].cash -= amount;
        next.players[command->player].cash += amount;
    } else if (card == 28) {
        target = command->target.id;
        if (target == command->player || command->target.kind != GAME_DOMAIN_PLAYER)
            return GAME_CARD_USE_INVALID;
        next.players[command->player].linked_player = target;
        next.players[target].linked_player = command->player;
        if (!rich4_card_emit_effect(&staged, command->player, command->subject,
                         command->target, target, 0)) return GAME_CARD_USE_INVALID;
    } else if (card == 29) {
        target = command->target.id;
        if (command->target.kind == GAME_DOMAIN_SPECIAL_PLAYER)
            next.special_player_status[target][3] = 3;
        else
            next.players[target].days_tortoise_walking =
                target == command->player ? 2 : 3;
        if (!rich4_game_event_emit_typed_domain(
                &staged, rich4_game_event_status_changed_from_fields,
                command->player, command->target, command->subject,
                command->target.kind == GAME_DOMAIN_SPECIAL_PLAYER ? 3 :
                    next.players[target].days_tortoise_walking, 0, 0))
            return GAME_CARD_USE_INVALID;
    } else {
        return GAME_CARD_USE_DEFERRED;
    }
    if ((!card_consumed && !rich4_card_consume(&next, command->player, card)) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields,
            command->player, command->subject,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, command->player), -1,
            rich4_card_inventory_count(&next, command->player), 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_card_used_from_fields,
            command->player, command->subject, command->target, target,
            next.card_amount[card], 0) || !rich4_card_append_events(events, &staged))
        return GAME_CARD_USE_INVALID;
    *state = next;
    return GAME_CARD_USE_APPLIED;
}
