/* R10.1e: platform scene callbacks retain resource/render ownership while
 * application code selects the active scene and dispatches the frame. */

#include "app/scene_frame_flow.h"
#include "platform/sdl1/game_loop_new_game_setup_input.h"
#include "platform/sdl1/game_loop_preview_update.h"
#include "platform/sdl1/game_loop_scene_presentation.h"
#include "platform/sdl1/game_loop_scene_update.h"
#include "platform/sdl1/runtime.h"

static void update_game_scene(void *opaque)
{
    GamePreviewFrameContext *frame = opaque;
    if (frame != NULL) game_update_preview_frame(frame);
}

static void update_intro_scene(void *opaque)
{
    GameSceneIntroFrameContext *intro = opaque;
    GameSceneIntroSurfaceContext *surface;
    GameSceneIntroContentContext *content;
    GameSceneIntroAudioContext *audio;
    GameSceneIntroTransitionContext *transition;
    if (intro == NULL) return;
    surface = intro->surface;
    content = intro->content;
    audio = intro->audio;
    transition = intro->transition;
    game_update_new_game_transition_scene(
        surface->screen, content->mkf, content->assets, content->settings,
        content->session, content->config, content->sdl_config,
        audio->music_dir, surface->cursor, surface->render_cache,
        transition->machine, &transition->machine->current, audio->audio,
        audio->main_map_music_track, audio->main_map_music_active,
        transition->trace_count, transition->setup_transition_input,
        intro->now);
}

static void update_new_game_scene(void *opaque)
{
    GameSceneSetupFrameContext *setup = opaque;
    Rich4SceneMachine *machine;
    if (setup == NULL) return;
    machine = setup->machine;
    game_update_new_game_setup_scene(
        setup->screen, setup->state->assets, setup->state->settings,
        setup->state->cursor, machine, &machine->current, setup->now,
        setup->audio);
}

static void update_fallback_menu_scene(void *opaque)
{
    game_draw_scene_fallback_menu(opaque);
}

static void update_fallback_options_menu_scene(void *opaque)
{
    game_draw_scene_fallback_options_menu(opaque);
}

static void update_fallback_options_game_scene(void *opaque)
{
    game_draw_scene_fallback_options_game(opaque);
}

void game_update_scene_frame(Rich4SceneMachine *machine,
                             GameSceneFramePayload *payload)
{
    Rich4AppSceneFrameServices services = {0};
    Rich4SceneId scene;
    Uint32 now;
    if (machine == NULL || payload == NULL) return;
    now = rich4_sdl1_ticks();
    /* GameSceneFramePayload is a union. Only the member tagged by the
     * current scene is live; writing intro/setup `now` during a game frame
     * overwrites GamePreviewFrameContext::render_cache. */
    switch (machine->current) {
    case RICH4_SCENE_GAME:
        services.game_context = payload->game;
        break;
    case RICH4_SCENE_INTRO:
        services.intro_context = payload->intro;
        if (payload->intro != NULL) payload->intro->now = now;
        break;
    case RICH4_SCENE_NEW_GAME:
        services.new_game_context = payload->setup;
        if (payload->setup != NULL) payload->setup->now = now;
        break;
    case RICH4_SCENE_MENU:
    case RICH4_SCENE_OPTIONS:
    default:
        if (payload->fallback != NULL) {
            services.fallback_menu_context = payload->fallback->menu;
            services.fallback_options_menu_context =
                payload->fallback->options_menu;
            services.fallback_options_game_context =
                payload->fallback->options_game;
        }
        break;
    }
    services.game = update_game_scene;
    services.intro = update_intro_scene;
    services.new_game = update_new_game_scene;
    services.fallback_menu = update_fallback_menu_scene;
    services.fallback_options_menu = update_fallback_options_menu_scene;
    services.fallback_options_game = update_fallback_options_game_scene;
    rich4_app_scene_frame_update(
        machine, &machine->current, now, &services, &scene);
}
