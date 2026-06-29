/* R10.1d: early SDL_KEYDOWN modal ownership gates moved verbatim out of
 * runtime_main.c. Game-specific command keys remain in the caller. */

#include "platform/sdl1/game_loop_key_down_modals.h"
#include "app/stock_interaction_flow.h"
#include "platform/sdl1/input_adapter.h"
#include "app/sale_interaction_flow.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/game_loop_orchestration.h"
#include <SDL/SDL.h>
#include "app/magic_house_flow.h"

int game_handle_key_down_modals(GameKeyDownModalsContext *context)
{
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.special_place_notice.queue.count > 0) return 1;
    if (context->app_state == RICH4_SCENE_GAME &&
        ((context->session->presentation.periodic_finance.active &&
          !context->session->presentation.bankruptcy.active &&
          !context->session->presentation.bankruptcy.awaiting_auction &&
          !context->session->presentation.auction.active) ||
         context->session->presentation.bank_settlement.active)) return 1;
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.magic_house.active &&
        context->session->presentation.magic_house.condition == RICH4_MAGIC_HOUSE_SURRENDER) {
        /* 0x433088 delegates keyboard messages without choosing/cancelling. */
        context->cursor->confirm_pressed = context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_OPTIONS &&
        rich4_options_panel_has_pending_action(&context->session->presentation.options)) {
        Rich4OptionsPanelState *p = &context->session->presentation.options;
        int vk = rich4_sdl1_original_hotkey(context->key);
        int mod = context->key_modifiers & KMOD_CTRL ? 0x11 : 0;
        int i;
        /* 0x45367E compares configured Yes/No, independent of hover. */
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        for (i = 8; i <= 9; ++i) {
            if (vk && context->config->hotkeys[i].key == vk &&
                context->config->hotkeys[i].mod == mod) {
                p->confirm_key_action = i - 7;
                context->cursor->confirm_pressed = 1;
                break;
            }
        }
        return 1;
    }
    if (context->app_state == RICH4_SCENE_OPTIONS &&
        context->session->presentation.options.child) {
        Rich4OptionsPanelState *p = &context->session->presentation.options;
        if (p->child == 4)
            rich4_options_child_key(p, rich4_sdl1_original_hotkey(context->key));
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.ai_tool.phase != GAME_AI_TOOL_NONE) {
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        (rich4_presentation_god_owns_frame(&context->session->presentation) ||
         (context->session->presentation.special_reward.active &&
          (context->session->presentation.special_reward.action == 14 ||
           context->session->presentation.special_reward.action == 15)))) {
        /* 0x43F23E / chest 0x4528B9 consume input until key-up. */
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_MENU &&
        context->session->presentation.save_load.mode ==
            GAME_SAVE_LOAD_PANEL_LOAD &&
        context->key == RICH4_APP_KEY_ESCAPE) {
        game_save_load_panel_cancel(context->session, context->audio);
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        context->session->presentation.save_load.mode !=
            GAME_SAVE_LOAD_PANEL_CLOSED) {
        (void)game_save_load_panel_key_down(
            context->session, context->assets, context->settings,
            context->audio, context->key);
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_sale_interaction_is_active(context->session)) {
        game_sale_panel_key_down(
            context->session, context->assets, context->settings,
            context->audio, context->key);
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    if (context->app_state == RICH4_SCENE_GAME &&
        rich4_app_stock_interaction_is_active(context->session)) {
        game_stock_panel_key_down(
            context->stock_panel, context->session, context->assets,
            context->settings, context->audio, context->key);
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->small_map_panel->active) {
        /* The original small-map callback delegates keyboard messages and
         * does not use them to close the panel. */
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->help_panel->active) {
        /* 0x44EA58 uses the configured Page Up/Page Down keys. */
        game_help_panel_key_down(
            context->help_panel, context->help_assets, context->config,
            context->audio, context->key, context->key_modifiers);
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->app_state == RICH4_SCENE_INTRO) {
        *context->setup_transition_input =
            RICH4_SETUP_TRANSITION_INPUT_KEY_DOWN;
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    } else if (context->session->presentation.intro.active) {
        /* Original FLC interruption is on WM_KEYUP, not keydown. */
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.speech.active) {
        /* player_say consumes the matching key-up as skip; the key-down does
         * not leak into the game UI. */
        return 1;
    } else if (context->app_state == RICH4_SCENE_GAME &&
               context->session->presentation.lottery.active) {
        /* sub_42F7FC delegates keyboard messages unchanged. */
        context->cursor->confirm_pressed = 0;
        context->cursor->cancel_pressed = 0;
        return 1;
    }
    return 0;
}
