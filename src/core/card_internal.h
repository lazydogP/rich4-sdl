#ifndef RICH4_CORE_CARD_INTERNAL_H
#define RICH4_CORE_CARD_INTERNAL_H

#include "core/card.h"

int rich4_card_append_events(GameEventQueue *events,
                             const GameEventQueue *staged);
int rich4_card_consume(GameState *state, int player, int card);
int rich4_card_resolve_attack_deferred_confinement(GameState *state, int actor,
    int target, GameCardAttackKind kind, int redirect_target, GameEventQueue *events);
int rich4_card_apply_average_cash(GameState *state, int actor,
                                   GameEventQueue *events);
int rich4_card_emit_effect(GameEventQueue *events, int actor,
                           GameDomainRef subject, GameDomainRef target,
                           int value, int value2);
int rich4_card_transfer_tool(GameState *state, int from, int to, int tool);
void rich4_card_add_relation_balance(GameState *state, int source, int target,
                                     int64_t amount);
int rich4_card_apply_property_card(GameState *state,
                                   const GameCommand *command,
                                   int card, GameEventQueue *events);

#endif
