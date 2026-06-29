#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_MINIMAP_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_MINIMAP_RENDER_H

#include <SDL/SDL.h>

#include "app/new_game_setup_flow.h"
#include "platform/sdl1/app_asset_types.h"
#include "platform/sdl1/app_panel_types.h"
#include "view/game_view_model.h"

/* R10.1c: minimap cluster, moved verbatim out of runtime_main.c. Covers the
 * always-on minimap panel (draw_game_minimap_panel), the full-screen overview
 * overlay (draw_game_small_map_panel), and the shared player-model sprite
 * primitives both use to place characters (draw_game_player_model /
 * draw_game_player_model_part), which the not-yet-extracted main map view
 * (draw_game_map_objects) also calls. */

int draw_game_small_map_panel(SDL_Surface *screen,
                              const GamePreviewAssets *assets,
                              const NewGameSettings *settings,
                              const GameViewModel *view,
                              const SmallMapPanelState *panel);

int draw_game_player_model_part(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                int character,
                                int part,
                                int visual_direction,
                                int moving,
                                int walk_frame,
                                int center_x,
                                int center_y);

int draw_game_player_model(SDL_Surface *screen,
                           const GamePreviewAssets *assets,
                           const GameViewModel *view,
                           int player_index,
                           int center_x,
                           int center_y,
                           int include_overlay);

int draw_game_minimap_panel(SDL_Surface *screen,
                            const GamePreviewAssets *assets,
                            const NewGameSettings *settings,
                            const GameViewModel *view,
                            int view_mode);

#endif
