#include <assert.h>
#include <stdio.h>

#include "platform/sdl1/game_loop_frame_end.h"
#undef main

static Rich4SceneMachine *g_machine;
static int g_switch_scene;
static int g_finalize_calls;
static int g_update_calls;
static int g_present_calls;
static int g_scope_calls;
static int g_host_pointer_sync_needed;

void runtime_profile_frame_phase(int phase) { (void)phase; }

int game_sync_runtime_asset_scope(GamePreviewAssets *assets, Rich4SceneId scene,
    const GameSession *session, const NewGameSettings *settings)
{
    (void)session; (void)settings;
    assert(assets != NULL && scene == g_machine->current);
    assert(g_finalize_calls == g_scope_calls + 1);
    ++g_scope_calls;
    return 1;
}

void game_finalize_input_frame(GameInputFrameFinalizeContext *context)
{
    assert(context != NULL);
    ++g_finalize_calls;
    if (g_switch_scene)
        g_machine->current = RICH4_SCENE_GAME;
}

void game_update_scene_frame(Rich4SceneMachine *machine,
                             GameSceneFramePayload *payload)
{
    assert(machine != NULL);
    assert(payload != NULL);
    assert(g_scope_calls == g_finalize_calls);
    ++g_update_calls;
    g_host_pointer_sync_needed = 1;
}

void game_present_runtime_frame(VirtualCursorPresentationContext *presentation)
{
    assert(presentation != NULL);
    assert(presentation->host_pointer_sync_needed ==
           &g_host_pointer_sync_needed);
    assert(*presentation->host_pointer_sync_needed ==
           g_host_pointer_sync_needed);
    ++g_present_calls;
}

int main(void)
{
    GameInputFrameFinalizeContext input = {0};
    GameInputFinalizeCursorContext cursor = {0};
    GamePreviewAssets assets = {0};
    Rich4SceneMachine machine = {0};
    GameSceneFramePayload payload = {0};
    VirtualCursorPresentationContext presentation = {0};

    g_machine = &machine;
    input.cursor = &cursor;
    presentation.host_pointer_sync_needed = &g_host_pointer_sync_needed;
    machine.current = RICH4_SCENE_OPTIONS;
    presentation.app_state = RICH4_SCENE_OPTIONS;
    g_switch_scene = 1;
    game_run_runtime_frame_end(&input, &machine, &payload, &presentation, &assets);
    assert(g_finalize_calls == 1);
    assert(g_update_calls == 0);
    assert(g_present_calls == 1);
    assert(presentation.app_state == RICH4_SCENE_GAME);

    g_switch_scene = 0;
    game_run_runtime_frame_end(&input, &machine, &payload, &presentation, &assets);
    assert(g_finalize_calls == 2);
    assert(g_update_calls == 1);
    assert(g_present_calls == 2);
    puts("SDL frame-end scene transition: ok");
    return 0;
}
