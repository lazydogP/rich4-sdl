#include <stdint.h>

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "app/tool_animation_flow.h"
#include "app/turn_movement_flow.h"
#include "app/special_actor_flow.h"
#include "core/game_event.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/game_loop_audio_events.h"

void game_update_turn_movement(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now)
{
    int background_index = settings != NULL
        ? settings->stage * GAME_MAPS_PER_STAGE + settings->map : -1;
    const MapData *map = assets != NULL && background_index >= 0 &&
        background_index < GAME_BACKGROUND_COUNT
        ? &assets->map.maps[background_index] : NULL;
    Rich4AppTurnMovementResult result;
    GameEvent event;

    (void)rich4_app_session_audio_borrow(session);
    (void)rich4_app_update_turn_movement(
        session, map, game_move_frame_ms_from_config(config), now, &result);
    if (session->flow.turn_state == GAME_TURN_STATE_MOVE)
        rich4_sdl1_wake_at(session->presentation.movement_tick +
                          (uint32_t)game_move_frame_ms_from_config(config));
    /* Queue loop stops before any explosion presentation sound. */
    rich4_app_collect_session_audio_events(session, map, &session->audio_events);
    while (rich4_game_event_queue_pop(&result.events, &event)) {
        if (event.type == GAME_EVENT_GOD_ACQUIRED)
            game_begin_god_presentation(session, assets, &event, now);
        else if (event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED)
            game_tool_animation_enqueue_from_platform(
                session, assets, &event, now);
        else if (event.type == GAME_EVENT_STATUS_CHANGED) {
            if (!rich4_tool_animation_defer_sync(session, event.player))
                game_sync_confinement_event(
                    session, assets, settings, &event);
        } else if (event.type == GAME_EVENT_TEXT_REQUESTED)
            rich4_tool_animation_handle_text_event(session, &event, now);
        else if (event.type == GAME_EVENT_VOICE_REQUESTED)
            rich4_tool_animation_handle_voice_event(session, &event, now);
    }
    game_session_audio_flush(session, audio);
    if (result.special_action_pending)
            rich4_special_actor_begin(session, map, &result.special_action);
}
