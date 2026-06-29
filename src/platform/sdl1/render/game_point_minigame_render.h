#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_POINT_MINIGAME_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_POINT_MINIGAME_RENDER_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/game_point_minigame_view.h"

/* R10.1c: point-minigame overlay (penguin/balloon/catcher sub-minigames),
 * split out of game_event_render.c to keep it under the file line-budget
 * gate. Same "session adapter" tier as the rest of the event cluster. */

void draw_point_minigame(SDL_Surface *screen,
                         const GamePreviewAssets *assets,
                         const GamePointMinigameRenderView *view);

#endif
