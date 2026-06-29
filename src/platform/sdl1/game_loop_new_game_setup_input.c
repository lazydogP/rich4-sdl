/* R10.1d/A5: setup pointer-up state mutation belongs to app flow; this
 * adapter keeps only audio, trace, scene and session-init side effects. */

#include <stdio.h>

#include "debug/trace.h"
#include "app/new_game_scene_flow.h"
#include "platform/sdl1/game_loop_new_game_setup_input.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/render/scene_loading_render.h"
#include "platform/sdl1/render/scene_new_game_setup_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_lazy_assets.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/runtime_trace.h"

typedef struct SetupDrawAdapterContext {
    SDL_Surface *screen;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    VirtualCursor *cursor;
} SetupDrawAdapterContext;

typedef struct TransitionFrameAdapterContext {
    SDL_Surface *screen;
    Rich4Mkf *mkf;
    GamePreviewAssets *assets;
    NewGameSettings *settings;
    GameSession *session;
    const Rich4Config *config;
    const Rich4SdlConfig *sdl_config;
    const char *music_dir;
    VirtualCursor *cursor;
    GameRenderCache *render_cache;
    Rich4Audio *audio;
    int *main_map_music_track;
    int *main_map_music_active;
    int *trace_count;
    Rich4SceneMachine *scene_machine;
} TransitionFrameAdapterContext;

typedef struct TransitionDrawAdapterContext {
    SDL_Surface *screen;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    VirtualCursor *cursor;
} TransitionDrawAdapterContext;

static void play_setup_ok(void *opaque)
{
    Rich4Audio *audio = opaque;
    if (audio != NULL) rich4_audio_play_ok(audio);
}

static void play_setup_transition(void *opaque)
{
    Rich4Audio *audio = opaque;
    if (audio != NULL) rich4_audio_play_new_game_transition(audio);
}

static void draw_setup_frame(void *opaque, int transition)
{
    SetupDrawAdapterContext *context = opaque;
    if (transition) runtime_profile_mark("setup.exit.begin");
    if (context != NULL)
        draw_new_game_setup(
            context->screen, context->assets, context->settings,
            context->cursor, transition);
}

static void complete_transition(void *opaque)
{
    TransitionFrameAdapterContext *context = opaque;
    unsigned int characters = 0;
    unsigned int started;
    int player;
    if (context == NULL) return;
    reset_game_start_trace();
    *context->trace_count = 0;
    trace_game_start("transition: loading");
    runtime_profile_mark("setup.exit.complete");
    started = runtime_profile_now();
    draw_loading_scene(context->screen, context->mkf);
    rich4_sdl1_present(context->screen);
    runtime_profile_stage("loading.present", started);
    runtime_profile_mark("loading.presented");
    /* WinMain 0x401CDC..0x401CE1 presents Data[601] before loading the
     * map. 0x401543 has no fixed sleep; loading occupies the real work. */
    for (player = 0; player < context->settings->selected_count &&
                     player < GAME_MAX_PLAYERS; player++) {
        int character = context->settings->selected_characters[player];
        if (character >= 0 && character < 12) characters |= 1u << character;
    }
    if (game_prepare_asset_scope(context->assets, 2,
            context->settings->stage * GAME_MAPS_PER_STAGE + context->settings->map,
            characters) < 0) {
        fprintf(stderr, "error: new game assets could not be loaded\n");
        context->scene_machine->running = 0;
        return;
    }
    started = runtime_profile_now();
    game_lazy_assets_prepare_intro(context->assets);
    runtime_profile_stage("intro.assets", started);
    trace_game_start("transition: before init");
    started = runtime_profile_now();
    init_game_session(
        context->session, context->assets, context->settings,
        context->config, context->render_cache,
        game_route_count_for_map(context->assets, context->settings));
    runtime_profile_stage("session.init", started);
    /* sub_415872 starts the BGM playlist with sub_454D91(1): CD
     * track 2 (or RICH08.MID fallback), independently of map.
     * MIDI13.MID is a separate playmid cue, not the BGM. */
    if (context->config->music != 0) {
        *context->main_map_music_track = 0;
        *context->main_map_music_active = start_music_bank_track(
            context->audio, context->music_dir,
            context->sdl_config->music_bank, 0, "main map");
    } else {
        *context->main_map_music_active = 0;
        rich4_audio_stop_music(context->audio);
    }
}

static void draw_transition_frame(void *opaque)
{
    TransitionDrawAdapterContext *context = opaque;
    if (context != NULL && context->settings->transition_timer == 0)
        runtime_profile_mark("setup.exit.frame");
    if (context != NULL)
        draw_new_game_setup(
            context->screen, context->assets, context->settings,
            context->cursor, 1);
}

static void mark_transition_ready(void *opaque)
{
    if (opaque != NULL)
        trace_game_start("transition: preview state");
}

void game_update_new_game_transition_scene(
    SDL_Surface *screen, Rich4Mkf *mkf,
    GamePreviewAssets *assets, NewGameSettings *settings,
    GameSession *session, const Rich4Config *config,
    const Rich4SdlConfig *sdl_config, const char *music_dir,
    VirtualCursor *cursor,
    GameRenderCache *render_cache,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    Rich4Audio *audio, int *main_map_music_track,
    int *main_map_music_active, int *game_preview_trace_count,
    Rich4SetupTransitionInput setup_transition_input, Uint32 now)
{
    TransitionFrameAdapterContext context;
    TransitionDrawAdapterContext draw_context;
    Rich4AppNewGameTransitionServices services;
    if (screen == NULL || settings == NULL || assets == NULL ||
        cursor == NULL ||
        session == NULL || config == NULL || sdl_config == NULL ||
        scene_machine == NULL || app_state == NULL || audio == NULL ||
        main_map_music_track == NULL || main_map_music_active == NULL ||
        game_preview_trace_count == NULL ||
        *app_state != RICH4_SCENE_INTRO)
        return;
    context.screen = screen;
    context.mkf = mkf;
    context.assets = assets;
    context.settings = settings;
    context.session = session;
    context.config = config;
    context.sdl_config = sdl_config;
    context.music_dir = music_dir;
    context.cursor = cursor;
    context.render_cache = render_cache;
    context.audio = audio;
    context.main_map_music_track = main_map_music_track;
    context.main_map_music_active = main_map_music_active;
    context.trace_count = game_preview_trace_count;
    context.scene_machine = scene_machine;
    draw_context.screen = screen;
    draw_context.assets = assets;
    draw_context.settings = settings;
    draw_context.cursor = cursor;
    services.complete_context = &context;
    services.draw_context = &draw_context;
    services.scene_ready_context = game_preview_trace_count;
    services.complete = complete_transition;
    services.draw = draw_transition_frame;
    services.scene_ready = mark_transition_ready;
    rich4_app_new_game_transition_frame_update(
        settings, (uint32_t)now, setup_transition_input,
        scene_machine, app_state, &services);
}

void game_update_new_game_setup_scene(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    NewGameSettings *settings, VirtualCursor *cursor,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    Uint32 now, Rich4Audio *audio)
{
    SetupDrawAdapterContext draw_context;
    Rich4AppNewGameSetupServices services;
    if (screen == NULL || assets == NULL || settings == NULL ||
        cursor == NULL || app_state == NULL)
        return;
    draw_context.screen = screen;
    draw_context.assets = assets;
    draw_context.settings = settings;
    draw_context.cursor = cursor;
    services.audio_context = audio;
    services.draw_context = &draw_context;
    services.play_ok = play_setup_ok;
    services.play_transition = play_setup_transition;
    services.draw = draw_setup_frame;
    rich4_app_new_game_setup_frame_update(
        settings, cursor->x, cursor->y, (uint32_t)now,
        scene_machine, app_state, &services);
}

int game_handle_new_game_setup_input(
    NewGameSettings *settings, const VirtualCursor *cursor,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    const Rich4Config *config, const char *music_dir, Rich4Audio *audio)
{
    Rich4AppNewGameSetupInputResult result;
    if (app_state == NULL || *app_state != RICH4_SCENE_NEW_GAME)
        return 0;
    result = rich4_app_new_game_setup_pointer_up(
        settings, cursor != NULL ? cursor->x : 0,
        cursor != NULL ? cursor->y : 0, scene_machine, app_state);
    if (result.action == RICH4_NEW_GAME_SETUP_CHARACTER_TOGGLED)
        rich4_audio_play_ok(audio);
    else if (result.action == RICH4_NEW_GAME_SETUP_SUBMIT) {
        rich4_trace_emit(RICH4_TRACE_NEW_GAME_SUBMIT, 0,
                         result.selected_count, result.map);
        rich4_audio_play_target_confirm(audio);
    } else if (result.action == RICH4_NEW_GAME_SETUP_EXIT) {
        rich4_audio_play_cancel(audio);
        if (config->music != 0) start_menu_music(audio, music_dir);
    } else if (result.action == RICH4_NEW_GAME_SETUP_DROPDOWN_OPENED) {
        rich4_audio_play_select(audio);
    } else if (result.action == RICH4_NEW_GAME_SETUP_MAP_CHANGED) {
        rich4_audio_play_ok(audio);
    }
    return 1;
}
