#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_POINTER_MOTION_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_POINTER_MOTION_H

#include "app/ai_settings_state.h"
#include "app/app_command.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"

/* R10.1d: absolute host-pointer motion handling moved out of runtime_main.c.
 * The caller retains SDL event polling and all state ownership. */
typedef struct GamePointerMotionContext {
    Rich4SceneId app_state;
    int view_mode;
    int screen_width;
    int screen_height;
    int mouse_left_down;
    const NewGameSettings *settings;
    GameSession *session;
    VirtualCursor *cursor;
    Rich4AiSettingsState *ai_settings_panel;
    Rich4OptionsPanelState *options_panel;
    const SmallMapPanelState *small_map_panel;
    const GamePreviewAssets *assets;
    StockPanelState *stock_panel;
    Rich4Audio *audio;
} GamePointerMotionContext;

void game_handle_mouse_motion(const Rich4AppCommand *command,
                              GamePointerMotionContext *context);

#endif
