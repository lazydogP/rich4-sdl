#include "platform/sdl1/game_lazy_assets.h"
/* R10.1d: options-scene confirmation/input dispatch moved verbatim out of
 * runtime_main.c. */

#include <time.h>
#include "platform/sdl1/test_adapter/diff_adapter.h"
#include <string.h>
#include "app/app_constants.h"
#include "app/game_session.h"
#include "app/new_game_setup_flow.h"
#include "app/options_game_flow.h"
#include "app/options_flow.h"
#include "platform/sdl1/game_loop_options_input.h"
#include "platform/sdl1/game_loop_audio_events.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/input_adapter.h"

static int options_apply_surrender(
    GameSession *session, Rich4Audio *audio)
{
    Rich4AudioEventQueue *audio_events;

    audio_events = rich4_app_session_audio_borrow(session);
    if (!rich4_app_surrender_current_player(
            session, audio_events, rich4_sdl1_ticks())) {
        game_session_audio_flush(session, audio);
        return 0;
    }
    game_session_audio_flush(session, audio);
    return 1;
}

static int options_apply_game_action(
    int action, Rich4OptionsPanelState *panel, Rich4Config *config,
    Rich4SceneMachine *scene_machine, Rich4SceneId *app_state,
    GameSession *session, NewGameSettings *settings,
    const char *music_dir, Rich4Audio *audio)
{
    Rich4SceneId return_scene;
    if (panel == NULL || config == NULL || scene_machine == NULL ||
        app_state == NULL || session == NULL ||
        panel->return_app_state != RICH4_SCENE_GAME)
        return 0;
    return_scene = panel->return_app_state;
    if (action == 4) {
        if (!options_apply_surrender(session, audio)) return 0;
        rich4_options_panel_cancel_action(panel);
        panel->active = 0;
        rich4_scene_machine_request_transition(
            scene_machine, app_state, return_scene);
        return 1;
    }
    if (action == 5) {
        /* 0x411B46 saves the committed config, then sets 0x46CAF9.
         * WinMain 0x401E0A tears down the application, not the match/menu. */
        rich4_config_write(config, "RICH4.CFG");
        rich4_options_panel_cancel_action(panel);
        panel->active = 0;
        scene_machine->running = 0;
        return 1;
    }
    if (action == 3) {
        time_t now = time(NULL);
        struct tm *today = r4diff_localtime(&now);
        /* 0x411AA3 resets the start date to DOS's current date and sets
         * n2_5=1; WinMain 0x401DE8 returns to its outer menu. */
        if (today != NULL) {
            config->year = today->tm_year + 1900;
            config->month = today->tm_mon + 1;
            config->day = today->tm_mday;
        }
        rich4_options_panel_cancel_action(panel);
        panel->active = 0;
        rich4_app_reset_session(session);
        if (settings != NULL)
            rich4_new_game_settings_init(settings, 0);
        if (config->music != 0)
            start_menu_music(audio, music_dir);
        else
            rich4_audio_stop_music(audio);
        rich4_scene_machine_request_transition(
            scene_machine, app_state, RICH4_SCENE_MENU);
        return 1;
    }
    return 0;
}

static int options_begin_title_child(Rich4OptionsPanelState *panel,
                                     const GamePreviewAssets *assets,
                                     const Rich4Config *config, int control)
{
    Rich4SprSmpChunk chunk;
    time_t now = time(NULL);
    struct tm *today = r4diff_localtime(&now);
    if (control < 3 || control > 5) return 0;
    if (control != 5 && (!game_asset_ready(assets, &assets->menu.has_options_ui) ||
        rich4_spr_smp_get_chunk(assets->menu.options_data, assets->menu.options_size,
            &assets->menu.options_spr, control == 3 ? 2 : 1, &chunk) != RICH4_SPR_SMP_OK))
        return 0;
    panel->child = control;
    panel->child_pressed = -1;
    panel->editing_key = -1;
    panel->child_draft = *config;
    panel->today = *config;
    if (today != NULL) {
        panel->today.year = today->tm_year + 1900;
        panel->today.month = today->tm_mon + 1;
        panel->today.day = today->tm_mday;
    }
    if (control != 5) {
        panel->child_left = 320 - chunk.width / 2;
        panel->child_top = 240 - chunk.height / 2;
    }
    return 1;
}

int game_handle_options_input(
    Rich4OptionsPanelState *panel, Rich4Config *config,
    Rich4SdlConfig *sdl_config, Rich4SceneMachine *scene_machine,
    Rich4SceneId *app_state, GameSession *session,
    const GamePreviewAssets *assets, NewGameSettings *settings,
    VirtualCursor *cursor, const char *music_dir,
    Rich4Audio *audio, int physical_pointer_up)
{
    int control;
    int pressed;
    if (app_state == NULL || *app_state != RICH4_SCENE_OPTIONS ||
        panel == NULL || cursor == NULL)
        return 0;

    if (panel->child) {
        if (panel->child == 5) return 1;
        control = rich4_options_child_hit(panel, cursor->x, cursor->y);
        if (physical_pointer_up) {
            int child = panel->child;
            pressed = panel->child_pressed;
            panel->child_pressed = -1;
            if (control == pressed && control >= 0) {
                rich4_options_child_apply(panel, config, control);
                if (child == 4 && control == 6)
                    rich4_config_write(config, "RICH4.CFG");
            }
        } else if (cursor->confirm_pressed) {
            int child = panel->child;
            rich4_options_child_apply(panel, config, control);
            if (child == 4 && control == 6) rich4_config_write(config, "RICH4.CFG");
            cursor->confirm_pressed = 0;
        }
        return 1;
    }
    if (physical_pointer_up &&
        rich4_options_panel_has_pending_action(panel)) {
        int left;
        int top;
        int width;
        int height;
        Rich4ConfirmDialogHit hit = RICH4_CONFIRM_DIALOG_NONE;
        if (game_options_confirm_dialog_bounds(
                assets, &left, &top, &width, &height))
            hit = rich4_sdl1_confirm_dialog_hit(
                cursor->x, cursor->y, left, top, width, height);
        if (hit == RICH4_CONFIRM_DIALOG_DECLINE) {
            rich4_audio_play_cancel(audio);
            rich4_options_panel_cancel_action(panel);
        } else if (hit == RICH4_CONFIRM_DIALOG_ACCEPT) {
            int action = rich4_options_panel_pending_action(panel);
            /* fcn_00453A32 accepts the YES half with Effect.mkf[2]. */
            rich4_audio_play_target_confirm(audio);
            if (!options_apply_game_action(
                    action, panel, config, scene_machine, app_state,
                    session, settings, music_dir, audio))
                rich4_options_panel_cancel_action(panel);
        }
        return 1;
    }
    if (physical_pointer_up) {
        pressed = rich4_options_panel_pressed(panel);
        if (pressed >= 3 && pressed <= 5) {
            rich4_options_panel_set_pressed(panel, -1);
            /* 0x410831 dispatches the saved down-control on release,
             * without another hit test at the release coordinates. */
            if (rich4_options_panel_context_variant(panel)) {
                int left, top, width, height;
                rich4_options_panel_begin_action(panel, pressed);
                if (game_options_confirm_dialog_bounds(assets, &left, &top, &width, &height))
                    rich4_sdl1_confirm_dialog_initial_cursor(left, top, &cursor->x, &cursor->y);
            } else {
                options_begin_title_child(panel, assets, config, pressed);
            }
            return 1;
        }
        return 0;
    }

    if (rich4_options_panel_has_pending_action(panel)) {
        int action = panel->confirm_key_action ? panel->confirm_key_action :
            rich4_options_panel_confirm_hover(panel);
        /* Keyboard/joystick confirmation follows the original default-left
         * behavior when the virtual cursor is outside the dialog. */
        if (action != 1 && action != 2) action = 1;
        if (cursor->confirm_pressed) {
            panel->confirm_key_action = 0;
            int pending = rich4_options_panel_pending_action(panel);
            if (action == 2) {
                rich4_audio_play_cancel(audio);
                rich4_options_panel_cancel_action(panel);
            } else {
                /* fcn_00453A32 accepts the YES half with Effect.mkf[2]. */
                rich4_audio_play_target_confirm(audio);
                if (!options_apply_game_action(
                        pending, panel, config, scene_machine, app_state,
                        session, settings, music_dir, audio))
                    rich4_options_panel_cancel_action(panel);
            }
            cursor->confirm_pressed = 0;
        }
        return 1;
    }
    control = rich4_options_panel_hit(panel, cursor->x, cursor->y);
    rich4_options_panel_set_hover(panel, control);
    if (control == 7) {
        /* 0x41168D: options Cancel uses 0x482332 / Effect.mkf[4]. */
        rich4_audio_play_cancel(audio);
        cancel_options_panel(panel, audio, music_dir);
        rich4_scene_machine_request_transition(
            scene_machine, app_state, panel->return_app_state);
    } else if (control == 8) {
        /* 0x41170F: options OK uses 0x48232A / Effect.mkf[2]. */
        rich4_audio_play_target_confirm(audio);
        *config = panel->draft;
        rich4_config_clamp(config);
        rich4_config_write(config, "RICH4.CFG");
        sdl_config->music_bank = panel->music_bank;
        rich4_sdl_config_clamp(sdl_config);
        rich4_sdl_config_write(sdl_config, RICH4_SDL_CONFIG_PATH);
        panel->saved = *config;
        panel->saved_music_bank = sdl_config->music_bank;
        panel->active = 0;
        if (panel->return_app_state == RICH4_SCENE_MENU &&
            config->music != 0) {
            start_menu_music(audio, music_dir);
        } else if (panel->return_app_state == RICH4_SCENE_MENU) {
            rich4_audio_stop_music(audio);
        }
        rich4_scene_machine_request_transition(
            scene_machine, app_state, panel->return_app_state);
    } else if (control == 6) {
        if (panel->draft.music != 0) {
            int rel_y = cursor->y - panel->top;
            int track = clamp_int((rel_y - 0xe2) / 0x0f + 1, 1, 8);
            panel->music_track = track;
            if (start_music_bank_track(
                    audio, music_dir, panel->music_bank,
                    (size_t)(track - 1), "options"))
                panel->music_preview_changed = 1;
        } else {
            rich4_audio_play_select(audio);
        }
    } else if (rich4_options_panel_apply_click(panel, control, cursor->x)) {
        rich4_audio_play_select(audio);
        if (control == 10 && panel->draft.music == 0) {
            rich4_audio_stop_music(audio);
            panel->music_preview_changed = 1;
        } else if ((control == 10 || control == 16) &&
                   panel->draft.music != 0 &&
                   start_music_bank_track(
                       audio, music_dir, panel->music_bank,
                       (size_t)(panel->music_track - 1), "options")) {
            panel->music_preview_changed = 1;
        }
    } else if (control >= 3 && control <= 5) {
        rich4_audio_play_ok(audio);
        if (rich4_options_panel_context_variant(panel))
            (void)rich4_options_panel_begin_action(panel, control);
        else options_begin_title_child(panel, assets, config, control);
    } else if (control == 6 || control >= 3) {
        rich4_audio_play_select(audio);
    }
    if (cursor->confirm_pressed &&
        ((control >= 3 && control <= 5) || control == 7 || control == 8))
        /* Physical action clicks are handled on WM_LBUTTONUP; virtual
         * confirmation is consumed here exactly once. */
        cursor->confirm_pressed = 0;
    return 1;
}

void game_update_options_hover(
    Rich4OptionsPanelState *panel, const GamePreviewAssets *assets,
    int x, int y, Rich4Audio *audio)
{
    int previous;
    if (panel == NULL || !panel->active) return;
    if (rich4_options_panel_has_pending_action(panel)) {
        int left;
        int top;
        int width;
        int height;
        Rich4ConfirmDialogHit hover = RICH4_CONFIRM_DIALOG_NONE;
        previous = rich4_options_panel_confirm_hover(panel);
        if (game_options_confirm_dialog_bounds(
                assets, &left, &top, &width, &height))
            hover = rich4_sdl1_confirm_dialog_hit(
                x, y, left, top, width, height);
        if (hover != RICH4_CONFIRM_DIALOG_NONE &&
            (int)hover != previous)
            rich4_audio_play_select(audio);
        rich4_options_panel_set_confirm_hover(panel, (int)hover);
        return;
    }
    rich4_options_panel_set_hover(panel,
        rich4_options_panel_hit(panel, x, y));
}
