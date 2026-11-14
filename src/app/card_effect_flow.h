#ifndef RICH4_APP_CARD_EFFECT_FLOW_H
#define RICH4_APP_CARD_EFFECT_FLOW_H
#include "app/game_session.h"
void rich4_card_effect_prepare(GameSession *session);
void rich4_card_effect_flash(GameSession *session, uint32_t now);
int rich4_card_effect_flash_update(GameSession *session, uint32_t now);
#endif
