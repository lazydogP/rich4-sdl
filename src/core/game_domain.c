#include "core/game_domain.h"

static int kind_limit(GameDomainKind kind)
{
    switch (kind) {
    case GAME_DOMAIN_PLAYER: return GAME_MAX_PLAYERS;
    case GAME_DOMAIN_LAND:
    case GAME_DOMAIN_FACILITY: return GAME_MAX_MAP_PROPERTIES;
    case GAME_DOMAIN_STOCK: return GAME_STOCK_COUNT;
    case GAME_DOMAIN_CARD: return GAME_CARD_COUNT;
    case GAME_DOMAIN_TOOL: return GAME_TOOL_COUNT;
    case GAME_DOMAIN_STATUS: return GAME_STATUS_COUNT;
    case GAME_DOMAIN_NEWS_EVENT: return GAME_NEWS_EVENT_COUNT;
    case GAME_DOMAIN_FORTUNE_EVENT: return GAME_FORTUNE_EVENT_COUNT;
    case GAME_DOMAIN_SPECIAL_PLACE_ACTION:
        return GAME_SPECIAL_PLACE_ACTION_COUNT;
    case GAME_DOMAIN_SAVE_SLOT: return GAME_SAVE_SLOT_COUNT;
    case GAME_DOMAIN_SPECIAL_PLAYER: return GAME_SPECIAL_PLAYER_COUNT;
    case GAME_DOMAIN_ROUTE_NODE: return GAME_MAX_MAP_ROUTE_NODES;
    case GAME_DOMAIN_GOD: return GAME_MAP_DYNAMIC_OBJECT_COUNT;
    case GAME_DOMAIN_MAP_OBJECT: return GAME_TOOL_PLACED_OBJECT_COUNT;
    case GAME_DOMAIN_COMPANY: return GAME_MAX_MAP_PROPERTIES;
    case GAME_DOMAIN_MARKET_LISTING:
        return GAME_MAX_PLAYERS * GAME_MARKET_LISTING_SLOT_COUNT;
    default: return 0;
    }
}

GameDomainRef rich4_game_domain_ref(GameDomainKind kind, int zero_based_id)
{
    GameDomainRef ref;
    ref.kind = kind;
    ref.id = zero_based_id;
    if (!rich4_game_domain_ref_is_valid(ref)) {
        ref.kind = GAME_DOMAIN_NONE;
        ref.id = -1;
    }
    return ref;
}

GameDomainRef rich4_game_domain_ref_from_original(GameDomainKind kind,
                                                   int one_based_id)
{
    if (one_based_id <= 0) return rich4_game_domain_ref(GAME_DOMAIN_NONE, -1);
    return rich4_game_domain_ref(kind, one_based_id - 1);
}

int rich4_game_domain_ref_is_valid(GameDomainRef ref)
{
    int limit;
    if (ref.kind == GAME_DOMAIN_NONE) return ref.id == -1;
    limit = kind_limit(ref.kind);
    return limit > 0 && ref.id >= 0 && ref.id < limit;
}

int rich4_game_domain_ref_is_none(GameDomainRef ref)
{
    return ref.kind == GAME_DOMAIN_NONE && ref.id == -1;
}
