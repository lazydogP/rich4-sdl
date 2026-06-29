#include "app/bank_interaction_flow.h"
#include "app/bank_flow.h"
#include "app/stock_interaction_flow.h"

#include "core/bank.h"
#include "core/route_event.h"
#include "core/route_event_executor.h"

static const GameRouteInteractionPresentationState *bank_interaction(
    const GameSession *session)
{
    return session == NULL ? NULL : &session->presentation.route_interaction;
}

static int bank_player(const GameSession *session)
{
    const GameRouteInteractionPresentationState *interaction =
        bank_interaction(session);
    if (interaction == NULL || interaction->accept.player < 0 ||
        interaction->accept.player >= GAME_MAX_PLAYERS)
        return -1;
    return interaction->accept.player;
}

static int bank_is_transfer(const GameSession *session)
{
    const GameRouteInteractionPresentationState *interaction =
        bank_interaction(session);
    return interaction != NULL && interaction->active &&
        interaction->accept.type == GAME_COMMAND_BANK_WITHDRAW;
}

static int bank_is_loan_page(const GameSession *session)
{
    const GameRouteInteractionPresentationState *interaction =
        bank_interaction(session);
    return interaction != NULL && interaction->active &&
        interaction->accept.type == GAME_COMMAND_BANK_APPLY_LOAN;
}

int rich4_app_bank_interaction_is_transfer(const GameSession *session)
{
    const GameRouteInteractionPresentationState *interaction =
        bank_interaction(session);
    return interaction != NULL && interaction->active &&
        interaction->accept.type == GAME_COMMAND_BANK_WITHDRAW;
}

int rich4_app_bank_interaction_is_loan_page(const GameSession *session)
{
    const GameRouteInteractionPresentationState *interaction =
        bank_interaction(session);
    return interaction != NULL && interaction->active &&
        interaction->accept.type == GAME_COMMAND_BANK_APPLY_LOAN;
}

int rich4_app_bank_interaction_is_amount_entry(const GameSession *session)
{
    return bank_is_loan_page(session) &&
        session->presentation.bank_interaction.amount_entry;
}

int rich4_app_bank_interaction_is_special_page(const GameSession *session)
{
    return bank_is_loan_page(session) &&
        session->presentation.bank_interaction.special_page;
}

int rich4_app_bank_interaction_pressed_control(const GameSession *session)
{
    return session == NULL
        ? -1 : session->presentation.bank_interaction.pressed_control;
}

void rich4_app_bank_interaction_set_pressed_control(
    GameSession *session, int control)
{
    if (bank_is_transfer(session) || bank_is_loan_page(session))
        session->presentation.bank_interaction.pressed_control = control;
}

static int bank_transfer_hit(int x, int y)
{
    static const int rects[18][4] = {
        {0x39,0x31,0x89,0x5a}, {0x8b,0x31,0xdb,0x5a},
        {0xdd,0x31,0x108,0x5a}, {0x35,0x89,0x10c,0xa6},
        {0x3a,0xd3,0x5b,0xe4}, {0x61,0xd3,0x82,0xe4},
        {0x88,0xd3,0xa9,0xe4}, {0x3a,0xe6,0x5b,0xf7},
        {0x61,0xe6,0x82,0xf7}, {0x88,0xe6,0xa9,0xf7},
        {0x3a,0xf9,0x5b,0x10a}, {0x61,0xf9,0x82,0x10a},
        {0x88,0xf9,0xa9,0x10a}, {0x3a,0x10c,0x5b,0x11d},
        {0x61,0x10c,0x82,0x11d}, {0x88,0x10c,0xa9,0x11d},
        {0xb7,0xe9,0xe8,0x102}, {0xaf,0x104,0xe8,0x11d}
    };
    int i;
    x -= 60;
    y -= 71;
    for (i = 0; i < 18; i++)
        if (x >= rects[i][0] && x <= rects[i][2] &&
            y >= rects[i][1] && y <= rects[i][3])
            return i;
    return -1;
}

static int bank_loan_hit(const GameSession *session,
                         const Rich4BankInteractionMask *mask,
                         int x, int y)
{
    const GameBankInteractionPresentationState *bank;
    if (session == NULL || mask == NULL) return -1;
    bank = &session->presentation.bank_interaction;
    if (bank->amount_entry) {
        Rich4StockQuantityMask quantity_mask;
        int control;
        quantity_mask.data = mask->data;
        quantity_mask.size = mask->size;
        quantity_mask.available = mask->available;
        control = rich4_app_stock_quantity_control_at_mask(
            &quantity_mask, x, y);
        return control >= 2 && control <= 16 ? control : -1;
    }
    if (bank->special_page) {
        if (x >= 11 && x <= 125 && y >= 305 && y <= 345)
            return RICH4_BANK_SPECIAL_BORROW;
        if (x >= 11 && x <= 125 && y >= 362 && y <= 402)
            return RICH4_BANK_SPECIAL_REPAY;
        if (x >= 11 && x <= 91 && y >= 419 && y <= 459)
            return RICH4_BANK_SPECIAL_BACK;
        return -1;
    }
    if (x >= 548 && x <= 628 && y >= 431 && y <= 471)
        return RICH4_BANK_LOAN_EXIT;
    if (x >= 282 && x <= 408 && y >= 324 && y <= 366)
        return RICH4_BANK_LOAN_APPLY;
    if (x >= 470 && x <= 590 && y >= 326 && y <= 366)
        return RICH4_BANK_LOAN_REPAY;
    if (x >= 268 && x <= 591 && y >= 51 && y <= 273)
        return RICH4_BANK_LOAN_MANAGER;
    return -1;
}

static void bank_set_special_page(
    GameBankInteractionPresentationState *bank, int special_page,
    uint32_t now)
{
    bank->special_page = special_page;
    bank->face_chunk = -1;
    bank->mouth_chunk = -1;
    bank->mouth_ticks = 0;
    bank->eye_mode = 0;
    bank->eye_phase = 0;
    bank->eye_current = 0;
    bank->animation_page = special_page;
    bank->stage = RICH4_BANK_PRESENT_READY;
    bank->speech = NULL;
    rich4_character_speech_snapshot_clear(&bank->speech_snapshot);
    bank->speech_pending = 0;
    bank->anim_tick = now;
}

int rich4_app_bank_interaction_hit(
    const GameSession *session, const Rich4BankInteractionMask *mask,
    int x, int y)
{
    if (bank_is_transfer(session)) return bank_transfer_hit(x, y);
    if (bank_is_loan_page(session)) return bank_loan_hit(session, mask, x, y);
    return -1;
}

int rich4_app_bank_transfer_key_control(Rich4AppKey key)
{
    if (key >= RICH4_APP_KEY_DIGIT_1 && key <= RICH4_APP_KEY_DIGIT_3)
        return 10 + (int)(key - RICH4_APP_KEY_DIGIT_1);
    if (key >= RICH4_APP_KEY_DIGIT_4 && key <= RICH4_APP_KEY_DIGIT_6)
        return 7 + (int)(key - RICH4_APP_KEY_DIGIT_4);
    if (key >= RICH4_APP_KEY_DIGIT_7 && key <= RICH4_APP_KEY_DIGIT_9)
        return 4 + (int)(key - RICH4_APP_KEY_DIGIT_7);
    if (key >= RICH4_APP_KEY_KP_1 && key <= RICH4_APP_KEY_KP_3)
        return 10 + (int)(key - RICH4_APP_KEY_KP_1);
    if (key >= RICH4_APP_KEY_KP_4 && key <= RICH4_APP_KEY_KP_6)
        return 7 + (int)(key - RICH4_APP_KEY_KP_4);
    if (key >= RICH4_APP_KEY_KP_7 && key <= RICH4_APP_KEY_KP_9)
        return 4 + (int)(key - RICH4_APP_KEY_KP_7);
    if (key == RICH4_APP_KEY_DIGIT_0 || key == RICH4_APP_KEY_KP_0) return 14;
    if (key == RICH4_APP_KEY_C) return 13;
    if (key == RICH4_APP_KEY_BACKSPACE) return 15;
    if (key == RICH4_APP_KEY_M) return 16;
    if (key == RICH4_APP_KEY_RETURN || key == RICH4_APP_KEY_KP_ENTER) return 17;
    if (key == RICH4_APP_KEY_H) return 3;
    return -1;
}

int rich4_app_bank_loan_key_control(Rich4AppKey key)
{
    if (key >= RICH4_APP_KEY_DIGIT_7 && key <= RICH4_APP_KEY_DIGIT_9)
        return 7 + (int)(key - RICH4_APP_KEY_DIGIT_7);
    if (key >= RICH4_APP_KEY_DIGIT_4 && key <= RICH4_APP_KEY_DIGIT_6)
        return 10 + (int)(key - RICH4_APP_KEY_DIGIT_4);
    if (key >= RICH4_APP_KEY_DIGIT_1 && key <= RICH4_APP_KEY_DIGIT_3)
        return 13 + (int)(key - RICH4_APP_KEY_DIGIT_1);
    if (key >= RICH4_APP_KEY_KP_7 && key <= RICH4_APP_KEY_KP_9)
        return 7 + (int)(key - RICH4_APP_KEY_KP_7);
    if (key >= RICH4_APP_KEY_KP_4 && key <= RICH4_APP_KEY_KP_6)
        return 10 + (int)(key - RICH4_APP_KEY_KP_4);
    if (key >= RICH4_APP_KEY_KP_1 && key <= RICH4_APP_KEY_KP_3)
        return 13 + (int)(key - RICH4_APP_KEY_KP_1);
    if (key == RICH4_APP_KEY_DIGIT_0 || key == RICH4_APP_KEY_KP_0) return 5;
    if (key == RICH4_APP_KEY_C) return 4;
    if (key == RICH4_APP_KEY_BACKSPACE) return 6;
    if (key == RICH4_APP_KEY_M) return 2;
    if (key == RICH4_APP_KEY_RETURN || key == RICH4_APP_KEY_KP_ENTER) return 3;
    return -1;
}

static int bank_transfer_control_enabled(const GameSession *session, int control)
{
    const GameRouteInteractionPresentationState *interaction =
        bank_interaction(session);
    if (interaction == NULL || !interaction->active || control < 0 ||
        control >= 18 || !bank_is_transfer(session)) return 0;
    return !((control == 0 || control == 1) &&
             control == session->presentation.bank_interaction.mode);
}

static int bank_loan_control_enabled(const GameSession *session, int control)
{
    const GameRouteInteractionPresentationState *interaction =
        bank_interaction(session);
    const GamePlayerState *player;
    const RouteEvent *event;
    int player_index = bank_player(session);
    if (interaction == NULL || !interaction->active || player_index < 0 ||
        !bank_is_loan_page(session) || control < 0 ||
        player_index >= session->state.player_count) return 0;
    player = &session->state.players[player_index];
    if (session->presentation.bank_interaction.amount_entry)
        return control >= 2 && control <= 16;
    if (session->presentation.bank_interaction.stage !=
            RICH4_BANK_PRESENT_READY)
        return 0;
    if (control == RICH4_BANK_LOAN_APPLY)
        return player->days_bank_no_loans == 0 &&
            rich4_bank_loan_available(&session->state, player_index) > 0;
    if (control == RICH4_BANK_LOAN_REPAY)
        return player->loan != 0;
    if (control == RICH4_BANK_SPECIAL_BORROW)
        return player->days_bank_no_loans == 0 &&
            rich4_bank_special_available(&session->state, player_index) > 0;
    if (control == RICH4_BANK_SPECIAL_REPAY)
        return player->special_finance != 0;
    if (control == RICH4_BANK_LOAN_MANAGER) {
        event = rich4_route_event_executor_current(&session->flow.route_events);
        return event != NULL && event->snapshot.property_owner == player_index;
    }
    return 1;
}

int rich4_app_bank_interaction_control_enabled(
    const GameSession *session, int control)
{
    return bank_is_transfer(session)
        ? bank_transfer_control_enabled(session, control)
        : bank_loan_control_enabled(session, control);
}

static int bank_available(const GameSession *session, int transfer_page,
                          int player_index)
{
    const GamePlayerState *player = &session->state.players[player_index];
    int mode = session->presentation.bank_interaction.mode;
    int available;
    if (transfer_page && mode == 0) available = player->bank;
    else if (transfer_page) available = player->cash;
    else if (mode == 0) available = rich4_bank_loan_available(
        &session->state, player_index);
    else if (mode == 1) available = player->loan;
    else if (mode == 2) available = rich4_bank_special_available(
        &session->state, player_index);
    else available = player->special_finance;
    return available;
}

static GameCommand bank_command(const GameSession *session, GameCommandType type,
                                int player, int amount)
{
    GameCommand command = rich4_game_command_bank_for_type(
        type, GAME_COMMAND_SOURCE_HUMAN, player, amount);
    return rich4_game_command_with_turn_token(
        command, session->presentation.route_interaction.accept.turn_token);
}

static GameCommand bank_end_command(const GameSession *session, int player)
{
    GameCommand command = rich4_game_command_end_interaction(
        GAME_COMMAND_SOURCE_HUMAN, player);
    return rich4_game_command_with_turn_token(
        command, session->presentation.route_interaction.accept.turn_token);
}

int rich4_app_bank_interaction_apply_control(
    GameSession *session, int control, int pointer_x, uint32_t now,
    GameCommand *out_command)
{
    static const int digits[9] = {7,8,9,4,5,6,1,2,3};
    const RouteEvent *event;
    int player_index;
    int transfer_page;
    int available;
    int amount;
    if (session == NULL || out_command == NULL ||
        !rich4_app_bank_interaction_control_enabled(session, control))
        return 0;
    player_index = bank_player(session);
    transfer_page = bank_is_transfer(session);
    event = rich4_route_event_executor_current(&session->flow.route_events);
    if (!transfer_page && !session->presentation.bank_interaction.amount_entry) {
        if (control == RICH4_BANK_LOAN_EXIT) {
            *out_command = bank_end_command(session, player_index);
            return 1;
        }
        if (control == RICH4_BANK_LOAN_APPLY ||
            control == RICH4_BANK_LOAN_REPAY) {
            session->presentation.bank_interaction.mode =
                control == RICH4_BANK_LOAN_APPLY ? 0 : 1;
            (void)rich4_bank_flow_begin_amount_prompt(
                session, session->presentation.bank_interaction.mode, now);
            return 0;
        }
        if (control == RICH4_BANK_SPECIAL_BORROW ||
            control == RICH4_BANK_SPECIAL_REPAY) {
            session->presentation.bank_interaction.mode =
                control == RICH4_BANK_SPECIAL_BORROW ? 2 : 3;
            session->presentation.bank_interaction.amount = 0;
            session->presentation.bank_interaction.amount_entry = 1;
            return 0;
        }
        if (control == RICH4_BANK_LOAN_MANAGER) {
            bank_set_special_page(
                &session->presentation.bank_interaction, 1, now);
            session->presentation.bank_interaction.started = now;
            return 0;
        }
        if (control == RICH4_BANK_SPECIAL_BACK) {
            bank_set_special_page(
                &session->presentation.bank_interaction, 0, now);
            session->presentation.bank_interaction.started = now;
            return 0;
        }
    }
    if (control == 0 || control == 1) {
        session->presentation.bank_interaction.mode = control;
        session->presentation.bank_interaction.amount = 0;
        return 0;
    }
    if (control == 2 &&
        (transfer_page || !session->presentation.bank_interaction.amount_entry)) {
        if (!transfer_page && event != NULL &&
            event->snapshot.property_owner == player_index) {
            session->presentation.bank_interaction.mode =
                session->presentation.bank_interaction.mode == 2 ? 3 : 2;
            session->presentation.bank_interaction.amount = 0;
        }
        return 0;
    }
    available = bank_available(session, transfer_page, player_index);
    if (available < 0) available = 0;
    amount = session->presentation.bank_interaction.amount;
    if (transfer_page && control == 3) {
        int position = pointer_x - 60 - 0x3a;
        int width = 0xcc;
        if (position < 0) amount = 0;
        else if (position >= width) amount = available;
        else {
            amount = (position / 6 + 1) * (available / 34 + 1);
            if (amount > available) amount = available;
        }
    } else if (transfer_page && control >= 4 && control <= 12) {
        long long appended = (long long)amount * 10 + digits[control - 4];
        amount = appended > available ? available : (int)appended;
    } else if (transfer_page && control == 13) amount = 0;
    else if (transfer_page && control == 14) {
        if (amount != 0) {
            long long appended = (long long)amount * 10;
            amount = appended > available ? available : (int)appended;
        }
    } else if (transfer_page && control == 15) amount /= 10;
    else if (transfer_page && control == 16) amount = available;
    else if (!transfer_page && control == 2) amount = available;
    else if (!transfer_page && control == 3) {
        GameCommandType type = session->presentation.bank_interaction.mode == 0
            ? GAME_COMMAND_BANK_APPLY_LOAN
            : session->presentation.bank_interaction.mode == 1
                ? GAME_COMMAND_BANK_REPAY_LOAN
                : session->presentation.bank_interaction.mode == 2
                    ? GAME_COMMAND_BANK_SPECIAL_BORROW
                    : GAME_COMMAND_BANK_SPECIAL_REPAY;
        /* sub_452C02 returns zero for an empty quantity.  Its bank callers
         * only commit and leave the amount dialog for a non-zero result. */
        if (amount <= 0) return 0;
        *out_command = bank_command(session, type, player_index, amount);
        return 1;
    } else if (!transfer_page && control == 4) amount = 0;
    else if (!transfer_page && control == 5) {
        if (amount != 0) {
            long long appended = (long long)amount * 10;
            amount = appended > available ? available : (int)appended;
        }
    } else if (!transfer_page && control == 6) amount /= 10;
    else if (!transfer_page && control >= 7 && control <= 15) {
        long long appended =
            (long long)amount * 10 + digits[control - 7];
        amount = appended > available ? available : (int)appended;
    } else if (!transfer_page && control == 16) {
        int selected = rich4_app_stock_quantity_from_bar(
            available, pointer_x);
        if (selected >= 0) amount = selected;
    } else if (transfer_page && control == 17) {
        GameCommandType type = session->presentation.bank_interaction.mode == 0
            ? GAME_COMMAND_BANK_WITHDRAW : GAME_COMMAND_BANK_DEPOSIT;
        *out_command = bank_command(session, type, player_index, amount);
        return 1;
    }
    session->presentation.bank_interaction.amount = amount;
    (void)now;
    return 0;
}

int rich4_app_bank_interaction_release(
    GameSession *session, int control, int pointer_x, uint32_t now,
    GameCommand *out_command)
{
    if (session == NULL || out_command == NULL) return 0;
    if (bank_is_transfer(session)) {
        if (control == 0 || control == 1 || control == 3) return 0;
        if (control == 2) {
            *out_command = bank_end_command(session, bank_player(session));
            return 1;
        }
    }
    else if (bank_is_loan_page(session)) {
        if (!session->presentation.bank_interaction.amount_entry) {
            if (control == RICH4_BANK_LOAN_EXIT) {
                *out_command = bank_end_command(session, bank_player(session));
                return 1;
            }
            if (control == RICH4_BANK_LOAN_APPLY ||
                control == RICH4_BANK_LOAN_REPAY) {
                session->presentation.bank_interaction.mode =
                    control == RICH4_BANK_LOAN_APPLY ? 0 : 1;
                (void)rich4_bank_flow_begin_amount_prompt(
                    session, session->presentation.bank_interaction.mode, now);
                return 0;
            }
            if (control == RICH4_BANK_SPECIAL_BORROW ||
                control == RICH4_BANK_SPECIAL_REPAY) {
                session->presentation.bank_interaction.mode =
                    control == RICH4_BANK_SPECIAL_BORROW ? 2 : 3;
                session->presentation.bank_interaction.amount = 0;
                session->presentation.bank_interaction.amount_entry = 1;
                return 0;
            }
            if (control == RICH4_BANK_LOAN_MANAGER) {
                bank_set_special_page(
                    &session->presentation.bank_interaction, 1, now);
                session->presentation.bank_interaction.started = now;
                return 0;
            }
            if (control == RICH4_BANK_SPECIAL_BACK) {
                bank_set_special_page(
                    &session->presentation.bank_interaction, 0, now);
                session->presentation.bank_interaction.started = now;
                return 0;
            }
            return 0;
        }
        if (control == 2) {
            session->presentation.bank_interaction.amount =
                bank_available(session, 0, bank_player(session));
            return 0;
        }
    }
    return rich4_app_bank_interaction_apply_control(
        session, control, pointer_x, now, out_command);
}

int rich4_app_bank_interaction_cancel(
    GameSession *session, uint32_t now, GameCommand *out_command)
{
    if (session == NULL || out_command == NULL) return 0;
    if (bank_is_transfer(session)) {
        *out_command = bank_end_command(session, bank_player(session));
        return 1;
    }
    if (!bank_is_loan_page(session)) return 0;
    if (session->presentation.bank_interaction.amount_entry) {
        if (session->presentation.bank_interaction.special_page) {
            session->presentation.bank_interaction.amount_entry = 0;
            session->presentation.bank_interaction.amount = 0;
            session->presentation.bank_interaction.pressed_control = -1;
        } else {
            rich4_bank_flow_cancel_amount_entry(session, now);
        }
        return 0;
    }
    if (session->presentation.bank_interaction.special_page) {
        bank_set_special_page(
            &session->presentation.bank_interaction, 0, now);
        session->presentation.bank_interaction.started = now;
        return 0;
    }
    *out_command = bank_end_command(session, bank_player(session));
    return 1;
}

int rich4_app_bank_interaction_right_button_up(
    GameSession *session, uint32_t now, GameCommand *out_command)
{
    GameBankInteractionPresentationState *bank;
    if (session == NULL || out_command == NULL) return 0;
    if (bank_is_transfer(session)) {
        *out_command = bank_end_command(session, bank_player(session));
        return 1;
    }
    if (!bank_is_loan_page(session)) return 0;
    bank = &session->presentation.bank_interaction;
    if (bank->amount_entry) {
        /* sub_434492 returns the 0x402 sentinel to sub_435062.  The parent
         * callback resumes in state 4; it does not run the whole-page card
         * retract used by the keyboard/virtual-cancel path. */
        bank->amount_entry = 0;
        bank->amount = 0;
        bank->pressed_control = -1;
        bank->speech = NULL;
        rich4_character_speech_snapshot_clear(&bank->speech_snapshot);
        bank->speech_pending = 0;
        bank->success_close = 0;
        bank->card_velocity = 0;
        bank->stage = RICH4_BANK_PRESENT_READY;
        bank->anim_tick = now;
        return 0;
    }
    if (bank->special_page) {
        bank_set_special_page(bank, 0, now);
        bank->started = now;
        return 0;
    }
    *out_command = bank_end_command(session, bank_player(session));
    return 1;
}

size_t rich4_app_bank_interaction_cursor_chunk_index(
    const GameSession *session, size_t default_chunk)
{
    return bank_is_loan_page(session) &&
        session->presentation.bank_interaction.amount_entry
        ? 27u : default_chunk;
}
