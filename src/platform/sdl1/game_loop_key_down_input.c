/* R10.1d: complete SDL_KEYDOWN dispatch moved verbatim out of
 * runtime_main.c. The individual modal and command handlers retain their
 * original ownership and side-effect timing. */

#include "platform/sdl1/game_loop_key_down_input.h"

#include "platform/sdl1/game_loop_confirm_company_issue.h"
#include "platform/sdl1/game_loop_key_down_modals.h"
#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/game_loop_input.h"
#include "platform/sdl1/game_loop_turns.h"
#include "platform/sdl1/input_adapter.h"
#include "app/map_input_flow.h"

int game_handle_key_down_input(GameKeyDownInputContext *context)
{
    GameKeyDownModalsContext key_down_modals;

    if (context == NULL || context->session == NULL ||
        context->cursor == NULL)
        return 0;
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.ai_settings.active &&
        context->key == context->cancel_key) return 1;
    key_down_modals.app_state = context->app_state;
    key_down_modals.session = context->session;
    key_down_modals.stock_panel = context->stock_panel;
    key_down_modals.small_map_panel = context->small_map_panel;
    key_down_modals.assets = context->assets;
    key_down_modals.settings = context->settings;
    key_down_modals.help_panel = context->help_panel;
    key_down_modals.help_assets = context->help_assets;
    key_down_modals.config = context->config;
    key_down_modals.audio = context->audio;
    key_down_modals.cursor = context->cursor;
    key_down_modals.key = context->key;
    key_down_modals.key_modifiers = context->key_modifiers;
    key_down_modals.setup_transition_input =
        context->setup_transition_input;
    if (game_handle_key_down_modals(&key_down_modals))
        return 1;

    /* fn@0x401010: configured window-group and info-page hotkeys. */
    if (context->app_state == RICH4_SCENE_GAME && context->config != NULL &&
        rich4_app_input_ui_active(context->app_state, context->settings,
                                 context->session, 0)) {
        int vk = rich4_sdl1_original_hotkey(context->key);
        int mod = context->key_modifiers & KMOD_CTRL ? 0x11 : 0;
        int keys[] = {7,25,26};
        int i;
        for (i = 0; i < 3; i++) {
            const Rich4Key *key = &context->config->hotkeys[keys[i]];
            if (!vk || key->key != vk || key->mod != mod) continue;
            if (i == 0) {
                context->config->view = (context->config->view + 1) % 3;
                rich4_app_cancel_minimap_interaction(context->session);
            }
            else if (context->config->view != 2)
                rich4_app_switch_info_tab(context->session,
                    (rich4_app_current_info_tab(context->session) + (i == 1 ? 3 : 1)) & 3);
            return 1;
        }
    }

    if (game_handle_company_issue_key_down(
            context->app_state, context->session, context->cursor,
            context->audio, context->key,
            context->confirm_key, context->cancel_key)) {
        /* Company-issue quantity entry consumed the key. */
    } else if (game_handle_bank_interaction_key_down(
                   context->app_state, context->session, context->assets,
                   context->settings, context->audio, context->key)) {
        /* Bank interaction consumed the key. */
    } else if (rich4_app_input_apply_key(
                   context->app_state, context->settings, context->session,
                   context->ai_settings_panel != NULL &&
                       context->ai_settings_panel->active,
                   context->cursor, context->key,
                   context->confirm_key, context->cancel_key)) {
        /* Virtual cursor consumed confirm/cancel. */
    } else if (game_handle_dice_shortcut_key_down(
                   context->app_state, context->session, context->assets,
                   context->settings, context->audio, context->key)) {
        /* Dice shortcut consumed the key. */
    }
    return 0;
}
