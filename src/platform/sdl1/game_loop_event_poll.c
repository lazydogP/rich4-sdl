/* R10.1d: SDL event polling and outer dispatch moved verbatim out of
 * runtime_main.c. Event-handler modules retain their original early exits. */

#include "platform/sdl1/game_loop_event_poll.h"

#include "platform/sdl1/input_adapter.h"
#include "platform/sdl1/game_loop_panels.h"
#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/runtime.h"
#include "platform/sdl1/test_adapter/diff_adapter.h"
#if defined(RICH4_PLATFORM_MIYOO)
#include "platform/sdl1/miyoo_input.h"
#endif
#include "app/ai_settings_flow.h"
#include "app/stock_interaction_flow.h"
#include "app/sale_interaction_flow.h"
#include "core/turn_rotation.h"

/* Equivalent to 0x40117A's callback depth == 1. Moving on the main map
 * is allowed; nested panels and modal presentation waits own their input. */
static int ai_cancel_main_callback(const GameKeyDownInputContext *input)
{
    const GameSession *s = input->session;
    const GamePresentationState *p = &s->presentation;
    Rich4PresentationActivity activity =
        rich4_presentation_activity_from_state(p, &s->state);
    return s->state.active && !p->intro.active && !p->ending.active &&
        !p->ai_settings.active && !p->player_info.active &&
        p->card_panel.phase == CARD_PANEL_CLOSED &&
        p->tool_panel.phase == TOOL_PANEL_CLOSED &&
        p->save_load.mode == GAME_SAVE_LOAD_PANEL_CLOSED &&
        !p->route_interaction.active &&
        p->ai_tool.phase == GAME_AI_TOOL_NONE &&
        !rich4_app_stock_interaction_is_active(s) &&
        !rich4_app_sale_interaction_is_active(s) &&
        !(input->small_map_panel && input->small_map_panel->active) &&
        !(input->help_panel && input->help_panel->active) &&
        rich4_presentation_mode_resolve(&activity) == RICH4_PRESENTATION_NONE;
}

void game_poll_runtime_events(GameEventPollContext *context)
{
    int *running;
    Rich4SceneId *app_state;
    SDL_Surface *screen;
    int *mouse_left_down;
    GamePointerMotionContext *pointer_motion;
    GameMouseButtonDownInputContext *mouse_button_down;
    GameMouseButtonUpInputContext *mouse_button_up;
    GameKeyDownInputContext *key_down;
    GameKeyUpModalsContext *key_up;
    GameJoyButtonDownInputContext *joy_button_down;
    SDL_Event event;

    if (context == NULL) return;
    running = context->running;
    app_state = context->app_state;
    screen = context->screen;
    mouse_left_down = context->mouse_left_down;
    pointer_motion = context->pointer_motion;
    mouse_button_down = context->mouse_button_down;
    mouse_button_up = context->mouse_button_up;
    key_down = context->key_down;
    key_up = context->key_up;
    joy_button_down = context->joy_button_down;
    while (r4diff_poll(&event)) {
#if defined(RICH4_PLATFORM_MIYOO)
        rich4_sdl1_miyoo_adapt_event(
            &event, key_down->cursor->x, key_down->cursor->y);
#endif
        if (event.type == SDL_QUIT) {
            *running = 0;
        } else if (event.type == SDL_MOUSEMOTION) {
            Rich4AppCommand command;
            if (rich4_sdl1_pointer_command(
                    &command, RICH4_APP_COMMAND_POINTER_MOVE,
                    event.motion.x, event.motion.y,
                    screen->w, screen->h, 0)) {
                (void)rich4_app_input_apply_pointer(
                    *app_state, pointer_motion->settings,
                    pointer_motion->session,
                    pointer_motion->ai_settings_panel != NULL &&
                        pointer_motion->ai_settings_panel->active,
                    pointer_motion->cursor, &command);
                pointer_motion->app_state = *app_state;
                pointer_motion->mouse_left_down = *mouse_left_down;
                game_handle_mouse_motion(&command, pointer_motion);
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            Rich4AppCommand command;
            int have_command = rich4_sdl1_pointer_command(
                &command, RICH4_APP_COMMAND_POINTER_DOWN,
                event.button.x, event.button.y,
                screen->w, screen->h,
                event.button.button);
            if (have_command)
                (void)rich4_app_input_apply_pointer(
                    *app_state, mouse_button_down->settings,
                    mouse_button_down->session,
                    mouse_button_down->ai_settings_panel != NULL &&
                        mouse_button_down->ai_settings_panel->active,
                    mouse_button_down->cursor, &command);
            mouse_button_down->button = have_command
                ? command.button : event.button.button;
            if (game_handle_mouse_button_down_input(
                    mouse_button_down))
                continue;
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            Rich4AppCommand command;
            int consumed;
            int have_command = rich4_sdl1_pointer_command(
                &command, RICH4_APP_COMMAND_POINTER_UP,
                event.button.x, event.button.y,
                screen->w, screen->h,
                event.button.button);
            if (have_command)
                (void)rich4_app_input_apply_pointer(
                    *app_state, pointer_motion->settings,
                    pointer_motion->session,
                    pointer_motion->ai_settings_panel != NULL &&
                        pointer_motion->ai_settings_panel->active,
                    pointer_motion->cursor, &command);
            mouse_button_up->app_state = *app_state;
            mouse_button_up->button = have_command
                ? command.button : event.button.button;
            consumed = game_handle_mouse_button_up_input(mouse_button_up);
            /* toolsProc posts the selected tool while consuming
             * WM_LBUTTONUP; tools_ui() invokes the handler only after that
             * callback has returned. */
            game_tool_panel_dispatch_pending_inventory_release(
                mouse_button_up->session, mouse_button_up->assets,
                mouse_button_up->settings, mouse_button_up->tool_panel,
                mouse_button_up->audio);
            if (consumed)
                continue;
        } else if (event.type == SDL_KEYDOWN) {
            Rich4AppCommand command;
            /* Debug aid: export the composed SDL framebuffer on the next
             * present. The hotkey is inactive in normal game runs. */
            if (event.key.keysym.sym == SDLK_F12 &&
                rich4_sdl1_request_debug_capture())
                continue;
            if (!rich4_sdl1_key_command(
                    &command, RICH4_APP_COMMAND_KEY_DOWN,
                    event.key.keysym.sym, event.key.keysym.mod))
                continue;
            key_down->app_state = *app_state;
            key_down->key = command.key;
            key_down->key_modifiers = command.modifiers;
            if (game_handle_key_down_input(key_down))
                continue;
        } else if (event.type == SDL_KEYUP) {
            Rich4AppCommand command;
            if (!rich4_sdl1_key_command(
                    &command, RICH4_APP_COMMAND_KEY_UP,
                    event.key.keysym.sym, event.key.keysym.mod))
                continue;
            key_up->app_state = *app_state;
            if (*app_state == RICH4_SCENE_GAME &&
                command.key == key_down->cancel_key) {
                if (key_up->session->presentation.ai_settings.active) {
                    rich4_ai_settings_cancel(
                        &key_up->session->presentation.ai_settings);
                    key_down->cursor->cancel_pressed = 0;
                    continue;
                }
                if (ai_cancel_main_callback(key_down)) {
                    (void)rich4_turn_request_ai_cancel(&key_up->session->state);
                    key_down->cursor->cancel_pressed = 0;
                    continue;
                }
            }
            game_handle_key_up_modals(key_up);
        } else if (event.type == SDL_JOYBUTTONDOWN) {
            Rich4AppCommand command;
            if (!rich4_sdl1_joy_command(
                    &command, event.jbutton.button))
                continue;
            joy_button_down->app_state = *app_state;
            joy_button_down->button = command.button;
            if (game_handle_joy_button_down_input(
                    joy_button_down))
                continue;
        }
    }
}
