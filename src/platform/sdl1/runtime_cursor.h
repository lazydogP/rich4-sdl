#ifndef RICH4_PLATFORM_SDL1_RUNTIME_CURSOR_H
#define RICH4_PLATFORM_SDL1_RUNTIME_CURSOR_H

#include <SDL/SDL.h>

#include "app/ai_settings_state.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/input/input_config_sdl.h"

int update_cursor(VirtualCursor *cursor,
                            const Rich4InputConfig *input,
                            SDL_Joystick *joystick);
void sync_host_pointer_to_virtual_cursor(const VirtualCursor *cursor);
/* R10.1d/A12.2: end-of-frame software-cursor composition keeps the render
 * resources and application/panel state in one direct presentation seam. */
typedef struct VirtualCursorPresentationContext {
    SDL_Surface *screen;
    SDL_Surface *cursor_free_frame;
    const CursorAssets *cursor_assets;
    VirtualCursor *cursor;
    const int *host_pointer_sync_needed;
    Rich4SceneId app_state;
    const NewGameSettings *settings;
    GameSession *session;
    const HelpPanelState *help_panel;
    const SmallMapPanelState *small_map_panel;
    const Rich4AiSettingsState *ai_settings_panel;
    const GamePreviewAssets *assets;
    const ToolPanelState *tool_panel;
    struct Rich4Audio *audio;
} VirtualCursorPresentationContext;

void draw_virtual_cursor_presentation(
    const VirtualCursorPresentationContext *context);

#endif
