#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

static RouteEventExecutionResult append_shop_transaction(
    GameState *state, const GameState *next, GameEventQueue *events,
    const GameEventQueue *staged)
{
    if (events->count + staged->count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = *next;
    return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
}

RouteEventExecutionResult rich4_route_event_executor_shop_buy_card(
    RouteEventExecutor *executor, GameState *state, int card,
    GameEventQueue *events)
{
    const GameCardInfo *info;
    GameState next;
    GameEventQueue staged;
    int player;
    if (state == NULL || events == NULL ||
        !presentation_matches(executor, 15))
        return ROUTE_EVENT_EXECUTION_INVALID;
    info = rich4_card_info(card);
    player = executor->state.execution_actor;
    if (info == NULL || state->card_amount[card] == 0 ||
        state->players[player].points < info->shop_price ||
        rich4_card_inventory_count(state, player) >= GAME_PLAYER_CARD_SLOTS)
        return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_card_inventory_add_from_pool(
            &next, player, card, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next.players[player].points =
        (uint16_t)(next.players[player].points - info->shop_price);
    shop_add_business_volume(
        &next, shop_company_index(executor, &next),
        (uint32_t)(info->shop_price * 10));
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
            -info->shop_price, next.players[player].points, 0))
        return ROUTE_EVENT_EXECUTION_INVALID;
    return append_shop_transaction(state, &next, events, &staged);
}

RouteEventExecutionResult rich4_route_event_executor_shop_buy_tool(
    RouteEventExecutor *executor, GameState *state, int tool,
    GameEventQueue *events)
{
    const GameToolInfo *info;
    GameState next;
    GameEventQueue staged;
    int player;
    if (state == NULL || events == NULL ||
        !presentation_matches(executor, 15))
        return ROUTE_EVENT_EXECUTION_INVALID;
    info = rich4_tool_info(tool);
    player = executor->state.execution_actor;
    if (info == NULL || tool < 0 || tool >= 8 ||
        state->tool_amount[tool] == 0 ||
        state->players[player].points < info->shop_price ||
        state->players[player].tool_inventory[tool] >= 9)
        return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    next.tool_amount[tool]--;
    next.players[player].tool_inventory[tool]++;
    next.players[player].points =
        (uint16_t)(next.players[player].points - info->shop_price);
    shop_add_business_volume(
        &next, shop_company_index(executor, &next),
        (uint32_t)(info->shop_price * 10));
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            1, next.players[player].tool_inventory[tool], 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            -info->shop_price, next.players[player].points, 0))
        return ROUTE_EVENT_EXECUTION_INVALID;
    return append_shop_transaction(state, &next, events, &staged);
}

RouteEventExecutionResult rich4_route_event_executor_shop_sell_card_slot(
    RouteEventExecutor *executor, GameState *state, int slot,
    GameEventQueue *events)
{
    const GameCardInfo *info;
    GameState next;
    GameEventQueue staged;
    int player;
    int card;
    int i;
    int proceeds;
    if (state == NULL || events == NULL ||
        !presentation_matches(executor, 15) ||
        slot < 0 || slot >= GAME_PLAYER_CARD_SLOTS)
        return ROUTE_EVENT_EXECUTION_INVALID;
    player = executor->state.execution_actor;
    if (state->players[player].card_inventory[slot] == 0)
        return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
    card = state->players[player].card_inventory[slot] - 1;
    info = rich4_card_info(card);
    if (info == NULL) return ROUTE_EVENT_EXECUTION_INVALID;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    for (i = slot; i + 1 < GAME_PLAYER_CARD_SLOTS; i++)
        next.players[player].card_inventory[i] =
            next.players[player].card_inventory[i + 1];
    next.players[player].card_inventory[GAME_PLAYER_CARD_SLOTS - 1] = 0;
    if (next.card_amount[card] < 255) next.card_amount[card]++;
    proceeds = (info->shop_price * 9) / 10;
    next.players[player].points =
        (uint16_t)(next.players[player].points + proceeds);
    shop_add_business_volume(
        &next, shop_company_index(executor, &next),
        (uint32_t)info->shop_price);
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
            -1, rich4_card_inventory_count(&next, player), 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_CARD, card),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            proceeds, next.players[player].points, 0))
        return ROUTE_EVENT_EXECUTION_INVALID;
    return append_shop_transaction(state, &next, events, &staged);
}

RouteEventExecutionResult rich4_route_event_executor_shop_sell_tool(
    RouteEventExecutor *executor, GameState *state, int tool,
    GameEventQueue *events)
{
    const GameToolInfo *info;
    GameState next;
    GameEventQueue staged;
    int player;
    int proceeds;
    if (state == NULL || events == NULL ||
        !presentation_matches(executor, 15))
        return ROUTE_EVENT_EXECUTION_INVALID;
    info = rich4_tool_info(tool);
    player = executor->state.execution_actor;
    if (info == NULL || state->players[player].tool_inventory[tool] == 0)
        return ROUTE_EVENT_EXECUTION_WAITING_FOR_COMMAND;
    next = *state;
    rich4_game_event_queue_reset(&staged);
    next.players[player].tool_inventory[tool]--;
    if (tool >= 0 && tool < GAME_TOOL_COUNT &&
        next.tool_amount[tool] < 255)
        next.tool_amount[tool]++;
    proceeds = (info->shop_price * 9) / 10;
    next.players[player].points =
        (uint16_t)(next.players[player].points + proceeds);
    shop_add_business_volume(
        &next, shop_company_index(executor, &next),
        (uint32_t)info->shop_price);
    if (!rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            -1, next.players[player].tool_inventory[tool], 0) ||
        !rich4_game_event_emit_typed_domain(
            &staged, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            proceeds, next.players[player].points, 0))
        return ROUTE_EVENT_EXECUTION_INVALID;
    return append_shop_transaction(state, &next, events, &staged);
}

RouteEventExecutionResult rich4_route_event_executor_finish_shop(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    RouteEventExecutor next_queue;
    GameEventQueue staged;
    (void)state;
    if (state == NULL || events == NULL ||
        !presentation_matches(executor, 15))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue = *executor;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    rich4_game_event_queue_reset(&staged);
    if (!rich4_route_event_executor_complete_events(
            &next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}

RouteEventExecutionResult rich4_route_event_executor_resume_ai_shop(
    RouteEventExecutor *executor, GameState *state, GameEventQueue *events)
{
    RouteEventExecutor next_queue;
    GameState next_state;
    GameEventQueue staged;
    if (state == NULL || events == NULL || !presentation_matches(executor, 15) ||
        (state->players[executor->state.execution_actor].play_state & 0xff) == 1)
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue = *executor;
    next_state = *state;
    rich4_game_event_queue_reset(&staged);
    if (!run_ai_shop(&next_state, executor->state.execution_actor,
                    shop_company_index(executor, &next_state), &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    next_queue.state.waiting_for_presentation = 0;
    next_queue.state.presentation_action = 0;
    if (!rich4_route_event_executor_complete_events(&next_queue, ROUTE_EVENT_APPLIED, &staged) ||
        events->count + staged.count > GAME_EVENT_QUEUE_CAPACITY ||
        !rich4_game_event_queue_append(events, &staged))
        return ROUTE_EVENT_EXECUTION_INVALID;
    *state = next_state;
    *executor = next_queue;
    return ROUTE_EVENT_EXECUTION_PROGRESSED;
}




