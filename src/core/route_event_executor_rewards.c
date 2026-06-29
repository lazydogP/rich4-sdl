#include "core/route_event_executor_internal.h"
#include "core/game_event_payload.h"

int apply_points_reward(GameState *state, int player, int amount,
                               GameEventQueue *events)
{
    uint16_t next_points;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count || amount < 0)
        return 0;
    next_points = (uint16_t)(state->players[player].points + amount);
    if (!rich4_game_event_emit_typed_domain(
            events, rich4_game_event_inventory_changed_from_fields, player,
            rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
            rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
            amount, next_points, 0))
        return 0;
    state->players[player].points = next_points;
    return 1;
}

int apply_automatic_point_minigame(
    GameState *state, int player, int action, GameEventQueue *events)
{
    Rich4Rng rng;
    int amount;
    int speech_variant;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count || action < 6 || action > 8)
        return 0;

    /*
     * 0x415457 is the common fallback reached from 0x415215, 0x4154DC and
     * 0x4155FC.  It consumes one rand() for 50..69 and a second rand() for
     * the character-table speech variant before 0x41982D adds AX to the
     * original 16-bit point field at player +0x30.
     */
    rng.state = state->ai_rng_state;

    amount = rich4_rng_original_state_range(&rng, 20) + 50;
    speech_variant = (int)(rich4_rng_original_state_next(&rng) & 1u);
    state->ai_rng_state = rng.state;
    if (!rich4_game_event_point_minigame_automatic_resolved_emit(
            events, player, action, amount, speech_variant, 0))
        return 0;
    return apply_points_reward(state, player, amount, events);
}

int run_ai_lottery_purchase(
    GameState *state, int player, GameEventQueue *events)
{
    int empty[36];
    int count = 0;
    int number;
    int i;
    Rich4Rng rng;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count)
        return 0;
    /* 0x43169E uses a strict signed dword comparison: cash must be >1000. */
    if (state->players[player].cash <= 1000) return 1;
    for (i = 0; i < 36; i++)
        if (state->lottery_owner[i] == 0) empty[count++] = i;
    if (count == 0) return 1;

    rng.state = state->ai_rng_state;

    number = empty[rich4_rng_original_state_range(&rng, count)];
    state->ai_rng_state = rng.state;
    state->lottery_owner[number] = (uint8_t)(player + 1);
    state->players[player].cash -= 1000;
    state->lottery_pool += 1000;
    return rich4_game_event_emit_typed_domain(
               events, rich4_game_event_player_cash_changed_from_fields, player,
               rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
               rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
               state->players[player].cash, -1000, 0) &&
           rich4_game_event_emit_typed_domain(
               events, rich4_game_event_inventory_changed_from_fields, player,
               rich4_game_domain_ref(GAME_DOMAIN_NONE, number),
               rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
               number, state->lottery_pool, 0);
}

int gain_weighted_random_card(GameState *state, int player,
                              int *out_card,
                              GameEventQueue *events)
{
    Rich4Rng rng;
    int total = 0;
    int pick;
    int card;
    int slot;
    int inventory_count;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count)
        return 0;
    if (out_card != NULL) *out_card = -1;
    for (card = 0; card < GAME_CARD_COUNT; card++)
        total += state->card_amount[card];
    if (total == 0) return 1;

    /*
     * sub_441E12 puts one entry in its temporary array for every remaining
     * public-pool copy, so the draw is weighted by stock rather than card ID.
     */
    rng.state = state->ai_rng_state;

    pick = rich4_rng_original_state_range(&rng, total);
    state->ai_rng_state = rng.state;
    for (card = 0; card < GAME_CARD_COUNT; card++) {
        if (pick < state->card_amount[card]) break;
        pick -= state->card_amount[card];
    }
    if (card >= GAME_CARD_COUNT) return 0;
    if (out_card != NULL) *out_card = card;

    inventory_count = rich4_card_inventory_count(state, player);
    if (inventory_count < 0) return 0;
    if (inventory_count == GAME_PLAYER_CARD_SLOTS) {
        int lowest_card = -1;
        int lowest_value = 10000;
        int i;
        /*
         * sub_4412E4 does not reject a full hand: sub_44128F selects the
         * first held card with the lowest card-table +5 value, returns it to
         * the public pool, then inserts the gift.
         */
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int held = state->players[player].card_inventory[i] - 1;
            const GameCardInfo *info = rich4_card_info(held);
            if (info != NULL && info->metadata_f6 < lowest_value) {
                lowest_value = info->metadata_f6;
                lowest_card = held;
            }
        }
        if (lowest_card < 0) return 0;
        for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS; slot++)
            if (state->players[player].card_inventory[slot] ==
                    lowest_card + 1)
                break;
        if (slot == GAME_PLAYER_CARD_SLOTS) return 0;
        for (; slot + 1 < GAME_PLAYER_CARD_SLOTS; slot++)
            state->players[player].card_inventory[slot] =
                state->players[player].card_inventory[slot + 1];
        state->players[player].card_inventory[GAME_PLAYER_CARD_SLOTS - 1] = 0;
        state->card_amount[lowest_card]++;
    }
    return rich4_card_inventory_add_from_pool(state, player, card, events);
}

static int gain_weighted_random_tool(GameState *state, int player,
                                     int *out_tool,
                                     GameEventQueue *events)
{
    Rich4Rng rng;
    int total = 0;
    int pick;
    int tool;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count)
        return 0;
    /* 0x445ADA only puts the eight shop tools into its weighted array. */
    for (tool = 0; tool < 8; tool++)
        if (state->players[player].tool_inventory[tool] < 9)
            total += state->tool_amount[tool];
    if (total == 0) {
        if (out_tool != NULL) *out_tool = -1;
        return 1;
    }
    rng.state = state->ai_rng_state;

    pick = rich4_rng_original_state_range(&rng, total);
    state->ai_rng_state = rng.state;
    for (tool = 0; tool < 8; tool++) {
        int amount = state->players[player].tool_inventory[tool] < 9
            ? state->tool_amount[tool] : 0;
        if (pick < amount) break;
        pick -= amount;
    }
    if (tool >= 8) return 0;
    state->tool_amount[tool]--;
    state->players[player].tool_inventory[tool]++;
    if (out_tool != NULL) *out_tool = tool;
    return rich4_game_event_emit_typed_domain(
        events, rich4_game_event_inventory_changed_from_fields, player,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
        1, state->players[player].tool_inventory[tool], 0);
}

int grant_shop_owner_gift(GameState *state, int player,
                                 GameDomainRef *out_gift,
                                 GameEventQueue *events)
{
    Rich4Rng rng;
    int choose_tool;
    int tool = -1;
    int card = -1;
    if (out_gift == NULL) return 0;
    *out_gift = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    rng.state = state->ai_rng_state;

    choose_tool = rich4_rng_original_state_range(&rng, 2) != 0;
    state->ai_rng_state = rng.state;
    if (choose_tool) {
        if (!gain_weighted_random_tool(
                state, player, &tool, events)) return 0;
        if (tool >= 0)
            *out_gift = rich4_game_domain_ref(GAME_DOMAIN_TOOL, tool);
        return 1;
    }
    if (!gain_weighted_random_card(
            state, player, &card, events)) return 0;
    if (card >= 0)
        *out_gift = rich4_game_domain_ref(GAME_DOMAIN_CARD, card);
    return 1;
}

static int ai_shop_sell_card_slot(GameState *state, int player, int slot)
{
    const GameCardInfo *info;
    int card;
    int i;
    int proceeds;
    if (slot < 0 || slot >= GAME_PLAYER_CARD_SLOTS ||
        state->players[player].card_inventory[slot] == 0)
        return 0;
    card = state->players[player].card_inventory[slot] - 1;
    info = rich4_card_info(card);
    if (info == NULL) return 0;
    for (i = slot; i + 1 < GAME_PLAYER_CARD_SLOTS; i++)
        state->players[player].card_inventory[i] =
            state->players[player].card_inventory[i + 1];
    state->players[player].card_inventory[GAME_PLAYER_CARD_SLOTS - 1] = 0;
    if (state->card_amount[card] < 255) state->card_amount[card]++;
    proceeds = (info->shop_price * 9) / 10;
    state->players[player].points =
        (uint16_t)(state->players[player].points + proceeds);
    /* 0x42D145 returns the full shop price as company business volume. */
    return info->shop_price;
}

static int ai_shop_sell_tool(GameState *state, int player, int tool,
                             int quantity)
{
    const GameToolInfo *info = rich4_tool_info(tool);
    int sold_quantity = quantity;
    int proceeds;
    if (info == NULL || quantity <= 0 ||
        quantity > state->players[player].tool_inventory[tool])
        return 0;
    state->players[player].tool_inventory[tool] -= (uint8_t)quantity;
    if (tool >= 0 && tool < GAME_TOOL_COUNT) {
        int available = 255 - state->tool_amount[tool];
        if (quantity > available) quantity = available;
        state->tool_amount[tool] += (uint8_t)quantity;
    }
    proceeds = ((info->shop_price * 9) / 10) * sold_quantity;
    state->players[player].points =
        (uint16_t)(state->players[player].points + proceeds);
    /* 0x42D1B2 returns full price * quantity, not the 90% proceeds. */
    return info->shop_price * sold_quantity;
}

void shop_add_business_volume(GameState *state, int company,
                                     uint32_t amount)
{
    if (state == NULL || company < 0 || company >= state->company_count)
        return;
    state->company_period_business[company] += amount;
    state->company_total_business[company] += amount;
}

int shop_company_index(const RouteEventExecutor *executor,
                              const GameState *state)
{
    const RouteEvent *event = rich4_route_event_executor_current(executor);
    int raw = event != NULL ? event->object_ref.raw_object_id : 0;
    int company = raw - 6001;
    return raw > 6000 && raw < 8000 && state != NULL &&
           company >= 0 && company < state->company_count
        ? company : -1;
}

int run_ai_shop(GameState *state, int player, int company,
                       GameEventQueue *events)
{
    static const int tool_order[6] = { 7, 1, 6, 0, 3, 2 };
    int personality;
    uint32_t business = 0;
    int slot;
    int tool;
    int card_budget;
    int tool_budget;
    int card;
    if (state == NULL || events == NULL || player < 0 ||
        player >= state->player_count)
        return 0;
    personality = state->players[player].ai_personality;

    /* 0x42EDD4 indexes difficulty by slot, then 0x42EDF7 sells the held
     * ID.  Recheck the same slot after compacting, including this original
     * indexing quirk.  The tool loop below does index by tool ID. */
    for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS;) {
        const GameCardInfo *info = rich4_card_info(slot);
        if (state->players[player].card_inventory[slot] == 0) break;
        if (info != NULL && info->ai_difficulty - personality == 2)
            business += (uint32_t)ai_shop_sell_card_slot(
                state, player, slot);
        else
            slot++;
    }
    for (tool = 0; tool < GAME_TOOL_COUNT; tool++) {
        const GameToolInfo *info = rich4_tool_info(tool);
        if (info != NULL && info->ai_difficulty - personality == 2)
            business += (uint32_t)ai_shop_sell_tool(
                state, player, tool,
                state->players[player].tool_inventory[tool]);
    }

    /* Below 100 points the original also sells the cheapest card, all
     * duplicate tools, and a held motorcycle/car matching current traffic. */
    if (state->players[player].points < 100) {
        int cheapest_slot = -1;
        int cheapest = 10000;
        for (slot = 0; slot < GAME_PLAYER_CARD_SLOTS; slot++) {
            int held = state->players[player].card_inventory[slot] - 1;
            const GameCardInfo *info = rich4_card_info(held);
            if (info != NULL && info->shop_price < cheapest) {
                cheapest = info->shop_price;
                cheapest_slot = slot;
            }
        }
        if (cheapest_slot >= 0)
            business += (uint32_t)ai_shop_sell_card_slot(
                state, player, cheapest_slot);
        for (tool = 0; tool < GAME_TOOL_COUNT; tool++)
            if (state->players[player].tool_inventory[tool] > 1)
                business += (uint32_t)ai_shop_sell_tool(
                    state, player, tool,
                    state->players[player].tool_inventory[tool] - 1);
        if ((state->players[player].traffic_method & 3) == 1 &&
            state->players[player].tool_inventory[4] != 0)
            business += (uint32_t)ai_shop_sell_tool(
                state, player, 4,
                state->players[player].tool_inventory[4]);
        if ((state->players[player].traffic_method & 3) == 2 &&
            state->players[player].tool_inventory[5] != 0)
            business += (uint32_t)ai_shop_sell_tool(
                state, player, 5,
                state->players[player].tool_inventory[5]);
    }

    /*
     * 0x42F025..0x42F30D takes two independent halves of the post-sale
     * points.  The repeated-stock card candidate list is sorted by the
     * descending-price comparator at 0x42D0EF.
     */
    card_budget = state->players[player].points / 2;
    tool_budget = state->players[player].points - card_budget;
    for (;;) {
        int best = -1;
        int best_price = -1;
        if (rich4_card_inventory_count(state, player) >=
            GAME_PLAYER_CARD_SLOTS)
            break;
        for (card = 0; card < GAME_CARD_COUNT; card++) {
            const GameCardInfo *info = rich4_card_info(card);
            if (info != NULL && state->card_amount[card] != 0 &&
                info->shop_price <= card_budget &&
                info->ai_difficulty - personality != 2 &&
                info->shop_price > best_price) {
                best = card;
                best_price = info->shop_price;
            }
        }
        if (best < 0) break;
        if (!rich4_card_inventory_add_from_pool(
                state, player, best, events)) return 0;
        state->players[player].points =
            (uint16_t)(state->players[player].points - best_price);
        card_budget -= best_price;
        business += (uint32_t)(best_price * 10);
    }
    if ((state->players[player].traffic_method & 3) != 1 &&
        state->players[player].tool_inventory[4] == 0 &&
        state->tool_amount[4] != 0 &&
        rich4_tool_catalog[4].shop_price < tool_budget) {
        state->tool_amount[4]--;
        state->players[player].tool_inventory[4]++;
        state->players[player].points =
            (uint16_t)(state->players[player].points -
                       rich4_tool_catalog[4].shop_price);
        tool_budget -= rich4_tool_catalog[4].shop_price;
        business += (uint32_t)(rich4_tool_catalog[4].shop_price * 10);
    }
    if ((state->players[player].traffic_method & 3) != 2 &&
        state->players[player].tool_inventory[5] == 0 &&
        state->tool_amount[5] != 0 &&
        rich4_tool_catalog[5].shop_price < tool_budget) {
        state->tool_amount[5]--;
        state->players[player].tool_inventory[5]++;
        state->players[player].points =
            (uint16_t)(state->players[player].points -
                       rich4_tool_catalog[5].shop_price);
        tool_budget -= rich4_tool_catalog[5].shop_price;
        business += (uint32_t)(rich4_tool_catalog[5].shop_price * 10);
    }
    for (slot = 0; slot < 6; slot++) {
        const GameToolInfo *info;
        tool = tool_order[slot];
        info = rich4_tool_info(tool);
        if (info != NULL &&
            state->players[player].tool_inventory[tool] < 9 &&
            info->ai_difficulty - personality != 2 &&
            info->shop_price <= tool_budget &&
            state->tool_amount[tool] != 0) {
            state->tool_amount[tool]--;
            state->players[player].tool_inventory[tool]++;
            state->players[player].points =
                (uint16_t)(state->players[player].points - info->shop_price);
            tool_budget -= info->shop_price;
            business += (uint32_t)(info->shop_price * 10);
        }
    }
    shop_add_business_volume(state, company, business);
    return rich4_game_event_emit_typed_domain(
        events, rich4_game_event_inventory_changed_from_fields, player,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, player),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
        (int)business, state->players[player].points, 0);
}


