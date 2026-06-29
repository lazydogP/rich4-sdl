#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_OVERLAY_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_OVERLAY_RENDER_H

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_overlay_view.h"

/* R10.1c: transient active-game overlay draw calls that are not part of the
 * GameViewModel scene snapshot. These render only the current session state
 * and are dispatched by the top-level frame adapter. */

void draw_game_monster_card_animation(
    SDL_Surface *screen, const GamePreviewAssets *assets,
    const GameOverlayRenderView *view);

void draw_game_tool_robot(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const NewGameSettings *settings,
                          const GameOverlayRenderView *view);

void draw_game_tool_animation(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const GameOverlayRenderView *view);

#endif
