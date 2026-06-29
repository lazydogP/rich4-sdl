#ifndef RICH4_CORE_GAME_DOMAIN_H
#define RICH4_CORE_GAME_DOMAIN_H

#include "core/game_constants.h"

#define GAME_NEWS_EVENT_COUNT 36
#define GAME_FORTUNE_EVENT_COUNT 37
#define GAME_SPECIAL_PLACE_ACTION_COUNT 12
/* Save slot IDs follow the original SAVE%d.DAT numbering: slot 0 is the
 * round-wrap auto-save, and slots 1..5 are the five user-visible slots. */
#define GAME_SAVE_AUTO_SLOT 0
#define GAME_SAVE_MANUAL_SLOT_FIRST 1
#define GAME_SAVE_MANUAL_SLOT_COUNT 5
#define GAME_SAVE_SLOT_COUNT \
    (GAME_SAVE_MANUAL_SLOT_FIRST + GAME_SAVE_MANUAL_SLOT_COUNT)
#define GAME_MARKET_LISTING_SLOT_COUNT 7

typedef enum GameDomainKind {
    GAME_DOMAIN_NONE = 0,
    GAME_DOMAIN_PLAYER,
    GAME_DOMAIN_LAND,
    GAME_DOMAIN_FACILITY,
    GAME_DOMAIN_STOCK,
    GAME_DOMAIN_CARD,
    GAME_DOMAIN_TOOL,
    GAME_DOMAIN_STATUS,
    GAME_DOMAIN_NEWS_EVENT,
    GAME_DOMAIN_FORTUNE_EVENT,
    GAME_DOMAIN_SPECIAL_PLACE_ACTION,
    GAME_DOMAIN_SAVE_SLOT,
    GAME_DOMAIN_SPECIAL_PLAYER,
    GAME_DOMAIN_ROUTE_NODE,
    /* Dynamic god/object slot selected by 请神符. Kept at the end so existing
     * serialized domain values remain stable. */
    GAME_DOMAIN_GOD,
    /* A slot in GameState.tool_placed_objects.  Appended so existing
     * serialized GameDomainKind values remain stable. */
    GAME_DOMAIN_MAP_OBJECT,
    /* Map auxiliary company record (raw object IDs 6001..7999). */
    GAME_DOMAIN_COMPANY,
    /* One of the original seven listings for a player; id is player-major. */
    GAME_DOMAIN_MARKET_LISTING
} GameDomainKind;

typedef enum GameStatusId {
    GAME_STATUS_DELAYED_BLOCK = 0,
    GAME_STATUS_DISAPPEARING,
    GAME_STATUS_PRISON,
    GAME_STATUS_HOSPITAL,
    GAME_STATUS_SLEEPING,
    GAME_STATUS_SLEEP_WALKING,
    GAME_STATUS_STOPPING,
    GAME_STATUS_TORTOISE_WALKING,
    GAME_STATUS_REJECTED_BY_BANK,
    GAME_STATUS_BANK_NO_LOANS,
    GAME_STATUS_GOD,
    GAME_STATUS_ASSURANCE,
    GAME_STATUS_COUNT
} GameStatusId;

typedef struct GameDomainRef {
    GameDomainKind kind;
    int id;
} GameDomainRef;

GameDomainRef rich4_game_domain_ref(GameDomainKind kind, int zero_based_id);
GameDomainRef rich4_game_domain_ref_from_original(GameDomainKind kind,
                                                   int one_based_id);
int rich4_game_domain_ref_is_valid(GameDomainRef ref);
int rich4_game_domain_ref_is_none(GameDomainRef ref);

#endif
