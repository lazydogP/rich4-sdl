#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app/monster_card_flow.h"

int main(void)
{
    GameSession session;

    memset(&session, 0, sizeof(session));
    session.presentation.monster_card.active = 1;
    session.presentation.monster_card.started = 1000;
    session.presentation.monster_card.until = 1300;
    session.presentation.monster_card.speech_player = -1;
    assert(rich4_monster_card_flow_update(&session, 3, 1000));
    assert(rich4_monster_card_flow_update(&session, 3, 1160));
    assert(session.presentation.monster_card.frame == 1);
    assert(rich4_monster_card_flow_update(&session, 3, 1161));
    assert(session.presentation.monster_card.frame == 1);
    assert(rich4_monster_card_flow_update(&session, 3, 1240));
    assert(session.presentation.monster_card.frame == 2);
    assert(rich4_monster_card_flow_update(&session, 3, 1320));
    assert(rich4_monster_card_flow_update(&session, 3, 1819));
    assert(rich4_monster_card_flow_update(&session, 3, 1820) == 0);
    assert(!session.presentation.monster_card.active);

    puts("app monster-card flow: ok");
    return 0;
}
