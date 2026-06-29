#ifndef RICH4_PLATFORM_SDL1_GAME_LOOP_SCENE_PRESENTATION_H
#define RICH4_PLATFORM_SDL1_GAME_LOOP_SCENE_PRESENTATION_H

#include <SDL/SDL.h>

#include "app/config_types.h"
#include "app/game_session.h"
#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "app/options_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "platform/sdl1/render/game_render_cache.h"

/* R10.1d/A12.2: non-preview scene presentation keeps each fallback variant's
 * direct borrowed inputs; a shared surface-only wrapper adds no meaning. */
typedef struct GameSceneFallbackPreviewContext {
    const GamePreviewAssets *assets;
    const NewGameSettings *settings;
    GameSession *session;
    GameRenderCache *render_cache;
    const Rich4Config *config;
} GameSceneFallbackPreviewContext;

typedef struct GameSceneFallbackMenuContext {
    SDL_Surface *screen;
    SDL_Surface *background;
    const MenuAssets *menu_assets;
    const VirtualCursor *cursor;
    const GamePreviewAssets *assets;
    const GameSession *session;
} GameSceneFallbackMenuContext;

typedef struct GameSceneFallbackOptionsMenuContext {
    SDL_Surface *screen;
    SDL_Surface *background;
    const GamePreviewAssets *assets;
    const MenuAssets *menu_assets;
    Rich4OptionsPanelState *options_panel;
    HelpPanelState *help_panel;
    const HelpAssets *help_assets;
} GameSceneFallbackOptionsMenuContext;

typedef struct GameSceneFallbackOptionsGameContext {
    SDL_Surface *screen;
    SDL_Surface *background;
    GameSceneFallbackPreviewContext *preview;
    const Rich4OptionsPanelState *options_panel;
} GameSceneFallbackOptionsGameContext;

typedef struct GameSceneFallbackFrameContext {
    GameSceneFallbackMenuContext *menu;
    GameSceneFallbackOptionsMenuContext *options_menu;
    GameSceneFallbackOptionsGameContext *options_game;
} GameSceneFallbackFrameContext;

void game_draw_scene_fallback_menu(
    const GameSceneFallbackMenuContext *context);
void game_draw_scene_fallback_options_menu(
    const GameSceneFallbackOptionsMenuContext *context);
void game_draw_scene_fallback_options_game(
    const GameSceneFallbackOptionsGameContext *context);

#endif
