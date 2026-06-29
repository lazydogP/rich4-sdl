#ifndef RICH4_CORE_GAME_MAP_DYNAMIC_OBJECT_H
#define RICH4_CORE_GAME_MAP_DYNAMIC_OBJECT_H

#include <stdint.h>

/* Bounded map-object value used by the deterministic tick and view adapter.
 * It contains no SDL state; renderers receive it only as const input.
 *
 * The byte fields intentionally follow the original 0x496d08 object layout:
 * +0x04 is the attached-god/bomb countdown, while +0x06 is the transient
 * flight-display flag.  They are separate state in the original renderer. */
typedef struct GameMapDynamicObjectState {
    uint8_t type;
    uint8_t node_direction;
    uint16_t node;
    uint8_t countdown;
    uint8_t attached_player;
    uint8_t flight_active;
    uint8_t flight_direction;
    float world_x;
    float world_y;
    float velocity_x;
    float velocity_y;
} GameMapDynamicObjectState;

#endif
