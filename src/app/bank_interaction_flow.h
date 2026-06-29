#ifndef RICH4_APP_BANK_INTERACTION_FLOW_H
#define RICH4_APP_BANK_INTERACTION_FLOW_H

#include <stddef.h>
#include <stdint.h>

#include "app/app_command.h"
#include "app/game_session.h"
#include "core/game_command.h"

/* App-owned bank interaction semantics. The mask is borrowed platform asset
 * data; the session owns all mutable interaction state. Evidence/tests: A5,
 * A6, test_bank_interaction_flow. */
enum {
    RICH4_BANK_LOAN_EXIT = 100,
    RICH4_BANK_LOAN_APPLY,
    RICH4_BANK_LOAN_REPAY,
    RICH4_BANK_LOAN_MANAGER,
    RICH4_BANK_SPECIAL_BORROW = 110,
    RICH4_BANK_SPECIAL_REPAY,
    RICH4_BANK_SPECIAL_BACK
};

typedef struct Rich4BankInteractionMask {
    const unsigned char *data;
    size_t size;
    int available;
} Rich4BankInteractionMask;

int rich4_app_bank_interaction_is_transfer(const GameSession *session);
int rich4_app_bank_interaction_is_loan_page(const GameSession *session);
int rich4_app_bank_interaction_is_amount_entry(const GameSession *session);
int rich4_app_bank_interaction_is_special_page(const GameSession *session);
int rich4_app_bank_interaction_pressed_control(const GameSession *session);
void rich4_app_bank_interaction_set_pressed_control(
    GameSession *session, int control);
int rich4_app_bank_interaction_hit(
    const GameSession *session, const Rich4BankInteractionMask *mask,
    int x, int y);
int rich4_app_bank_transfer_key_control(Rich4AppKey key);
int rich4_app_bank_loan_key_control(Rich4AppKey key);
int rich4_app_bank_interaction_control_enabled(
    const GameSession *session, int control);
int rich4_app_bank_interaction_apply_control(
    GameSession *session, int control, int pointer_x, uint32_t now,
    GameCommand *out_command);
int rich4_app_bank_interaction_release(
    GameSession *session, int control, int pointer_x, uint32_t now,
    GameCommand *out_command);
int rich4_app_bank_interaction_cancel(
    GameSession *session, uint32_t now, GameCommand *out_command);
/* Physical WM_RBUTTONUP equivalent.  It closes only the current bank layer:
 * amount entry -> loan page, special page -> normal page, and the ready loan
 * or transfer page -> the outer interaction. */
int rich4_app_bank_interaction_right_button_up(
    GameSession *session, uint32_t now, GameCommand *out_command);
size_t rich4_app_bank_interaction_cursor_chunk_index(
    const GameSession *session, size_t default_chunk);

#endif
