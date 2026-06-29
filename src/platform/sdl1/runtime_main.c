#include <SDL/SDL.h>
#ifdef _WIN32
#undef main
#include <windows.h>
#endif

#include <stdint.h>

#include "app/audio_event.h"
#include "app/new_game_setup_flow.h"
#include "app/scene.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "core/game_constants.h"
#include "core/rng.h"
#include "platform/sdl1/test_adapter/diff_adapter.h"
#include "platform/sdl1/input/input_config_sdl.h"
#include "resource/mkf.h"
#include "platform/sdl1/game_loop_frame_begin.h"
#include "platform/sdl1/game_loop_frame_end.h"
#include "platform/sdl1/render/game_render_cache.h"
#include "platform/sdl1/game_loop_event_poll.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_route_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_asset_bootstrap.h"
#include "platform/sdl1/runtime_audio_bootstrap.h"
#include "platform/sdl1/runtime_config_bootstrap.h"
#include "platform/sdl1/runtime_input_bootstrap.h"
#include "platform/sdl1/runtime_input_config.h"
#include "platform/sdl1/runtime_initial_scene.h"
#include "platform/sdl1/runtime_loop_state_bootstrap.h"
#include "platform/sdl1/runtime_options_bootstrap.h"
#include "platform/sdl1/runtime_resource_bootstrap.h"
#include "platform/sdl1/runtime_shutdown.h"
#include "platform/sdl1/runtime_sdl_bootstrap.h"
#include "platform/sdl1/runtime_startup_failure.h"
#include "platform/sdl1/runtime_state_bootstrap.h"
#include "platform/sdl1/text/runtime_text.h"
#include "platform/sdl1/runtime_window_bootstrap.h"
#include "resource/resource_store.h"
#include "platform/sdl1/runtime_trace.h"
#include "platform/sdl1/render/game_flc_cache.h"

/* Platform runtime entry and lifecycle wiring stay here; scene, frame, input,
 * and presentation implementations live in their focused adapters. */

/* The runtime owns process-level resources and the main event loop.  Scene,
 * frame, input, update, and rendering details are exposed through focused
 * platform seams; their implementation ownership is documented with those
 * seams rather than repeated here. */

/* Missing-asset fallback keeps the old visible behavior: draw a clean
 * background and report the diagnostic on stderr instead of drawing the
 * removed placeholder/debug overlay. */

static int rich4_sdl1_frame_digest(
    SDL_Surface *surface, uint32_t *out_digest)
{
    uint32_t digest = 2166136261u;
    int y;

    if (surface == NULL || surface->pixels == NULL || out_digest == NULL)
        return 0;
    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0)
        return 0;
    for (y = 0; y < surface->h; y++) {
        const unsigned char *row = (const unsigned char *)surface->pixels +
            y * surface->pitch;
        int x;
        for (x = 0; x < surface->w * surface->format->BytesPerPixel; x++) {
            digest ^= row[x];
            digest *= 16777619u;
        }
    }
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    *out_digest = digest;
    return 1;
}


int rich4_sdl1_main(int argc, char **argv)
{
    SdlOptions options;
    Rich4InputConfig input;
    DrawSelection selection;
    VirtualCursor cursor;
    MenuAssets menu_assets;
    CursorAssets cursor_assets;
    HelpAssets help_assets;
    HelpPanelState help_panel;
    SmallMapPanelState small_map_panel;
    GamePreviewAssets game_preview_assets;
    NewGameSettings new_game_settings;
    GameSession game_session;
    GameRenderCache render_cache = {0};
    Rich4Config rich4_config;
    Rich4SdlConfig rich4_sdl_config;
    StockPanelState stock_panel;
    char mkf_path[512];
    Rich4Mkf *mkf;
    Rich4Mkf *effect_mkf = NULL;
    Rich4Mkf *speaking_mkf = NULL;
    ResourceStore *resource_store;
    Rich4Audio *audio = NULL;
    SDL_Surface *screen;
    SDL_Surface *background;
    SDL_Surface *cursor_free_frame = NULL;
    SDL_Joystick *joystick = NULL;
    Rich4SceneMachine scene_machine;
    Rich4SceneId *scene_state;
    int running = 1;
    int mouse_left_down = 0;
    int last_menu_hover = -1;
    int last_top_button_hover = -1;
    int last_new_game_character_hover = -1;
    int game_preview_trace_count = 0;
    int main_map_music_track = 0;
    int main_map_music_active = 0;
    int shop_music_restore_pending = 0;
    int bank_music_active = 0;
    int lottery_music_active = 0;
    int magic_house_music_active = 0;
    int auction_music_active = 0;
    int confinement_music_kind = GAME_CONFINEMENT_NONE;
    Uint32 ending_wait_started = 0;
    int ending_wait_active = 0;
    int ending_map_mask = 0;
    size_t smoke_frame_count = 0;
    uint32_t smoke_frame_digest = 0;
    uint32_t smoke_sequence_digest = 2166136261u;

    game_init_runtime_loop_state(&scene_machine, &game_session.audio_events);
    scene_state = &scene_machine.current;

    if (!game_parse_runtime_options(argc, argv, &options))
        return 2;
    if (!r4diff_init()) return 3;
    rich4_sdl_text_set_font_path(options.font_path);
    game_load_runtime_configs(&rich4_config, &rich4_sdl_config);
    game_init_runtime_panel_states(
        &game_session.presentation.options,
        &game_session.presentation.ai_settings, &help_panel,
        &small_map_panel, &game_session.presentation.card_panel,
        &game_session.presentation.tool_panel, &stock_panel);

    game_load_runtime_input_config(&input, options.input_config);

    if (!game_open_runtime_resources(&resource_store, &mkf, &options,
                                     mkf_path, sizeof(mkf_path)))
        return 1;
    {
        RuntimeAssetBootstrapContext asset_bootstrap = {
            resource_store, &options, mkf, &effect_mkf, &speaking_mkf,
            &menu_assets, &cursor_assets, &game_preview_assets, &help_assets
        };
        game_load_runtime_asset_bundle(&asset_bootstrap);
    }

    {
        RuntimeSdlBootstrapContext sdl_bootstrap = { &game_preview_assets, &cursor_assets, &menu_assets, resource_store };
        if (!game_init_runtime_sdl(&sdl_bootstrap))
            return 1;
    }
    rich4_sdl1_set_deterministic_ticks(options.smoke_frame_limit > 0);
    /* Smoke runs are verification fixtures: keep their game bootstrap and
     * any early AI/dynamic-object choices reproducible.  Interactive runs
     * retain the original time-based seed. */
    rich4_rng_seed(r4diff_active() ? r4diff_ticks() : options.smoke_frame_limit > 0
        ? 0x4d337f37u
#ifdef _WIN32
        : (unsigned int)GetTickCount());
#else
        : (unsigned int)rich4_sdl1_ticks());
#endif
    {
        RuntimeAudioBootstrapContext audio_bootstrap = {
            &audio, effect_mkf, speaking_mkf
        };
        game_init_runtime_audio(&audio_bootstrap);
    }
    game_init_runtime_input_devices(&joystick);

    {
        RuntimeWindowBootstrapContext window_bootstrap = {
            &screen, audio, joystick, &game_preview_assets,
            &cursor_assets, &menu_assets, resource_store
        };
        if (!game_open_runtime_window(&window_bootstrap)) {
            return 1;
        }
    }
    {
        RuntimeInitialSceneContext initial_scene = {
            screen, &background, &cursor_free_frame, &selection,
            &cursor, &new_game_settings, &game_session, &options,
            &rich4_config, audio, mkf, &menu_assets, &cursor_assets
        };
    if (!game_prepare_initial_scene(&initial_scene)) {
            RuntimeStartupFailureContext failure = {
                audio, joystick, screen, &game_preview_assets,
                &cursor_assets, &menu_assets, resource_store
            };
            game_abort_runtime_startup(&failure);
            return 1;
        }
    }
    if (options.smoke_start_game) {
        /* The bounded game smoke intentionally starts from the same session
         * initializer used by the new-game transition. It is a runtime
         * rendering check, not a second game-start implementation. */
        rich4_new_game_settings_init(&new_game_settings, 0);
        new_game_settings.selected_count = 2;
        new_game_settings.selected_characters[0] = 0;
        new_game_settings.selected_characters[1] = 1;
        new_game_settings.selected_ai[0] = 1;
        new_game_settings.selected_ai[1] = 1;
        if (game_prepare_asset_scope(&game_preview_assets, 2, 0, 3u) < 0)
            return 1;
        init_game_session(
            &game_session, &game_preview_assets, &new_game_settings,
            &rich4_config, &render_cache,
            game_route_count_for_map(&game_preview_assets,
                                      &new_game_settings));
        if (!game_session.state.active) {
            fprintf(stderr, "error: SDL game smoke session initialization failed\n");
            RuntimeStartupFailureContext failure = {
                audio, joystick, screen, &game_preview_assets,
                &cursor_assets, &menu_assets, resource_store
            };
            game_abort_runtime_startup(&failure);
            return 1;
        }
        rich4_scene_machine_request_transition(
            &scene_machine, scene_state, RICH4_SCENE_GAME);
    }
    while (running && scene_machine.running) {
        if (!r4diff_barrier(screen,(int)*scene_state)) break;
        rich4_sdl1_begin_frame();
        rich4_audio_begin_visual_frame(audio);
        runtime_profile_frame_begin((int)*scene_state);
        game_lazy_assets_begin_frame(&game_preview_assets);
        game_flc_cache_begin_frame(&game_preview_assets);
        if (game_sync_runtime_asset_scope(&game_preview_assets, *scene_state,
                &game_session, &new_game_settings) < 0) {
            fprintf(stderr, "error: selected scene assets could not be loaded\n");
            running = 0;
            break;
        }
#ifdef _WIN32
        rich4_app_sync_rng_clock(&game_session, r4diff_active() ? r4diff_ticks() : options.smoke_frame_limit > 0
            ? rich4_sdl1_ticks() : (uint32_t)GetTickCount(), rich4_sdl1_ticks());
#else
        rich4_app_sync_rng_clock(&game_session, rich4_sdl1_ticks(), rich4_sdl1_ticks());
#endif
        int host_pointer_sync_needed = 0;
        Rich4SetupTransitionInput setup_transition_input =
            RICH4_SETUP_TRANSITION_INPUT_NONE;
        game_begin_runtime_frame(
            &cursor, *scene_state, &game_session.presentation.options,
            &rich4_config, audio, &game_session.presentation.card_panel,
            &game_session, &game_preview_assets, &new_game_settings,
            mouse_left_down);
        {
            GamePointerMotionContext pointer_motion = { *scene_state, rich4_config.view, screen->w, screen->h, mouse_left_down, &new_game_settings, &game_session, &cursor, &game_session.presentation.ai_settings, &game_session.presentation.options, &small_map_panel, &game_preview_assets, &stock_panel, audio };
            GameMouseButtonDownInputContext mouse_button_down = { scene_state, &rich4_config, &rich4_sdl_config, &game_preview_assets, &game_session, &new_game_settings, &game_session.presentation.options, &scene_machine, &help_panel, &help_assets, &small_map_panel, &game_session.presentation.ai_settings, &game_session.presentation.card_panel, &game_session.presentation.tool_panel, &stock_panel, &cursor, audio, &mouse_left_down, 0 };
            GameMouseButtonUpInputContext mouse_button_up = { *scene_state, &game_session, &game_preview_assets, &new_game_settings, &stock_panel, &help_panel, &help_assets, &small_map_panel, &game_session.presentation.tool_panel, &cursor, audio, cursor_free_frame != NULL ? cursor_free_frame : screen, 0, &mouse_left_down, &setup_transition_input, &shop_music_restore_pending, &rich4_config, &rich4_sdl_config, &game_session.presentation.options, &scene_machine, scene_state, options.music_dir };
            GameKeyDownInputContext key_down = { *scene_state, &game_session, &stock_panel, &small_map_panel, &game_preview_assets, &new_game_settings, &help_panel, &help_assets, &rich4_config, audio, &cursor, &game_session.presentation.ai_settings, RICH4_APP_KEY_UNKNOWN, 0, &setup_transition_input, input.confirm, input.cancel };
            GameKeyUpModalsContext key_up = { *scene_state, &game_session, audio };
            GameJoyButtonDownInputContext joy_button_down_input = { *scene_state, &game_session, &small_map_panel, &help_panel, &cursor, &setup_transition_input, &new_game_settings, &game_session.presentation.ai_settings, 0, input.joy_confirm, input.joy_cancel };
            GameEventPollContext event_poll = { &running, scene_state, screen, &mouse_left_down, &pointer_motion, &mouse_button_down, &mouse_button_up, &key_down, &key_up, &joy_button_down_input };
            game_poll_runtime_events(&event_poll);
        }
        {
            GameInputFinalizeCursorContext input_cursor = {
                scene_state, &new_game_settings, &game_session.presentation.ai_settings,
                &game_session, &cursor, &input, joystick,
                &host_pointer_sync_needed
            };
            GameInputHoverContext input_hover = {
                *scene_state, rich4_config.view, &rich4_sdl_config,
                &game_preview_assets,
                &new_game_settings, &game_session, &cursor, &menu_assets,
                &game_session.presentation.options, &help_panel, &small_map_panel,
                &game_session.presentation.ai_settings, &game_session.presentation.card_panel,
                &game_session.presentation.tool_panel, &stock_panel,
                audio, &last_menu_hover, &last_new_game_character_hover,
                &last_top_button_hover
            };
            GameCancelInputContext input_cancel = {
                scene_state, &game_session, &game_preview_assets,
                &new_game_settings, &game_session.presentation.options, &scene_machine,
                &rich4_config, &stock_panel, &small_map_panel,
                &game_session.presentation.card_panel,
                &game_session.presentation.tool_panel, &game_session.presentation.ai_settings,
                &cursor, audio,
                options.music_dir, &shop_music_restore_pending, &running
            };
            GameConfirmInputContext input_confirm = {
                scene_state, &menu_assets, &game_preview_assets,
                &game_session, &new_game_settings, &rich4_config,
                &rich4_sdl_config, &game_session.presentation.options, &scene_machine,
                &help_panel, &help_assets, &small_map_panel,
                &game_session.presentation.ai_settings, &game_session.presentation.card_panel,
                &game_session.presentation.tool_panel, &stock_panel,
                &cursor, audio, options.music_dir, mouse_left_down,
                &host_pointer_sync_needed, &running
            };
            GameInputFrameFinalizeContext input_frame = {
                &game_session, &input_cursor,
                &input_hover, &input_cancel, &input_confirm
            };
            GamePreviewFrameContext game_frame = {
                screen, &game_preview_assets, &new_game_settings,
                &game_session, &render_cache, &rich4_config,
                &rich4_sdl_config, audio,
                options.music_dir, &main_map_music_track,
                &main_map_music_active, &confinement_music_kind,
                &bank_music_active,
                &lottery_music_active, &magic_house_music_active,
                &auction_music_active,
                &shop_music_restore_pending, &help_panel, &help_assets,
                &small_map_panel, &game_session.presentation.options,
                &scene_machine, scene_state, &ending_map_mask, &running,
                &ending_wait_started, &ending_wait_active,
                &game_session.presentation.ai_settings,
                &game_session.presentation.card_panel,
                &game_session.presentation.tool_panel, &stock_panel,
                cursor_free_frame, &cursor, &host_pointer_sync_needed,
                &game_preview_trace_count
            };
            GameSceneIntroSurfaceContext intro_surface = {
                screen, &cursor, &render_cache
            };
            GameSceneIntroContentContext intro_content = {
                mkf, &game_preview_assets, &new_game_settings, &game_session,
                &rich4_config, &rich4_sdl_config
            };
            GameSceneIntroAudioContext intro_audio = {
                options.music_dir, audio, &main_map_music_track,
                &main_map_music_active
            };
            GameSceneIntroTransitionContext intro_transition = {
                &scene_machine, &game_preview_trace_count,
                setup_transition_input
            };
            GameSceneIntroFrameContext intro_frame = {
                &intro_surface, &intro_content, &intro_audio,
                &intro_transition, 0
            };
            GameSceneSetupStateContext setup_state = {
                &game_preview_assets, &new_game_settings, &cursor
            };
            GameSceneSetupFrameContext setup_frame = {
                screen, &setup_state, &scene_machine, audio, 0
            };
            GameSceneFallbackPreviewContext fallback_preview = {
                &game_preview_assets, &new_game_settings, &game_session,
                &render_cache, &rich4_config
            };
            GameSceneFallbackMenuContext fallback_menu_context = {
                screen, background, &menu_assets, &cursor,
                &game_preview_assets, &game_session
            };
            GameSceneFallbackOptionsMenuContext fallback_options_menu = {
                screen, background, &game_preview_assets, &menu_assets,
                &game_session.presentation.options, &help_panel, &help_assets
            };
            GameSceneFallbackOptionsGameContext fallback_options_game = {
                screen, background, &fallback_preview,
                &game_session.presentation.options
            };
            GameSceneFallbackFrameContext fallback_frame = {
                &fallback_menu_context, &fallback_options_menu,
                &fallback_options_game
            };
            GameSceneFramePayload scene_payload = { 0 };
            switch (scene_machine.current) {
            case RICH4_SCENE_GAME:
                scene_payload.game = &game_frame;
                break;
            case RICH4_SCENE_INTRO:
                scene_payload.intro = &intro_frame;
                break;
            case RICH4_SCENE_NEW_GAME:
                scene_payload.setup = &setup_frame;
                break;
            case RICH4_SCENE_MENU:
            case RICH4_SCENE_OPTIONS:
                scene_payload.fallback = &fallback_frame;
                break;
            default:
                scene_payload.fallback = &fallback_frame;
                break;
            }
            VirtualCursorPresentationContext cursor_presentation = {
                screen, cursor_free_frame, &cursor_assets, &cursor,
                &host_pointer_sync_needed, *scene_state, &new_game_settings,
                &game_session, &help_panel, &small_map_panel,
                &game_session.presentation.ai_settings, &game_preview_assets,
                &game_session.presentation.tool_panel, audio
            };
            game_run_runtime_frame_end(
                &input_frame, &scene_machine, &scene_payload,
                &cursor_presentation, &game_preview_assets);
            runtime_profile_frame_end();
            if (options.smoke_frame_limit > 0) {
                if (!rich4_sdl1_frame_digest(screen, &smoke_frame_digest)) {
                    fprintf(stderr, "error: SDL smoke framebuffer read failed\n");
                    running = 0;
                    continue;
                }
                {
                    int digest_byte;
                    for (digest_byte = 0; digest_byte < 4; digest_byte++) {
                        smoke_sequence_digest ^=
                            (smoke_frame_digest >> (digest_byte * 8)) & 0xffu;
                        smoke_sequence_digest *= 16777619u;
                    }
                }
                smoke_frame_count++;
                if (smoke_frame_count >= options.smoke_frame_limit) {
                    printf("SDL smoke: frames=%lu scene=%d framebuffer=%08x "
                           "sequence=%08x\n",
                           (unsigned long)smoke_frame_count,
                           (int)scene_machine.current,
                           (unsigned int)smoke_frame_digest,
                           (unsigned int)smoke_sequence_digest);
                    fflush(stdout);
                    running = 0;
                }
            }
        }
    }

    {
        RuntimeShutdownUiContext shutdown_ui = {
            &help_panel, &small_map_panel, &stock_panel, &render_cache
        };
        RuntimeShutdownAssetContext shutdown_assets = {
            cursor_free_frame, background, &menu_assets,
            &game_preview_assets, &cursor_assets
        };
        RuntimeShutdownServiceContext shutdown_services = {
            joystick, audio, screen, resource_store
        };
        RuntimeShutdownContext shutdown = {
            &shutdown_ui, &shutdown_assets, &shutdown_services
        };
        {
            GameFlcCacheStats stats = game_flc_cache_stats();
            /* Temporary diagnostics share the existing opt-in profiler. */
            if (runtime_profile_now() != 0)
                printf("FLC cache: hits=%lu misses=%lu decoded_frames=%lu rewinds=%lu evictions=%lu live_bytes=%lu peak_bytes=%lu\n",
                    stats.hits, stats.misses, stats.decoded_frames, stats.rewinds, stats.evictions,
                    (unsigned long)stats.live_bytes, (unsigned long)stats.peak_bytes);
        }
        runtime_profile_close();
        game_shutdown_runtime(&shutdown);
    }
    return 0;
}
