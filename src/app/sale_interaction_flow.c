#include "app/sale_interaction_flow.h"

#include <limits.h>
#include <string.h>

#include "core/card.h"
#include "core/tool.h"

enum {
    SALE_ROWS_PER_PAGE = 8,
    SALE_PROPERTY_ROWS_PER_PAGE = 11,
    SALE_HIT_NONE = -1,
    SALE_HIT_CLOSE = 1,
    SALE_HIT_SELL,
    SALE_HIT_BROWSE,
    SALE_HIT_ITEMS,
    SALE_HIT_CATEGORY_BASE = 10,
    SALE_HIT_ROW_BASE = 100,
    SALE_HIT_PREVIOUS = 200,
    SALE_HIT_NEXT,
    SALE_HIT_ACTION,
    SALE_HIT_CONFIRM_NO,
    SALE_HIT_CONFIRM_YES,
    SALE_HIT_GRID_BASE = 300,
    SALE_HIT_PRICE_KEY_BASE = 400,
    SALE_HIT_PURCHASE_CONFIRM_YES = 500,
    SALE_HIT_PURCHASE_CONFIRM_NO
};

enum { SALE_PICKER_OPEN = -1 };

static GameSaleInteractionPresentationState *sale_state(GameSession *session)
{
    return session == NULL ? NULL : &session->presentation.sale_interaction;
}

static const GameSaleInteractionPresentationState *sale_state_const(
    const GameSession *session)
{
    return session == NULL ? NULL : &session->presentation.sale_interaction;
}

static int sale_property_filter(const GameSession *session)
{
    const GameSaleInteractionPresentationState *state =
        sale_state_const(session);
    if (state == NULL || state->category != RICH4_SALE_CATEGORY_PROPERTY ||
        state->selected_slot < 0)
        return 0;
    /* The original property window keeps its selected one of five filters
     * independently from the selected row.  selected_slot is otherwise
     * unused by this page and already serialized in the presentation state. */
    return state->selected_slot;
}

static void sale_update_property_filter(
    GameSaleInteractionPresentationState *state, int x, int y,
    int property_panel_width)
{
    int right;
    int filter;
    if (state == NULL || state->page != RICH4_SALE_PAGE_ITEMS ||
        state->category != RICH4_SALE_CATEGORY_PROPERTY)
        return;
    right = property_panel_width > 0 ? property_panel_width + 95 : 591;
    /* IDA sub_42608F changes the property filter on LBUTTONDOWN over the
     * five tabs (y=33..61). Mousemove over the item rows only changes the
     * row frame and must not switch filters. */
    if (x <= 112 || x >= right || y <= 32 || y >= 62) return;
    filter = (x - 112) / 80;
    if (filter < 0 || filter >= 5) return;
    if (filter == state->selected_slot) return;
    state->selected_slot = filter;
    state->page_offset = 0;
    state->selected_row = -1;
}

static int sale_rows_per_page(const GameSaleInteractionPresentationState *state)
{
    return state != NULL && state->category == RICH4_SALE_CATEGORY_PROPERTY
        ? SALE_PROPERTY_ROWS_PER_PAGE : SALE_ROWS_PER_PAGE;
}

static int sale_current_player(const GameState *state)
{
    if (state == NULL || state->player_count <= 0 ||
        state->player_count > GAME_MAX_PLAYERS || state->current_player < 0 ||
        state->current_player >= state->player_count)
        return -1;
    if ((state->players[state->current_player].play_state & 0xff) == 0)
        return -1;
    return state->current_player;
}

static void sale_result_clear(Rich4AppSaleInteractionResult *result)
{
    if (result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->command = rich4_game_command(
        GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, 0, 0);
}

static int sale_point_in_rect(int px, int py, int x, int y,
                              int width, int height)
{
    return px >= x && px < x + width && py >= y && py < y + height;
}

static int sale_picker_category_at(int x, int y)
{
    int column;
    int row;
    if (x <= 473 || x >= 598 || y <= 125 || y >= 202)
        return SALE_HIT_NONE;
    column = (x - 473) / 63;
    row = (y - 125) / 39;
    if (column < 0 || column > 1 || row < 0 || row > 1)
        return SALE_HIT_NONE;
    return SALE_HIT_CATEGORY_BASE + row + column * 2;
}

static int sale_hover_row_at(
    const GameSaleInteractionPresentationState *state, int x, int y,
    int stock_panel_width, int property_panel_width)
{
    int right;
    if (state == NULL || state->page != RICH4_SALE_PAGE_ITEMS) return -1;
    if (state->category == RICH4_SALE_CATEGORY_STOCK) {
        right = stock_panel_width > 0 ? stock_panel_width + 152 : 520;
        if (x > 152 && x < right && y > 64 && y < 448)
            return (y - 64) / 32;
    } else if (state->category == RICH4_SALE_CATEGORY_PROPERTY) {
        right = property_panel_width > 0
            ? property_panel_width + 95 : 591;
        if (x > 112 && x < right && y > 96 && y < 448)
            return (y - 96) / 32;
    }
    return -1;
}

static int sale_confirm_control_at(
    const GameSession *session,
    const GameSaleInteractionPresentationState *state, int x, int y)
{
    const GameMarketListing *listing;
    int modal_height;
    int action_y;
    int modal_left = (640 - 192) / 2;
    int button_y;
    if (session == NULL || state == NULL || state->selected_seller < 0 ||
        state->selected_seller >= session->state.player_count ||
        state->selected_slot < 0 ||
        state->selected_slot >= GAME_MARKET_LISTING_SLOT_COUNT)
        return SALE_HIT_NONE;
    listing = &session->state.market_listings[state->selected_seller]
                                             [state->selected_slot];
    /* IDA 0x42704E places each action row relative to the matching Panel[73]
     * detail chunk and accepts clicks in the original +/-12px vertical band. */
    switch (listing->type) {
    case GAME_MARKET_LISTING_STOCK:
        modal_height = 256;
        action_y = 236;
        break;
    case GAME_MARKET_LISTING_ASSET:
        modal_height = 288;
        action_y = 265;
        break;
    case GAME_MARKET_LISTING_TOOL:
    case GAME_MARKET_LISTING_CARD:
        modal_height = 224;
        action_y = 203;
        break;
    default:
        return SALE_HIT_NONE;
    }
    button_y = (480 - modal_height) / 2 + action_y;
    if (sale_point_in_rect(x, y, modal_left + 17, button_y - 11, 71, 23))
        return SALE_HIT_CONFIRM_YES;
    if (sale_point_in_rect(x, y, modal_left + 105, button_y - 11, 71, 23))
        return SALE_HIT_CONFIRM_NO;
    return SALE_HIT_NONE;
}

static int sale_purchase_dialog_control_at(
    const GameSaleInteractionPresentationState *state,
    int x, int y, int dialog_width, int dialog_height)
{
    int left;
    int top;
    if (state == NULL || dialog_width < 2 || dialog_height <= 0)
        return SALE_HIT_NONE;
    left = (640 - dialog_width) / 2;
    top = (480 - dialog_height) / 2;
    if (!sale_point_in_rect(x, y, left, top, dialog_width, dialog_height))
        return SALE_HIT_NONE;
    return x < left + dialog_width / 2
        ? SALE_HIT_PURCHASE_CONFIRM_YES : SALE_HIT_PURCHASE_CONFIRM_NO;
}

static int sale_control_at(
    const GameSession *session,
    const GameSaleInteractionPresentationState *state,
    const Rich4StockQuantityMask *mask, int x, int y,
    int stock_panel_width, int property_panel_width,
    int confirm_dialog_width, int confirm_dialog_height)
{
    if (state == NULL) return SALE_HIT_NONE;
    if (state->page == RICH4_SALE_PAGE_BROWSE) {
        if (state->message == SALE_PICKER_OPEN)
            return sale_picker_category_at(x, y);
        /* 0x427C21 uses strict hit bounds for the two original top-right
         * controls. Sell opens its menu on button-down. */
        if (x > 464 && x < 536 && y > 74 && y < 114)
            return SALE_HIT_SELL;
        if (x > 536 && x < 608 && y > 74 && y < 114)
            return SALE_HIT_CLOSE;
        if (session != NULL && x > 104 && x < 608 && y > 114 &&
            y < 114 + session->state.player_count * 72) {
            int seller = (y - 114) / 72;
            int visible_slot = (x - 104) / 72;
            int slot;
            int shown = 0;
            if (seller < 0 || seller >= session->state.player_count ||
                visible_slot < 0 ||
                visible_slot >= GAME_MARKET_LISTING_SLOT_COUNT)
                return SALE_HIT_NONE;
            /* Board rendering packs occupied offers left-to-right. Resolve
             * the clicked visible cell back to its backing market slot. */
            for (slot = 0; slot < GAME_MARKET_LISTING_SLOT_COUNT; slot++) {
                if (session->state.market_listings[seller][slot].type ==
                    GAME_MARKET_LISTING_EMPTY)
                    continue;
                if (shown++ == visible_slot)
                    return SALE_HIT_GRID_BASE +
                           seller * GAME_MARKET_LISTING_SLOT_COUNT + slot;
            }
        }
    } else if (state->page == RICH4_SALE_PAGE_ITEMS) {
        int ordinal = -1;
        if (state->category == RICH4_SALE_CATEGORY_STOCK) {
            int right = stock_panel_width > 0
                ? stock_panel_width + 152 : 520;
            if (x > 462 && x < 485 && y > 38 && y < 60)
                return SALE_HIT_CLOSE;
            /* Original row hit boxes begin at y=64; text baselines are at
             * y=80, 112, ... . */
            if (x > 152 && x < right && y > 64 && y < 448)
                ordinal = (y - 64) / 32;
        } else if (state->category == RICH4_SALE_CATEGORY_PROPERTY) {
            int right = property_panel_width > 0
                ? property_panel_width + 95 : 591;
            /* 0x42608F places these controls in a 17px strip beyond chunk 2. */
            if (x > right && x < right + 17) {
                if (y > 32 && y < 51) return SALE_HIT_CLOSE;
                if (y > 64 && y < 96) return SALE_HIT_PREVIOUS;
                if (y > 96 && y < 128) return SALE_HIT_NEXT;
            }
            /* Horizontal position selects one of five property filters;
             * the row is selected by y alone, as in the original callback. */
            if (x > 112 && x < right && y > 96 && y < 448)
                ordinal = (y - 96) / 32;
        } else {
            if (x > 429 && x < 499 && y > 161 && y < 191)
                return SALE_HIT_CLOSE;
            /* Card/tool callbacks only address the three original rows. */
            if (x > 140 && x < 500 && y > 192 && y < 288) {
                int column = (x - 140) / 72;
                int row = (y - 192) / 32;
                if (column >= 0 && column < 5 && row >= 0 && row < 3)
                    ordinal = row * 5 + column;
            }
        }
        if (ordinal >= 0) {
            if (state->category == RICH4_SALE_CATEGORY_STOCK) {
                if (ordinal < rich4_app_sale_item_count(
                                  session, RICH4_SALE_CATEGORY_STOCK))
                    return SALE_HIT_ROW_BASE + ordinal;
            } else if (state->page_offset + ordinal <
                       rich4_app_sale_item_count(session, state->category)) {
                return SALE_HIT_ROW_BASE + ordinal;
            }
        }
    } else if (state->page == RICH4_SALE_PAGE_PRICE) {
        int control = rich4_app_stock_quantity_control_at_mask(mask, x, y);
        if (control >= RICH4_STOCK_QUANTITY_CONTROL_MAX &&
            control <= RICH4_STOCK_QUANTITY_CONTROL_SLIDER)
            return SALE_HIT_PRICE_KEY_BASE + control;
    } else if (state->page == RICH4_SALE_PAGE_CONFIRM) {
        return sale_confirm_control_at(session, state, x, y);
    } else if (state->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
        return sale_purchase_dialog_control_at(
            state, x, y, confirm_dialog_width, confirm_dialog_height);
    }
    return SALE_HIT_NONE;
}

int rich4_app_sale_item_count(const GameSession *session,
                              Rich4SaleInteractionCategory category)
{
    const GameState *state;
    int player;
    int count = 0;
    int i;
    if (session == NULL ||
        (player = sale_current_player(&session->state)) < 0)
        return 0;
    state = &session->state;
    switch (category) {
    case RICH4_SALE_CATEGORY_STOCK:
        for (i = 0; i < GAME_STOCK_COUNT; i++)
            if (state->player_stock_amount[player][i] > 0) count++;
        break;
    case RICH4_SALE_CATEGORY_PROPERTY:
        {
            int filter = sale_property_filter(session);
            for (i = 0; i < state->land_count; i++) {
                int developed = state->land_level[i] > 0;
                int matches = filter == 0 || filter == 1 ||
                    (filter == 3 && developed && state->land_type[i] == 0) ||
                    (filter == 4 && developed && state->land_type[i] != 0);
                if (filter != 2 && matches && state->land_owner[i] == player)
                    count++;
            }
            if (filter == 0 || filter == 2)
                for (i = 0; i < state->facility_count; i++)
                    if (state->facility_owner[i] == player) count++;
        }
        break;
    case RICH4_SALE_CATEGORY_CARD:
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++)
            if (state->players[player].card_inventory[i] > 0) count++;
        break;
    case RICH4_SALE_CATEGORY_TOOL:
        for (i = 0; i < GAME_TOOL_COUNT; i++)
            if (state->players[player].tool_inventory[i] > 0) count++;
        break;
    default:
        break;
    }
    return count;
}

int rich4_app_sale_item_at(const GameSession *session,
                           Rich4SaleInteractionCategory category,
                           int ordinal, GameDomainRef *out_item,
                           int *out_maximum)
{
    const GameState *state;
    int player;
    int count = 0;
    int i;
    if (out_item == NULL || out_maximum == NULL || ordinal < 0 ||
        session == NULL ||
        (player = sale_current_player(&session->state)) < 0)
        return 0;
    state = &session->state;
    *out_item = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    *out_maximum = 0;
    if (category == RICH4_SALE_CATEGORY_STOCK) {
        for (i = 0; i < GAME_STOCK_COUNT; i++) {
            int amount = state->player_stock_amount[player][i];
            if (amount <= 0) continue;
            if (count++ == ordinal) {
                *out_item = rich4_game_domain_ref(GAME_DOMAIN_STOCK, i);
                *out_maximum = amount;
                return 1;
            }
        }
    } else if (category == RICH4_SALE_CATEGORY_PROPERTY) {
        int filter = sale_property_filter(session);
        for (i = 0; i < state->land_count; i++) {
            int developed = state->land_level[i] > 0;
            int matches = filter == 0 || filter == 1 ||
                (filter == 3 && developed && state->land_type[i] == 0) ||
                (filter == 4 && developed && state->land_type[i] != 0);
            if (filter == 2 || !matches || state->land_owner[i] != player)
                continue;
            if (count++ == ordinal) {
                *out_item = rich4_game_domain_ref(GAME_DOMAIN_LAND, i);
                *out_maximum = 1;
                return 1;
            }
        }
        if (filter == 0 || filter == 2) {
            for (i = 0; i < state->facility_count; i++) {
                if (state->facility_owner[i] != player) continue;
                if (count++ == ordinal) {
                    *out_item = rich4_game_domain_ref(GAME_DOMAIN_FACILITY, i);
                    *out_maximum = 1;
                    return 1;
                }
            }
        }
    } else if (category == RICH4_SALE_CATEGORY_CARD) {
        for (i = 0; i < GAME_PLAYER_CARD_SLOTS; i++) {
            int card = state->players[player].card_inventory[i];
            if (card == 0) continue;
            if (count++ == ordinal) {
                *out_item = rich4_game_domain_ref(GAME_DOMAIN_CARD, card - 1);
                *out_maximum = 1;
                return 1;
            }
        }
    } else if (category == RICH4_SALE_CATEGORY_TOOL) {
        for (i = 0; i < GAME_TOOL_COUNT; i++) {
            if (state->players[player].tool_inventory[i] == 0) continue;
            if (count++ == ordinal) {
                *out_item = rich4_game_domain_ref(GAME_DOMAIN_TOOL, i);
                *out_maximum = 1;
                return 1;
            }
        }
    }
    return 0;
}

int rich4_app_sale_browse_count(const GameSession *session)
{
    int player;
    int count = 0;
    int slot;
    if (session == NULL) return 0;
    for (player = 0; player < session->state.player_count; player++) {
        if ((session->state.players[player].play_state & 0xff) == 0) continue;
        for (slot = 0; slot < GAME_MARKET_LISTING_SLOT_COUNT; slot++)
            if (session->state.market_listings[player][slot].type !=
                GAME_MARKET_LISTING_EMPTY)
                count++;
    }
    return count;
}

int rich4_app_sale_browse_listing_at(const GameSession *session, int ordinal,
                                     int *out_seller, int *out_slot)
{
    int player;
    int slot;
    int count = 0;
    if (session == NULL || ordinal < 0 || out_seller == NULL ||
        out_slot == NULL)
        return 0;
    for (player = 0; player < session->state.player_count; player++) {
        if ((session->state.players[player].play_state & 0xff) == 0) continue;
        for (slot = 0; slot < GAME_MARKET_LISTING_SLOT_COUNT; slot++) {
            if (session->state.market_listings[player][slot].type ==
                GAME_MARKET_LISTING_EMPTY)
                continue;
            if (count++ == ordinal) {
                *out_seller = player;
                *out_slot = slot;
                return 1;
            }
        }
    }
    return 0;
}

static void sale_clear_selection(GameSaleInteractionPresentationState *state)
{
    state->selected_row = -1;
    state->selected_seller = -1;
    state->selected_slot = -1;
    state->selected_item = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state->item_quantity_max = 0;
}

static int sale_activate_item(GameSession *session)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    int maximum;
    int ordinal;
    if (state == NULL || session == NULL || state->selected_row < 0)
        return 0;
    ordinal = state->page_offset + state->selected_row;
    if (!rich4_app_sale_item_at(
                   session, state->category, ordinal,
                   &state->selected_item, &maximum)) {
        return 0;
    }
    state->item_quantity_max = maximum;
    state->ask_price = 0;
    state->quantity = 0;
    state->edit_quantity = state->selected_item.kind == GAME_DOMAIN_STOCK;
    state->message = 0;
    state->page = RICH4_SALE_PAGE_PRICE;
    /* Native stock/property WM_LBUTTONUP restores the transient row frame
     * before opening the sale prompt. */
    state->selected_row = -1;
    return 1;
}

static int sale_entry_reference_price(
    const GameSession *session,
    const GameSaleInteractionPresentationState *state)
{
    long long value = 0;
    if (session == NULL || state == NULL) return 0;
    switch (state->selected_item.kind) {
    case GAME_DOMAIN_STOCK:
        if (state->selected_item.id >= 0 &&
            state->selected_item.id < GAME_STOCK_COUNT &&
            state->quantity > 0) {
            double price = (double)state->quantity *
                session->state.stocks[state->selected_item.id].f20;
            if (price >= INT_MAX) return INT_MAX;
            if (price > 0.0) return (int)price;
        }
        return 0;
    case GAME_DOMAIN_LAND:
        if (state->selected_item.id >= 0 &&
            state->selected_item.id < session->state.land_count)
            value = (long long)session->state.land_value[
                        state->selected_item.id] * session->state.price_index;
        break;
    case GAME_DOMAIN_FACILITY:
        if (state->selected_item.id >= 0 &&
            state->selected_item.id < session->state.facility_count)
            value = (long long)session->state.facility_value[
                        state->selected_item.id] * session->state.price_index;
        break;
    case GAME_DOMAIN_CARD: {
        const GameCardInfo *info = rich4_card_info(state->selected_item.id);
        if (info != NULL)
            value = (long long)info->shop_price * 100 *
                    session->state.price_index;
        break;
    }
    case GAME_DOMAIN_TOOL: {
        const GameToolInfo *info = rich4_tool_info(state->selected_item.id);
        if (info != NULL)
            value = (long long)info->shop_price * 100 *
                    session->state.price_index;
        break;
    }
    default:
        return 0;
    }
    if (value <= 0) return 0;
    return value > INT_MAX ? INT_MAX : (int)value;
}

int rich4_app_sale_interaction_entry_value(const GameSession *session)
{
    const GameSaleInteractionPresentationState *state =
        sale_state_const(session);
    if (state == NULL || state->page != RICH4_SALE_PAGE_PRICE) return 0;
    return state->edit_quantity ? state->quantity : state->ask_price;
}

int rich4_app_sale_interaction_entry_maximum(const GameSession *session)
{
    const GameSaleInteractionPresentationState *state =
        sale_state_const(session);
    long long maximum;
    if (state == NULL || state->page != RICH4_SALE_PAGE_PRICE) return 0;
    if (state->edit_quantity) return state->item_quantity_max;
    maximum = (long long)sale_entry_reference_price(session, state) * 10;
    return maximum > INT_MAX ? INT_MAX : (int)maximum;
}

int rich4_app_sale_interaction_entry_reference_price(
    const GameSession *session)
{
    const GameSaleInteractionPresentationState *state =
        sale_state_const(session);
    if (state == NULL || state->page != RICH4_SALE_PAGE_PRICE ||
        state->edit_quantity)
        return 0;
    return sale_entry_reference_price(session, state);
}

int rich4_app_sale_interaction_entry_is_quantity(const GameSession *session)
{
    const GameSaleInteractionPresentationState *state =
        sale_state_const(session);
    return state != NULL && state->page == RICH4_SALE_PAGE_PRICE &&
           state->edit_quantity;
}

static void sale_close_price_entry(
    GameSaleInteractionPresentationState *state)
{
    if (state == NULL) return;
    state->page = RICH4_SALE_PAGE_ITEMS;
    state->selected_row = -1;
    state->selected_item = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state->item_quantity_max = 0;
    state->ask_price = 0;
    state->quantity = 0;
    state->edit_quantity = 0;
    state->pressed_control = SALE_HIT_NONE;
    state->message = 0;
}

static int sale_browse_selected_ordinal(
    const GameSession *session,
    const GameSaleInteractionPresentationState *state)
{
    int ordinal;
    if (session == NULL || state == NULL || state->selected_seller < 0 ||
        state->selected_slot < 0)
        return -1;
    for (ordinal = 0; ordinal < rich4_app_sale_browse_count(session); ordinal++) {
        int seller;
        int slot;
        if (rich4_app_sale_browse_listing_at(
                session, ordinal, &seller, &slot) &&
            seller == state->selected_seller && slot == state->selected_slot)
            return ordinal;
    }
    return -1;
}

static int sale_browse_select_ordinal(
    const GameSession *session, GameSaleInteractionPresentationState *state,
    int ordinal)
{
    int seller;
    int slot;
    if (session == NULL || state == NULL ||
        !rich4_app_sale_browse_listing_at(session, ordinal, &seller, &slot)) {
        if (state != NULL) sale_clear_selection(state);
        return 0;
    }
    state->selected_row = seller * GAME_MARKET_LISTING_SLOT_COUNT + slot;
    state->selected_seller = seller;
    state->selected_slot = slot;
    return 1;
}

static void sale_prepare_command(GameSession *session,
                                Rich4AppSaleInteractionResult *result)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    int player;
    if (state == NULL || session == NULL || result == NULL) return;
    player = sale_current_player(&session->state);
    if (player < 0) return;
    if (state->page == RICH4_SALE_PAGE_PRICE) {
        if (state->edit_quantity) {
            if (state->quantity <= 0 ||
                state->quantity > state->item_quantity_max) {
                sale_close_price_entry(state);
                result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
                return;
            }
            state->edit_quantity = 0;
            state->ask_price = 0;
            state->message = 0;
            result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
            return;
        }
        if (state->ask_price <= 0) {
            sale_close_price_entry(state);
            result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
            return;
        }
        if (state->selected_item.kind == GAME_DOMAIN_STOCK &&
            (state->quantity <= 0 ||
             state->quantity > state->item_quantity_max)) {
            sale_close_price_entry(state);
            result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
            return;
        }
        result->command = rich4_game_command_create_market_listing(
            GAME_COMMAND_SOURCE_HUMAN, player, state->selected_item,
            state->ask_price,
            state->selected_item.kind == GAME_DOMAIN_STOCK
                ? state->quantity : 0);
        result->command_ready = 1;
    } else if (state->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM &&
               state->selected_seller != player) {
        const GameMarketListing *listing;
        if (state->selected_seller < 0 ||
            state->selected_seller >= session->state.player_count ||
            state->selected_slot < 0 ||
            state->selected_slot >= GAME_MARKET_LISTING_SLOT_COUNT)
            return;
        listing = &session->state.market_listings[state->selected_seller]
                                                 [state->selected_slot];
        if (listing->type == GAME_MARKET_LISTING_EMPTY ||
            listing->ask_price <= 0 ||
            session->state.players[player].cash < listing->ask_price) {
            /* The original purchase path checks cash before transferring an
             * asset and shows the centered Panel[73] warning on failure. */
            state->page = RICH4_SALE_PAGE_CONFIRM;
            state->selected_row = state->selected_seller *
                                  GAME_MARKET_LISTING_SLOT_COUNT +
                                  state->selected_slot;
            state->message = 4;
            result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
            return;
        }
        result->command = rich4_game_command_buy_market_listing(
            GAME_COMMAND_SOURCE_HUMAN, player, state->selected_seller,
            state->selected_slot);
        result->command_ready = 1;
    } else if (state->page == RICH4_SALE_PAGE_CONFIRM &&
        state->selected_seller == player && state->selected_slot >= 0 &&
        state->selected_slot < GAME_MARKET_LISTING_SLOT_COUNT) {
        result->command = rich4_game_command_cancel_market_listing(
            GAME_COMMAND_SOURCE_HUMAN, player, state->selected_slot);
        result->command_ready = 1;
    }
}

static void sale_set_entry_value(GameSession *session, int value)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    int *entry;
    int maximum;
    if (state == NULL || state->page != RICH4_SALE_PAGE_PRICE) return;
    maximum = rich4_app_sale_interaction_entry_maximum(session);
    if (value < 0) value = 0;
    if (value > maximum) value = maximum;
    entry = state->edit_quantity ? &state->quantity : &state->ask_price;
    *entry = value;
}

static void sale_append_digit(GameSession *session, int digit)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    int value;
    int maximum;
    long long next;
    if (state == NULL || state->page != RICH4_SALE_PAGE_PRICE ||
        digit < 0 || digit > 9)
        return;
    value = rich4_app_sale_interaction_entry_value(session);
    maximum = rich4_app_sale_interaction_entry_maximum(session);
    /* The original keypad accepts at most nine typed digits; M can still set
     * a larger maximum directly. A leading zero remains a single zero. */
    if (value >= 100000000) return;
    next = (long long)value * 10 + digit;
    if (next > maximum) next = maximum;
    sale_set_entry_value(session, (int)next);
}

static void sale_keypad_control(
    GameSession *session, int control, int pointer_x,
    Rich4AppSaleInteractionResult *result)
{
    int value;
    if (control < SALE_HIT_PRICE_KEY_BASE +
                      RICH4_STOCK_QUANTITY_CONTROL_MAX ||
        control > SALE_HIT_PRICE_KEY_BASE +
                      RICH4_STOCK_QUANTITY_CONTROL_SLIDER)
        return;
    control -= SALE_HIT_PRICE_KEY_BASE;
    if (control == RICH4_STOCK_QUANTITY_CONTROL_SLIDER) {
        value = rich4_app_stock_quantity_from_bar(
            rich4_app_sale_interaction_entry_maximum(session), pointer_x);
        if (value >= 0) sale_set_entry_value(session, value);
        return;
    }
    if (control == RICH4_STOCK_QUANTITY_CONTROL_MAX) {
        sale_set_entry_value(
            session, rich4_app_sale_interaction_entry_maximum(session));
    } else if (control == RICH4_STOCK_QUANTITY_CONTROL_ENTER) {
        sale_prepare_command(session, result);
        return;
    } else if (control == 4) {
        sale_set_entry_value(session, 0);
    } else if (control == 5) {
        sale_append_digit(session, 0);
    } else if (control == 6) {
        sale_set_entry_value(
            session, rich4_app_sale_interaction_entry_value(session) / 10);
    } else if (control >= 7 && control <= 15) {
        static const int keypad_digits[9] = {
            7, 8, 9, 4, 5, 6, 1, 2, 3
        };
        sale_append_digit(session, keypad_digits[control - 7]);
    }
    if (result != NULL) result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
}

static void sale_activate(GameSession *session, int control,
                          Rich4AppSaleInteractionResult *result)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    if (state == NULL || !state->active) return;
    if (control == SALE_HIT_CLOSE) {
        if (state->page == RICH4_SALE_PAGE_ITEMS) {
            state->page = RICH4_SALE_PAGE_BROWSE;
            sale_clear_selection(state);
        } else {
            state->active = 0;
        }
        state->message = 0;
        result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
        return;
    }
    if (control == SALE_HIT_SELL) {
        state->message = SALE_PICKER_OPEN;
        state->selected_row = -1;
        return;
    }
    if (control == SALE_HIT_BROWSE) {
        state->page = RICH4_SALE_PAGE_BROWSE;
        state->page_offset = 0;
        state->message = 0;
        sale_clear_selection(state);
        result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
        return;
    }
    if (control == SALE_HIT_ITEMS) {
        state->page = RICH4_SALE_PAGE_ITEMS;
        state->page_offset = 0;
        state->message = 0;
        sale_clear_selection(state);
        result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
        return;
    }
    if (control >= SALE_HIT_CATEGORY_BASE &&
        control < SALE_HIT_CATEGORY_BASE + RICH4_SALE_CATEGORY_COUNT) {
        state->category = (Rich4SaleInteractionCategory)
            (control - SALE_HIT_CATEGORY_BASE);
        state->page = RICH4_SALE_PAGE_ITEMS;
        state->page_offset = 0;
        state->message = 0;
        sale_clear_selection(state);
        if (state->category == RICH4_SALE_CATEGORY_PROPERTY)
            state->selected_slot = 0;
        result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
        return;
    }
    if (control >= SALE_HIT_GRID_BASE &&
        control < SALE_HIT_GRID_BASE +
                      GAME_MAX_PLAYERS * GAME_MARKET_LISTING_SLOT_COUNT) {
        int cell = control - SALE_HIT_GRID_BASE;
        int seller = cell / GAME_MARKET_LISTING_SLOT_COUNT;
        int slot = cell % GAME_MARKET_LISTING_SLOT_COUNT;
        if (seller < session->state.player_count &&
            (session->state.players[seller].play_state & 0xff) != 0 &&
            session->state.market_listings[seller][slot].type !=
                GAME_MARKET_LISTING_EMPTY) {
            state->selected_row = cell;
            state->selected_seller = seller;
            state->selected_slot = slot;
            state->page = RICH4_SALE_PAGE_CONFIRM;
            result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
        } else {
            sale_clear_selection(state);
        }
        state->message = 0;
        return;
    }
    if (control >= SALE_HIT_ROW_BASE &&
        control < SALE_HIT_ROW_BASE + 50 &&
        state->page == RICH4_SALE_PAGE_ITEMS) {
        state->selected_row = control - SALE_HIT_ROW_BASE;
        result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
        if (!sale_activate_item(session)) state->message = 3;
        return;
    }
    if (control == SALE_HIT_PREVIOUS || control == SALE_HIT_NEXT) {
        int rows_per_page = sale_rows_per_page(state);
        int count = state->page == RICH4_SALE_PAGE_ITEMS
            ? rich4_app_sale_item_count(session, state->category)
            : rich4_app_sale_browse_count(session);
        int new_offset = state->page_offset +
            (control == SALE_HIT_NEXT ? rows_per_page : -rows_per_page);
        if (new_offset < 0) new_offset = 0;
        if (new_offset >= count && count > 0)
            new_offset = ((count - 1) / rows_per_page) * rows_per_page;
        state->page_offset = new_offset;
        state->selected_row = -1;
        state->selected_seller = -1;
        state->selected_slot = -1;
        result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
        return;
    }
    if (control == SALE_HIT_ACTION) {
        if (state->page == RICH4_SALE_PAGE_ITEMS) {
            if (!sale_activate_item(session)) state->message = 3;
        } else if (state->page == RICH4_SALE_PAGE_BROWSE) {
            if (state->selected_seller < 0 || state->selected_slot < 0 ||
                state->selected_seller >= session->state.player_count ||
                state->selected_slot >= GAME_MARKET_LISTING_SLOT_COUNT ||
                session->state.market_listings[state->selected_seller]
                                               [state->selected_slot].type ==
                    GAME_MARKET_LISTING_EMPTY) {
                state->message = 3;
            } else {
                state->page = RICH4_SALE_PAGE_CONFIRM;
                state->message = 0;
            }
        } else if (state->page == RICH4_SALE_PAGE_PRICE) {
            sale_prepare_command(session, result);
        }
        return;
    }
    if (control == SALE_HIT_CONFIRM_NO) {
        if (state->page == RICH4_SALE_PAGE_CONFIRM)
            state->page = RICH4_SALE_PAGE_BROWSE;
        state->message = 0;
        result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
        return;
    }
    if (control == SALE_HIT_CONFIRM_YES) {
        if (state->page == RICH4_SALE_PAGE_CONFIRM &&
            state->selected_seller != session->state.current_player) {
            state->page = RICH4_SALE_PAGE_PURCHASE_CONFIRM;
            state->selected_row = 1;
            state->message = 0;
            result->audio_cue = RICH4_APP_SALE_AUDIO_SELECT;
            return;
        }
        sale_prepare_command(session, result);
        return;
    }
    if (control == SALE_HIT_PURCHASE_CONFIRM_NO) {
        state->page = RICH4_SALE_PAGE_CONFIRM;
        state->selected_row = state->selected_seller *
                              GAME_MARKET_LISTING_SLOT_COUNT +
                              state->selected_slot;
        state->message = 0;
        result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
        return;
    }
    if (control == SALE_HIT_PURCHASE_CONFIRM_YES) {
        sale_prepare_command(session, result);
        return;
    }
}

int rich4_app_sale_interaction_is_active(const GameSession *session)
{
    const GameSaleInteractionPresentationState *state =
        sale_state_const(session);
    return state != NULL && state->active;
}

void rich4_app_sale_interaction_open(GameSession *session)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    int player;
    if (state == NULL || session == NULL ||
        (player = sale_current_player(&session->state)) < 0 ||
        session->state.players[player].is_ai)
        return;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->page = RICH4_SALE_PAGE_BROWSE;
    state->category = RICH4_SALE_CATEGORY_STOCK;
    state->selected_row = -1;
    state->selected_seller = -1;
    state->selected_slot = -1;
    state->selected_item = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    state->pressed_control = SALE_HIT_NONE;
}

void rich4_app_sale_interaction_cancel(GameSession *session)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    if (state == NULL || !state->active) return;
    state->pressed_control = SALE_HIT_NONE;
    if (state->message == SALE_PICKER_OPEN) {
        state->message = 0;
        state->selected_row = -1;
    } else if (state->page == RICH4_SALE_PAGE_PRICE) {
        sale_close_price_entry(state);
    } else if (state->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
        state->page = RICH4_SALE_PAGE_CONFIRM;
        state->selected_row = state->selected_seller *
                              GAME_MARKET_LISTING_SLOT_COUNT +
                              state->selected_slot;
        state->message = 0;
    } else if (state->page == RICH4_SALE_PAGE_CONFIRM) {
        state->page = RICH4_SALE_PAGE_BROWSE;
        state->message = 0;
    } else if (state->page == RICH4_SALE_PAGE_ITEMS) {
        state->page = RICH4_SALE_PAGE_BROWSE;
        state->page_offset = 0;
        state->message = 0;
        sale_clear_selection(state);
    } else {
        state->active = 0;
    }
}

void rich4_app_sale_interaction_mouse_down(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y,
    int stock_panel_width, int property_panel_width,
    int confirm_dialog_width, int confirm_dialog_height)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    if (state == NULL || !state->active) return;
    sale_update_property_filter(state, x, y, property_panel_width);
    state->pressed_control = sale_control_at(
        session, state, mask, x, y, stock_panel_width, property_panel_width,
        confirm_dialog_width, confirm_dialog_height);
    if (state->page == RICH4_SALE_PAGE_BROWSE &&
        state->pressed_control == SALE_HIT_SELL) {
        state->message = SALE_PICKER_OPEN;
        state->selected_row = -1;
    } else if (state->page == RICH4_SALE_PAGE_BROWSE &&
               state->pressed_control >= SALE_HIT_GRID_BASE &&
               state->pressed_control < SALE_HIT_GRID_BASE +
                   GAME_MAX_PLAYERS * GAME_MARKET_LISTING_SLOT_COUNT) {
        int cell = state->pressed_control - SALE_HIT_GRID_BASE;
        int seller = cell / GAME_MARKET_LISTING_SLOT_COUNT;
        int slot = cell % GAME_MARKET_LISTING_SLOT_COUNT;
        if (seller < session->state.player_count &&
            session->state.market_listings[seller][slot].type !=
                GAME_MARKET_LISTING_EMPTY) {
            state->selected_seller = seller;
            state->selected_slot = slot;
            state->selected_row = cell;
        }
    }
}

void rich4_app_sale_interaction_mouse_motion(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y,
    int mouse_left_down, int stock_panel_width, int property_panel_width,
    int confirm_dialog_width, int confirm_dialog_height)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    int control;
    if (state == NULL || !state->active) return;
    if (state->page == RICH4_SALE_PAGE_PRICE && mouse_left_down &&
        state->pressed_control == SALE_HIT_PRICE_KEY_BASE +
            RICH4_STOCK_QUANTITY_CONTROL_SLIDER &&
        rich4_app_stock_quantity_control_at_mask(mask, x, y) ==
            RICH4_STOCK_QUANTITY_CONTROL_SLIDER) {
        int value = rich4_app_stock_quantity_from_bar(
            rich4_app_sale_interaction_entry_maximum(session), x);
        if (value >= 0) sale_set_entry_value(session, value);
    } else if (state->page == RICH4_SALE_PAGE_BROWSE &&
        state->message == SALE_PICKER_OPEN &&
        state->pressed_control == SALE_HIT_SELL) {
        /* 0x427C21 tracks picker hover only during the held SELL click. */
        if (!mouse_left_down) return;
        control = sale_picker_category_at(x, y);
        state->selected_row = control == SALE_HIT_NONE
            ? -1 : control - SALE_HIT_CATEGORY_BASE;
    } else if (state->page == RICH4_SALE_PAGE_ITEMS) {
        if (state->category == RICH4_SALE_CATEGORY_STOCK ||
            state->category == RICH4_SALE_CATEGORY_PROPERTY) {
            /* sub_4258C1/sub_42608F use WM_MOUSEMOVE for their row frame,
             * including clearing it as soon as the pointer leaves the list. */
            state->selected_row = sale_hover_row_at(
                state, x, y, stock_panel_width, property_panel_width);
        }
    } else if (state->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
        control = sale_purchase_dialog_control_at(
            state, x, y, confirm_dialog_width, confirm_dialog_height);
        state->selected_row = control == SALE_HIT_PURCHASE_CONFIRM_YES ? 1 :
            control == SALE_HIT_PURCHASE_CONFIRM_NO ? 2 : 0;
    }
}

void rich4_app_sale_interaction_mouse_release(
    GameSession *session, const Rich4StockQuantityMask *mask, int button,
    int x, int y, int stock_panel_width, int property_panel_width,
    int confirm_dialog_width, int confirm_dialog_height,
    Rich4AppSaleInteractionResult *out_result)
{
    Rich4AppSaleInteractionResult ignored_result;
    GameSaleInteractionPresentationState *state = sale_state(session);
    int control;
    if (out_result == NULL) out_result = &ignored_result;
    sale_result_clear(out_result);
    if (state == NULL || !state->active) return;
    control = sale_control_at(session, state, mask, x, y,
                              stock_panel_width, property_panel_width,
                              confirm_dialog_width, confirm_dialog_height);
    if (button == RICH4_SALE_POINTER_BUTTON_RIGHT) {
        if (state->page == RICH4_SALE_PAGE_PRICE) {
            sale_close_price_entry(state);
            state->pressed_control = SALE_HIT_NONE;
            out_result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
        }
        return;
    }
    if (button != RICH4_SALE_POINTER_BUTTON_LEFT) return;
    if (state->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
        state->pressed_control = SALE_HIT_NONE;
        if (control == SALE_HIT_PURCHASE_CONFIRM_YES ||
            control == SALE_HIT_PURCHASE_CONFIRM_NO)
            sale_activate(session, control, out_result);
        return;
    }
    if (state->pressed_control == SALE_HIT_SELL) {
        state->pressed_control = SALE_HIT_NONE;
        if (state->selected_row >= 0 &&
            state->selected_row < RICH4_SALE_CATEGORY_COUNT) {
            sale_activate(session,
                SALE_HIT_CATEGORY_BASE + state->selected_row, out_result);
        } else {
            /* sub_4249C2 redraws the parent over the picker on mouseup. */
            state->message = 0;
            state->selected_row = -1;
        }
        return;
    }
    if (state->pressed_control != SALE_HIT_NONE &&
        state->pressed_control != control)
        control = SALE_HIT_NONE;
    state->pressed_control = SALE_HIT_NONE;
    if (control >= SALE_HIT_PRICE_KEY_BASE &&
        control <= SALE_HIT_PRICE_KEY_BASE +
                       RICH4_STOCK_QUANTITY_CONTROL_SLIDER) {
        sale_keypad_control(session, control, x, out_result);
        return;
    }
    if (out_result != NULL) sale_activate(session, control, out_result);
}

void rich4_app_sale_interaction_key_down(
    GameSession *session, Rich4AppKey key,
    Rich4AppSaleInteractionResult *out_result)
{
    Rich4AppSaleInteractionResult ignored_result;
    GameSaleInteractionPresentationState *state = sale_state(session);
    int count;
    int max_row;
    if (out_result == NULL) out_result = &ignored_result;
    sale_result_clear(out_result);
    if (state == NULL || !state->active) return;
    if (key == RICH4_APP_KEY_ESCAPE) {
        rich4_app_sale_interaction_cancel(session);
        if (out_result != NULL)
            out_result->audio_cue = RICH4_APP_SALE_AUDIO_CANCEL;
        return;
    }
    if (state->page == RICH4_SALE_PAGE_PRICE) {
        if (key >= RICH4_APP_KEY_DIGIT_0 && key <= RICH4_APP_KEY_DIGIT_9)
            sale_append_digit(session, key - RICH4_APP_KEY_DIGIT_0);
        else if (key >= RICH4_APP_KEY_KP_0 && key <= RICH4_APP_KEY_KP_9)
            sale_append_digit(session, key - RICH4_APP_KEY_KP_0);
        else if (key == RICH4_APP_KEY_BACKSPACE) {
            sale_set_entry_value(
                session,
                rich4_app_sale_interaction_entry_value(session) / 10);
        } else if (key == RICH4_APP_KEY_DELETE ||
                   key == RICH4_APP_KEY_LETTER_C) {
            sale_set_entry_value(session, 0);
        } else if (key == RICH4_APP_KEY_LETTER_M) {
            sale_set_entry_value(
                session, rich4_app_sale_interaction_entry_maximum(session));
        } else if (key == RICH4_APP_KEY_RETURN ||
                   key == RICH4_APP_KEY_KP_ENTER) {
            sale_prepare_command(session, out_result);
        }
        return;
    }
    if (state->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
        if (key == RICH4_APP_KEY_RETURN || key == RICH4_APP_KEY_KP_ENTER)
            sale_prepare_command(session, out_result);
        return;
    }
    if (state->page == RICH4_SALE_PAGE_CONFIRM) {
        if (key == RICH4_APP_KEY_RETURN || key == RICH4_APP_KEY_KP_ENTER)
            sale_activate(session, SALE_HIT_CONFIRM_YES, out_result);
        return;
    }
    if (state->page == RICH4_SALE_PAGE_BROWSE) {
        int selected = sale_browse_selected_ordinal(session, state);
        count = rich4_app_sale_browse_count(session);
        max_row = count - 1;
        if (key == RICH4_APP_KEY_UP && selected > 0)
            sale_browse_select_ordinal(session, state, selected - 1);
        else if (key == RICH4_APP_KEY_DOWN) {
            if (selected < 0 && max_row >= 0)
                sale_browse_select_ordinal(session, state, 0);
            else if (selected < max_row)
                sale_browse_select_ordinal(session, state, selected + 1);
        } else if (key == RICH4_APP_KEY_PAGEUP && selected > 0) {
            int previous = selected - GAME_MARKET_LISTING_SLOT_COUNT;
            if (previous < 0) previous = 0;
            sale_browse_select_ordinal(session, state, previous);
        } else if (key == RICH4_APP_KEY_PAGEDOWN && selected < max_row) {
            int next = selected + GAME_MARKET_LISTING_SLOT_COUNT;
            if (next > max_row) next = max_row;
            sale_browse_select_ordinal(session, state, next);
        }
    } else if (state->page == RICH4_SALE_PAGE_ITEMS) {
        count = rich4_app_sale_item_count(session, state->category);
        max_row = count - state->page_offset - 1;
        {
            int rows_per_page = sale_rows_per_page(state);
            if (max_row >= rows_per_page) max_row = rows_per_page - 1;
        }
        if (max_row < 0) max_row = -1;
        if (key == RICH4_APP_KEY_UP && state->selected_row > 0)
            state->selected_row--;
        else if (key == RICH4_APP_KEY_DOWN) {
            if (state->selected_row < 0 && max_row >= 0)
                state->selected_row = 0;
            else if (state->selected_row < max_row)
                state->selected_row++;
        } else if (key == RICH4_APP_KEY_PAGEUP) {
            sale_activate(session, SALE_HIT_PREVIOUS, out_result);
        } else if (key == RICH4_APP_KEY_PAGEDOWN) {
            sale_activate(session, SALE_HIT_NEXT, out_result);
        }
    }
    if (key == RICH4_APP_KEY_RETURN ||
        key == RICH4_APP_KEY_KP_ENTER) {
        sale_activate(session,
                      SALE_HIT_ACTION, out_result);
    }
}

void rich4_app_sale_interaction_command_applied(
    GameSession *session, int succeeded,
    Rich4AppSaleInteractionResult *result)
{
    GameSaleInteractionPresentationState *state = sale_state(session);
    if (state == NULL || result == NULL || !result->command_ready) return;
    if (!succeeded) {
        if (state->page == RICH4_SALE_PAGE_PURCHASE_CONFIRM) {
            state->page = RICH4_SALE_PAGE_CONFIRM;
            state->selected_row = state->selected_seller *
                                  GAME_MARKET_LISTING_SLOT_COUNT +
                                  state->selected_slot;
        }
        state->message = 0;
        return;
    }
    state->message = 0;
    if (state->page == RICH4_SALE_PAGE_PRICE) {
        /* The quantity/price prompt is a child of the selected sell list.
         * Closing the child after submit leaves that category window open. */
        sale_close_price_entry(state);
        if (state->category == RICH4_SALE_CATEGORY_PROPERTY &&
            state->selected_slot < 0)
            state->selected_slot = 0;
    } else {
        state->page_offset = 0;
        state->page = RICH4_SALE_PAGE_BROWSE;
        sale_clear_selection(state);
    }
    result->audio_cue = RICH4_APP_SALE_AUDIO_OK;
}
