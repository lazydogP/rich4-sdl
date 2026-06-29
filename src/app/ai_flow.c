#include "app/ai_flow.h"

#include <limits.h>
#include <stdint.h>

#include "core/card.h"
#include "core/facility.h"
#include "core/property.h"
#include "core/rng.h"
#include "core/stock.h"
#include "core/tool.h"

static int ai_market_ask_price(int base_price, int price_index)
{
    int64_t ask;
    if (base_price <= 0 || price_index <= 0) return 0;
    ask = (int64_t)base_price * 100 * price_index;
    return ask > 0 && ask <= INT_MAX ? (int)ask : 0;
}

static int ai_market_item_ask_price(GameDomainRef item, int price_index)
{
    const GameCardInfo *card;
    const GameToolInfo *tool;
    if (item.kind == GAME_DOMAIN_CARD) {
        card = rich4_card_info(item.id);
        return card == NULL ? 0 : ai_market_ask_price(
            card->shop_price, price_index);
    }
    if (item.kind == GAME_DOMAIN_TOOL) {
        tool = rich4_tool_info(item.id);
        return tool == NULL ? 0 : ai_market_ask_price(
            tool->shop_price, price_index);
    }
    return 0;
}

static int ai_market_listing_item(const GameMarketListing *listing,
                                  GameDomainRef *out_item)
{
    if (listing == NULL || out_item == NULL) return 0;
    if (listing->type == GAME_MARKET_LISTING_CARD) {
        if (listing->item_id == 0) return 0;
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_CARD, (int)listing->item_id - 1);
    } else if (listing->type == GAME_MARKET_LISTING_TOOL) {
        if (listing->item_id == 0) return 0;
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_TOOL, (int)listing->item_id - 1);
    } else {
        return 0;
    }
    return rich4_game_domain_ref_is_valid(*out_item);
}

static int ai_market_asset_item(const GameMarketListing *listing,
                                GameDomainRef *out_item)
{
    if (listing == NULL || out_item == NULL ||
        listing->type != GAME_MARKET_LISTING_ASSET)
        return 0;
    if (listing->item_id > 0x7d0 && listing->item_id < 0xfa0)
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_LAND, (int)listing->item_id - 0x7d1);
    else if (listing->item_id >= 0xfa1 && listing->item_id < 0x1770)
        *out_item = rich4_game_domain_ref(
            GAME_DOMAIN_FACILITY, (int)listing->item_id - 0xfa1);
    else
        return 0;
    return rich4_game_domain_ref_is_valid(*out_item);
}

static int ai_market_choose_listing_item(const GameState *state, int player,
                                         Rich4Rng *rng,
                                         GameDomainRef *out_item,
                                         int *out_ask_price)
{
    /* 0x4284BE counts raw card slots first, then scans tool_amount when its
     * duplicate-card branch produces no candidates. Keep duplicate weights. */
    int candidates[GAME_PLAYER_CARD_SLOTS *
                   (GAME_PLAYER_CARD_SLOTS - 1)];
    int candidate_count = 0;
    int card_count;
    int i;
    if (state == NULL || rng == NULL || out_item == NULL ||
        out_ask_price == NULL || player < 0 ||
        player >= state->player_count)
        return 0;
    card_count = rich4_card_inventory_count(state, player);
    if (card_count > 12) {
        for (i = 0; i < card_count; i++) {
            int j;
            int card_id = state->players[player].card_inventory[i];
            for (j = 0; j < card_count; j++) {
                if (j != i && card_id ==
                    state->players[player].card_inventory[j])
                    candidates[candidate_count++] = card_id;
            }
        }
        if (candidate_count != 0) {
            int raw_id = candidates[
                rich4_rng_original_state_range(rng, candidate_count)];
            const GameCardInfo *card = rich4_card_info(raw_id - 1);
            if (card == NULL) return 0;
            *out_item = rich4_game_domain_ref(GAME_DOMAIN_CARD, raw_id - 1);
            *out_ask_price = ai_market_ask_price(
                card->shop_price, state->price_index);
            return *out_ask_price > 0;
        }
    }
    for (i = 0; i < 13; i++) {
        int quantity = state->players[player].tool_inventory[i];
        const GameToolInfo *tool = rich4_tool_info(i);
        if (quantity >= 3 || (quantity != 0 && tool != NULL &&
            tool->ai_difficulty - state->players[player].ai_personality == 2))
            candidates[candidate_count++] = i + 1;
    }
    if (candidate_count == 0) return 0;
    {
        int raw_id = candidates[
            rich4_rng_original_state_range(rng, candidate_count)];
        const GameToolInfo *tool = rich4_tool_info(raw_id - 1);
        if (tool == NULL) return 0;
        *out_item = rich4_game_domain_ref(GAME_DOMAIN_TOOL, raw_id - 1);
        *out_ask_price = ai_market_ask_price(
            tool->shop_price, state->price_index);
    }
    return *out_ask_price > 0;
}

static int ai_market_submit(GameSession *session, Rich4Rng *rng,
    Rich4AiCommandSink *sink, const GameCommand *command, uint32_t now)
{
    int result;
    /* The handler may consume rand itself; resume from its committed word. */
    session->state.ai_rng_state = rng->state;
    result = sink->submit(sink, command, now);
    rng->state = session->state.ai_rng_state;
    return result;
}

static void ai_market_try_create_listing(GameSession *session, int player,
                                         Rich4Rng *rng, uint32_t now,
                                         Rich4AiCommandSink *sink)
{
    GameDomainRef item;
    GameCommand command;
    int ask_price;
    if (rich4_rng_original_state_range(rng, 15) != 0 ||
        !ai_market_choose_listing_item(
            &session->state, player, rng, &item, &ask_price))
        return;
    if (session->state.market_listings[player]
            [GAME_MARKET_LISTING_SLOT_COUNT - 1].type !=
        GAME_MARKET_LISTING_EMPTY) {
        command = rich4_game_command_cancel_market_listing(
            GAME_COMMAND_SOURCE_AI, player, 0);
        if (!ai_market_submit(session, rng, sink, &command, now)) return;
    }
    command = rich4_game_command_create_market_listing(
        GAME_COMMAND_SOURCE_AI, player, item, ask_price, 0);
    (void)ai_market_submit(session, rng, sink, &command, now);
}

static void ai_market_try_reprice(GameSession *session, int player,
                                  Rich4Rng *rng, uint32_t now,
                                  Rich4AiCommandSink *sink)
{
    int slot;
    if (rich4_rng_original_state_range(rng, 3) != 0) return;
    for (slot = 0; slot < GAME_MARKET_LISTING_SLOT_COUNT; slot++) {
        const GameMarketListing *listing =
            &session->state.market_listings[player][slot];
        GameDomainRef item;
        GameCommand command;
        int ask_price;
        if (!ai_market_listing_item(listing, &item)) continue;
        ask_price = ai_market_item_ask_price(
            item, session->state.price_index);
        if (ask_price <= 0 || ask_price == listing->ask_price) continue;
        command = rich4_game_command_reprice_market_listing(
            GAME_COMMAND_SOURCE_AI, player, slot, ask_price);
        (void)ai_market_submit(session, rng, sink, &command, now);
    }
}

static int ai_market_property_value(const GameState *state,
                                    GameDomainRef item, int64_t *out_value)
{
    int value;
    if (item.kind == GAME_DOMAIN_LAND &&
        rich4_land_purchase_price(state, item.id, &value)) {
        *out_value = value;
        return 1;
    }
    if (item.kind == GAME_DOMAIN_FACILITY &&
        rich4_facility_asset_value(state, item.id, &value)) {
        *out_value = (int64_t)value * state->price_index;
        return *out_value <= INT_MAX;
    }
    return 0;
}

static void ai_market_try_buy_listing(GameSession *session, int player,
                                      Rich4Rng *rng, uint32_t now,
                                      Rich4AiCommandSink *sink)
{
    int seller;
    if (rich4_rng_original_state_range(rng, 4) != 0) return;
    for (seller = 0; seller < session->state.player_count; seller++) {
        int slot;
        if (seller == player ||
            (session->state.players[seller].play_state & 0xff) == 0)
            continue;
        for (slot = 0; slot < GAME_MARKET_LISTING_SLOT_COUNT; slot++) {
            const GameMarketListing *listing =
                &session->state.market_listings[seller][slot];
            GameDomainRef item;
            int should_buy = 0;
            if (listing->type == GAME_MARKET_LISTING_STOCK) {
                int stock = listing->item_id;
                if (stock >= 0 && stock < GAME_STOCK_COUNT &&
                    listing->quantity > 0 &&
                    listing->ask_price / listing->quantity <
                        session->state.stocks[stock].f20)
                    should_buy = 1;
            } else if (listing->type == GAME_MARKET_LISTING_ASSET &&
                       listing->ask_price > 0 &&
                       ai_market_asset_item(listing, &item)) {
                int64_t asset_value;
                if (ai_market_property_value(
                        &session->state, item, &asset_value) &&
                    (int64_t)asset_value * 3 > listing->ask_price &&
                    (int64_t)listing->ask_price * 2 <
                        session->state.players[player].cash)
                    should_buy = 1;
            }
            if (should_buy) {
                GameCommand command = rich4_game_command_buy_market_listing(
                    GAME_COMMAND_SOURCE_AI, player, seller, slot);
                if (ai_market_submit(session, rng, sink, &command, now)) return;
            }
        }
    }
}

static void ai_market_preactions(GameSession *session, int player,
                                 uint32_t now,
                                 Rich4AiCommandSink *sink)
{
    Rich4Rng rng;
    rng.state = session->state.ai_rng_state;
    ai_market_try_create_listing(session, player, &rng, now, sink);
    ai_market_try_reprice(session, player, &rng, now, sink);
    ai_market_try_buy_listing(session, player, &rng, now, sink);
    session->state.ai_rng_state = rng.state;
}

int rich4_ai_prepare_move(GameSession *session, int player,
                          GameCommand *out_command)
{
    int count;
    uint32_t after;
    if (session == NULL || out_command == NULL ||
        !rich4_ai_select_dice_count(&session->state, player,
            session->state.ai_rng_state, &count, &after)) return 0;
    session->state.ai_rng_state = after;
    if (count == session->state.players[player].ndices) return 0;
    *out_command = rich4_game_command(GAME_COMMAND_SELECT_DICE_COUNT,
        GAME_COMMAND_SOURCE_AI, player, count);
    return 1;
}

void rich4_ai_run_preactions_with_projection(
    GameSession *session, GameDomainRef current_property,
    const uint8_t *player_on_map,
    const GameAiMapTargetContext *target_context,
    const GameAiGlobalProjection *projection, int player, uint32_t now,
    Rich4AiCommandSink *sink)
{
    GameAiPhase phase = GAME_AI_PHASE_STOCK_BUY;
    int guard = 0;
    int market_preactions_done = 0;

    (void)now;
    if (session == NULL || player_on_map == NULL || sink == NULL ||
        sink->submit == NULL || player < 0 ||
        player >= session->state.player_count)
        return;

    while (phase != GAME_AI_PHASE_DONE && guard++ < 24) {
        GameAiDecision decision;
        GameAiPhase previous = phase;

        if (phase == GAME_AI_PHASE_CARD_OR_TOOL &&
            !market_preactions_done) {
            market_preactions_done = 1;
            ai_market_preactions(session, player, now, sink);
        }

        /* 0x418C55 AI preaction: stock buy/sell, funds, card XOR tool.
         * Stop-tile upgrades are handled by the route-event executor. */
        if (phase == GAME_AI_PHASE_PROPERTY_UPGRADE) {
            phase = GAME_AI_PHASE_MOVE;
            continue;
        }
        if (phase == GAME_AI_PHASE_MOVE) {
            GameCommand count_command;
            if (rich4_ai_prepare_move(session, player, &count_command))
                (void)sink->submit(sink, &count_command, now);
        }
        if (!rich4_ai_select_command_with_selection_memory(
                &session->state, current_property, player_on_map,
                target_context, projection, &session->ai_selection, player, phase,
                session->state.ai_rng_state, &decision))
            return;
        session->state.ai_rng_state = decision.rng_after;
        session->ai_selection = decision.selection_after;
        phase = decision.next_phase;
        if (!decision.has_command) continue;
        int submitted = sink->submit(sink, &decision.command, now);
        if (submitted == RICH4_AI_COMMAND_DEFERRED) return;
        if (!submitted) {
            if (decision.command.type == GAME_COMMAND_SELL_STOCK) {
                session->ai_selection.stock_distress = 0;
                phase = GAME_AI_PHASE_FUNDS;
            }
            if (phase == previous &&
                decision.command.type != GAME_COMMAND_SELL_STOCK)
                phase = (GameAiPhase)(previous + 1);
            continue;
        }
        if (decision.command.type == GAME_COMMAND_ROLL_DICE) return;
        /* A distress-sale phase repeats only when a sale actually changed the
         * holding. Rejected commands must still make forward progress. */
        if (phase == previous &&
            decision.command.type != GAME_COMMAND_SELL_STOCK)
            phase = (GameAiPhase)(previous + 1);
    }
}

void rich4_ai_run_preactions(GameSession *session,
                             GameDomainRef current_property,
                             const uint8_t *player_on_map,
                             const GameAiMapTargetContext *target_context,
                             int player, uint32_t now,
                             Rich4AiCommandSink *sink)
{
    rich4_ai_run_preactions_with_projection(
        session, current_property, player_on_map, target_context, NULL,
        player, now, sink);
}
