#ifndef RICH4_CORE_CHARACTER_TEMPLATE_H
#define RICH4_CORE_CHARACTER_TEMPLATE_H

#include "core/game_types.h"

/* Immutable domain character/AI defaults shared by new-game initialization,
 * app setup semantics and platform presentation adapters. */
const GameCharacterTemplate *rich4_character_template(int character);

#endif
