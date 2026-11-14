#ifndef RICH4_APP_CARD_USE_FLOW_H
#define RICH4_APP_CARD_USE_FLOW_H

#include "app/game_session.h"
#include "data/map_data.h"
#include "view/game_view_model.h"

enum { RICH4_CARD_NOTICE_MS = 1500 };
typedef enum Rich4CardUseStep {
    RICH4_CARD_USE_WAIT = 0,
    RICH4_CARD_USE_COMMAND,
    RICH4_CARD_USE_DISPATCH_EFFECTS,
    RICH4_CARD_USE_MAX_LEVEL,
    RICH4_CARD_USE_REVENGE_COMMAND,
    RICH4_CARD_USE_FINISHED
} Rich4CardUseStep;

/* 0x441BAA/0x441F73: one notice for every selected AI card, before its
 * handler. Human selections have already shown the same notice in the panel. */
int rich4_card_use_begin(GameSession *session, const GameCommand *command,
    const MapData *map, int show_notice, uint32_t now);
int rich4_card_use_begin_selection(GameSession *session, int player, int card,
    GameDomainRef target, const MapData *map, int panel_phase, uint32_t now);
int rich4_card_use_begin_stock_selection(GameSession *session,
    int player, int card, uint32_t now);
int rich4_card_use_skip_notice(GameSession *session, uint32_t now);
Rich4CardUseStep rich4_card_use_update(GameSession *session,
    const GameViewportModel *viewport, uint32_t now);
void rich4_card_use_command_completed(GameSession *session, int accepted, uint32_t now);
/* Original property-animation branches; zero means no such animation. */
int rich4_card_use_animation_resource(int card, GameDomainRef target);

#endif
