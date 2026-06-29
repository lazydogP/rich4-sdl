#include "platform/sdl1/game_lazy_assets.h"
#include "app/property_flow.h"
#include "core/game_event_payload.h"
#include "app/game_session.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"

/* SDL adapter for property presentation. App owns phases and rules; this
 * file only converts resource metadata and mixer playback to scalar inputs. */
int game_property_start_max_level_animation(
    GameSession *session, const GamePreviewAssets *assets, Uint32 now)
{
    int frame_count = assets != NULL && game_asset_ready(assets, &assets->tool.has_property_max_level_graph)
        ? game_asset_graph_frames(assets, &assets->tool.property_max_level_data, assets->tool.property_max_level_size) : 0;
    now = rich4_sdl1_ticks();
    return rich4_property_start_max_level_animation(session, frame_count, now);
}

int game_property_update_max_level_animation(
    GameSession *session, const GamePreviewAssets *assets, Uint32 now)
{
    int frame_count = assets != NULL && game_asset_ready(assets, &assets->tool.has_property_max_level_graph)
        ? game_asset_graph_frames(assets, &assets->tool.property_max_level_data, assets->tool.property_max_level_size) : 0;
    int delay_ms = assets != NULL && game_asset_ready(assets, &assets->tool.has_property_max_level_graph)
        ? game_asset_graph_delay(assets, &assets->tool.property_max_level_data, assets->tool.property_max_level_size) : 0;
    now = rich4_sdl1_ticks();
    return rich4_property_update_max_level_animation(
        session, frame_count, delay_ms, now);
}

void game_property_begin_level_presentation(
    GameSession *session, const GamePreviewAssets *assets,
    const GameEvent *event, Uint32 now)
{
    int frame_count = event != NULL && rich4_game_event_property_level(event) == 5 &&
        assets != NULL && game_asset_ready(assets, &assets->tool.has_property_max_level_graph)
        ? game_asset_graph_frames(assets, &assets->tool.property_max_level_data, assets->tool.property_max_level_size) : 0;
    rich4_property_begin_level_presentation(session, event, frame_count, now);
}

int game_property_update_charge_presentation(
    GameSession *session, Rich4Audio *audio, Uint32 now)
{
    if (game_update_property_speech(session, audio, now)) return 1;
    return rich4_property_update_charge_presentation(session, now);
}
