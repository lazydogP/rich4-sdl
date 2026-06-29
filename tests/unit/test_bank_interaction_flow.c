#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/bank_interaction_flow.h"

int main(void)
{
    GameSession session;
    GameCommand command;
    unsigned char loan_mask_data[128 * 192];
    Rich4BankInteractionMask loan_mask;

    memset(&session, 0, sizeof(session));
    session.state.active = 1;
    session.state.player_count = 1;
    session.state.players[0].bank = 100;
    session.state.players[0].cash = 50;
    session.presentation.route_interaction.active = 1;
    session.presentation.route_interaction.accept.type =
        GAME_COMMAND_BANK_WITHDRAW;
    session.presentation.route_interaction.accept.player = 0;
    session.presentation.route_interaction.accept.turn_token = 9;
    assert(rich4_app_bank_interaction_right_button_up(&session, 100, &command));
    assert(command.type == GAME_COMMAND_END_INTERACTION);
    assert(command.player == 0);
    assert(command.turn_token == 9);
    assert(rich4_app_bank_interaction_hit(&session, NULL, 60 + 0x39, 71 + 0x31)
           == 0);
    assert(rich4_app_bank_interaction_control_enabled(&session, 8));
    assert(!rich4_app_bank_interaction_apply_control(
        &session, 8, 0, 100, &command));
    assert(session.presentation.bank_interaction.amount == 5);
    assert(rich4_app_bank_interaction_release(
        &session, 17, 0, 100, &command));
    assert(command.type == GAME_COMMAND_BANK_WITHDRAW);
    assert(command.value == 5);
    assert(command.turn_token == 9);
    assert(rich4_app_bank_transfer_key_control(RICH4_APP_KEY_DIGIT_5) == 8);

    memset(&session.presentation.bank_interaction, 0,
           sizeof(session.presentation.bank_interaction));
    session.presentation.route_interaction.accept.type =
        GAME_COMMAND_BANK_APPLY_LOAN;
    session.presentation.route_interaction.accept.player = 0;
    session.presentation.route_interaction.accept.turn_token = 11;
    session.presentation.bank_interaction.amount_entry = 1;
    session.presentation.bank_interaction.stage =
        RICH4_BANK_PRESENT_REPAY_AMOUNT;
    session.presentation.bank_interaction.mode = 1;
    session.state.players[0].bank = 10;
    session.state.players[0].cash = 0;
    session.state.players[0].loan = 100;
    assert(!rich4_app_bank_interaction_apply_control(
        &session, 2, 0, 200, &command));
    assert(session.presentation.bank_interaction.amount == 100);
    session.presentation.bank_interaction.mode = 0;
    session.presentation.bank_interaction.stage =
        RICH4_BANK_PRESENT_LOAN_AMOUNT;
    session.presentation.bank_interaction.amount = 0;
    session.state.players[0].bank = 100;
    session.state.players[0].cash = 50;
    session.state.players[0].loan = 0;
    memset(loan_mask_data, 0, sizeof(loan_mask_data));
    loan_mask_data[20 * 128 + 10] = 16;
    loan_mask.data = loan_mask_data;
    loan_mask.size = sizeof(loan_mask_data);
    loan_mask.available = 1;
    assert(rich4_app_bank_interaction_hit(
        &session, &loan_mask, 256 + 10, 144 + 20) == 16);
    assert(!rich4_app_bank_interaction_apply_control(
        &session, 13, 0, 200, &command));
    assert(session.presentation.bank_interaction.amount == 1);
    assert(!rich4_app_bank_interaction_apply_control(
        &session, 14, 0, 200, &command));
    assert(session.presentation.bank_interaction.amount == 12);
    assert(!rich4_app_bank_interaction_apply_control(
        &session, 16, 266 + 53, 200, &command));
    assert(session.presentation.bank_interaction.amount == 72);
    assert(!rich4_app_bank_interaction_apply_control(
        &session, 16, 266 + 107, 200, &command));
    assert(session.presentation.bank_interaction.amount == 150);
    session.presentation.bank_interaction.amount = 0;
    memset(&command, 0, sizeof(command));
    assert(!rich4_app_bank_interaction_apply_control(
        &session, 3, 0, 200, &command));
    assert(command.type == GAME_COMMAND_NONE);

    session.presentation.bank_interaction.amount = 12;
    assert(rich4_app_bank_interaction_release(
        &session, 3, 0, 200, &command));
    assert(command.type == GAME_COMMAND_BANK_APPLY_LOAN);
    assert(command.value == 12);
    assert(command.turn_token == 11);

    session.presentation.bank_interaction.amount_entry = 0;
    session.presentation.bank_interaction.special_page = 1;
    session.presentation.bank_interaction.stage = RICH4_BANK_PRESENT_READY;
    session.state.players[1].bank = 500;
    session.state.players[0].days_bank_no_loans = 1;
    assert(!rich4_app_bank_interaction_control_enabled(
        &session, RICH4_BANK_SPECIAL_BORROW));
    session.state.players[0].days_bank_no_loans = 0;
    assert(rich4_app_bank_interaction_control_enabled(
        &session, RICH4_BANK_SPECIAL_BORROW));
    assert(!rich4_app_bank_interaction_control_enabled(
        &session, RICH4_BANK_SPECIAL_REPAY));
    session.state.players[0].special_finance = 25;
    assert(rich4_app_bank_interaction_control_enabled(
        &session, RICH4_BANK_SPECIAL_REPAY));
    session.state.players[0].special_finance = 0;

    session.presentation.bank_interaction.amount_entry = 1;
    session.presentation.bank_interaction.special_page = 0;
    session.presentation.bank_interaction.stage =
        RICH4_BANK_PRESENT_LOAN_AMOUNT;
    session.presentation.bank_interaction.card_x = 440;
    session.presentation.bank_interaction.amount = 12;
    session.presentation.bank_interaction.pressed_control = 4;
    memset(&command, 0, sizeof(command));
    assert(!rich4_app_bank_interaction_right_button_up(
        &session, 200, &command));
    assert(command.type == GAME_COMMAND_NONE);
    assert(!session.presentation.bank_interaction.amount_entry);
    assert(session.presentation.bank_interaction.amount == 0);
    assert(session.presentation.bank_interaction.pressed_control == -1);
    assert(session.presentation.bank_interaction.stage ==
           RICH4_BANK_PRESENT_READY);
    assert(session.presentation.bank_interaction.card_x == 440);
    assert(session.presentation.bank_interaction.card_velocity == 0);

    session.presentation.bank_interaction.special_page = 1;
    session.presentation.bank_interaction.stage = RICH4_BANK_PRESENT_READY;
    session.presentation.bank_interaction.started = 100;
    memset(&command, 0, sizeof(command));
    assert(!rich4_app_bank_interaction_right_button_up(
        &session, 300, &command));
    assert(command.type == GAME_COMMAND_NONE);
    assert(!session.presentation.bank_interaction.special_page);
    assert(session.presentation.bank_interaction.started == 300);

    assert(rich4_app_bank_interaction_right_button_up(&session, 400, &command));
    assert(command.type == GAME_COMMAND_END_INTERACTION);
    assert(command.player == 0);

    session.presentation.route_interaction.active = 1;
    session.presentation.bank_interaction.stage = RICH4_BANK_PRESENT_READY;
    session.presentation.bank_interaction.special_page = 0;
    session.presentation.bank_interaction.amount_entry = 0;
    assert(rich4_app_bank_interaction_cursor_chunk_index(&session, 41) == 41);
    assert(!rich4_app_bank_interaction_release(
        &session, RICH4_BANK_LOAN_APPLY, 0, 500, &command));
    assert(session.presentation.bank_interaction.stage ==
           RICH4_BANK_PRESENT_LOAN_PROMPT);
    assert(!session.presentation.bank_interaction.amount_entry);
    assert(session.presentation.bank_interaction.card_velocity == -40);
    session.presentation.bank_interaction.amount_entry = 1;
    assert(rich4_app_bank_interaction_cursor_chunk_index(&session, 41) == 27);
    puts("app bank interaction flow: ok");
    return 0;
}
