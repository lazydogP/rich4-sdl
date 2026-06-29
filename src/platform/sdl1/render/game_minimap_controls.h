#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_MINIMAP_CONTROLS_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_MINIMAP_CONTROLS_H

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"
#include "view/game_view_model.h"

void draw_game_minimap_rotate_buttons(SDL_Surface *screen,
                                      const GamePreviewAssets *assets,
                                      const GameViewModel *view,
                                      int minimap_y);

#endif
