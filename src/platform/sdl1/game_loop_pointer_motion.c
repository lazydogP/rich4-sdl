/* R10.1d: absolute host-pointer motion handling moved verbatim out of
 * runtime_main.c. Host motion remains authoritative for the software cursor. */

#include "platform/sdl1/game_loop_pointer_motion.h"
#include "app/ai_settings_flow.h"
#include "app/dice_flow.h"
#include "app/map_input_flow.h"
#include "app/options_flow.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/runtime_render_api.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/render/scene_options_render.h"
#include "platform/sdl1/game_loop_options_input.h"
#include "platform/sdl1/game_loop_confirm_company_issue.h"

void game_handle_mouse_motion(const Rich4AppCommand *command,
                              GamePointerMotionContext *context)
{
    if (context == NULL || !rich4_app_command_is_valid(command) ||
        command->type != RICH4_APP_COMMAND_POINTER_MOVE) return;
    if (context->app_state == RICH4_SCENE_GAME && context->mouse_left_down &&
        game_company_issue_pointer(context->session, context->assets, context->audio,
            1, SDL_BUTTON_LEFT, context->cursor->x, context->cursor->y)) return;
    if (!rich4_app_command_is_valid(command) ||
        command->type != RICH4_APP_COMMAND_POINTER_MOVE ||
        !rich4_app_input_ui_active(
            context->app_state, context->settings, context->session,
            context->ai_settings_panel != NULL &&
                context->ai_settings_panel->active))
        return;
    rich4_app_input_clear_map_info(context->session);
    if (context->app_state == RICH4_SCENE_MENU &&
        context->session->presentation.save_load.mode ==
            GAME_SAVE_LOAD_PANEL_LOAD) {
        game_save_load_panel_mouse_motion(context->session,
            context->cursor->x, context->cursor->y);
        return;
    }
    /* Host mouse motion is authoritative and absolute. The app input flow
     * commits the normalized cursor position before this adapter runs. */
    if (context->app_state == RICH4_SCENE_GAME) {
        if (context->session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED) {
            game_save_load_panel_mouse_motion(
                context->session, context->cursor->x, context->cursor->y);
        } else if (context->small_map_panel->active) {
            /* The modal overview tracks only the software cursor. */
        } else if (rich4_app_sale_interaction_is_active(context->session)) {
            game_sale_panel_mouse_motion(
                context->session, context->assets, context->cursor->x,
                context->cursor->y, context->mouse_left_down);
        } else if (rich4_app_stock_interaction_is_active(context->session)) {
            if (context->mouse_left_down)
                game_stock_panel_mouse_motion(
                    context->stock_panel, context->session, context->assets,
                    context->audio, context->cursor->x, context->cursor->y);
        } else if (context->ai_settings_panel->active) {
            rich4_ai_settings_set_hover(
                context->ai_settings_panel,
                rich4_ai_settings_hit(
                    context->cursor->x, context->cursor->y));
            if (context->mouse_left_down &&
                (context->ai_settings_panel->hover == 11 ||
                 context->ai_settings_panel->hover == 12))
                rich4_ai_settings_apply(
                    context->ai_settings_panel, context->session,
                    context->ai_settings_panel->hover, context->cursor->x);
        } else if (context->session->presentation.dice.status_dragging) {
            rich4_dice_update_status_drag(
                context->session, context->cursor->x, context->cursor->y);
        } else if (context->session->presentation.dragging_minimap) {
            /* Dragging is a button capture, never a persistent map mode.
             * If another modal consumed the matching button-up, the next
             * motion still observes the physical state and releases capture. */
            if (context->mouse_left_down) {
                (void)rich4_app_update_minimap_drag(
                    context->session, context->cursor->x,
                    context->cursor->y, context->view_mode);
            } else {
                rich4_app_end_minimap_drag(context->session);
            }
        }
    } else if (context->app_state == RICH4_SCENE_OPTIONS) {
        game_update_options_hover(
            context->options_panel, context->assets,
            context->cursor->x, context->cursor->y, context->audio);
    }
}
