#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_MAP_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_MAP_RENDER_H

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "view/game_view_model.h"

/* R10.1c: main map cluster, moved verbatim out of runtime_main.c. Covers the
 * ground-tile textured background (draw_game_main_map_view) and the unified
 * actor/property/map-object paint queue (draw_game_map_objects), the last
 * caller of game_minimap_render.h's draw_game_player_model /
 * draw_game_player_model_part. */

int draw_game_main_map_view(SDL_Surface *screen,
                            const GamePreviewAssets *assets,
                            const NewGameSettings *settings,
                            const GameViewModel *view);

void game_reset_ground_render_cache(void);

void draw_game_map_objects(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const NewGameSettings *settings,
                           const GameViewModel *view,
                           const GameMapDynamicObjectState *dynamic_objects);

#endif
