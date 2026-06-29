#ifndef RICH4_APP_MONSTER_CARD_FLOW_H
#define RICH4_APP_MONSTER_CARD_FLOW_H

#include <stdint.h>

#include "app/game_session.h"

/* Advances the app-owned monster-card presentation. The platform supplies
 * only the decoded animation frame count and current clock. */
int rich4_monster_card_flow_update(GameSession *session, int frame_count,
                                   uint32_t now);

#endif
