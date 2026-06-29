#ifndef RICH4_APP_SPECIAL_PLACE_NOTICE_FLOW_H
#define RICH4_APP_SPECIAL_PLACE_NOTICE_FLOW_H

#include "app/game_session.h"

int rich4_special_place_notice_enqueue(
    GameSession *session, const GameEvent *event, uint32_t now);
const GameEvent *rich4_special_place_notice_current(
    const GamePresentationState *presentation);
int rich4_special_place_notice_update(GameSession *session, uint32_t now);
int rich4_special_place_notice_skip(GameSession *session, uint32_t now);

#endif
