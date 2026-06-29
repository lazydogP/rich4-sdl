#include "platform/sdl1/game_lazy_assets.h"
/* R10.1d: preview-frame update/presentation orchestration moved verbatim out
 * of runtime_main.c. The scene loop retains ownership of state, surfaces and
 * the outer Rich4SceneId branch; this seam keeps the original sub-order intact. */

#include "platform/sdl1/game_loop_preview_update.h"
#include "core/rng.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"

#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/game_loop_confirm_route_interaction.h"
#include "platform/sdl1/game_loop_modal_music.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_preview_toolbar_input.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/game_preview_frame.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_flow_api.h"
#include "platform/sdl1/runtime_trace.h"

enum { GAME_ENDING_IDLE_RETURN_MS = 10000 };
enum { GAME_ENDING_JUMP_PREVIEW_MS = 250 };

static int game_first_active_player(const GameSession *session)
{
    int player;
    if (session == NULL) return -1;
    for (player = 0; player < session->state.player_count; player++)
        if ((session->state.players[player].play_state &
             GAME_PLAYER_PLAY_STATE_MASK) != 0)
            return player;
    return -1;
}

static void game_begin_ending_jump_transition(
    GamePreviewFrameContext *context,
    const Rich4AppLifecycleRequest *request)
{
    GameEndingPresentationState *ending;
    int map;
    int winner;

    if (context == NULL || context->session == NULL || request == NULL)
        return;
    ending = &context->session->presentation.ending;
    if (!ending->active) {
        /* Bankruptcy's finish_with_lifecycle clears the ordinary ending
         * snapshot before setting code 2/3.  0x4075C1 then chooses the first
         * active character again, so rebuild that small transient snapshot. */
        winner = game_first_active_player(context->session);
        ending->active = 1;
        ending->winner_player = winner;
        ending->winner_character = winner >= 0
            ? context->session->state.players[winner].character : -1;
        ending->reason = 0;
        ending->assets = 0;
    }
    map = context->settings != NULL
        ? context->settings->map : context->session->state.map;
    if (context->ending_map_mask != NULL && map >= 0 && map < 4)
        *context->ending_map_mask |= 1 << map;

    ending->continuation_active = 0;
    ending->ending_video_pending = 1;
    ending->winner_voice_pending = 0;
    ending->jump_transition_active = 1;
    ending->jump_transition_code = request->code;
    ending->jump_transition_started = rich4_sdl1_ticks();

    /* 0x4075C1 replaces the normal map music with MIDI07 while it waits for
     * the JUMP transition.  The fixed-track helper is given play-once
     * semantics here so the SDL wait has the same finite boundary. */
    if (context->main_map_music_active != NULL)
        *context->main_map_music_active = 0;
    if (context->config != NULL && context->config->music != 0)
        (void)start_fixed_music_once(
            context->audio, context->music_dir, 6,
            "ending transition");
    else
        rich4_audio_stop_music(context->audio);
}

static int game_update_ending_jump_transition(
    GamePreviewFrameContext *context)
{
    GameEndingPresentationState *ending;
    Uint32 now;
    int map_mask;
    int code;

    if (context == NULL || context->session == NULL)
        return 0;
    ending = &context->session->presentation.ending;
    if (!ending->jump_transition_active)
        return 0;
    now = rich4_sdl1_ticks();
    if (now - ending->jump_transition_started <
            GAME_ENDING_JUMP_PREVIEW_MS)
        return 0;
    if (context->audio != NULL &&
        rich4_audio_music_playing(context->audio))
        return 0;

    map_mask = context->ending_map_mask != NULL
        ? *context->ending_map_mask : 0;
    code = ending->jump_transition_code;
    ending->jump_transition_active = 0;
    ending->jump_transition_code = RICH4_APP_LIFECYCLE_NONE;
    ending->ending_video_pending = 0;

    if ((map_mask & 0x0f) == 0x0f) {
        /* The original sets the terminal latch regardless of whether the
         * host video backend can open the AVI. */
        (void)play_ending_avi_sequence(
            context->assets == NULL ? NULL : context->assets->data_dir,
            context->settings == NULL ? 0 : context->settings->stage,
            ending->winner_character, map_mask);
        if (context->running != NULL)
            *context->running = 0;
        return 1;
    }

    rich4_app_reset_session(context->session);
    if (code == RICH4_APP_LIFECYCLE_RESTART_GAME) {
        rich4_scene_machine_request_transition(
            context->scene_machine, context->app_state,
            RICH4_SCENE_NEW_GAME);
    } else {
        if (code == RICH4_APP_LIFECYCLE_OUTER_MENU_AFTER_JUMP &&
            context->ending_map_mask != NULL)
            /* WinMain's code-3 branch returns through LABEL_5, where the
             * original clears lpBuffer__27 before showing the outer menu. */
            *context->ending_map_mask = 0;
        if (context->config != NULL && context->config->music != 0)
            (void)start_menu_music(context->audio, context->music_dir);
        else
            rich4_audio_stop_music(context->audio);
        rich4_scene_machine_request_transition(
            context->scene_machine, context->app_state,
            RICH4_SCENE_MENU);
    }
    return 1;
}

static int game_update_ending_confirmation_animation(
    GamePreviewFrameContext *context)
{
    GameEndingPresentationState *ending;
    int frame_count;

    if (context == NULL || context->session == NULL)
        return 0;
    ending = &context->session->presentation.ending;
    if (!ending->continuation_confirmation_active)
        return 0;
    frame_count = context->assets != NULL &&
        game_asset_ready(context->assets, &context->assets->ui.has_ending_success_animation)
        ? game_asset_graph_frames(context->assets, &context->assets->ui.ending_success_data, context->assets->ui.ending_success_size) : 0;
    if (frame_count > 0 && rich4_animation_clock_step(
            &context->session->presentation.animation_clock[RICH4_ANIMATION_ENDING],
            rich4_sdl1_ticks(), 50)) ending->continuation_confirmation_frame++;
    if (ending->continuation_confirmation_frame < frame_count) return 0;

    ending->continuation_confirmation_active = 0;
    ending->continuation_confirmation_ready = 1;
    /* The real callback returns 4 only after the FLC wait has completed. */
    (void)game_save_load_panel_open_after_ending(
        context->session, context->assets);
    return 1;
}

static int game_consume_lifecycle_request(GamePreviewFrameContext *context)
{
    Rich4AppLifecycleRequest request;
    if (context == NULL || context->session == NULL ||
        context->scene_machine == NULL || context->app_state == NULL ||
        !rich4_app_take_lifecycle(context->session, &request))
        return 0;

    /* The main-thread rand word survives returning to the outer menu. */
    {
        Rich4Rng rng = { context->session->state.ai_rng_state };
        rich4_rng_global_import(&rng);
    }
    switch (request.code) {
    case RICH4_APP_LIFECYCLE_RESTART_GAME:
    case RICH4_APP_LIFECYCLE_OUTER_MENU_AFTER_JUMP:
        game_begin_ending_jump_transition(context, &request);
        /* Keep drawing this frame so the JUMP.MKF/Panel[93] transition is
         * visible before the finite MIDI/video boundary completes. */
        return 0;
    case RICH4_APP_LIFECYCLE_OUTER_MENU:
        rich4_app_reset_session(context->session);
        if (context->config != NULL && context->config->music != 0)
            start_menu_music(context->audio, context->music_dir);
        else
            rich4_audio_stop_music(context->audio);
        rich4_scene_machine_request_transition(
            context->scene_machine, context->app_state,
            RICH4_SCENE_MENU);
        return 1;
    case RICH4_APP_LIFECYCLE_LOAD_GAME:
        if (game_save_load_panel_open_after_ending(
                context->session, context->assets))
            return 1;
        /* If the resource-backed continuation panel cannot be opened, retain
         * the original safe fallback: leave the match through the outer UI. */
        rich4_app_reset_session(context->session);
        rich4_scene_machine_request_transition(
            context->scene_machine, context->app_state,
            RICH4_SCENE_MENU);
        return 1;
    case RICH4_APP_LIFECYCLE_NONE:
    default:
        return 0;
    }
}

static int game_update_ending_idle_return(GamePreviewFrameContext *context)
{
    GameEndingPresentationState *ending;
    int continuation_active;
    Uint32 now;
    Rich4SceneCommand command;
    if (context == NULL || context->session == NULL ||
        context->app_state == NULL || context->ending_wait_started == NULL ||
        context->ending_wait_active == NULL)
        return 0;
    ending = &context->session->presentation.ending;
    if (*context->app_state != RICH4_SCENE_GAME || !ending->active ||
        ending->ending_video_pending ||
        ending->continuation_confirmation_active ||
        context->session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED) {
        *context->ending_wait_active = 0;
        return 0;
    }
    now = rich4_sdl1_ticks();
    if (!*context->ending_wait_active) {
        *context->ending_wait_started = now;
        *context->ending_wait_active = 1;
        return 0;
    }
    if (now - *context->ending_wait_started < GAME_ENDING_IDLE_RETURN_MS)
        return 0;
    *context->ending_wait_active = 0;
    continuation_active = ending->continuation_active;
    if (continuation_active) {
        rich4_app_reset_session(context->session);
        /* The timeout is the original 0x407842 return-1 path: leave the
         * continuation modal and let the outer menu resume its own track. */
        if (context->config != NULL && context->config->music != 0)
            (void)start_menu_music(context->audio, context->music_dir);
        else
            rich4_audio_stop_music(context->audio);
    }
    command.type = RICH4_SCENE_COMMAND_ENDING_TIMEOUT;
    command.return_scene = RICH4_SCENE_NONE;
    (void)rich4_scene_machine_handle(context->scene_machine, &command);
    if (context->scene_machine != NULL)
        *context->app_state = context->scene_machine->current;
    return 1;
}

void game_update_preview_frame(GamePreviewFrameContext *context)
{
    int route_interaction_was_active;
    int input_refresh_was_active;
    GameModalMusicContext modal_music;
    unsigned int span_started;

    if (context == NULL) return;

    if (game_consume_lifecycle_request(context)) return;

    if (game_update_ending_jump_transition(context)) return;

    if (game_update_ending_confirmation_animation(context)) return;

    /* sub_404165 keeps the refreshed save slots visible for 500 ms after
     * WM_LBUTTONDOWN commits the file. */
    if (context->session != NULL)
        (void)rich4_presentation_save_load_panel_finish_confirmation(
            &context->session->presentation.save_load, rich4_sdl1_ticks());

    /* A confirmed ending input has already opened the load panel by this
     * point. The original ten-second timeout applies only before that choice. */
    if (game_update_ending_idle_return(context)) return;

    /* sub_4018E7 keeps dispatching all window messages while help and the
     * full map own the nested modal loop. The original playlist's end
     * notification therefore remains live while simulation is frozen. */
    span_started = runtime_profile_now();
    game_roll_main_map_music_if_finished(
        context->audio, context->music_dir, context->sdl_config->music_bank,
        context->config->music != 0, context->main_map_music_track,
        context->main_map_music_active);
    runtime_profile_span("update", "music.roll", span_started, -1, 0);
    if (game_draw_preview_modal_overlay(
            context->screen, context->assets, context->settings,
            context->session, context->help_assets,
            context->help_panel, context->small_map_panel,
            context->cursor))
        return;

    route_interaction_was_active =
        context->session->presentation.route_interaction.active;
    input_refresh_was_active =
        context->session->presentation.dice.input_refresh_active;
    game_capture_modal_music_context(
        &modal_music, context->session, context->audio, context->music_dir,
        context->sdl_config->music_bank, context->config->music != 0,
        context->main_map_music_track, context->main_map_music_active,
        context->confinement_music_kind, context->bank_music_active,
        context->lottery_music_active,
        context->magic_house_music_active, context->auction_music_active);
    game_restore_shop_music_if_pending(
        context->audio, context->music_dir, context->sdl_config->music_bank,
        context->config->music != 0, context->main_map_music_track,
        context->main_map_music_active, context->shop_music_restore_pending);

    if (context->cursor->confirm_pressed &&
        !rich4_app_sale_interaction_is_active(context->session)) {
        GamePreviewToolbarConfirmContext toolbar = {
            context->config,
            context->sdl_config,
            context->session,
            context->assets,
            context->settings,
            context->options_panel,
            context->scene_machine,
            context->app_state,
            context->help_panel,
            context->help_assets,
            context->small_map_panel,
            context->ai_settings_panel,
            context->card_panel,
            context->tool_panel,
            context->stock_panel,
            context->cursor_free_frame != NULL
                ? context->cursor_free_frame : context->screen,
            context->cursor,
            context->audio
        };
        if (!game_handle_preview_toolbar_confirm(&toolbar))
            game_handle_preview_minimap_rotate_confirm(&toolbar);
    }
    if (context->trace_count != NULL && *context->trace_count < 3)
        trace_game_start("preview: update begin");
    if (context->card_panel->phase == CARD_PANEL_CLOSED &&
        context->tool_panel->phase == TOOL_PANEL_CLOSED &&
        !rich4_app_stock_interaction_is_active(context->session) &&
        !rich4_app_sale_interaction_is_active(context->session))
        update_game_session(
            context->session, context->assets, context->settings,
            context->config, context->audio, rich4_sdl1_ticks(),
            context->cursor->x);
    else if (context->card_panel->phase == CARD_PANEL_CLOSED &&
             context->tool_panel->phase == TOOL_PANEL_SPEECH &&
             !rich4_app_sale_interaction_is_active(context->session))
        game_update_property_speech(
            context->session, context->audio, rich4_sdl1_ticks());
    span_started = runtime_profile_now();
    game_tool_panel_finish_speech(
        context->tool_panel, context->session,
        context->assets, context->settings,
        context->cursor, context->host_pointer_sync_needed,
        context->audio);
    game_update_modal_music(&modal_music);
    runtime_profile_span("update", "modal.music_and_speech", span_started, -1, 0);
    if (!input_refresh_was_active &&
        game_sync_dice_action_cursor(context->session, context->cursor)) {
        /* 0x418DB0 positions the pointer relative to the current actor
         * action panel when sub_40C912(0) requests an input refresh. */
        *context->host_pointer_sync_needed = 1;
    }
    if (game_sync_route_interaction_initial_cursor(
            *context->app_state, context->session, context->assets,
            context->cursor, route_interaction_was_active))
        *context->host_pointer_sync_needed = 1;
    span_started = runtime_profile_now();
    game_emit_session_audio_events(
        context->session, context->assets, context->settings,
        context->audio);
    runtime_profile_span("update", "audio.events", span_started, -1, 0);
    runtime_profile_frame_phase(1);
    draw_game_preview_frame(
        context->screen, context->assets, context->settings,
        context->session, context->render_cache,
        context->ai_settings_panel,
        context->card_panel,
        context->tool_panel, context->stock_panel,
        context->config->view,
        context->config->calendar_view,
        context->trace_count);
}
