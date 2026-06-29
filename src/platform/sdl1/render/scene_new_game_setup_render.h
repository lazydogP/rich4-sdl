#ifndef RICH4_PLATFORM_SDL1_RENDER_SCENE_NEW_GAME_SETUP_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_SCENE_NEW_GAME_SETUP_RENDER_H

#include <SDL/SDL.h>

#include "app/input_flow.h"
#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"

/* SDL-owned new-game setup drawing. Hit tests, selection mutation and both
 * animation timelines belong to app/new_game_setup_flow.h. */

void draw_new_game_setup(SDL_Surface *screen,
                         const GamePreviewAssets *assets,
                         const NewGameSettings *settings,
                         const VirtualCursor *cursor,
                         int transition);

#endif
