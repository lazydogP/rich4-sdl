#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_DICE_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_DICE_RENDER_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/render/game_render_cache.h"
#include "view/game_view_model.h"

/* R10.1c: dice-roll popup (animated graph + result faces) and the dice status
 * overlay, moved verbatim out of runtime_main.c. The popup caches two work
 * surfaces in the session's GameRenderCache to composite FLC frames
 * incrementally; reset_game_dice_cache frees them. */

void reset_game_dice_cache(GameRenderCache *cache);

void draw_game_dice_popup(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          const GameViewModel *view,
                          GameRenderCache *cache);

void draw_game_dice_status_overlay(SDL_Surface *screen,
                                   const GamePreviewAssets *assets,
                                   const GameViewModel *view);

#endif
