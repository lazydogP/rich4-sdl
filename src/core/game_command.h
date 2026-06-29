#ifndef RICH4_CORE_GAME_COMMAND_H
#define RICH4_CORE_GAME_COMMAND_H

#include "core/game_domain.h"

typedef enum GameCommandType {
    GAME_COMMAND_NONE = 0,
    GAME_COMMAND_CONFIRM,
    GAME_COMMAND_CANCEL,
    GAME_COMMAND_ROLL_DICE,
    GAME_COMMAND_SELECT_DICE_COUNT,
    GAME_COMMAND_END_INTERACTION,
    GAME_COMMAND_PURCHASE_PROPERTY,
    GAME_COMMAND_DECLINE_PROPERTY_PURCHASE,
    GAME_COMMAND_UPGRADE_PROPERTY,
    GAME_COMMAND_DECLINE_PROPERTY_UPGRADE,
    GAME_COMMAND_SELECT_TARGET,
    GAME_COMMAND_USE_CARD,
    GAME_COMMAND_USE_TOOL,
    GAME_COMMAND_BUY_STOCK,
    GAME_COMMAND_SELL_STOCK,
    GAME_COMMAND_SELL_ASSET,
    GAME_COMMAND_BANK_WITHDRAW,
    GAME_COMMAND_BANK_DEPOSIT,
    GAME_COMMAND_BANK_APPLY_LOAN,
    GAME_COMMAND_BANK_REPAY_LOAN,
    GAME_COMMAND_BANK_SPECIAL_BORROW,
    GAME_COMMAND_BANK_SPECIAL_REPAY,
    GAME_COMMAND_SAVE_GAME,
    GAME_COMMAND_LOAD_GAME,
    /* Internal AI preaction for original 0x436B0A(0). */
    GAME_COMMAND_SETTLE_AI_SPECIAL_FINANCE,
    GAME_COMMAND_CREATE_MARKET_LISTING,
    GAME_COMMAND_CANCEL_MARKET_LISTING,
    GAME_COMMAND_BUY_MARKET_LISTING,
    GAME_COMMAND_REPRICE_MARKET_LISTING
} GameCommandType;

typedef enum GameCommandSource {
    GAME_COMMAND_SOURCE_HUMAN = 0,
    GAME_COMMAND_SOURCE_AI = 1
} GameCommandSource;

typedef struct GameCommand {
    GameCommandType type;
    GameCommandSource source;
    int player;
    int value;
    int value2;
    unsigned int flags;
    unsigned int turn_token;
    GameDomainRef subject;
    GameDomainRef target;
    /* Second-stage target used by original multi-stage actions such as
     * 0x447428 (传送机).  Appended to preserve all existing field offsets
     * before the domain references. */
    GameDomainRef target2;
} GameCommand;

GameCommand rich4_game_command(GameCommandType type,
                               GameCommandSource source,
                               int player,
                               int value);
GameCommand rich4_game_domain_command(GameCommandType type,
                                      GameCommandSource source,
                                      int player,
                                      GameDomainRef subject,
                                      GameDomainRef target,
                                      int value);
GameCommand rich4_game_command_confirm(GameCommandSource source, int player);
GameCommand rich4_game_command_cancel(GameCommandSource source, int player);
GameCommand rich4_game_command_roll_dice(GameCommandSource source, int player);
GameCommand rich4_game_command_select_dice_count(
    GameCommandSource source, int player, int count);
GameCommand rich4_game_command_end_interaction(
    GameCommandSource source, int player);
GameCommand rich4_game_command_purchase_property(
    GameCommandSource source, int player, GameDomainRef property);
GameCommand rich4_game_command_decline_property_purchase(
    GameCommandSource source, int player, GameDomainRef property);
GameCommand rich4_game_command_upgrade_property(
    GameCommandSource source, int player, GameDomainRef property, int level);
GameCommand rich4_game_command_decline_property_upgrade(
    GameCommandSource source, int player, GameDomainRef property);
GameCommand rich4_game_command_property(
    GameCommandType type, GameCommandSource source, int player,
    GameDomainRef property, int level);
GameCommand rich4_game_command_target(
    GameCommandSource source, int player, GameDomainRef target);
GameCommand rich4_game_command_use_card(
    GameCommandSource source, int player, GameDomainRef card,
    int value2, unsigned int flags);
GameCommand rich4_game_command_use_card_on_target(
    GameCommandSource source, int player, GameDomainRef card,
    GameDomainRef target, int value, int value2, unsigned int flags);
GameCommand rich4_game_command_use_tool(
    GameCommandSource source, int player, GameDomainRef tool, int value,
    GameDomainRef target, GameDomainRef target2);
GameCommand rich4_game_command_stock(
    GameCommandType type, GameCommandSource source, int player,
    GameDomainRef stock, int quantity);
GameCommand rich4_game_command_buy_stock(
    GameCommandSource source, int player, GameDomainRef stock, int quantity);
GameCommand rich4_game_command_sell_stock(
    GameCommandSource source, int player, GameDomainRef stock, int quantity);
GameCommand rich4_game_command_sell_asset(
    GameCommandSource source, int player, GameDomainRef asset, int amount);
GameCommand rich4_game_command_create_market_listing(
    GameCommandSource source, int player, GameDomainRef item,
    int ask_price, int quantity);
GameCommand rich4_game_command_cancel_market_listing(
    GameCommandSource source, int player, int slot);
GameCommand rich4_game_command_buy_market_listing(
    GameCommandSource source, int player, int seller, int slot);
GameCommand rich4_game_command_reprice_market_listing(
    GameCommandSource source, int player, int slot, int ask_price);
/* Selects one of the named bank constructors when the UI mode is dynamic. */
GameCommand rich4_game_command_bank_for_type(
    GameCommandType type, GameCommandSource source, int player, int amount);
GameCommand rich4_game_command_bank_withdraw(
    GameCommandSource source, int player, int amount);
GameCommand rich4_game_command_bank_deposit(
    GameCommandSource source, int player, int amount);
GameCommand rich4_game_command_bank_apply_loan(
    GameCommandSource source, int player, int amount);
GameCommand rich4_game_command_bank_repay_loan(
    GameCommandSource source, int player, int amount);
GameCommand rich4_game_command_bank_special_borrow(
    GameCommandSource source, int player, int amount);
GameCommand rich4_game_command_bank_special_repay(
    GameCommandSource source, int player, int amount);
GameCommand rich4_game_command_save_load(
    GameCommandType type, GameCommandSource source, int player, int slot);
GameCommand rich4_game_command_save_game(
    GameCommandSource source, int player, int slot);
GameCommand rich4_game_command_load_game(
    GameCommandSource source, int player, int slot);
GameCommand rich4_game_command_with_turn_token(
    GameCommand command, unsigned int turn_token);
int rich4_game_command_is_valid(const GameCommand *command);

#endif
