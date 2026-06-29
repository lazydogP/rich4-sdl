#include "core/game_command.h"

#include <stdint.h>
#include <string.h>

GameCommand rich4_game_command(GameCommandType type,
                               GameCommandSource source,
                               int player,
                               int value)
{
    GameCommand command;
    memset(&command, 0, sizeof(command));
    command.type = type;
    command.source = source;
    command.player = player;
    command.value = value;
    command.subject = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    command.target = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    command.target2 = rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    return command;
}

GameCommand rich4_game_domain_command(GameCommandType type,
                                      GameCommandSource source,
                                      int player,
                                      GameDomainRef subject,
                                      GameDomainRef target,
                                      int value)
{
    GameCommand command = rich4_game_command(type, source, player, value);
    command.subject = subject;
    command.target = target;
    return command;
}

GameCommand rich4_game_command_confirm(GameCommandSource source, int player)
{
    return rich4_game_command(GAME_COMMAND_CONFIRM, source, player, 0);
}

GameCommand rich4_game_command_cancel(GameCommandSource source, int player)
{
    return rich4_game_command(GAME_COMMAND_CANCEL, source, player, 0);
}

GameCommand rich4_game_command_roll_dice(GameCommandSource source, int player)
{
    return rich4_game_command(GAME_COMMAND_ROLL_DICE, source, player, 0);
}

GameCommand rich4_game_command_select_dice_count(
    GameCommandSource source, int player, int count)
{
    return rich4_game_command(
        GAME_COMMAND_SELECT_DICE_COUNT, source, player, count);
}

GameCommand rich4_game_command_end_interaction(
    GameCommandSource source, int player)
{
    return rich4_game_command(GAME_COMMAND_END_INTERACTION, source, player, 0);
}

GameCommand rich4_game_command_purchase_property(
    GameCommandSource source, int player, GameDomainRef property)
{
    return rich4_game_command_property(
        GAME_COMMAND_PURCHASE_PROPERTY, source, player, property, 0);
}

GameCommand rich4_game_command_decline_property_purchase(
    GameCommandSource source, int player, GameDomainRef property)
{
    return rich4_game_command_property(
        GAME_COMMAND_DECLINE_PROPERTY_PURCHASE, source, player, property, 0);
}

GameCommand rich4_game_command_upgrade_property(
    GameCommandSource source, int player, GameDomainRef property, int level)
{
    return rich4_game_command_property(
        GAME_COMMAND_UPGRADE_PROPERTY, source, player, property, level);
}

GameCommand rich4_game_command_decline_property_upgrade(
    GameCommandSource source, int player, GameDomainRef property)
{
    return rich4_game_command_property(
        GAME_COMMAND_DECLINE_PROPERTY_UPGRADE, source, player, property, 0);
}

GameCommand rich4_game_command_property(
    GameCommandType type, GameCommandSource source, int player,
    GameDomainRef property, int level)
{
    return rich4_game_domain_command(type, source, player,
                                     property,
                                     rich4_game_domain_ref(GAME_DOMAIN_NONE, -1),
                                     level);
}

GameCommand rich4_game_command_target(
    GameCommandSource source, int player, GameDomainRef target)
{
    return rich4_game_domain_command(
        GAME_COMMAND_SELECT_TARGET, source, player,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), target, 0);
}

GameCommand rich4_game_command_use_card(
    GameCommandSource source, int player, GameDomainRef card,
    int value2, unsigned int flags)
{
    return rich4_game_command_use_card_on_target(
        source, player, card,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0, value2, flags);
}

GameCommand rich4_game_command_use_card_on_target(
    GameCommandSource source, int player, GameDomainRef card,
    GameDomainRef target, int value, int value2, unsigned int flags)
{
    GameCommand command = rich4_game_domain_command(
        GAME_COMMAND_USE_CARD, source, player, card, target, value);
    command.value2 = value2;
    command.flags = flags;
    return command;
}

GameCommand rich4_game_command_use_tool(
    GameCommandSource source, int player, GameDomainRef tool, int value,
    GameDomainRef target, GameDomainRef target2)
{
    GameCommand command = rich4_game_domain_command(
        GAME_COMMAND_USE_TOOL, source, player, tool, target, value);
    command.target2 = target2;
    return command;
}

GameCommand rich4_game_command_stock(
    GameCommandType type, GameCommandSource source, int player,
    GameDomainRef stock, int quantity)
{
    return rich4_game_domain_command(
        type, source, player, stock,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), quantity);
}

GameCommand rich4_game_command_buy_stock(
    GameCommandSource source, int player, GameDomainRef stock, int quantity)
{
    return rich4_game_command_stock(
        GAME_COMMAND_BUY_STOCK, source, player, stock, quantity);
}

GameCommand rich4_game_command_sell_stock(
    GameCommandSource source, int player, GameDomainRef stock, int quantity)
{
    return rich4_game_command_stock(
        GAME_COMMAND_SELL_STOCK, source, player, stock, quantity);
}

GameCommand rich4_game_command_sell_asset(
    GameCommandSource source, int player, GameDomainRef asset, int amount)
{
    return rich4_game_domain_command(
        GAME_COMMAND_SELL_ASSET, source, player, asset,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), amount);
}

GameCommand rich4_game_command_create_market_listing(
    GameCommandSource source, int player, GameDomainRef item,
    int ask_price, int quantity)
{
    GameCommand command = rich4_game_domain_command(
        GAME_COMMAND_CREATE_MARKET_LISTING, source, player, item,
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), ask_price);
    command.value2 = quantity;
    return command;
}

GameCommand rich4_game_command_cancel_market_listing(
    GameCommandSource source, int player, int slot)
{
    int listing_id = player * GAME_MARKET_LISTING_SLOT_COUNT + slot;
    return rich4_game_domain_command(
        GAME_COMMAND_CANCEL_MARKET_LISTING, source, player,
        rich4_game_domain_ref(GAME_DOMAIN_MARKET_LISTING, listing_id),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
}

GameCommand rich4_game_command_buy_market_listing(
    GameCommandSource source, int player, int seller, int slot)
{
    int listing_id = seller * GAME_MARKET_LISTING_SLOT_COUNT + slot;
    return rich4_game_domain_command(
        GAME_COMMAND_BUY_MARKET_LISTING, source, player,
        rich4_game_domain_ref(GAME_DOMAIN_MARKET_LISTING, listing_id),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
}

GameCommand rich4_game_command_reprice_market_listing(
    GameCommandSource source, int player, int slot, int ask_price)
{
    int listing_id = player * GAME_MARKET_LISTING_SLOT_COUNT + slot;
    return rich4_game_domain_command(
        GAME_COMMAND_REPRICE_MARKET_LISTING, source, player,
        rich4_game_domain_ref(GAME_DOMAIN_MARKET_LISTING, listing_id),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), ask_price);
}

GameCommand rich4_game_command_bank_for_type(
    GameCommandType type, GameCommandSource source, int player, int amount)
{
    switch (type) {
    case GAME_COMMAND_BANK_WITHDRAW:
        return rich4_game_command_bank_withdraw(source, player, amount);
    case GAME_COMMAND_BANK_DEPOSIT:
        return rich4_game_command_bank_deposit(source, player, amount);
    case GAME_COMMAND_BANK_APPLY_LOAN:
        return rich4_game_command_bank_apply_loan(source, player, amount);
    case GAME_COMMAND_BANK_REPAY_LOAN:
        return rich4_game_command_bank_repay_loan(source, player, amount);
    case GAME_COMMAND_BANK_SPECIAL_BORROW:
        return rich4_game_command_bank_special_borrow(source, player, amount);
    case GAME_COMMAND_BANK_SPECIAL_REPAY:
        return rich4_game_command_bank_special_repay(source, player, amount);
    default:
        return rich4_game_command(GAME_COMMAND_NONE, source, player, 0);
    }
}

GameCommand rich4_game_command_bank_withdraw(
    GameCommandSource source, int player, int amount)
{
    return rich4_game_command(
        GAME_COMMAND_BANK_WITHDRAW, source, player, amount);
}

GameCommand rich4_game_command_bank_deposit(
    GameCommandSource source, int player, int amount)
{
    return rich4_game_command(
        GAME_COMMAND_BANK_DEPOSIT, source, player, amount);
}

GameCommand rich4_game_command_bank_apply_loan(
    GameCommandSource source, int player, int amount)
{
    return rich4_game_command(
        GAME_COMMAND_BANK_APPLY_LOAN, source, player, amount);
}

GameCommand rich4_game_command_bank_repay_loan(
    GameCommandSource source, int player, int amount)
{
    return rich4_game_command(
        GAME_COMMAND_BANK_REPAY_LOAN, source, player, amount);
}

GameCommand rich4_game_command_bank_special_borrow(
    GameCommandSource source, int player, int amount)
{
    return rich4_game_command(
        GAME_COMMAND_BANK_SPECIAL_BORROW, source, player, amount);
}

GameCommand rich4_game_command_bank_special_repay(
    GameCommandSource source, int player, int amount)
{
    return rich4_game_command(
        GAME_COMMAND_BANK_SPECIAL_REPAY, source, player, amount);
}

GameCommand rich4_game_command_save_load(
    GameCommandType type, GameCommandSource source, int player, int slot)
{
    return rich4_game_domain_command(
        type, source, player,
        rich4_game_domain_ref(GAME_DOMAIN_SAVE_SLOT, slot),
        rich4_game_domain_ref(GAME_DOMAIN_NONE, -1), 0);
}

GameCommand rich4_game_command_save_game(
    GameCommandSource source, int player, int slot)
{
    return rich4_game_command_save_load(
        GAME_COMMAND_SAVE_GAME, source, player, slot);
}

GameCommand rich4_game_command_load_game(
    GameCommandSource source, int player, int slot)
{
    return rich4_game_command_save_load(
        GAME_COMMAND_LOAD_GAME, source, player, slot);
}

GameCommand rich4_game_command_with_turn_token(
    GameCommand command, unsigned int turn_token)
{
    command.turn_token = turn_token;
    return command;
}

static int is_property(GameDomainRef ref)
{
    return rich4_game_domain_ref_is_valid(ref) &&
           (ref.kind == GAME_DOMAIN_LAND || ref.kind == GAME_DOMAIN_FACILITY);
}

int rich4_game_command_is_valid(const GameCommand *command)
{
    if (command == 0 || command->type <= GAME_COMMAND_NONE ||
        command->type > GAME_COMMAND_REPRICE_MARKET_LISTING ||
        (command->source != GAME_COMMAND_SOURCE_HUMAN &&
         command->source != GAME_COMMAND_SOURCE_AI) || command->player < 0 ||
        command->player >= GAME_MAX_PLAYERS ||
        !rich4_game_domain_ref_is_valid(command->subject) ||
        !rich4_game_domain_ref_is_valid(command->target) ||
        !rich4_game_domain_ref_is_valid(command->target2)) {
        return 0;
    }
    if (command->type != GAME_COMMAND_USE_TOOL &&
        !rich4_game_domain_ref_is_none(command->target2)) return 0;
    if (command->type != GAME_COMMAND_USE_CARD &&
        command->type != GAME_COMMAND_CREATE_MARKET_LISTING &&
        (command->value2 != 0 || command->flags != 0)) return 0;
    if (command->type == GAME_COMMAND_CREATE_MARKET_LISTING &&
        command->flags != 0) return 0;
    if (command->type == GAME_COMMAND_SELECT_DICE_COUNT) {
        return command->value >= 1 && command->value <= 3 &&
               rich4_game_domain_ref_is_none(command->subject) &&
               rich4_game_domain_ref_is_none(command->target);
    }
    switch (command->type) {
    case GAME_COMMAND_PURCHASE_PROPERTY:
    case GAME_COMMAND_DECLINE_PROPERTY_PURCHASE:
    case GAME_COMMAND_DECLINE_PROPERTY_UPGRADE:
        return command->value == 0 && is_property(command->subject) &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_UPGRADE_PROPERTY:
        return ((command->subject.kind == GAME_DOMAIN_LAND && command->value == 0) ||
                (command->subject.kind == GAME_DOMAIN_FACILITY &&
                 command->value >= 0 && command->value <= 4)) &&
               is_property(command->subject) &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_SELECT_TARGET:
        return command->value == 0 &&
               rich4_game_domain_ref_is_none(command->subject) &&
               !rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_USE_CARD:
        return command->subject.kind == GAME_DOMAIN_CARD;
    case GAME_COMMAND_USE_TOOL:
        return command->subject.kind == GAME_DOMAIN_TOOL &&
               ((command->subject.id == 7 && command->value >= 1 &&
                 command->value <= 6) ||
                (command->subject.id == 8 && command->value >= 0 &&
                 command->value <= 4) ||
                (command->subject.id != 7 && command->subject.id != 8 &&
                 command->value == 0));
    case GAME_COMMAND_BUY_STOCK:
    case GAME_COMMAND_SELL_STOCK:
        return command->value > 0 && command->subject.kind == GAME_DOMAIN_STOCK &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_SELL_ASSET:
        return command->value > 0 &&
               (is_property(command->subject) ||
                command->subject.kind == GAME_DOMAIN_STOCK) &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_CREATE_MARKET_LISTING:
        return command->value > 0 &&
               ((command->subject.kind == GAME_DOMAIN_STOCK &&
                 command->value2 > 0 && command->value2 <= INT16_MAX) ||
                ((command->subject.kind == GAME_DOMAIN_LAND ||
                  command->subject.kind == GAME_DOMAIN_FACILITY ||
                  command->subject.kind == GAME_DOMAIN_CARD ||
                  command->subject.kind == GAME_DOMAIN_TOOL) &&
                 command->value2 == 0)) &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_CANCEL_MARKET_LISTING:
    case GAME_COMMAND_BUY_MARKET_LISTING:
        return command->value == 0 &&
               command->subject.kind == GAME_DOMAIN_MARKET_LISTING &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_REPRICE_MARKET_LISTING:
        return command->value > 0 &&
               command->subject.kind == GAME_DOMAIN_MARKET_LISTING &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_BANK_WITHDRAW:
    case GAME_COMMAND_BANK_DEPOSIT:
    case GAME_COMMAND_BANK_APPLY_LOAN:
    case GAME_COMMAND_BANK_REPAY_LOAN:
    case GAME_COMMAND_BANK_SPECIAL_BORROW:
    case GAME_COMMAND_BANK_SPECIAL_REPAY:
        return command->value >= 0 &&
               rich4_game_domain_ref_is_none(command->subject) &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_SAVE_GAME:
    case GAME_COMMAND_LOAD_GAME:
        return command->value == 0 &&
               command->subject.kind == GAME_DOMAIN_SAVE_SLOT &&
               rich4_game_domain_ref_is_none(command->target);
    case GAME_COMMAND_SETTLE_AI_SPECIAL_FINANCE:
        return command->value == 0 &&
               rich4_game_domain_ref_is_none(command->subject) &&
               rich4_game_domain_ref_is_none(command->target);
    default:
        return command->value == 0 &&
               rich4_game_domain_ref_is_none(command->subject) &&
               rich4_game_domain_ref_is_none(command->target);
    }
}
