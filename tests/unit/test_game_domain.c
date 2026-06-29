#include <assert.h>
#include <stdio.h>

#include "core/game_domain.h"

static void boundary(GameDomainKind kind, int count)
{
    assert(rich4_game_domain_ref_is_valid(rich4_game_domain_ref(kind, 0)));
    assert(rich4_game_domain_ref_is_valid(rich4_game_domain_ref(kind, count - 1)));
    assert(rich4_game_domain_ref_is_none(rich4_game_domain_ref(kind, -1)));
    assert(rich4_game_domain_ref_is_none(rich4_game_domain_ref(kind, count)));
}

int main(void)
{
    GameDomainRef ref;
    boundary(GAME_DOMAIN_PLAYER, GAME_MAX_PLAYERS);
    boundary(GAME_DOMAIN_LAND, GAME_MAX_MAP_PROPERTIES);
    boundary(GAME_DOMAIN_FACILITY, GAME_MAX_MAP_PROPERTIES);
    boundary(GAME_DOMAIN_STOCK, GAME_STOCK_COUNT);
    boundary(GAME_DOMAIN_CARD, GAME_CARD_COUNT);
    boundary(GAME_DOMAIN_SPECIAL_PLAYER, GAME_SPECIAL_PLAYER_COUNT);
    boundary(GAME_DOMAIN_ROUTE_NODE, GAME_MAX_MAP_ROUTE_NODES);
    boundary(GAME_DOMAIN_GOD, GAME_MAP_DYNAMIC_OBJECT_COUNT);
    boundary(GAME_DOMAIN_TOOL, GAME_TOOL_COUNT);
    boundary(GAME_DOMAIN_STATUS, GAME_STATUS_COUNT);
    boundary(GAME_DOMAIN_NEWS_EVENT, GAME_NEWS_EVENT_COUNT);
    boundary(GAME_DOMAIN_FORTUNE_EVENT, GAME_FORTUNE_EVENT_COUNT);
    boundary(GAME_DOMAIN_SPECIAL_PLACE_ACTION, GAME_SPECIAL_PLACE_ACTION_COUNT);
    boundary(GAME_DOMAIN_SAVE_SLOT, GAME_SAVE_SLOT_COUNT);

    ref = rich4_game_domain_ref_from_original(GAME_DOMAIN_CARD, 1);
    assert(ref.kind == GAME_DOMAIN_CARD && ref.id == 0);
    ref = rich4_game_domain_ref_from_original(GAME_DOMAIN_CARD, GAME_CARD_COUNT);
    assert(ref.id == GAME_CARD_COUNT - 1);
    assert(rich4_game_domain_ref_is_none(
        rich4_game_domain_ref_from_original(GAME_DOMAIN_CARD, 0)));
    assert(rich4_game_domain_ref_is_none(
        rich4_game_domain_ref_from_original(GAME_DOMAIN_CARD,
                                            GAME_CARD_COUNT + 1)));
    puts("stable zero-based gameplay IDs: ok");
    return 0;
}
