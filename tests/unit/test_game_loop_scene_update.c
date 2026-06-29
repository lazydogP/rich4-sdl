#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "app/scene_frame_flow.h"
#include "platform/sdl1/game_loop_scene_update.h"
#undef main

static GamePreviewFrameContext *g_expected_frame;
static GameRenderCache *g_expected_cache;
static int g_game_updates;

uint32_t rich4_sdl1_ticks(void)
{
    return 0x12345678u;
}

void game_update_preview_frame(GamePreviewFrameContext *context)
{
    assert(context == g_expected_frame);
    assert(context->render_cache == g_expected_cache);
    ++g_game_updates;
}

int rich4_app_scene_frame_update(
    Rich4SceneMachine *machine, const Rich4SceneId *fallback_scene,
    uint32_t now_ms, const Rich4AppSceneFrameServices *services,
    Rich4SceneId *out_scene)
{
    (void)fallback_scene;
    assert(machine->current == RICH4_SCENE_GAME);
    assert(now_ms == 0x12345678u);
    assert(services->game_context == g_expected_frame);
    assert(services->intro_context == NULL);
    assert(services->new_game_context == NULL);
    assert(services->fallback_menu_context == NULL);
    services->game(services->game_context);
    *out_scene = machine->current;
    return 1;
}

void game_update_new_game_transition_scene(
    SDL_Surface *screen, Rich4Mkf *mkf, GamePreviewAssets *assets,
    NewGameSettings *settings, GameSession *session,
    const Rich4Config *config, const Rich4SdlConfig *sdl_config,
    const char *music_dir, VirtualCursor *cursor,
    GameRenderCache *render_cache, Rich4SceneMachine *scene_machine,
    Rich4SceneId *app_state, Rich4Audio *audio,
    int *main_map_music_track, int *main_map_music_active,
    int *game_preview_trace_count,
    Rich4SetupTransitionInput setup_transition_input, Uint32 now)
{
    (void)screen; (void)mkf; (void)assets; (void)settings; (void)session;
    (void)config; (void)sdl_config; (void)music_dir; (void)cursor;
    (void)render_cache; (void)scene_machine; (void)app_state; (void)audio;
    (void)main_map_music_track; (void)main_map_music_active;
    (void)game_preview_trace_count; (void)setup_transition_input; (void)now;
    assert(!"unexpected intro update");
}

void game_update_new_game_setup_scene(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    NewGameSettings *settings, VirtualCursor *cursor,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    Uint32 now, Rich4Audio *audio)
{
    (void)screen; (void)assets; (void)settings; (void)cursor;
    (void)scene_machine; (void)app_state; (void)now; (void)audio;
    assert(!"unexpected setup update");
}

void game_draw_scene_fallback_menu(
    const GameSceneFallbackMenuContext *context)
{
    (void)context;
    assert(!"unexpected menu update");
}

void game_draw_scene_fallback_options_menu(
    const GameSceneFallbackOptionsMenuContext *context)
{
    (void)context;
    assert(!"unexpected options-menu update");
}

void game_draw_scene_fallback_options_game(
    const GameSceneFallbackOptionsGameContext *context)
{
    (void)context;
    assert(!"unexpected options-game update");
}

int main(void)
{
    Rich4SceneMachine machine = {0};
    GameRenderCache cache = {0};
    GamePreviewFrameContext frame = {0};
    GameSceneFramePayload payload = {0};

    machine.current = RICH4_SCENE_GAME;
    frame.render_cache = &cache;
    payload.game = &frame;
    g_expected_frame = &frame;
    g_expected_cache = &cache;
    game_update_scene_frame(&machine, &payload);
    assert(frame.render_cache == &cache);
    assert(g_game_updates == 1);
    puts("SDL scene-update union ownership: ok");
    return 0;
}
