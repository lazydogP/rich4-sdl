#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_FRAME_END_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_FRAME_END_H

#include "platform/sdl1/game_loop_frame_present.h"
#include "platform/sdl1/game_loop_input_frame_finalize.h"
#include "platform/sdl1/game_loop_scene_update.h"

/* Post-event sequencing owns no feature state. Its dependencies stay
 * explicit rather than being repacked into another one-use context. */
void game_run_runtime_frame_end(
    GameInputFrameFinalizeContext *input, Rich4SceneMachine *scene_machine,
    GameSceneFramePayload *scene_payload,
    VirtualCursorPresentationContext *presentation, GamePreviewAssets *assets);

#endif
