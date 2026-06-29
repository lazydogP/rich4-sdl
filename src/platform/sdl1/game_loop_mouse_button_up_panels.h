#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_UP_PANELS_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_MOUSE_BUTTON_UP_PANELS_H

#include <SDL/SDL.h>

#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/audio/audio_sdl_mixer.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"

/* R10.1d: the stock/help panel release-and-consume branches moved out of
 * runtime_main.c. Small-map release ordering and session cleanup remain in
 * the event loop until their own seam is extracted. */
typedef struct GameMouseButtonUpPanelsContext {
    Rich4SceneId app_state;
    GameSession *session;
    const GamePreviewAssets *assets;
    NewGameSettings *settings;
    StockPanelState *stock_panel;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
    Rich4Audio *audio;
    VirtualCursor *cursor;
    SDL_Surface *capture_source;
    int button;
    int cursor_x;
    int cursor_y;
    int *mouse_left_down;
} GameMouseButtonUpPanelsContext;

/* Returns non-zero when a panel consumed the button-up event. */
int game_handle_mouse_button_up_panels(
    GameMouseButtonUpPanelsContext *context);

#endif
