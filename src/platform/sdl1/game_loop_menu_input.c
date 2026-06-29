/* R10.1d: menu click dispatch, including the asset-less fallback, moved
 * verbatim out of runtime_main.c. */

#include "debug/game_debug_trace.h"
#include "debug/trace.h"
#include "app/menu_flow.h"
#include "platform/sdl1/game_loop_menu_input.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/render/scene_menu_render.h"
#include "platform/sdl1/render/game_dice_render.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/runtime_bootstrap.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/game_asset_api.h"

int game_handle_menu_input(
    const MenuAssets *menu_assets, GamePreviewAssets *preview_assets,
    Rich4OptionsPanelState *options_panel, Rich4Config *config,
    Rich4SdlConfig *sdl_config, GameSession *session,
    NewGameSettings *settings, Rich4SceneMachine *scene_machine,
    Rich4SceneId *app_state, const VirtualCursor *cursor,
    const char *music_dir, Rich4Audio *audio, int *running)
{
    int button = -1;
    Rich4MenuAction action;
    if (app_state == NULL || *app_state != RICH4_SCENE_MENU)
        return 0;
    if (session->presentation.save_load.mode == GAME_SAVE_LOAD_PANEL_LOAD) {
        if (game_save_load_panel_menu_activate(session, preview_assets,
                settings, cursor->x, cursor->y)) {
            rich4_audio_play_ok(audio);
            rich4_scene_machine_request_transition(
                scene_machine, app_state, RICH4_SCENE_GAME);
        }
        return 1;
    }
    if (menu_assets->available) {
        button = menu_button_at(menu_assets, cursor);
    } else {
        button = rich4_menu_fallback_button_at(cursor->x, cursor->y);
    }
    if (button < 0) return 1;
    rich4_audio_play_ok(audio);
    action = rich4_menu_apply_button(session, settings, button);
    if (action == RICH4_MENU_START_NEW_GAME) {
        rich4_trace_emit(RICH4_TRACE_MENU_NEW_GAME, 0, button, 0);
        rich4_scene_machine_request_transition(
            scene_machine, app_state, RICH4_SCENE_NEW_GAME);
        if (config->music != 0)
            start_new_game_music(audio, music_dir);
    } else if (action == RICH4_MENU_OPEN_LOAD) {
        (void)game_save_load_panel_open_menu(session, preview_assets);
    } else if (action == RICH4_MENU_OPEN_OPTIONS) {
        /* 0x411B53 reads Data[3] before deriving the centered rectangle.
         * The menu scope does not retain this resource. */
        (void)game_sync_runtime_asset_scope(preview_assets, RICH4_SCENE_OPTIONS,
                                            session, settings);
        rich4_audio_stop_music(audio);
        open_options_panel(
            options_panel, preview_assets, config, sdl_config,
            RICH4_SCENE_MENU);
        rich4_scene_machine_request_transition(scene_machine, app_state, RICH4_SCENE_OPTIONS);
    } else if (action == RICH4_MENU_QUIT) {
        *running = 0;
    }
    return 1;
}
