#ifndef RICH4_CORE_AI_INTERNAL_H
#define RICH4_CORE_AI_INTERNAL_H

#include "core/ai.h"

void rich4_ai_select_stock_buy(const GameState *state, int player,
                               Rich4Rng *rng, GameAiDecision *decision);
void rich4_ai_select_stock_sell(const GameState *state, int player,
                                Rich4Rng *rng, GameAiDecision *decision);

#endif
