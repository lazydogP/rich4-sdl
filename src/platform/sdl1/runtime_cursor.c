/* R10.1d: virtual cursor device polling, host-pointer synchronization and
 * UI-availability gating moved out of runtime_main.c. The event loop retains
 * only the calls and owns the cursor/state values. */

#include "platform/sdl1/runtime_cursor.h"
#include "platform/sdl1/test_adapter/diff_adapter.h"

#include "platform/sdl1/input_adapter.h"
#include "app/stock_interaction_flow.h"
#include "app/point_minigame_flow.h"
#include "platform/sdl1/game_asset_api.h"
#include "platform/sdl1/game_loop_input.h"
#include "platform/sdl1/game_loop_panels.h"

#include "view/view_constants.h"

static int rich4_sdl1_key_is_pressed(Rich4AppKey key)
{
    int native = rich4_sdl1_native_key(key);
    Uint8 *keys;
    if (native == SDLK_UNKNOWN) return 0;
    if (r4diff_active()) return r4diff_key_pressed(native);
    keys = SDL_GetKeyState(NULL);
    return keys != NULL && keys[native] != 0;
}

int update_cursor(VirtualCursor *cursor,
                         const Rich4InputConfig *input,
                         SDL_Joystick *joystick)
{
    int old_x = cursor->x;
    int old_y = cursor->y;
    if (r4diff_active()) joystick = NULL;
    if (rich4_sdl1_key_is_pressed(input->cursor_left)) {
        cursor->x -= input->cursor_speed;
    }
    if (rich4_sdl1_key_is_pressed(input->cursor_right)) {
        cursor->x += input->cursor_speed;
    }
    if (rich4_sdl1_key_is_pressed(input->cursor_up)) {
        cursor->y -= input->cursor_speed;
    }
    if (rich4_sdl1_key_is_pressed(input->cursor_down)) {
        cursor->y += input->cursor_speed;
    }

    if (joystick != NULL) {
        if (SDL_JoystickNumAxes(joystick) >= 2) {
            Sint16 axis_x = SDL_JoystickGetAxis(joystick, 0);
            Sint16 axis_y = SDL_JoystickGetAxis(joystick, 1);
            if (axis_x < -12000) cursor->x -= input->cursor_speed;
            if (axis_x > 12000) cursor->x += input->cursor_speed;
            if (axis_y < -12000) cursor->y -= input->cursor_speed;
            if (axis_y > 12000) cursor->y += input->cursor_speed;
        }
        if (SDL_JoystickNumHats(joystick) > 0) {
            Uint8 hat = SDL_JoystickGetHat(joystick, 0);
            if ((hat & SDL_HAT_LEFT) != 0) cursor->x -= input->cursor_speed;
            if ((hat & SDL_HAT_RIGHT) != 0) cursor->x += input->cursor_speed;
            if ((hat & SDL_HAT_UP) != 0) cursor->y -= input->cursor_speed;
            if ((hat & SDL_HAT_DOWN) != 0) cursor->y += input->cursor_speed;
        }
    }

    if (cursor->x < 0) cursor->x = 0;
    if (cursor->y < 0) cursor->y = 0;
    if (cursor->x >= SCREEN_W) cursor->x = SCREEN_W - 1;
    if (cursor->y >= SCREEN_H) cursor->y = SCREEN_H - 1;
    return cursor->x != old_x || cursor->y != old_y;
}

void sync_host_pointer_to_virtual_cursor(const VirtualCursor *cursor)
{
    Uint8 app_state;
    if (cursor == NULL) return;
    if (r4diff_active()) return;
    app_state = SDL_GetAppState();
    if ((app_state & SDL_APPMOUSEFOCUS) == 0 ||
        (app_state & SDL_APPINPUTFOCUS) == 0) return;
    SDL_WarpMouse((Uint16)cursor->x, (Uint16)cursor->y);
}

void draw_virtual_cursor_presentation(
    const VirtualCursorPresentationContext *context)
{
    CursorAssets display_cursor;
    int software_cursor_visible;

    if (context == NULL || context->screen == NULL ||
        context->cursor == NULL || context->cursor_assets == NULL)
        return;

    /* The cursor-free copy must be captured before the cursor is composited,
     * and only for ordinary game-preview frames. Help and small-map modals
     * own their captured background and must not overwrite it here. */
    if (context->cursor_free_frame != NULL &&
        context->app_state == RICH4_SCENE_GAME &&
        (context->help_panel == NULL || !context->help_panel->active) &&
        (context->small_map_panel == NULL ||
         !context->small_map_panel->active))
        SDL_BlitSurface(context->screen, NULL,
                        context->cursor_free_frame, NULL);

    display_cursor = *context->cursor_assets;
    if (context->app_state == RICH4_SCENE_GAME) {
        int warp_x = rich4_point_minigame_take_cursor_warp(context->session);
        if (warp_x >= 0) {
            context->cursor->x = warp_x;
            sync_host_pointer_to_virtual_cursor(context->cursor);
        }
    }
    software_cursor_visible = rich4_app_input_display_visible(
        context->app_state, context->settings, context->session,
        context->ai_settings_panel != NULL &&
            context->ai_settings_panel->active);
    if (context->app_state == RICH4_SCENE_GAME) {
        display_cursor.chunk_index = game_tool_panel_cursor_chunk_index(
            context->assets, context->settings, context->session,
            context->tool_panel, context->cursor->x, context->cursor->y,
            display_cursor.chunk_index);
        display_cursor.chunk_index = rich4_app_bank_interaction_cursor_chunk_index(
            context->session, display_cursor.chunk_index);
        display_cursor.chunk_index =
            rich4_app_stock_interaction_cursor_chunk_index(
                context->session, display_cursor.chunk_index);
    }
    if (context->host_pointer_sync_needed != NULL &&
        *context->host_pointer_sync_needed)
        sync_host_pointer_to_virtual_cursor(context->cursor);
    if (software_cursor_visible)
        (void)game_draw_resource_cursor(
            context->screen, &display_cursor, context->cursor);
}
