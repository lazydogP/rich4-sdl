#ifndef RICH4_PLATFORM_SDL1_RENDER_GAME_PANEL_RENDER_H
#define RICH4_PLATFORM_SDL1_RENDER_GAME_PANEL_RENDER_H

#include <stddef.h>

#include <SDL/SDL.h>

#include "platform/sdl1/app_asset_types.h"

/* R10.1c: stateless Panel.mkf chunk / FLC graph-frame draw primitives, moved
 * verbatim out of runtime_main.c. These are the shared drawing layer the game
 * sub-scenes (dice, minimap, hud, map, event) build on. No scene state; only
 * assets + screen. Analogous to render/blit.c but for the game panel/graph
 * chunk indirection. */

int draw_game_panel_chunk(SDL_Surface *screen,
                          const GamePreviewAssets *assets,
                          int resource_index,
                          size_t chunk_index,
                          int x,
                          int y,
                          int transparent_zero);

int draw_game_panel_chunk_at(SDL_Surface *screen,
                             const GamePreviewAssets *assets,
                             int resource_index,
                             size_t chunk_index,
                             int center_x,
                             int center_y,
                             int transparent_zero);

/* SMP-hotspot-anchored variant: (anchor_x, anchor_y) is the point the
 * chunk's own x/y hotspot should land on, per sub_456418. Shared by the
 * confinement/lottery/special-event/shop event-cluster renderers. */
int draw_confinement_panel_chunk(SDL_Surface *screen,
                                 const GamePreviewAssets *assets,
                                 int resource,
                                 size_t chunk_index,
                                 int anchor_x,
                                 int anchor_y);

int draw_game_graph_frame_step_data(SDL_Surface *screen,
                                    const unsigned char *data,
                                    size_t size,
                                    int frame_index,
                                    int x,
                                    int y,
                                    const unsigned char *source_pixels,
                                    int source_pitch);

int draw_game_panel_graph_frame_step(SDL_Surface *screen,
                                     const GamePreviewAssets *assets,
                                     int resource_index,
                                     int frame_index,
                                     int x,
                                     int y,
                                     const unsigned char *source_pixels,
                                     int source_pitch);

int draw_game_panel_graph_frame(SDL_Surface *screen,
                                const GamePreviewAssets *assets,
                                int resource_index,
                                int frame_index,
                                int x,
                                int y);

int draw_game_graph_frame_data(SDL_Surface *screen,
                               const unsigned char *data,
                               size_t size,
                               int frame_index,
                               int x,
                               int y);

#endif
