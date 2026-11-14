#ifndef RICH4_APP_CARD_DEFENSE_FLOW_H
#define RICH4_APP_CARD_DEFENSE_FLOW_H
#include "app/game_session.h"
/* 0x444691/0x44476A/0x444A60/0x444BB2 share card, choice and speech waits. */
void rich4_card_defense_begin(GameSession *session, uint32_t now);
void rich4_card_defense_revenge_begin(GameSession *session, uint32_t now);
int rich4_card_defense_update(GameSession *session, uint32_t now);
int rich4_card_defense_choose(GameSession *session, int target, uint32_t now);
int rich4_card_defense_hover(GameSession *session, int target);
#endif
