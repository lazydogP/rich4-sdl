#ifndef RICH4_PLATFORM_SDL1_APP_PANEL_TYPES_H
#define RICH4_PLATFORM_SDL1_APP_PANEL_TYPES_H

#include <SDL/SDL.h>

#include "core/game_command.h"
#include "core/game_constants.h"
#include "core/game_types.h"
#include "data/music.h"
#include "app/ai_settings_flow.h"
#include "app/card_panel_state.h"
#include "app/options_flow.h"
#include "app/tool_panel_state.h"
#include "platform/sdl1/app_config_types.h"

#ifndef GAME_HELP_CATEGORY_COUNT
#define GAME_HELP_CATEGORY_COUNT 8
#endif

typedef struct HelpPanelState {
    int active;
    int button_pressed;
    int category;
    int selected_item;
    int scroll[GAME_HELP_CATEGORY_COUNT];
    int page;
    int pressed_control;
    SDL_Surface *saved_background;
} HelpPanelState;

/* SDL-owned stock render cache. Interaction state lives in the app-owned
 * GameSession presentation state; this type owns only the SDL surface. */
typedef struct StockPanelState {
    SDL_Surface *render_cache;
    int render_dirty;
} StockPanelState;

/* Full-screen overview map overlay (distinct from the always-on minimap
 * panel). saved_background is the captured frame restored under the
 * overview each redraw; owned by the event loop's open/close lifecycle. */
typedef struct SmallMapPanelState {
    int active;
    int button_pressed;
    SDL_Surface *saved_background;
} SmallMapPanelState;

#endif
