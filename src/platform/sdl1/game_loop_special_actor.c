#include "app/special_actor_flow.h"
#include "app/config_types.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/turn_rotation_flow.h"
#include "core/turn_rotation.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_route_api.h"

int game_update_special_actor_flow(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Uint32 now)
{
    int background_index = settings != NULL
        ? settings->stage * GAME_MAPS_PER_STAGE + settings->map : -1;
    const MapData *map = assets != NULL && background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT
        ? &assets->map.maps[background_index] : NULL;
    Rich4SpecialActorUpdateResult result;

    if (map == NULL || session == NULL ||
        !session->presentation.special_actor.active)
        return 0;
    result = rich4_special_actor_update(
        session, map, game_move_frame_ms_from_config(config), now);
    if (result == RICH4_SPECIAL_ACTOR_UPDATE_DOG) {
        GameEvent event;
        int count = session->deferred_events.count;
        while (count-- > 0 && rich4_game_event_queue_pop(&session->deferred_events, &event)) {
            if (event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED &&
                rich4_game_event_tool_resource(&event) == 532)
                game_tool_animation_enqueue_from_platform(session, assets, &event, now);
            else
                (void)rich4_game_event_queue_push(&session->deferred_events, &event);
        }
        return 1;
    }
    if (result == RICH4_SPECIAL_ACTOR_UPDATE_FINISHED) {
        Rich4AppTurnRotationResult rotation = { 0 };
        if (rich4_app_advance_turn_rotation(
                session, map, now, &rotation) &&
            rotation.special_action_pending)
            rich4_special_actor_begin(
                session, map, &rotation.special_action);
        return session->presentation.special_actor.active ||
               session->presentation.confinement_exit.active;
    }
    return result == RICH4_SPECIAL_ACTOR_UPDATE_ACTIVE;
}
