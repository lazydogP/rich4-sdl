#include "app/stock_interaction_flow.h"

#include <string.h>

#include "core/stock.h"

enum {
    STOCK_QUANTITY_PANEL_X = 256,
    STOCK_QUANTITY_PANEL_Y = 144,
    STOCK_QUANTITY_PANEL_WIDTH = 128,
    STOCK_QUANTITY_PANEL_HEIGHT = 192,
    STOCK_QUANTITY_BAR_OFFSET_X = 10,
    /* There are 34 lookup entries (0..33), but the original slider scales
     * both input and rendering over 33 intervals. */
    STOCK_QUANTITY_BAR_STEPS = 34,
    STOCK_QUANTITY_BAR_SCALE = 33
};

static const unsigned char stock_quantity_bar_thresholds[STOCK_QUANTITY_BAR_STEPS] = {
    3, 6, 9, 12, 15, 19, 22, 25, 28, 31, 34, 38,
    41, 44, 47, 50, 53, 57, 60, 63, 66, 69, 73, 76,
    79, 82, 85, 88, 91, 94, 98, 101, 104, 107
};

/* App-owned stock interaction: page, selection, quantity and typed trade
 * intent. It borrows GameSession domain state, emits no SDL/resource calls,
 * and leaves clock/audio/command application to the platform adapter.
 * Invariant: invalid or zero quantities never produce a command.
 * Test: tests/unit/test_stock_interaction_flow.c. */

static int stock_wrap_index(int value, int count)
{
    if (count <= 0) return 0;
    value %= count;
    return value < 0 ? value + count : value;
}

static int stock_point_in_rect(int px, int py, int x, int y, int width,
                               int height)
{
    return px >= x && px < x + width && py >= y && py < y + height;
}

static GameStockInteractionPresentationState *stock_state(
    GameSession *session)
{
    return session == NULL ? NULL : &session->presentation.stock_interaction;
}

static const GameStockInteractionPresentationState *stock_state_const(
    const GameSession *session)
{
    return session == NULL ? NULL : &session->presentation.stock_interaction;
}

static void stock_result_clear(Rich4AppStockInteractionResult *result)
{
    if (result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->command = rich4_game_command(
        GAME_COMMAND_NONE, GAME_COMMAND_SOURCE_HUMAN, 0, 0);
}

int rich4_app_stock_interaction_is_active(const GameSession *session)
{
    const GameStockInteractionPresentationState *state =
        stock_state_const(session);
    return state != NULL && state->active;
}

int rich4_app_stock_interaction_hit(int x, int y)
{
    if (stock_point_in_rect(x, y, 17, 10, 107, 29))
        return RICH4_STOCK_HIT_TOGGLE;
    if (stock_point_in_rect(x, y, 129, 10, 69, 29))
        return RICH4_STOCK_HIT_BUY;
    if (stock_point_in_rect(x, y, 203, 10, 69, 29))
        return RICH4_STOCK_HIT_SELL;
    if (stock_point_in_rect(x, y, 277, 10, 133, 29))
        return RICH4_STOCK_HIT_INFO;
    if (stock_point_in_rect(x, y, 553, 10, 70, 29))
        return RICH4_STOCK_HIT_CLOSE;
    if (x >= 16 && x < 624 && y >= 81 && y < 464)
        return RICH4_STOCK_HIT_ROW_BASE + (y - 81) / 32;
    return RICH4_STOCK_HIT_NONE;
}

static int stock_quantity_control_at_geometry(int x, int y)
{
    int local_x = x - STOCK_QUANTITY_PANEL_X;
    int local_y = y - STOCK_QUANTITY_PANEL_Y;
    if (local_x < 0 || local_x >= STOCK_QUANTITY_PANEL_WIDTH ||
        local_y < 0 || local_y >= STOCK_QUANTITY_PANEL_HEIGHT)
        return RICH4_STOCK_QUANTITY_CONTROL_NONE;
    /* These bounds mirror Panel.mkf[22].  The resource is preferred at
     * runtime; this fallback keeps the app flow testable without assets. */
    if ((local_y == 41 && local_x >= 25) ||
        (local_y >= 42 && local_y <= 54 &&
         (local_x < 7 || local_x >= 25)) ||
        (local_y == 55 && local_x < 7))
        return RICH4_STOCK_QUANTITY_CONTROL_SLIDER;
    if (local_y >= 64 && local_y < 88) {
        if (local_x >= 24 && local_x < 71) return 2;
        if (local_x >= 80) return 3;
    }
    if (local_y >= 96 && local_y < 112) {
        if (local_x >= 24 && local_x < 56) return 4;
        if (local_x >= 64 && local_x < 96) return 5;
        if (local_x >= 104) return 6;
    }
    if (local_y >= 121 && local_y < 136) {
        if (local_x >= 24 && local_x < 56) return 7;
        if (local_x >= 64 && local_x < 96) return 8;
        if (local_x >= 104) return 9;
    }
    if (local_y >= 144 && local_y < 160) {
        if (local_x >= 24 && local_x < 56) return 10;
        if (local_x >= 64 && local_x < 96) return 11;
        if (local_x >= 104) return 12;
    }
    if (local_y >= 168 && local_y < 184) {
        if (local_x >= 24 && local_x < 56) return 13;
        if (local_x >= 64 && local_x < 96) return 14;
        if (local_x >= 104) return 15;
    }
    return RICH4_STOCK_QUANTITY_CONTROL_NONE;
}

int rich4_app_stock_quantity_control_at(int x, int y)
{
    int control = stock_quantity_control_at_geometry(x, y);
    if (control < RICH4_STOCK_QUANTITY_CONTROL_DRAG ||
        control > RICH4_STOCK_QUANTITY_CONTROL_SLIDER)
        return RICH4_STOCK_QUANTITY_CONTROL_NONE;
    return control;
}

int rich4_app_stock_quantity_control_at_mask(
    const Rich4StockQuantityMask *mask, int x, int y)
{
    int local_x = x - STOCK_QUANTITY_PANEL_X;
    int local_y = y - STOCK_QUANTITY_PANEL_Y;
    size_t offset;
    int control;

    if (mask == NULL || !mask->available || mask->data == NULL ||
        local_x < 0 || local_x >= STOCK_QUANTITY_PANEL_WIDTH ||
        local_y < 0 || local_y >= STOCK_QUANTITY_PANEL_HEIGHT ||
        mask->size < (size_t)(STOCK_QUANTITY_PANEL_WIDTH *
                              STOCK_QUANTITY_PANEL_HEIGHT))
        return stock_quantity_control_at_geometry(x, y);
    offset = (size_t)local_y * STOCK_QUANTITY_PANEL_WIDTH +
             (size_t)local_x;
    control = mask->data[offset];
    return control >= RICH4_STOCK_QUANTITY_CONTROL_DRAG && control <=
               RICH4_STOCK_QUANTITY_CONTROL_SLIDER
        ? control : RICH4_STOCK_QUANTITY_CONTROL_NONE;
}

int rich4_app_stock_quantity_bar_width(int quantity, int maximum)
{
    int index;
    if (quantity <= 0 || maximum <= 0) return 0;
    if (quantity > maximum) quantity = maximum;
    index = (int)(((long long)quantity * STOCK_QUANTITY_BAR_SCALE) /
                  maximum);
    if (index < 0) index = 0;
    if (index >= STOCK_QUANTITY_BAR_STEPS)
        index = STOCK_QUANTITY_BAR_STEPS - 1;
    return stock_quantity_bar_thresholds[index];
}

int rich4_app_stock_quantity_from_bar(int maximum, int pointer_x)
{
    int position;
    int index;
    if (maximum <= 0) return 0;
    position = pointer_x - STOCK_QUANTITY_PANEL_X -
               STOCK_QUANTITY_BAR_OFFSET_X;
    if (position <= 0) return 0;
    for (index = 0; index < STOCK_QUANTITY_BAR_STEPS; index++) {
        if (position <= stock_quantity_bar_thresholds[index])
            return (int)(((long long)maximum * index) /
                         STOCK_QUANTITY_BAR_SCALE);
    }
    /* The original mask has a small right-hand padding area.  It is not an
     * extra percentage step, so leave the current amount unchanged. */
    return -1;
}

void rich4_app_stock_interaction_open(GameSession *session)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->active = 1;
    state->page = RICH4_STOCK_PANEL_QUOTES;
    state->pressed_control = RICH4_STOCK_HIT_NONE;
    state->trade_type = GAME_COMMAND_NONE;
}

void rich4_app_stock_interaction_close(GameSession *session)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    if (state == NULL) return;
    state->active = 0;
    state->pressed_control = RICH4_STOCK_HIT_NONE;
    state->quantity_active = 0;
    state->quantity_pressed_control = RICH4_STOCK_QUANTITY_CONTROL_NONE;
    state->message = RICH4_STOCK_MESSAGE_NONE;
}

void rich4_app_stock_interaction_cancel(GameSession *session)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    if (state == NULL || !state->active) return;
    state->pressed_control = RICH4_STOCK_HIT_NONE;
    if (state->message != RICH4_STOCK_MESSAGE_NONE) {
        state->message = RICH4_STOCK_MESSAGE_NONE;
    } else if (state->quantity_active) {
        state->quantity_active = 0;
        state->quantity_pressed_control =
            RICH4_STOCK_QUANTITY_CONTROL_NONE;
        state->trade_type = GAME_COMMAND_NONE;
    } else if (state->page == RICH4_STOCK_PANEL_DETAIL ||
               state->page == RICH4_STOCK_PANEL_HOLDINGS) {
        state->page = RICH4_STOCK_PANEL_QUOTES;
    } else {
        rich4_app_stock_interaction_close(session);
    }
}

static void stock_interaction_mouse_down(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    int hit;
    if (state == NULL || !state->active ||
        state->message != RICH4_STOCK_MESSAGE_NONE)
        return;
    if (state->quantity_active) {
        state->quantity_pressed_control =
            rich4_app_stock_quantity_control_at_mask(mask, x, y);
        return;
    }
    if (state->page == RICH4_STOCK_PANEL_DETAIL) return;
    hit = rich4_app_stock_interaction_hit(x, y);
    state->pressed_control =
        hit >= RICH4_STOCK_HIT_TOGGLE && hit <= RICH4_STOCK_HIT_CLOSE
            ? hit : RICH4_STOCK_HIT_NONE;
}

void rich4_app_stock_interaction_mouse_down(
    GameSession *session, int x, int y)
{
    stock_interaction_mouse_down(session, NULL, x, y);
}

void rich4_app_stock_interaction_mouse_down_with_mask(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y)
{
    if (session == NULL) return;
    stock_interaction_mouse_down(session, mask, x, y);
}

static int stock_trade_maximum(const GameSession *session,
                               int stock, GameCommandType type)
{
    const GameState *game;
    const GameStockInfo *info;
    int player;
    int maximum;
    if (session == NULL || stock < 0 || stock >= GAME_STOCK_COUNT)
        return 0;
    game = &session->state;
    player = stock_wrap_index(game->current_player, game->player_count);
    info = &game->stocks[stock];
    if (rich4_stock_market_is_closed(game) || info->f6 != 0 ||
        info->f20 <= 0.0f) return 0;
    if (type == GAME_COMMAND_SELL_STOCK)
        return game->player_stock_amount[player][stock];
    maximum = (int)((double)game->players[player].bank / info->f20);
    if (maximum > info->f10) maximum = info->f10;
    return maximum > 0 ? maximum : 0;
}

static void stock_begin_trade(GameSession *session,
                              GameCommandType type)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    const GameStockInfo *info;
    GameStockPriceState price_state;
    int maximum;
    if (state == NULL || session == NULL || state->selected_stock < 0 ||
        state->selected_stock >= GAME_STOCK_COUNT) return;
    info = &session->state.stocks[state->selected_stock];
    if (info->f6 != 0) return;
    price_state = rich4_stock_price_state(info->f16, info->f20);
    if (type == GAME_COMMAND_BUY_STOCK &&
        price_state == GAME_STOCK_PRICE_LIMIT_UP) {
        state->message = RICH4_STOCK_MESSAGE_LIMIT_UP;
        return;
    }
    if (type == GAME_COMMAND_SELL_STOCK &&
        price_state == GAME_STOCK_PRICE_LIMIT_DOWN) {
        state->message = RICH4_STOCK_MESSAGE_LIMIT_DOWN;
        return;
    }
    maximum = stock_trade_maximum(session, state->selected_stock, type);
    if (maximum <= 0) return;
    state->quantity_active = 1;
    state->trade_type = type;
    state->quantity = 0;
    state->maximum = maximum;
    state->quantity_pressed_control = RICH4_STOCK_QUANTITY_CONTROL_NONE;
}

static void stock_append_digit(GameStockInteractionPresentationState *state,
                               int digit)
{
    long next;
    if (state == NULL || !state->quantity_active || digit < 0 || digit > 9)
        return;
    next = (long)state->quantity * 10 + digit;
    state->quantity = next > state->maximum ? state->maximum : (int)next;
}

static void stock_prepare_command(
    GameSession *session, Rich4AppStockInteractionResult *result)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    int player;
    if (state == NULL || result == NULL || !state->quantity_active ||
        state->quantity <= 0 || state->quantity > state->maximum)
        return;
    player = stock_wrap_index(session->state.current_player,
                              session->state.player_count);
    result->command = state->trade_type == GAME_COMMAND_BUY_STOCK
        ? rich4_game_command_buy_stock(
            GAME_COMMAND_SOURCE_HUMAN, player,
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, state->selected_stock),
            state->quantity)
        : rich4_game_command_sell_stock(
            GAME_COMMAND_SOURCE_HUMAN, player,
            rich4_game_domain_ref(GAME_DOMAIN_STOCK, state->selected_stock),
            state->quantity);
    result->command_ready = 1;
}

static void stock_quantity_control(
    GameSession *session, int control, int pointer_x,
    Rich4AppStockInteractionResult *result)
{
    static const int digits[16] = {
        -1, -1, -1, -1, -1, 0, -1, 7,
        8, 9, 4, 5, 6, 1, 2, 3
    };
    GameStockInteractionPresentationState *state = stock_state(session);
    if (state == NULL || !state->quantity_active) return;
    if (control == RICH4_STOCK_QUANTITY_CONTROL_SLIDER) {
        int quantity = rich4_app_stock_quantity_from_bar(
            state->maximum, pointer_x);
        if (quantity >= 0) state->quantity = quantity;
        if (result != NULL)
            result->audio_cue = RICH4_APP_STOCK_AUDIO_SLIDER;
    } else if (control == RICH4_STOCK_QUANTITY_CONTROL_MAX) {
        state->quantity = state->maximum;
    } else if (control == RICH4_STOCK_QUANTITY_CONTROL_ENTER) {
        stock_prepare_command(session, result);
    } else if (control == 4) {
        state->quantity = 0;
    } else if (control == 6) {
        state->quantity /= 10;
    } else if (control >= 0 && control < 16 && digits[control] >= 0) {
        stock_append_digit(state, digits[control]);
    }
    if ((control == RICH4_STOCK_QUANTITY_CONTROL_MAX ||
         (control >= 4 && control <= 15)) && result != NULL)
        result->audio_cue = RICH4_APP_STOCK_AUDIO_KEY;
}

static void stock_activate_hit(GameSession *session, int hit,
                               Rich4AppStockInteractionResult *result)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    if (state == NULL || !state->active || session == NULL) return;
    if (rich4_stock_market_is_closed(&session->state)) {
        rich4_app_stock_interaction_close(session);
        return;
    }
    if (hit >= RICH4_STOCK_HIT_ROW_BASE &&
        hit < RICH4_STOCK_HIT_ROW_BASE + GAME_STOCK_COUNT) {
        int row = hit - RICH4_STOCK_HIT_ROW_BASE;
        if (state->selected_stock == row)
            state->page = RICH4_STOCK_PANEL_DETAIL;
        else
            state->selected_stock = row;
        if (result != NULL)
            result->audio_cue = RICH4_APP_STOCK_AUDIO_SELECT;
    } else if (hit == RICH4_STOCK_HIT_TOGGLE) {
        state->page = state->page == RICH4_STOCK_PANEL_HOLDINGS
            ? RICH4_STOCK_PANEL_QUOTES : RICH4_STOCK_PANEL_HOLDINGS;
        if (result != NULL)
            result->audio_cue = RICH4_APP_STOCK_AUDIO_SELECT;
    } else if (hit == RICH4_STOCK_HIT_BUY) {
        stock_begin_trade(session, GAME_COMMAND_BUY_STOCK);
    } else if (hit == RICH4_STOCK_HIT_SELL) {
        stock_begin_trade(session, GAME_COMMAND_SELL_STOCK);
    } else if (hit == RICH4_STOCK_HIT_INFO && state->selected_stock >= 0) {
        state->page = RICH4_STOCK_PANEL_DETAIL;
        if (result != NULL)
            result->audio_cue = RICH4_APP_STOCK_AUDIO_SELECT;
    } else if (hit == RICH4_STOCK_HIT_CLOSE) {
        rich4_app_stock_interaction_close(session);
        if (result != NULL)
            result->audio_cue = RICH4_APP_STOCK_AUDIO_CANCEL;
    }
}

static void stock_interaction_mouse_release(
    GameSession *session, const Rich4StockQuantityMask *mask, int button,
    int x, int y, Rich4AppStockInteractionResult *out_result)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    int pressed_control;
    stock_result_clear(out_result);
    if (state == NULL || !state->active) return;
    pressed_control = state->pressed_control;
    state->pressed_control = RICH4_STOCK_HIT_NONE;
    if (rich4_stock_market_is_closed(&session->state)) {
        rich4_app_stock_interaction_close(session);
        return;
    }
    if (state->message != RICH4_STOCK_MESSAGE_NONE) {
        state->message = RICH4_STOCK_MESSAGE_NONE;
        return;
    }
    if (state->page == RICH4_STOCK_PANEL_DETAIL &&
        (button == RICH4_APP_POINTER_BUTTON_LEFT ||
         button == RICH4_APP_POINTER_BUTTON_RIGHT)) {
        state->page = RICH4_STOCK_PANEL_QUOTES;
        return;
    }
    if (button == RICH4_APP_POINTER_BUTTON_RIGHT) {
        rich4_app_stock_interaction_cancel(session);
        out_result->audio_cue = RICH4_APP_STOCK_AUDIO_CANCEL;
        return;
    }
    if (button != RICH4_APP_POINTER_BUTTON_LEFT) return;
    if (state->quantity_active) {
        int control = state->quantity_pressed_control;
        int hit = rich4_app_stock_quantity_control_at_mask(mask, x, y);
        state->quantity_pressed_control =
            RICH4_STOCK_QUANTITY_CONTROL_NONE;
        if (control < RICH4_STOCK_QUANTITY_CONTROL_MAX || control >
                RICH4_STOCK_QUANTITY_CONTROL_SLIDER)
            control = hit;
        if (control == RICH4_STOCK_QUANTITY_CONTROL_SLIDER && hit !=
                RICH4_STOCK_QUANTITY_CONTROL_SLIDER)
            return;
        stock_quantity_control(session, control, x, out_result);
        return;
    }
    if (state->page != RICH4_STOCK_PANEL_DETAIL)
        stock_activate_hit(
            session,
            pressed_control >= RICH4_STOCK_HIT_TOGGLE &&
                    pressed_control <= RICH4_STOCK_HIT_CLOSE
                ? pressed_control : rich4_app_stock_interaction_hit(x, y),
            out_result);
}

void rich4_app_stock_interaction_mouse_release(
    GameSession *session, int button, int x, int y,
    Rich4AppStockInteractionResult *out_result)
{
    stock_interaction_mouse_release(session, NULL, button, x, y,
                                     out_result);
}

void rich4_app_stock_interaction_mouse_release_with_mask(
    GameSession *session, const Rich4StockQuantityMask *mask, int button,
    int x, int y, Rich4AppStockInteractionResult *out_result)
{
    if (session == NULL) {
        stock_result_clear(out_result);
        return;
    }
    stock_interaction_mouse_release(session, mask, button, x, y,
                                    out_result);
}

void rich4_app_stock_interaction_mouse_motion(
    GameSession *session, const Rich4StockQuantityMask *mask, int x, int y,
    Rich4AppStockInteractionResult *out_result)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    stock_result_clear(out_result);
    if (state == NULL || !state->active || !state->quantity_active ||
        state->quantity_pressed_control !=
            RICH4_STOCK_QUANTITY_CONTROL_SLIDER ||
        rich4_app_stock_quantity_control_at_mask(mask, x, y) !=
            RICH4_STOCK_QUANTITY_CONTROL_SLIDER)
        return;
    stock_quantity_control(session,
                           RICH4_STOCK_QUANTITY_CONTROL_SLIDER, x,
                           out_result);
}

void rich4_app_stock_interaction_key_down(
    GameSession *session, Rich4AppKey key,
    Rich4AppStockInteractionResult *out_result)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    stock_result_clear(out_result);
    if (state == NULL || !state->active) return;
    if (state->message != RICH4_STOCK_MESSAGE_NONE) {
        state->message = RICH4_STOCK_MESSAGE_NONE;
        return;
    }
    if (key == RICH4_APP_KEY_ESCAPE) {
        rich4_app_stock_interaction_cancel(session);
        out_result->audio_cue = RICH4_APP_STOCK_AUDIO_CANCEL;
        return;
    }
    if (state->quantity_active) {
        if (key >= RICH4_APP_KEY_DIGIT_0 && key <= RICH4_APP_KEY_DIGIT_9)
            stock_append_digit(state, key - RICH4_APP_KEY_DIGIT_0);
        else if (key >= RICH4_APP_KEY_KP_0 && key <= RICH4_APP_KEY_KP_9)
            stock_append_digit(state, key - RICH4_APP_KEY_KP_0);
        else if (key == RICH4_APP_KEY_BACKSPACE)
            state->quantity /= 10;
        else if (key == RICH4_APP_KEY_DELETE || key == RICH4_APP_KEY_C)
            state->quantity = 0;
        else if (key == RICH4_APP_KEY_M)
            state->quantity = state->maximum;
        else if (key == RICH4_APP_KEY_RETURN ||
                 key == RICH4_APP_KEY_KP_ENTER)
            stock_prepare_command(session, out_result);
        return;
    }
    if (key == RICH4_APP_KEY_UP && state->selected_stock > 0)
        state->selected_stock--;
    else if (key == RICH4_APP_KEY_DOWN &&
             state->selected_stock + 1 < GAME_STOCK_COUNT)
        state->selected_stock++;
    else if (key == RICH4_APP_KEY_TAB)
        state->page = state->page == RICH4_STOCK_PANEL_HOLDINGS
            ? RICH4_STOCK_PANEL_QUOTES : RICH4_STOCK_PANEL_HOLDINGS;
    else if (key == RICH4_APP_KEY_RETURN)
        state->page = RICH4_STOCK_PANEL_DETAIL;
}

int rich4_app_stock_interaction_command_applied(
    GameSession *session, int succeeded)
{
    GameStockInteractionPresentationState *state = stock_state(session);
    if (state == NULL || !succeeded || !state->quantity_active ||
        state->quantity <= 0 || state->quantity > state->maximum)
        return RICH4_APP_STOCK_AUDIO_NONE;
    state->quantity_active = 0;
    state->quantity_pressed_control = RICH4_STOCK_QUANTITY_CONTROL_NONE;
    state->trade_type = GAME_COMMAND_NONE;
    state->quantity = 0;
    state->maximum = 0;
    return RICH4_APP_STOCK_AUDIO_OK;
}

size_t rich4_app_stock_interaction_cursor_chunk_index(
    const GameSession *session, size_t default_chunk)
{
    const GameStockInteractionPresentationState *state =
        stock_state_const(session);
    return state != NULL && state->active && state->quantity_active
        ? 27u : default_chunk;
}
