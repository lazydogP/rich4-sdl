#include <SDL/SDL.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_animation_flow.h"
#include "app/insurance_flow.h"
#include "core/game_event_payload.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_turns.h"

/* SDL adapter for resource frame lookup, audio playback and actor sync. */
int game_update_tool_animation_queue(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio, Uint32 now,
    int animation_enabled)
{
    const GameToolAnimationRequest *request;
    const unsigned char *data;
    size_t size;
    int frame_count = 0;
    int frame_delay_ms = 0;
    Rich4AudioEventQueue *audio_events;
    Rich4ToolAnimationCompletion completion;
    int active;
    GameEvent event;

    if (session == NULL || session->presentation.tool_animation_count <= 0)
        return 0;
    request = &session->presentation.tool_animation[
        session->presentation.tool_animation_head];
    if (game_tool_effect_data(assets, request->resource, &data, &size)) {
        frame_count = game_graph_frame_count_data(data, size);
        frame_delay_ms = game_graph_frame_delay_ms_data(data, size);
    }
    audio_events = rich4_app_session_audio_borrow(session);
    if (!animation_enabled)
        rich4_tool_animation_skip_optional_confinement(session);
    now = rich4_sdl1_ticks();
    active = rich4_tool_animation_update(
        session, frame_count, frame_delay_ms, now, audio_events, &completion);
    if (!animation_enabled)
        rich4_tool_animation_skip_optional_confinement(session);
    game_session_audio_flush(session, audio);
    if (completion.sync_player >= 0) {
        game_sync_player_actor_to_node(
            session, assets, settings, completion.sync_player, 1);
        rich4_tool_animation_actor_synced(session, completion.sync_player);
    }
    while (rich4_game_event_queue_pop(&completion.events, &event)) {
        if (event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED) {
            game_tool_animation_enqueue_from_platform(session, assets, &event, now);
            if (!animation_enabled)
                rich4_tool_animation_skip_optional_confinement(session);
        } else if (event.type == GAME_EVENT_STATUS_CHANGED) {
            if (!rich4_tool_animation_defer_sync(session, event.player))
                game_sync_confinement_event(session, assets, settings, &event);
        } else if (event.type == GAME_EVENT_VOICE_REQUESTED)
            (void)rich4_tool_animation_handle_voice_event(session, &event, now);
        else if (event.type == GAME_EVENT_INSURANCE_CLAIM_PRESENTATION_REQUESTED) {
            rich4_insurance_flow_enqueue_confinement_claim(session, &event, now);
        }
    }
    return active || session->presentation.tool_animation_count > 0;

}
