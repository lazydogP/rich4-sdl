#include "core/character_template.h"

const GameCharacterTemplate *rich4_character_template(int character)
{
    static const GameCharacterTemplate templates[12] = {
        { 0x00946126, 1, 3, 2, 60, 50, 30 },
        { 0x00bdc3c6, 1, 3, 1, 100, 40, 45 },
        { 0x0041323b, 1, 3, 2, 0, 70, 0 },
        { 0x00c626c3, 0, 3, 2, 100, 60, 30 },
        { 0x00c5b830, 1, 3, 1, 50, 40, 25 },
        { 0x00ed9d9d, 0, 3, 1, 75, 70, 30 },
        { 0x0000f038, 1, 3, 1, 100, 50, 20 },
        { 0x00ffffa0, 0, 3, 0, 0, 40, 35 },
        { 0x00e77c08, 0, 3, 0, 0, 60, 20 },
        { 0x00cc1a20, 0, 3, 0, 50, 50, 0 },
        { 0x002017fe, 1, 3, 1, 30, 55, 15 },
        { 0x000ebdbd, 0, 3, 2, 80, 80, 0 }
    };

    if (character < 0 ||
        character >= (int)(sizeof(templates) / sizeof(templates[0]))) {
        character = 0;
    }
    return &templates[character];
}
