#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/shop_flow.h"

int main(void)
{
    GameSession session;
    memset(&session, 0, sizeof(session));
    session.state.player_count = 1;
    session.presentation.shop_visit.active = 1;
    session.presentation.shop_visit.phase = RICH4_SHOP_ACTIVE;
    session.presentation.shop_visit.mode = 0;
    session.presentation.shop_visit.card_count = 1;
    session.presentation.shop_visit.card_merchandise[0] = 0;
    rich4_shop_mark_music_started(&session);
    assert(session.presentation.shop_visit.music_started == 1);
    assert(rich4_shop_message_big5(0) != 0);
    assert(rich4_shop_message_big5(11) == 0);
    assert(rich4_shop_hit(&session, 20, 85) == 0);
    assert(rich4_shop_hit(&session, 550, 20) == RICH4_SHOP_HIT_TAB);
    assert(rich4_shop_hit(&session, 560, 250) == RICH4_SHOP_HIT_EXIT);
    assert(rich4_shop_pointer_down(&session, 550, 20, 100, NULL));
    assert(session.presentation.shop_visit.pressed == RICH4_SHOP_HIT_TAB);
    rich4_shop_pointer_up(&session, RICH4_SHOP_HIT_TAB, 550, 20, 100, NULL);
    assert(session.presentation.shop_visit.pressed == RICH4_SHOP_HIT_NONE);
    assert(session.presentation.shop_visit.mode == 1);
    session.presentation.shop_visit.phase = RICH4_SHOP_ACTIVE;
    session.presentation.shop_visit.mode = 1;
    session.presentation.shop_visit.tool_merchandise[0] = 1;
    assert(rich4_shop_hit(&session, 20, 85) == 0);
    puts("app shop flow: ok");
    return 0;
}
