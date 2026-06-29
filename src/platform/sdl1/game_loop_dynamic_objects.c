#include "app/dynamic_object_flow.h"
#include "app/game_speech_flow.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include <SDL/SDL.h>
#include "core/game_constants.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_view_model_input.h"
#include "view/game_view_model.h"
#include "view/map_renderer.h"

/* R10.1d: dynamic map-object ViewModel refresh, moved verbatim out of
 * update_game_session. */

void game_update_dynamic_objects(GameSession *session,
                                 const GamePreviewAssets *assets,
                                 const NewGameSettings *settings,
                                 Uint32 now)
{
    int background_index;
    GameViewModel view;
    GameViewModelInput view_input;
    const MapData *map;

    if (session == NULL || assets == NULL || settings == NULL ||
        session->presentation.intro.active)
        return;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT)
        return;
    map = &assets->map.maps[background_index];
    rich4_sdl1_game_view_model_input_build(&session->flow,
                                           &session->presentation,
                                           &view_input);
    if (!rich4_game_view_model_build(&session->state, &view_input,
                                     map, &view))
        return;
    rich4_app_sync_tool_dynamic_objects(
                                     &session->presentation, &session->state);
    rich4_app_update_dynamic_objects(&session->presentation,
                                     &view.viewport, -1);
    (void)now;
}

/* 0x40E5EF: end before displaying/waiting for a chunk entirely above y=40. */
void game_limit_god_detachment(GameSession *session,
    const GamePreviewAssets *assets, const NewGameSettings *settings)
{
    GameGodDetachmentPresentationState *d;
    GameViewModelInput input;
    GameViewportModel viewport;
    int index, background, y, frame;
    if (session == NULL || assets == NULL || settings == NULL) return;
    d = &session->presentation.god_detachment;
    if (!d->active || d->god_type <= 0 ||
        d->god_type > GAME_DATA_MAP_STATUS_RESOURCE_COUNT) return;
    index = d->god_type - 1;
    if (!assets->map.has_data_map_status[index]) return;
    background = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background < 0 || background >= GAME_BACKGROUND_COUNT) return;
    rich4_sdl1_game_view_model_input_build(&session->flow,
        &session->presentation, &input);
    rich4_game_viewport_build(&session->state, &input,
        &assets->map.maps[background], &viewport);
    rich4_map_god_detachment_anchor(&viewport, d->world_x, d->world_y,
        d->direction, NULL, &y);
    for (frame = 0; frame < d->frame_count; frame++) {
        Rich4SprSmpChunk chunk;
        int direction = (d->direction + frame + 1) & 7;
        if (rich4_spr_smp_get_chunk(assets->map.data_map_status_data[index],
                assets->map.data_map_status_size[index],
                &assets->map.data_map_status_spr[index], direction, &chunk)
                != RICH4_SPR_SMP_OK) return;
        if (y - (frame + 1) * 10 - chunk.y + chunk.height < 40) {
            d->frame_count = frame;
            return;
        }
    }
}

int game_update_god_detachment(GameSession *session, Rich4Audio *audio, Uint32 now)
{
    GameGodDetachmentPresentationState *d;
    if (session == NULL) return 0;
    d = &session->presentation.god_detachment;
    if (d->speech_pending) {
        if (game_update_property_speech(session, audio, now)) return 1;
        d->speech_pending = 0;
        return 0;
    }
    if (!d->active) return 0;
    if (rich4_app_update_dynamic_god_detachment(&session->presentation, now)) return 1;
    if ((d->god_type == 5 || d->god_type == 6 || d->god_type == 7 ||
         d->god_type == 8 || d->god_type == 15) && d->player >= 0 &&
        d->player < session->state.player_count) {
        d->speech_pending = game_start_speech_entry(session, d->player,
            rich4_character_god_departure_speech(session->state.players[d->player].character),
            2, session->presentation.property.phase, now);
    }
    return d->speech_pending;
}
