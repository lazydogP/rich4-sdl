#include "core/game_dispatch.h"
#include "core/game_event_internal.h"

#include "core/card.h"
#include "core/bank.h"
#include "core/ledger.h"
#include "core/property.h"
#include "core/facility.h"
#include "core/player_status.h"
#include "core/tool.h"
#include "core/stock.h"

static int emit(GameEventQueue *events, GameEventType type,
                const GameCommand *command, GameCommandRejectReason reason,
                int value)
{
    GameEvent event = rich4_game_event_internal(type, 0, 0, value, 0, 0);
    if (command != NULL) event.command = *command;
    event.reject_reason = reason;
    return rich4_game_event_queue_push(events, &event);
}

static int reject(GameEventQueue *events, const GameCommand *command,
                  GameCommandRejectReason reason)
{
    emit(events, GAME_EVENT_COMMAND_REJECTED, command, reason, 0);
    return 0;
}

static int dispatch_command(GameState *state, GameFlowState *flow,
    const GameCommand *command, GameEventQueue *events, int defer_confinement);

int rich4_game_dispatch_command(GameState *state, GameFlowState *flow,
    const GameCommand *command, GameEventQueue *events)
{
    return dispatch_command(state, flow, command, events, 0);
}

int rich4_game_dispatch_command_deferred_confinement(GameState *state,
    GameFlowState *flow, const GameCommand *command, GameEventQueue *events)
{
    return dispatch_command(state, flow, command, events, 1);
}

static int player_control_is_locked(const GameState *state, int player)
{
    PlayerStatusDecision decision;
    return rich4_player_status_evaluate(state, player, 0, &decision) &&
           decision.status_slot >= 50;
}

static int dispatch_command(GameState *state,
                                GameFlowState *flow,
                                const GameCommand *command,
                                GameEventQueue *events, int defer_confinement)
{
    int current;
    GamePlayerState *player;
    if (events == NULL) return 0;
    if (!rich4_game_command_is_valid(command))
        return reject(events, command, GAME_COMMAND_REJECT_INVALID);
    if (state == NULL || flow == NULL || !state->active || state->player_count <= 0)
        return reject(events, command, GAME_COMMAND_REJECT_INACTIVE);
    if (state->game_over)
        return reject(events, command, GAME_COMMAND_REJECT_STATE);
    current = state->current_player % state->player_count;
    if (current < 0) current += state->player_count;
    if (command->player != current)
        return reject(events, command, GAME_COMMAND_REJECT_PLAYER);
    player = &state->players[current];
    if ((command->source == GAME_COMMAND_SOURCE_AI) != (player->is_ai != 0))
        return reject(events, command, GAME_COMMAND_REJECT_SOURCE);
    if (flow->turn_state == GAME_TURN_STATE_ROUTE_EVENTS &&
        rich4_route_event_executor_is_waiting(&flow->route_events)) {
        RouteEventCommandResult route_result =
            rich4_route_event_executor_submit(&flow->route_events,
                                               state,
                                               command, events);
        if (route_result == ROUTE_EVENT_COMMAND_APPLIED) return 1;
        if (route_result == ROUTE_EVENT_COMMAND_DEFERRED)
            return reject(events, command, GAME_COMMAND_REJECT_UNSUPPORTED);
        if (route_result == ROUTE_EVENT_COMMAND_STALE ||
            route_result == ROUTE_EVENT_COMMAND_UNEXPECTED)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        return reject(events, command, GAME_COMMAND_REJECT_INVALID);
    }
    /*
     * 0x40C912(0) never exposes ordinary player control for +50..+55.
     * A sleepwalking turn reaches PREROLL through the automatic path below,
     * so an already active turn flow must remain able to request its roll.
     */
    if (!flow->turn_flow_active &&
        player_control_is_locked(state, current))
        return reject(events, command, GAME_COMMAND_REJECT_STATE);
    switch (command->type) {
    case GAME_COMMAND_ROLL_DICE:
        if (flow->turn_flow_active && flow->turn_state == 2)
            return emit(events, GAME_EVENT_DICE_ROLL_REQUESTED, command,
                        GAME_COMMAND_REJECT_NONE, 0);
        if (flow->turn_flow_active || flow->steps_remaining > 0)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        return emit(events, GAME_EVENT_TURN_FLOW_REQUESTED, command,
                    GAME_COMMAND_REJECT_NONE, 0);
    case GAME_COMMAND_SELECT_DICE_COUNT:
        if (flow->turn_flow_active &&
            !(command->source == GAME_COMMAND_SOURCE_AI && flow->turn_state == 2))
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        if (command->value > player->traffic_method + 1)
            return reject(events, command, GAME_COMMAND_REJECT_VALUE);
        if (events->count >= GAME_EVENT_QUEUE_CAPACITY) return 0;
        player->ndices = command->value;
        return emit(events, GAME_EVENT_DICE_COUNT_CHANGED, command,
                    GAME_COMMAND_REJECT_NONE, command->value);
    case GAME_COMMAND_SELL_ASSET:
    {
        GameState next = *state;
        GameEventQueue staged;
        rich4_game_event_queue_reset(&staged);
        if (!rich4_ledger_sell_asset(&next, command, &staged) ||
            !rich4_bank_settle_special_finance(
                &next, 0, current, &staged) ||
            events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
            !rich4_game_event_queue_append(events, &staged))
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        *state = next;
        return 1;
    }
    case GAME_COMMAND_CREATE_MARKET_LISTING:
    case GAME_COMMAND_CANCEL_MARKET_LISTING:
    case GAME_COMMAND_BUY_MARKET_LISTING:
    case GAME_COMMAND_REPRICE_MARKET_LISTING: {
        GameState next = *state;
        GameEventQueue staged;
        if (flow->turn_flow_active || flow->steps_remaining > 0)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        rich4_game_event_queue_reset(&staged);
        if (!rich4_ledger_apply_market_command(
                &next, command, &staged) ||
            events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
            !rich4_game_event_queue_append(events, &staged))
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        *state = next;
        return 1;
    }
    case GAME_COMMAND_SETTLE_AI_SPECIAL_FINANCE: {
        GameState next;
        GameEventQueue staged;
        if (command->source != GAME_COMMAND_SOURCE_AI || !player->is_ai ||
            flow->turn_flow_active || flow->steps_remaining > 0)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        next = *state;
        rich4_game_event_queue_reset(&staged);
        if (!rich4_bank_settle_special_finance(
                &next, 0, current, &staged) ||
            events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
            !rich4_game_event_queue_append(events, &staged))
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        *state = next;
        return 1;
    }
    case GAME_COMMAND_PURCHASE_PROPERTY:
    case GAME_COMMAND_UPGRADE_PROPERTY:
        if (flow->turn_flow_active || flow->steps_remaining > 0)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        if (command->subject.kind == GAME_DOMAIN_LAND) {
            if (rich4_land_apply_command(state, command, events)) return 1;
        } else if (command->subject.kind == GAME_DOMAIN_FACILITY) {
            if (rich4_facility_apply_command(state, command, events)) return 1;
        }
        return reject(events, command, GAME_COMMAND_REJECT_STATE);
    case GAME_COMMAND_BUY_STOCK:
    case GAME_COMMAND_SELL_STOCK:
    {
        GameStockTradeResult stock_result;
        if (flow->turn_flow_active || flow->steps_remaining > 0)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        if (command->type == GAME_COMMAND_SELL_STOCK) {
            GameState next = *state;
            GameEventQueue staged;
            rich4_game_event_queue_reset(&staged);
            stock_result = rich4_stock_apply_command(
                &next, command, &staged);
            if (stock_result == GAME_STOCK_TRADE_APPLIED &&
                rich4_bank_settle_special_finance(
                    &next, 0, current, &staged) &&
                events->count + staged.count <= GAME_EVENT_QUEUE_CAPACITY &&
                rich4_game_event_queue_append(events, &staged)) {
                *state = next;
                return 1;
            }
            if (stock_result == GAME_STOCK_TRADE_INVALID)
                return reject(events, command, GAME_COMMAND_REJECT_INVALID);
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        } else {
            stock_result = rich4_stock_apply_command(state, command, events);
        }
        if (stock_result == GAME_STOCK_TRADE_APPLIED) return 1;
        if (stock_result == GAME_STOCK_TRADE_INVALID)
            return reject(events, command, GAME_COMMAND_REJECT_INVALID);
        return reject(events, command, GAME_COMMAND_REJECT_STATE);
    }
    case GAME_COMMAND_USE_CARD: {
        GameCardUseResult card_result;
        if (flow->turn_flow_active || flow->steps_remaining > 0)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        card_result = (defer_confinement ? rich4_card_apply_command_deferred_confinement :
            rich4_card_apply_command)(state, command, events);
        if (card_result == GAME_CARD_USE_APPLIED) return 1;
        if (card_result == GAME_CARD_USE_DEFERRED ||
            card_result == GAME_CARD_USE_UNAVAILABLE)
            return reject(events, command, GAME_COMMAND_REJECT_UNSUPPORTED);
        return reject(events, command, GAME_COMMAND_REJECT_STATE);
    }
    case GAME_COMMAND_USE_TOOL: {
        GameToolUseResult tool_result;
        if (flow->turn_flow_active || flow->steps_remaining > 0)
            return reject(events, command, GAME_COMMAND_REJECT_STATE);
        tool_result = (defer_confinement ? rich4_tool_apply_command_deferred_confinement :
            rich4_tool_apply_command)(state, command, events);
        if (tool_result == GAME_TOOL_USE_APPLIED) return 1;
        if (tool_result == GAME_TOOL_USE_DEFERRED ||
            tool_result == GAME_TOOL_USE_RESTORE_REQUIRED ||
            tool_result == GAME_TOOL_USE_UNAVAILABLE)
            return reject(events, command, GAME_COMMAND_REJECT_UNSUPPORTED);
        return reject(events, command, GAME_COMMAND_REJECT_STATE);
    }
    default:
        return reject(events, command, GAME_COMMAND_REJECT_UNSUPPORTED);
    }
}
