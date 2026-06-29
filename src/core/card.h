#ifndef RICH4_CORE_CARD_H
#define RICH4_CORE_CARD_H

#include <stdint.h>

#include "core/game_event.h"
#include "core/game_state.h"

typedef enum GameCardTargetKind {
    GAME_CARD_TARGET_NONE = 0,
    GAME_CARD_TARGET_PLAYER,
    GAME_CARD_TARGET_PROPERTY,
    GAME_CARD_TARGET_STOCK,
    GAME_CARD_TARGET_GOD,
    GAME_CARD_TARGET_CURRENT_PROPERTY,
    /* Original card 12's 0x0E0C0626 selector accepts built property
     * records and dynamic tool objects 0x10..0x12. */
    GAME_CARD_TARGET_PROPERTY_OR_MAP_OBJECT
} GameCardTargetKind;

typedef enum GameCardImplementation {
    GAME_CARD_IMPLEMENTED = 1,
    GAME_CARD_DEFERRED,
    GAME_CARD_UNAVAILABLE
} GameCardImplementation;

typedef struct GameCardInfo {
    int original_id;
    const char *name;
    int initial_pool;
    int shop_price;
    int metadata_f6;
    int ai_difficulty;
    GameCardTargetKind target_kind;
    GameCardImplementation implementation;
    uintptr_t handler_slot_address;
    const char *handler_symbol;
    const char *precondition;
    const char *side_effect;
} GameCardInfo;

typedef enum GameCardUseResult {
    GAME_CARD_USE_INVALID = 0,
    GAME_CARD_USE_APPLIED,
    GAME_CARD_USE_DEFERRED,
    GAME_CARD_USE_UNAVAILABLE
} GameCardUseResult;

/* Extra choices carried by GameCommand for card handlers that have a second
 * prompt. target is always the primary selected object:
 * - 换地/换屋: value=current/source property id; SECONDARY_FACILITY selects kind.
 * - 改建: value2=chosen facility subtype.
 * - 拍卖: value=winner (-1 means unsold), value2=winning price.
 * - 抢夺: value=zero-based card/tool id; ROB_TOOL selects the tool row.
 * - 梦游/陷害/查税: USE_MISDIRECT + value=redirected player.
 * - 查税: ACCEPT_FREE records acceptance of 免费卡.
 * - 请神: value=selected dynamic object's god type.
 * Fields not listed for a card are ignored. */
enum {
    GAME_CARD_FLAG_SECONDARY_FACILITY = 1u << 0,
    GAME_CARD_FLAG_USE_MISDIRECT = 1u << 1,
    GAME_CARD_FLAG_ACCEPT_FREE = 1u << 2,
    GAME_CARD_FLAG_ROB_TOOL = 1u << 3
};

typedef enum GameCardAttackKind {
    GAME_CARD_ATTACK_SLEEP_WALK = 1,
    GAME_CARD_ATTACK_FRAME = 2
} GameCardAttackKind;

GameCardUseResult rich4_card_apply_command_deferred_confinement(GameState *state,
    const GameCommand *command, GameEventQueue *events);

/* Passive chains are public because landing charges and special events enter
 * them without going through the active-card command dispatcher. */
int rich4_card_resolve_attack(GameState *state, int actor, int target,
                              GameCardAttackKind kind, int redirect_target,
                              GameEventQueue *events);
int rich4_card_resolve_landing_charge(GameState *state, int payer, int payee,
    int amount, int allow_free, int accept_free, int redirect_target,
    int *out_payer, int *out_amount, GameEventQueue *events);
int rich4_card_resolve_charge(GameState *state, int payer, int payee,
                              int amount, int accept_free,
                              int redirect_target, int *out_payer,
                              int *out_amount, GameEventQueue *events);

extern const GameCardInfo rich4_card_catalog[GAME_CARD_COUNT];

const GameCardInfo *rich4_card_info(int card);
int rich4_card_inventory_count(const GameState *state, int player);
int rich4_card_inventory_has(const GameState *state, int player, int card);
int rich4_card_inventory_add_from_pool(GameState *state, int player, int card,
                                       GameEventQueue *events);
/* Moves one existing card without touching the public card stock. */
int rich4_card_inventory_transfer(GameState *state, int from, int to, int card);
GameCardUseResult rich4_card_inspect(const GameState *state,
                                     const GameCommand *command);
GameCardUseResult rich4_card_apply_command(GameState *state,
                                           const GameCommand *command,
                                           GameEventQueue *events);

#endif
