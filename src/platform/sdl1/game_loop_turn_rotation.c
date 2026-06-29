#include <stddef.h>
#include <stdint.h>

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/turn_rotation_flow.h"
#include "app/special_actor_flow.h"
#include "app/turn_status_flow.h"
#include "core/confinement.h"
#include "core/game_constants.h"
#include "core/game_event.h"
#include "core/game_event_payload.h"
#include "core/special_actor_action.h"
#include "core/turn_rotation.h"
#include "data/map_data.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_view_model_input.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/render/game_scene_view.h"
#include "view/game_view_model.h"

/* R10.1d: turn rotation, status countdowns, confinement release and the
 * control-locked-turn presentation moved verbatim out of update_game_session.
 * This module only adapts the app result to the SDL special-actor animation;
 * route movement and route-event execution remain in their dedicated modules. */

int game_update_confinement_exit(
    GameSession *session, const Rich4Config *config, Uint32 now)
{
    return rich4_app_update_confinement_exit(
        session, now, game_move_frame_ms_from_config(config));
}

void game_finish_control_locked_turn(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now)
{
    int background_index;
    const MapData *map = NULL;
    Rich4AppTurnRotationResult result = { 0 };

    if (session == NULL) return;
    background_index = settings != NULL
        ? settings->stage * GAME_MAPS_PER_STAGE + settings->map : -1;
    if (assets != NULL && background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT)
        map = &assets->map.maps[background_index];
    if (!rich4_app_finish_control_locked_turn(session, map, now, &result))
        return;
    if (result.special_action_pending)
        rich4_special_actor_begin(session, map, &result.special_action);
}

int game_update_confinement_status_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now)
{
    int status = rich4_app_update_confinement_status_presentation(
        session, now, session != NULL && session->presentation.speech.active);
    if (status == 2) {
        game_finish_control_locked_turn(session, assets, settings, now);
        return 1;
    }
    return status;
}
