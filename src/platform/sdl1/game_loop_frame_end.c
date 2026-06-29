/* R10.1d: post-event frame orchestration moved verbatim out of
 * runtime_main.c. The existing helper order remains explicit below. */

#include "platform/sdl1/game_loop_frame_end.h"
#include "platform/sdl1/game_loop_frame_present.h"
#include "platform/sdl1/game_loop_input_frame_finalize.h"
#include "platform/sdl1/game_loop_scene_update.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/runtime_trace.h"

void game_run_runtime_frame_end(
    GameInputFrameFinalizeContext *input, Rich4SceneMachine *scene_machine,
    GameSceneFramePayload *scene_payload,
    VirtualCursorPresentationContext *presentation, GamePreviewAssets *assets)
{
    Rich4SceneId payload_scene;
    if (input == NULL || scene_machine == NULL || scene_payload == NULL ||
        presentation == NULL)
        return;
    payload_scene = scene_machine->current;
    game_finalize_input_frame(input);
    /* Save/load and menu actions may have changed map/party during input.
     * Resolve the new scene's bytes before any update can use its views. */
    if (assets != NULL && input->cursor != NULL &&
        game_sync_runtime_asset_scope(assets, scene_machine->current,
            input->cursor->session, input->cursor->settings) < 0) {
        scene_machine->running = 0;
        return;
    }
    /* runtime_main builds GameSceneFramePayload as a union for the scene that
     * was active before input finalization.  A confirm/cancel action may
     * switch scenes here; reinterpreting that union as the new scene's member
     * turns (for example) an options fallback pointer into a game-frame
     * pointer.  Defer the new scene's first update until the next frame, when
     * runtime_main has built the matching payload. */
    runtime_profile_frame_phase(0);
    if (payload_scene != RICH4_SCENE_GAME) runtime_profile_frame_phase(1);
    if (scene_machine->current == payload_scene)
        game_update_scene_frame(scene_machine, scene_payload);
    runtime_profile_frame_phase(2);
    presentation->app_state = scene_machine->current;
    game_present_runtime_frame(presentation);
}
