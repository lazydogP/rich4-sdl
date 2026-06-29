#include <assert.h>
#include <stdio.h>

#include "core/game_command.h"

static GameDomainRef none(void)
{
    return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
}

static void valid_subject_command(GameCommandType type, GameDomainKind kind,
                                  int id, int value)
{
    GameCommand command = rich4_game_domain_command(
        type, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(kind, id), none(), value);
    assert(rich4_game_command_is_valid(&command));
    command.subject.id = -1;
    assert(!rich4_game_command_is_valid(&command));
}

int main(void)
{
    GameCommand command = rich4_game_command(GAME_COMMAND_ROLL_DICE,
                                             GAME_COMMAND_SOURCE_AI, 2, 0);
    assert(rich4_game_command_is_valid(&command));
    assert(command.type == GAME_COMMAND_ROLL_DICE);
    assert(command.source == GAME_COMMAND_SOURCE_AI && command.player == 2);
    command = rich4_game_command(GAME_COMMAND_SELECT_DICE_COUNT,
                                 GAME_COMMAND_SOURCE_HUMAN, 0, 3);
    assert(rich4_game_command_is_valid(&command));
    command.value = 4;
    assert(!rich4_game_command_is_valid(&command));
    command = rich4_game_command(GAME_COMMAND_CONFIRM,
                                 GAME_COMMAND_SOURCE_HUMAN, -1, 0);
    assert(!rich4_game_command_is_valid(&command));

    valid_subject_command(GAME_COMMAND_PURCHASE_PROPERTY, GAME_DOMAIN_LAND, 0, 0);
    valid_subject_command(GAME_COMMAND_DECLINE_PROPERTY_PURCHASE,
                          GAME_DOMAIN_FACILITY, GAME_MAX_MAP_PROPERTIES - 1, 0);
    valid_subject_command(GAME_COMMAND_UPGRADE_PROPERTY, GAME_DOMAIN_LAND, 1, 0);
    valid_subject_command(GAME_COMMAND_UPGRADE_PROPERTY, GAME_DOMAIN_FACILITY, 1, 4);
    valid_subject_command(GAME_COMMAND_DECLINE_PROPERTY_UPGRADE,
                          GAME_DOMAIN_FACILITY, 1, 0);
    valid_subject_command(GAME_COMMAND_USE_CARD, GAME_DOMAIN_CARD,
                          GAME_CARD_COUNT - 1, 0);
    valid_subject_command(GAME_COMMAND_USE_TOOL, GAME_DOMAIN_TOOL,
                          GAME_TOOL_COUNT - 1, 0);
    command = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 1),
        rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE,
                              GAME_MAX_MAP_ROUTE_NODES - 1), 0);
    assert(rich4_game_command_is_valid(&command));
    valid_subject_command(GAME_COMMAND_BUY_STOCK, GAME_DOMAIN_STOCK,
                          GAME_STOCK_COUNT - 1, 1);
    valid_subject_command(GAME_COMMAND_SELL_STOCK, GAME_DOMAIN_STOCK, 0, 1);
    valid_subject_command(GAME_COMMAND_SELL_ASSET, GAME_DOMAIN_LAND, 0, 1);
    valid_subject_command(GAME_COMMAND_SAVE_GAME, GAME_DOMAIN_SAVE_SLOT, 0, 0);
    valid_subject_command(GAME_COMMAND_LOAD_GAME, GAME_DOMAIN_SAVE_SLOT,
                          GAME_SAVE_SLOT_COUNT - 1, 0);

    command = rich4_game_domain_command(
        GAME_COMMAND_SELECT_TARGET, GAME_COMMAND_SOURCE_HUMAN, 0, none(),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, GAME_MAX_PLAYERS - 1), 0);
    assert(rich4_game_command_is_valid(&command));
    command.target.id = GAME_MAX_PLAYERS;
    assert(!rich4_game_command_is_valid(&command));

    command = rich4_game_domain_command(
        GAME_COMMAND_BUY_STOCK, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0), none(), 0);
    assert(!rich4_game_command_is_valid(&command));
    command.value = 1;
    command.flags = 1;
    assert(!rich4_game_command_is_valid(&command));
    command = rich4_game_domain_command(
        GAME_COMMAND_UPGRADE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0), none(), 5);
    assert(!rich4_game_command_is_valid(&command));

    command = rich4_game_command_confirm(GAME_COMMAND_SOURCE_HUMAN, 0);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_cancel(GAME_COMMAND_SOURCE_HUMAN, 0);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_roll_dice(GAME_COMMAND_SOURCE_HUMAN, 0);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_select_dice_count(
        GAME_COMMAND_SOURCE_HUMAN, 0, 2);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_property(
        GAME_COMMAND_UPGRADE_PROPERTY, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0), 2);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_target(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_use_card(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 0), 3, 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_use_card_on_target(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_CARD, 0),
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1), 2, 3, 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_use_tool(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_TOOL, 1), 0,
        rich4_game_domain_ref(GAME_DOMAIN_ROUTE_NODE, 2), none());
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_stock(
        GAME_COMMAND_BUY_STOCK, GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_sell_asset(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command(
        GAME_COMMAND_BANK_WITHDRAW, GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_save_load(
        GAME_COMMAND_SAVE_GAME, GAME_COMMAND_SOURCE_HUMAN, 0, 0);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_purchase_property(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_decline_property_purchase(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_upgrade_property(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_decline_property_upgrade(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_target(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_buy_stock(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_sell_stock(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_withdraw(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_deposit(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_apply_loan(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_repay_loan(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_special_borrow(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_special_repay(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_save_game(
        GAME_COMMAND_SOURCE_HUMAN, 0, 0);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_load_game(
        GAME_COMMAND_SOURCE_HUMAN, 0, GAME_SAVE_SLOT_COUNT - 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_with_turn_token(command, 42);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_purchase_property(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_decline_property_purchase(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_LAND, 0));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_upgrade_property(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_decline_property_upgrade(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_FACILITY, 0));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_target(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_PLAYER, 1));
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_buy_stock(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_sell_stock(
        GAME_COMMAND_SOURCE_HUMAN, 0,
        rich4_game_domain_ref(GAME_DOMAIN_STOCK, 0), 1);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_withdraw(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_deposit(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_apply_loan(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_repay_loan(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_special_borrow(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_bank_special_repay(
        GAME_COMMAND_SOURCE_HUMAN, 0, 20);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_save_game(
        GAME_COMMAND_SOURCE_HUMAN, 0, 0);
    assert(rich4_game_command_is_valid(&command));
    command = rich4_game_command_load_game(
        GAME_COMMAND_SOURCE_HUMAN, 0, GAME_SAVE_SLOT_COUNT - 1);
    assert(rich4_game_command_is_valid(&command));
    puts("game command model: ok");
    return 0;
}
