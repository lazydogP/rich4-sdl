#ifndef RICH4_APP_SPECIAL_ACTOR_FLOW_H
#define RICH4_APP_SPECIAL_ACTOR_FLOW_H

#include <stdint.h>

#include "app/game_session.h"
#include "core/turn_rotation.h"
#include "data/map_data.h"

/* App-owned special-actor movement and action timing.  MapData is borrowed;
 * SDL supplies only the frame interval and consumes the completion seam. */
typedef enum Rich4SpecialActorUpdateResult {
    RICH4_SPECIAL_ACTOR_UPDATE_IDLE = 0,
    RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE,
    RICH4_SPECIAL_ACTOR_UPDATE_DOG,
    RICH4_SPECIAL_ACTOR_UPDATE_FINISHED
} Rich4SpecialActorUpdateResult;

void rich4_special_actor_begin(
    GameSession *session, const MapData *map,
    const TurnSpecialAutoActionResult *action);

Rich4SpecialActorUpdateResult rich4_special_actor_update(
    GameSession *session, const MapData *map, int frame_ms,
    uint32_t now);

#endif
