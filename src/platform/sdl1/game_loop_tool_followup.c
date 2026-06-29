#include <SDL/SDL.h>

#include "app/audio_event.h"
#include "app/game_session.h"
#include "app/game_presentation.h"
#include "app/new_game_setup_flow.h"
#include "app/tool_followup_flow.h"
#include "app/dynamic_object_flow.h"
#include "app/special_actor_flow.h"
#include "app/turn_rotation_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_scene_view_builder.h"

static const MapData *game_tool_followup_map(
    const GamePreviewAssets *assets, const NewGameSettings *settings)
{
    int background_index;
    if (assets == NULL || settings == NULL) return NULL;
    background_index = settings->stage * GAME_MAPS_PER_STAGE + settings->map;
    if (background_index < 0 || background_index >= GAME_BACKGROUND_COUNT)
        return NULL;
    return &assets->map.maps[background_index];
}

/* SDL adapter for app-owned placement timing and audio playback. */
int game_tool_followup_update_placement(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Rich4Audio *audio, Uint32 now)
{
    GameSceneView scene;
    const GameViewportModel *viewport = NULL;
    Rich4AudioEventQueue *audio_events =
        rich4_app_session_audio_borrow(session);
    if (build_game_session_render_view(
            assets, settings, session, &scene))
        viewport = &scene.game.viewport;
    int active = rich4_tool_followup_update_placement(
        session, viewport, now, audio_events);
    game_session_audio_flush(session, audio);
    return active;
}

/* SDL adapter for robot coordinates, tool-effect events and audio. */
int game_tool_followup_update_robot(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, const Rich4Config *config,
    Rich4Audio *audio, Uint32 now)
{
    GameEventQueue events;
    Rich4AudioEventQueue *audio_events;
    Rich4ToolRobotUpdate update;
    GameEvent event;
    int active;
    rich4_game_event_queue_reset(&events);
    audio_events = rich4_app_session_audio_borrow(session);
    active = rich4_tool_followup_update_robot(
        session, game_tool_followup_map(assets, settings),
        game_move_frame_ms_from_config(config), now,
        &events, audio_events, &update);
    if (update.node > 0 && !update.stopped) {
        rich4_presentation_set_view_override(
            &session->presentation,
            session->presentation.tool_robot_motion.world_x,
            session->presentation.tool_robot_motion.world_y);
    }
    if (update.pulse && !update.stopped) {
        GameSceneView scene;
        if (build_game_session_render_view(assets, settings, session, &scene))
            rich4_app_update_dynamic_objects(&session->presentation,
                &scene.game.viewport, update.launched_object);
    }
    while (rich4_game_event_queue_pop(&events, &event)) {
        if (event.type == GAME_EVENT_TOOL_PRESENTATION_REQUESTED)
            game_tool_animation_enqueue_from_platform(
                session, assets, &event, now);
    }
    game_session_audio_flush(session, audio);
    return active;
}

int game_tool_followup_finish_trigger_turn(
    GameSession *session, const GamePreviewAssets *assets,
    const NewGameSettings *settings, Uint32 now)
{
    Rich4AppTurnRotationResult result = { 0 };
    const MapData *map = game_tool_followup_map(assets, settings);
    if (!rich4_tool_followup_finish_trigger_turn_and_advance(
            session, map, now, &result))
        return 0;
    if (result.special_action_pending)
        rich4_special_actor_begin(session, map, &result.special_action);
    return 1;
}
